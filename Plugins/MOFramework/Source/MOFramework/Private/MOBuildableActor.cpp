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
#include "MOUIContractInterface.h"
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
		InteractableComponent->OnHandleSecondaryInteract.BindUObject(this, &AMOBuildableActor::HandleSecondaryInteract);
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

void AMOBuildableActor::RestoreGhostCollisionCache()
{
	for (const TPair<TObjectPtr<UPrimitiveComponent>, TEnumAsByte<ECollisionEnabled::Type>>& Entry : CachedGhostCollisionStates)
	{
		if (IsValid(Entry.Key))
		{
			Entry.Key->SetCollisionEnabled(Entry.Value);
		}
	}
	CachedGhostCollisionStates.Reset();
}

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

	// Stay in ghost mode until construction completes - just enable interaction
	// The ghost visual remains until OnConstructionCompleted is called
	bIsGhost = false; // We're no longer in "placement preview" mode

	// Restore collision on every primitive that SetGhostMode(true) disabled.
	// Critical for hit-trace interactions — without this, the placed building's
	// InteractableComponent (and any BP-added collision triggers) stay at
	// NoCollision and the player can't right-click to manage construction.
	RestoreGhostCollisionCache();

	// Backstop for the rare case the placed building was spawned directly
	// (not via the placement pipeline → no cache entry for MeshComponent).
	if (MeshComponent && MeshComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	// Enable interaction so player can configure and start build
	if (InteractableComponent)
	{
		InteractableComponent->SetCanInteract(true);
	}

	// Keep the ghost material - it will be restored when construction completes
	// SetGhostMode(false) is NOT called here - we want to keep the translucent look

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildableActor] Initialized building with recipe: %s (ghost visual maintained until built)"), *RecipeId.ToString());
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

		// Disable collision on EVERY primitive component on the actor — not
		// just MeshComponent. BPs commonly add their own collision shapes
		// (interaction triggers, child meshes, decoration components) that
		// otherwise stay solid and block / push the player. Cache the
		// original state per-component so we can restore exactly on exit.
		CachedGhostCollisionStates.Reset();
		TArray<UPrimitiveComponent*> Prims;
		GetComponents<UPrimitiveComponent>(Prims);
		for (UPrimitiveComponent* Prim : Prims)
		{
			if (!IsValid(Prim)) continue;
			CachedGhostCollisionStates.Add(Prim, Prim->GetCollisionEnabled());
			Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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

		// Restore collision on every primitive we disabled on entry. Anything
		// not in the cache (added since SetGhostMode(true)) is left at its
		// current state — that's the BP-author's default and we shouldn't
		// override it.
		RestoreGhostCollisionCache();

		// Safety backstop: if the actor exited ghost mode without ever
		// entering (e.g., a directly-spawned non-ghost path), MeshComponent
		// still needs the placed-building collision state.
		if (MeshComponent && MeshComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
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

bool AMOBuildableActor::HandleSecondaryInteract(AController* Controller)
{
	EMOBuildState State = GetBuildState();

	switch (State)
	{
	case EMOBuildState::Ghost:
	case EMOBuildState::Paused:
		// Right-click on ghost opens the build context menu
		OnGhostInteracted(Controller);
		return true;

	case EMOBuildState::Constructing:
		// Could show status or allow pause
		return true;

	case EMOBuildState::Complete:
		// Right-click on complete building could open context menu
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
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildableActor] Ghost interacted - showing build widget"));

	// Use interface to request UI - decouples from specific controller type
	if (Controller && Controller->Implements<UMOUIContractInterface>())
	{
		IMOUIContractInterface::Execute_RequestShowBuildWidget(Controller, this);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildableActor] Controller does not implement IMOUIContractInterface"));
	}
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
	else if (State == EMOBuildState::Ghost || State == EMOBuildState::Paused)
	{
		// Restore ghost visual for incomplete buildings
		// bIsGhost tracks placement preview mode, not build state
		// We need ghost material but with collision/interaction enabled
		CreateGhostMaterial();

		// Enable collision and interaction for placed ghost
		if (MeshComponent)
		{
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		if (InteractableComponent)
		{
			InteractableComponent->SetCanInteract(true);
		}

		UE_LOG(LogMOFramework, Log, TEXT("[MOBuildableActor] Restored ghost visual for building in state: %d"), (int32)State);
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

// ============================================================================
// IMOIdentifiableInterface IMPLEMENTATION
// ============================================================================

UMOIdentityComponent* AMOBuildableActor::GetIdentityComponent_Implementation() const
{
	return IdentityComponent;
}

FGuid AMOBuildableActor::GetPersistentGuid_Implementation() const
{
	return IdentityComponent ? IdentityComponent->GetGuid() : FGuid();
}

bool AMOBuildableActor::HasValidIdentity_Implementation() const
{
	return IdentityComponent && IdentityComponent->GetGuid().IsValid();
}
