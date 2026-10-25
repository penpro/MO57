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
#include "MOBuildingComponent.h"
#include "MORecipeDatabaseSettings.h"
#include "MOworldSaveGame.h"
#include "MOFramework.h"
#include "MOUIContractInterface.h"
#include "MOQuestSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"

AMOBuildableActor::AMOBuildableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Replicate so server-spawned buildings appear on all clients (H19).
	bReplicates = true;
	SetReplicateMovement(false); // buildings are static once placed

	// Create root scene component
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootSceneComponent);

	// Create mesh component
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(RootSceneComponent);
	// Default to IgnoreOnlyPawn for the under-construction state:
	//   - Blocks Visibility / Camera / WorldStatic / WorldDynamic so the
	//     interactor's LineTraceSingleByChannel(ECC_Visibility) lands and
	//     right-click opens the build context menu.
	//   - Ignores Pawn so the player walks through unfinished structures.
	// OverlapAll does NOT work here — LineTrace*ByChannel skips overlap
	// responses, so the trace would pass straight through the building.
	// OnConstructionCompleted promotes this to BlockAll when the building
	// is finished. BP subclasses inherit this default.
	MeshComponent->SetCollisionProfileName(TEXT("IgnoreOnlyPawn"));

	// Create identity component
	IdentityComponent = CreateDefaultSubobject<UMOIdentityComponent>(TEXT("Identity"));

	// Create interactable component
	InteractableComponent = CreateDefaultSubobject<UMOInteractableComponent>(TEXT("Interactable"));

	// Create build progress component
	BuildProgressComponent = CreateDefaultSubobject<UMOBuildProgressComponent>(TEXT("BuildProgress"));
}

void AMOBuildableActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMOBuildableActor, RecipeId);
}

void AMOBuildableActor::OnRep_RecipeId()
{
	// A server-spawned building just replicated its recipe; reconstruct local visuals/
	// collision/interaction from it (mirrors the authority InitializeBuilding path).
	if (!RecipeId.IsNone())
	{
		InitializeBuilding(RecipeId);
	}
}

void AMOBuildableActor::BeginPlay()
{
	Super::BeginPlay();

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
		BuildProgressComponent->OnConstructionStateChanged.AddDynamic(this, &AMOBuildableActor::HandleBuildStateChanged);
		BuildProgressComponent->OnConstructionProgress.AddDynamic(this, &AMOBuildableActor::HandleBuildProgress);
	}

	// Apply the unbuilt collision to every part of the building (GetBuildingMeshes).
	// This overrides any BP-set NoCollision on the visible cube child so the
	// right-click trace lands. EnterCompletedState makes the Mesh subtree BlockAll
	// and gives every other part its Blueprint collision back when it is finished.
	//
	// Skip this for ghost mode — SetGhostMode(true) caches+disables all primitives
	// and we'd undo its work. The ghost path restores collision via the cache
	// when the building leaves ghost mode.
	if (!bIsGhost)
	{
		ApplyUnbuiltCollision();
	}

	// The planned look shows only while a player of this machine is placing (ApplyGhostVisibility).
	LocalPlacementModeHandle = UMOBuildingComponent::OnLocalPlacementModeChanged().AddUObject(this, &AMOBuildableActor::HandleLocalPlacementModeChanged);

	// A client that joins after the building was finished gets its replicated state before BeginPlay bound OnConstructionCompleted above.
	if (!HasAuthority() && IsComplete())
	{
		EnterCompletedState();
	}
}

void AMOBuildableActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UMOBuildingComponent::OnLocalPlacementModeChanged().Remove(LocalPlacementModeHandle);
	LocalPlacementModeHandle.Reset();
	Super::EndPlay(EndPlayReason);
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

	// Apply the unbuilt collision to every part of the building (GetBuildingMeshes).
	// This is the single source of truth for under-construction collision --
	// overrides any BP NoCollision on the visible cube child so right-click
	// traces land, and nothing else collides with a plan (ApplyUnbuiltCollisionTo).
	ApplyUnbuiltCollision();

	// Enable interaction so player can configure and start build
	if (InteractableComponent)
	{
		InteractableComponent->SetCanInteract(true);
	}

	// A placed building wears the planned look until it is finished (RefreshBuildStateVisual). On a client the replicated build state can arrive
	// before the recipe (OnRep_RecipeId lands here): a finished building looks finished.
	if (!HasAuthority() && IsComplete() && HasActorBegunPlay())
	{
		EnterCompletedState();
	}
	else
	{
		RefreshBuildStateVisual();
	}

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
		// Enter ghost mode: the whole preview in the ghost material (the tint follows in SetGhostVisual)
		ApplyGhostMaterialToAllMeshes();

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

	TintGhost(Color);
}

void AMOBuildableActor::TintGhost(const FLinearColor& Color, float InnerGlow)
{
	if (!GhostMaterialInstance)
	{
		return;
	}
	// M_BuildPreview (the ghost material) is an inner glow + outer rim: "InnerColor" / "OuterColor". This set only "Color" / "Opacity",
	// which it does not have -- the tint never changed and an invalid ghost stayed the preview's default green, so a player saw "fine
	// here" and the click did nothing (first-ten-minutes pass, Oct 15 2026). The inner glow is HDR (the authored green is ~6.6).
	GhostMaterialInstance->SetVectorParameterValue(TEXT("InnerColor"), FLinearColor(Color.R * InnerGlow, Color.G * InnerGlow, Color.B * InnerGlow, 1.0f));
	GhostMaterialInstance->SetVectorParameterValue(TEXT("OuterColor"), FLinearColor(Color.R, Color.G, Color.B, 1.0f));
	GhostMaterialInstance->SetVectorParameterValue(TEXT("Color"), Color);   // a ghost material with a single tint
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

void AMOBuildableActor::EnterCompletedState()
{
	SetCompletedVisual();

	// Promote the structural subtree from the unbuilt collision (the interaction
	// trace only, ApplyUnbuiltCollisionTo) to BlockAll now that the building is
	// real geometry. ApplyStructuralCollisionProfile propagates to every
	// static-mesh descendant of MeshComponent, so the visible cube child
	// becomes solid even if its BP-defined profile was NoCollision.
	ApplyStructuralCollisionProfile(FName("BlockAll"));

	// Every other part (the stick walls' staged sticks hang off the root, BR25-7) was unbuilt too: it gets the collision its Blueprint authored.
	TArray<UStaticMeshComponent*> Parts;
	GetBuildingMeshes(Parts);
	for (UStaticMeshComponent* Part : Parts)
	{
		if (Part != MeshComponent && !(MeshComponent && Part->IsAttachedTo(MeshComponent)))
		{
			RestoreAuthoredCollision(Part);
		}
	}
}

void AMOBuildableActor::FinishAsBuilt(FName InRecipeId)
{
	if (!HasAuthority())
	{
		return;
	}
	if (!InRecipeId.IsNone())
	{
		InitializeBuilding(InRecipeId);
	}
	if (BuildProgressComponent)
	{
		FMOBuildProgress Done;
		BuildProgressComponent->BuildSaveData(Done);
		Done.State = EMOBuildState::Complete;
		Done.ElapsedTime = Done.TotalBuildTime;
		BuildProgressComponent->ApplySaveData(Done);
	}
	EnterCompletedState();
}

void AMOBuildableActor::OnConstructionCompleted_Implementation()
{
	EnterCompletedState();

	// Notify the quest system. Tutorial_BuildCampfire and similar quests
	// listen for ItemCraft events keyed by RecipeId; the building system
	// wasn't routed through the crafting subsystem so we wire it directly
	// here. RecipeId is set in InitializeBuilding and is the recipe ROW name
	// (BuildCampfire, BuildFloor, etc) — not an item id.
	// The credit goes to the player whose pawn finished the construction, on their own machine (UMOQuestSubsystem::ReportForActor); it used to
	// go to whichever machine ran this -- the host -- so a joiner's "Construct a campfire" could never complete.
	if (!RecipeId.IsNone() && BuildProgressComponent)
	{
		// Construction runs on the server (UMOBuildingComponent::RequestBuildAction), so this fires there; on clients it fires again from the
		// replicated state (UMOBuildProgressComponent::OnRep_RepState) for the visuals above only.
		if (HasAuthority())
		{
			UMOQuestSubsystem::ReportForActor(BuildProgressComponent->GetCompletedBy(), EMOObjectiveType::ItemCraft, RecipeId, 1);
		}
	}

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
		// Repopulate recipe-derived BuildParts/weights FIRST. ApplySaveData restores only the
		// FMOBuildProgress payload (deposits/elapsed/state), NOT BuildParts -- so without this a
		// loaded in-progress building has an empty parts list: required materials read as zero,
		// AreAllMaterialsDeposited() is vacuously true, and Start Build finishes it for free.
		// InitializeFromRecipe resets Progress; the ApplySaveData below then overwrites it with
		// the saved deposits/state (order matters).
		if (const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(InRecord.RecipeId))
		{
			BuildProgressComponent->InitializeFromRecipe(*Recipe);
		}
		BuildProgressComponent->ApplySaveData(InRecord.Progress);
	}

	// Update visuals based on state. Complete goes through the shared
	// completed-state routine: BeginPlay applied the unbuilt collision before this
	// ran, so restoring visuals alone left loaded walls walk-through.
	EMOBuildState State = GetBuildState();
	if (State == EMOBuildState::Complete)
	{
		EnterCompletedState();
	}
	else if (State == EMOBuildState::Constructing)
	{
		RefreshBuildStateVisual();
		SetConstructionVisual(InRecord.Progress.GetOverallProgress());
	}
	else if (State == EMOBuildState::Ghost || State == EMOBuildState::Paused)
	{
		// bIsGhost tracks placement preview mode, not build state: a loaded unfinished building wears the planned look, with interaction
		RefreshBuildStateVisual();

		// The unbuilt collision (the interaction trace only) and interaction for a placed plan
		ApplyUnbuiltCollision();
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

void AMOBuildableActor::RefreshBuildStateVisual()
{
	if (bIsGhost)
	{
		return;   // the placement preview: SetGhostMode / SetGhostVisual own its look
	}
	if (IsComplete())
	{
		RestoreOriginalMaterials();
		ApplyStagedParts(GetStagedPartComponents().Num());
		ApplyGhostVisibility();   // nothing wears the ghost now: whatever it hid comes back
		return;
	}
	if (ApplyGhostMaterialToAllMeshes())
	{
		TintGhost(PlannedColor, PlannedGlow);
	}
	if (!StagedBuildParts.IsEmpty() && BuildProgressComponent)
	{
		ApplyStagedParts(CountBuiltStagedParts(BuildProgressComponent->GetState(), BuildProgressComponent->GetProgress(), GetStagedPartComponents().Num()));
	}
	ApplyGhostVisibility();
}

void AMOBuildableActor::ApplyGhostVisibility()
{
	// The placement preview is always drawn (it IS the placing); a plan only while a player here is placing.
	const bool bShowPlans = bIsGhost || UMOBuildingComponent::IsLocalPlayerPlacing(GetWorld());
	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh))
		{
			continue;
		}
		const bool bWearsGhost = GhostMaterialInstance && Mesh->GetNumMaterials() > 0 && Mesh->GetMaterial(0) == GhostMaterialInstance;
		if (bWearsGhost && !bShowPlans)
		{
			if (Mesh->GetVisibleFlag())
			{
				Mesh->SetVisibility(false);
				GhostHiddenMeshes.Add(Mesh);
			}
		}
		else if (GhostHiddenMeshes.Remove(Mesh) > 0)
		{
			Mesh->SetVisibility(true);
		}
	}
}

void AMOBuildableActor::HandleLocalPlacementModeChanged()
{
	if (!bIsGhost && HasActorBegunPlay())
	{
		ApplyGhostVisibility();
	}
}

void AMOBuildableActor::GetBuildingMeshes(TArray<UStaticMeshComponent*>& OutMeshes) const
{
	TArray<UStaticMeshComponent*> All;
	GetComponents<UStaticMeshComponent>(All);
	for (UStaticMeshComponent* SMC : All)
	{
		if (IsValid(SMC) && (SMC == MeshComponent || SMC->GetStaticMesh()))
		{
			OutMeshes.Add(SMC);
		}
	}
}

void AMOBuildableActor::ApplyUnbuiltCollision()
{
	TArray<UStaticMeshComponent*> Parts;
	GetBuildingMeshes(Parts);
	for (UStaticMeshComponent* Part : Parts)
	{
		ApplyUnbuiltCollisionTo(Part);
	}
}

void AMOBuildableActor::RestoreAuthoredCollision(UPrimitiveComponent* Primitive)
{
	if (!IsValid(Primitive))
	{
		return;
	}
	const UPrimitiveComponent* Authored = Cast<UPrimitiveComponent>(Primitive->GetArchetype());
	if (!Authored || Authored == Primitive)
	{
		Primitive->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		return;
	}
	const FName Profile = Authored->GetCollisionProfileName();
	Primitive->SetCollisionProfileName(Profile);
	if (Profile == UCollisionProfile::CustomCollisionProfileName)
	{
		// A custom profile is its own settings: copy them (a named profile carries them with its name).
		Primitive->SetCollisionObjectType(Authored->GetCollisionObjectType());
		Primitive->SetCollisionResponseToChannels(Authored->GetCollisionResponseToChannels());
		Primitive->SetCollisionEnabled(Authored->GetCollisionEnabled());
	}
}

void AMOBuildableActor::ApplyUnbuiltCollisionTo(UPrimitiveComponent* Primitive)
{
	if (!IsValid(Primitive))
	{
		return;
	}
	Primitive->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Primitive->SetCollisionObjectType(ECC_WorldDynamic);
	Primitive->SetCollisionResponseToAllChannels(ECR_Ignore);
	Primitive->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);   // the interaction trace (UMOInteractorComponent::TraceChannel)
}

void AMOBuildableActor::HandleBuildStateChanged(EMOBuildState NewState)
{
	RefreshBuildStateVisual();
}

bool AMOBuildableActor::ApplyGhostMaterialToAllMeshes()
{
	if (!GhostMaterialInstance)
	{
		UMaterialInterface* BaseMaterial = GhostMaterialBase.LoadSynchronous();
		if (!BaseMaterial)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildableActor] %s: no GhostMaterialBase set, it keeps its own materials while unbuilt"), *GetName());
			return false;
		}
		GhostMaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		if (!GhostMaterialInstance)
		{
			return false;
		}
	}
	// EVERY mesh: a Blueprint's visible geometry is usually its own child components (the stick layers of a floor), not MeshComponent --
	// ghosting MeshComponent alone left an unbuilt floor looking finished.
	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh) || Mesh->GetNumMaterials() == 0)
		{
			continue;
		}
		UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Mesh);
		if (!SavedMaterials.Contains(Mesh))
		{
			FMOBuildableSavedMaterials& Saved = SavedMaterials.Add(Mesh);
			for (int32 i = 0; i < Mesh->GetNumMaterials(); ++i)
			{
				Saved.Materials.Add(Mesh->GetMaterial(i));
			}
			Saved.bForceDisableNanite = StaticMesh && StaticMesh->IsForceDisableNanite();
		}
		// The ghost material is translucent; Nanite draws only opaque / masked materials and gives anything else the engine's default grey material
		// ("Invalid material ... used on Nanite static mesh", Oct 19 playtest: the campfire stones' preview and ghost). The non-Nanite fallback draws it.
		if (StaticMesh && !StaticMesh->IsForceDisableNanite())
		{
			StaticMesh->SetForceDisableNanite(true);
		}
		for (int32 i = 0; i < Mesh->GetNumMaterials(); ++i)
		{
			Mesh->SetMaterial(i, GhostMaterialInstance);
		}
	}
	return true;
}

void AMOBuildableActor::ApplyStructuralCollisionProfile(FName ProfileName)
{
	if (!MeshComponent)
	{
		return;
	}

	MeshComponent->SetCollisionProfileName(ProfileName);

	// Propagate to every static-mesh descendant of MeshComponent. Recursive
	// (true) so deeply-nested BP hierarchies still work.
	TArray<USceneComponent*> Descendants;
	MeshComponent->GetChildrenComponents(/*bIncludeAllDescendants=*/true, Descendants);
	int32 PropagatedCount = 0;
	for (USceneComponent* Child : Descendants)
	{
		UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Child);
		if (!IsValid(SMC) || !SMC->GetStaticMesh())
		{
			continue;
		}
		SMC->SetCollisionProfileName(ProfileName);
		++PropagatedCount;
	}

	// DEBUG (MOBuildableActor, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose,
	// 	TEXT("[MOBuildableActor] %s: applied structural profile '%s' to MeshComponent + %d descendant(s)"),
	// 	*GetName(), *ProfileName.ToString(), PropagatedCount);
}

void AMOBuildableActor::RestoreOriginalMaterials()
{
	for (TPair<TObjectPtr<UMeshComponent>, FMOBuildableSavedMaterials>& Pair : SavedMaterials)
	{
		RestoreOriginalMaterialsOn(Pair.Key.Get());
	}
	SavedMaterials.Reset();
	GhostMaterialInstance = nullptr;
}

void AMOBuildableActor::RestoreOriginalMaterialsOn(UMeshComponent* Mesh)
{
	// The saved entry stays: a staged part turned real mid-build keeps it, so the next ghost pass does not take the ghost for its original.
	const FMOBuildableSavedMaterials* Saved = IsValid(Mesh) ? SavedMaterials.Find(Mesh) : nullptr;
	if (!Saved)
	{
		return;
	}
	const int32 Num = FMath::Min(Mesh->GetNumMaterials(), Saved->Materials.Num());
	for (int32 i = 0; i < Num; ++i)
	{
		Mesh->SetMaterial(i, Saved->Materials[i]);   // a null original clears the override: the mesh asset's own material again
	}
	if (UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Mesh))
	{
		StaticMesh->SetForceDisableNanite(Saved->bForceDisableNanite);   // built: Nanite again (the ghost had forced the fallback)
	}
}

int32 AMOBuildableActor::CountBuiltStagedParts(EMOBuildState State, float Progress, int32 NumParts)
{
	if (NumParts <= 0 || State == EMOBuildState::Ghost)
	{
		return 0;
	}
	if (State == EMOBuildState::Complete)
	{
		return NumParts;
	}
	// A part is built once progress reaches its share; the small epsilon keeps an exact share (5 of 10 at 0.5) from flickering on float error.
	return FMath::Clamp(FMath::FloorToInt(FMath::Clamp(Progress, 0.0f, 1.0f) * NumParts + 1.0e-4f), 0, NumParts);
}

const TArray<TWeakObjectPtr<UMeshComponent>>& AMOBuildableActor::GetStagedPartComponents()
{
	if (!bStagedPartsResolved)
	{
		bStagedPartsResolved = true;
		TArray<UMeshComponent*> Meshes;
		GetComponents<UMeshComponent>(Meshes);
		for (const FName PartName : StagedBuildParts)
		{
			UMeshComponent* const* Found = Meshes.FindByPredicate([PartName](const UMeshComponent* Mesh) { return Mesh && Mesh->GetFName() == PartName; });
			if (Found)
			{
				StagedPartComponents.Add(*Found);
			}
			else
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildableActor] %s: staged part '%s' is not one of its mesh components"), *GetName(), *PartName.ToString());
			}
		}
	}
	return StagedPartComponents;
}

void AMOBuildableActor::ApplyStagedParts(int32 Built)
{
	ShownBuiltStagedParts = Built;
	const TArray<TWeakObjectPtr<UMeshComponent>>& Parts = GetStagedPartComponents();
	if (Parts.IsEmpty())
	{
		return;
	}
	const EMOBuildState State = BuildProgressComponent ? BuildProgressComponent->GetState() : EMOBuildState::Complete;
	const bool bHideUnbuilt = UnbuiltStagedPartStyle == EMOStagedPartStyle::Hidden && (State == EMOBuildState::Constructing || State == EMOBuildState::Paused);
	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		UMeshComponent* Mesh = Parts[i].Get();
		if (!IsValid(Mesh))
		{
			continue;
		}
		if (i < Built)
		{
			RestoreOriginalMaterialsOn(Mesh);
			Mesh->SetVisibility(true);
		}
		else
		{
			Mesh->SetVisibility(!bHideUnbuilt);   // the ghost pass already gave it the planned look
		}
	}
}

void AMOBuildableActor::HandleBuildProgress(float Progress)
{
	if (StagedBuildParts.IsEmpty() || bIsGhost || !BuildProgressComponent)
	{
		return;
	}
	if (CountBuiltStagedParts(BuildProgressComponent->GetState(), Progress, GetStagedPartComponents().Num()) != ShownBuiltStagedParts)
	{
		RefreshBuildStateVisual();
	}
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
