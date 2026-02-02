/**
 * =============================================================================
 * MOBuildableActor.cpp - Implementation
 * =============================================================================
 *
 * DELEGATE CONNECTIONS ESTABLISHED IN THIS FILE:
 *
 * BeginPlay():
 *   InteractableComponent->OnHandleInteract → HandleInteract()
 *     - Routes player interaction to our state-based handler
 *     - Interaction flow: Player->Interact->InteractorComponent->ServerInteract
 *       ->InteractableComponent->OnHandleInteract->HandleInteract()
 *
 *   BuildProgressComponent->OnConstructionCompleted → OnConstructionCompleted()
 *     - Called when timed construction finishes
 *     - Updates visual state to completed appearance
 *
 * KEY METHODS:
 *   HandleInteract() - State machine for interaction behavior
 *   InitializeBuilding() - Transitions from placement ghost to placed building
 *   SetGhostMode() - Toggles between ghost and solid states
 *   BuildSaveData()/ApplySaveData() - Persistence support
 *
 * =============================================================================
 */

#include "MOBuildableActor.h"
#include "MOIdentityComponent.h"
#include "MOInteractableComponent.h"
#include "MOBuildProgressComponent.h"
#include "MORecipeDatabaseSettings.h"
#include "MOworldSaveGame.h"
#include "MOFramework.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AMOBuildableActor::AMOBuildableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create root scene component
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootSceneComponent);

	// Create mesh component
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(RootSceneComponent);
	MeshComponent->SetCollisionProfileName(TEXT("BlockAll"));

	// Create identity component
	IdentityComponent = CreateDefaultSubobject<UMOIdentityComponent>(TEXT("Identity"));

	// Create interactable component
	InteractableComponent = CreateDefaultSubobject<UMOInteractableComponent>(TEXT("Interactable"));

	// Create build progress component
	BuildProgressComponent = CreateDefaultSubobject<UMOBuildProgressComponent>(TEXT("BuildProgress"));
}

void AMOBuildableActor::BeginPlay()
{
	Super::BeginPlay();

	// Save original materials for restoration when exiting ghost mode
	SaveOriginalMaterials();

	// ==========================================================================
	// DELEGATE BINDING: Interaction Routing
	// ==========================================================================
	// The InteractableComponent receives interaction requests from players.
	// Its OnHandleInteract delegate is a C++ delegate (not dynamic) that allows
	// the owning actor to provide custom interaction behavior.
	//
	// Interaction flow:
	// 1. Player presses interact key
	// 2. InteractorComponent on player performs line trace
	// 3. Hit actor's InteractableComponent receives ServerInteract() call
	// 4. InteractableComponent validates (CanInteract) then calls HandleInteract()
	// 5. HandleInteract() checks if OnHandleInteract is bound
	// 6. If bound (which it is here), calls our HandleInteract() below
	// 7. We route to OnGhostInteracted or OnCompleteInteracted based on state
	//
	// This binding makes the buildable actor the handler for its own interactions.
	// ==========================================================================
	if (InteractableComponent)
	{
		InteractableComponent->OnHandleInteract.BindUObject(this, &AMOBuildableActor::HandleInteract);
	}

	// ==========================================================================
	// DELEGATE BINDING: Construction Completion
	// ==========================================================================
	// The BuildProgressComponent tracks timed construction and broadcasts
	// OnConstructionCompleted when ElapsedTime >= TotalBuildTime.
	//
	// This dynamic multicast delegate allows Blueprint extension via
	// BlueprintNativeEvent. Our OnConstructionCompleted_Implementation updates
	// visuals to the completed state.
	//
	// Note: Using AddDynamic for dynamic multicast delegates.
	// ==========================================================================
	if (BuildProgressComponent)
	{
		BuildProgressComponent->OnConstructionCompleted.AddDynamic(this, &AMOBuildableActor::OnConstructionCompleted);
	}
}

// ============================================================================
// STATE
// ============================================================================

EMOBuildState AMOBuildableActor::GetBuildState() const
{
	if (BuildProgressComponent)
	{
		return BuildProgressComponent->GetState();
	}
	return bIsGhost ? EMOBuildState::Ghost : EMOBuildState::Complete;
}

bool AMOBuildableActor::IsComplete() const
{
	return GetBuildState() == EMOBuildState::Complete;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void AMOBuildableActor::InitializeBuilding(FName InRecipeId)
{
	RecipeId = InRecipeId;

	// Get recipe definition
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildableActor] Recipe not found: %s"), *RecipeId.ToString());
		return;
	}

	// Initialize build progress
	if (BuildProgressComponent)
	{
		BuildProgressComponent->InitializeFromRecipe(*Recipe);
	}

	// Exit ghost mode (now a real placed building)
	SetGhostMode(false);

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildableActor] Initialized building with recipe: %s"), *RecipeId.ToString());
}

void AMOBuildableActor::SetGhostMode(bool bGhost)
{
	if (bIsGhost == bGhost)
	{
		return;
	}

	bIsGhost = bGhost;

	if (bGhost)
	{
		// Enter ghost mode
		CreateGhostMaterial();

		// Disable collision for ghost
		if (MeshComponent)
		{
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		// Disable interaction for ghost during placement
		if (InteractableComponent)
		{
			InteractableComponent->SetCanInteract(false);
		}
	}
	else
	{
		// Exit ghost mode - restore original materials
		RestoreOriginalMaterials();

		// Enable collision
		if (MeshComponent)
		{
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}

		// Enable interaction
		if (InteractableComponent)
		{
			InteractableComponent->SetCanInteract(true);
		}
	}
}

void AMOBuildableActor::SetGhostVisual(const FLinearColor& Color)
{
	if (!bIsGhost || !GhostMaterialInstance)
	{
		return;
	}

	GhostMaterialInstance->SetVectorParameterValue(TEXT("Color"), Color);
	GhostMaterialInstance->SetScalarParameterValue(TEXT("Opacity"), Color.A);
}

// ============================================================================
// VISUAL STATES
// ============================================================================

void AMOBuildableActor::SetConstructionVisual(float Progress)
{
	// Base implementation - subclasses can override for custom construction visuals
	// Could interpolate between ghost and final materials, show scaffolding, etc.
}

void AMOBuildableActor::SetCompletedVisual()
{
	// Ensure original materials are restored
	RestoreOriginalMaterials();
}

// ============================================================================
// COLLISION
// ============================================================================

bool AMOBuildableActor::IsOverlappingBlockingActors() const
{
	if (!MeshComponent)
	{
		return false;
	}

	TArray<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors);

	for (AActor* Actor : OverlappingActors)
	{
		// Ignore self
		if (Actor == this)
		{
			continue;
		}

		// Check if this is a blocking actor we care about
		// For now, any overlap with another buildable or world geometry is blocking
		if (Actor->IsA<AMOBuildableActor>())
		{
			return true;
		}

		// Could add more sophisticated collision checks here
	}

	return false;
}

// ============================================================================
// INTERACTION
// ============================================================================

bool AMOBuildableActor::HandleInteract(AController* Controller)
{
	EMOBuildState State = GetBuildState();

	switch (State)
	{
	case EMOBuildState::Ghost:
	case EMOBuildState::Paused:
		OnGhostInteracted(Controller);
		return true;

	case EMOBuildState::Constructing:
		// Currently constructing - could show progress or allow pause
		return true;

	case EMOBuildState::Complete:
		OnCompleteInteracted(Controller);
		return true;

	default:
		return false;
	}
}

FText AMOBuildableActor::GetInteractionText() const
{
	EMOBuildState State = GetBuildState();

	switch (State)
	{
	case EMOBuildState::Ghost:
	case EMOBuildState::Paused:
		return FText::FromString(TEXT("Build"));

	case EMOBuildState::Constructing:
		return FText::FromString(TEXT("Building..."));

	case EMOBuildState::Complete:
		return FText::FromString(TEXT("Use"));

	default:
		return FText::GetEmpty();
	}
}

bool AMOBuildableActor::CanInteract(AController* Controller) const
{
	// Can always interact unless we're in placement ghost mode
	return !bIsGhost || GetBuildState() != EMOBuildState::Ghost;
}

// ============================================================================
// EVENTS
// ============================================================================

void AMOBuildableActor::OnConstructionCompleted_Implementation()
{
	SetCompletedVisual();
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildableActor] Construction completed for: %s"), *GetName());
}

void AMOBuildableActor::OnGhostInteracted_Implementation(AController* Controller)
{
	// Base implementation - show build widget
	// This should be handled by UI manager in production
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildableActor] Ghost interacted - should show build widget"));
}

void AMOBuildableActor::OnCompleteInteracted_Implementation(AController* Controller)
{
	// Base implementation - override in subclasses for specific behavior
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildableActor] Complete building interacted"));
}

// ============================================================================
// SAVE/LOAD
// ============================================================================

void AMOBuildableActor::BuildSaveData(FMOPersistedBuildingRecord& OutRecord) const
{
	if (IdentityComponent)
	{
		OutRecord.BuildingGuid = IdentityComponent->GetGuid();
	}

	OutRecord.Transform = GetActorTransform();
	OutRecord.ActorClassPath = FSoftClassPath(GetClass());
	OutRecord.RecipeId = RecipeId;

	if (BuildProgressComponent)
	{
		BuildProgressComponent->BuildSaveData(OutRecord.Progress);
	}
}

void AMOBuildableActor::ApplySaveData(const FMOPersistedBuildingRecord& InRecord)
{
	RecipeId = InRecord.RecipeId;

	if (IdentityComponent)
	{
		IdentityComponent->SetGuid(InRecord.BuildingGuid);
	}

	SetActorTransform(InRecord.Transform);

	if (BuildProgressComponent)
	{
		BuildProgressComponent->ApplySaveData(InRecord.Progress);
	}

	// Update visuals based on state
	EMOBuildState State = GetBuildState();
	if (State == EMOBuildState::Complete)
	{
		SetCompletedVisual();
	}
	else if (State == EMOBuildState::Constructing)
	{
		SetConstructionVisual(InRecord.Progress.GetOverallProgress());
	}
}

// ============================================================================
// INTERNAL
// ============================================================================

void AMOBuildableActor::CreateGhostMaterial()
{
	if (!MeshComponent)
	{
		return;
	}

	UMaterialInterface* BaseMaterial = GhostMaterialBase.LoadSynchronous();
	if (!BaseMaterial)
	{
		// Create a simple translucent material if none specified
		// In production, you'd want to set this in the Blueprint
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildableActor] No GhostMaterialBase set, ghost will use original materials"));
		return;
	}

	GhostMaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	if (!GhostMaterialInstance)
	{
		return;
	}

	// Apply to all material slots
	int32 NumMaterials = MeshComponent->GetNumMaterials();
	for (int32 i = 0; i < NumMaterials; ++i)
	{
		MeshComponent->SetMaterial(i, GhostMaterialInstance);
	}
}

void AMOBuildableActor::SaveOriginalMaterials()
{
	if (!MeshComponent)
	{
		return;
	}

	OriginalMaterials.Empty();
	int32 NumMaterials = MeshComponent->GetNumMaterials();
	for (int32 i = 0; i < NumMaterials; ++i)
	{
		OriginalMaterials.Add(MeshComponent->GetMaterial(i));
	}
}

void AMOBuildableActor::RestoreOriginalMaterials()
{
	if (!MeshComponent || OriginalMaterials.Num() == 0)
	{
		return;
	}

	int32 NumMaterials = FMath::Min(MeshComponent->GetNumMaterials(), OriginalMaterials.Num());
	for (int32 i = 0; i < NumMaterials; ++i)
	{
		if (OriginalMaterials[i])
		{
			MeshComponent->SetMaterial(i, OriginalMaterials[i]);
		}
	}

	GhostMaterialInstance = nullptr;
}
