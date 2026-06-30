/**
 * =============================================================================
 * MOBuildingComponent.cpp - Building Placement Controller Implementation
 * =============================================================================
 *
 * COMPONENT OWNER: AMOPlayerController
 *
 * DELEGATE BROADCASTS FROM THIS COMPONENT:
 *
 *   OnPlacementModeEntered(FName RecipeId)
 *     - Broadcast in: EnterPlacementMode() after successful setup
 *     - Listeners: UI systems for mode indicators
 *     - Purpose: Notify systems that placement mode is active
 *
 *   OnPlacementModeExited(bool bPlaced)
 *     - Broadcast in: ExitPlacementMode()
 *     - bPlaced: true if building was successfully placed, false if cancelled
 *     - Listeners: UI systems, could be used for tutorials
 *
 *   OnGhostPlaced(AMOBuildableActor* Ghost)
 *     - Broadcast in: TryPlaceGhost() after successful placement
 *     - Listeners: Persistence system, could track placed buildings
 *
 * KEY METHODS AND THEIR ROLES:
 *
 *   EnterPlacementMode(RecipeId)
 *     - Validates recipe is a building type
 *     - Spawns ghost actor from recipe's BuildableActorClass
 *     - Enables tick for position updates
 *     - Switches input context to BaseBuilding
 *
 *   UpdateGhostPosition() [called every tick]
 *     - Performs camera line trace
 *     - Validates surface type against placement preferences
 *     - Calculates rotation from surface normal + user offset
 *     - Updates ghost transform and validity visual
 *
 *   TryPlaceGhost()
 *     - Called when player confirms placement (left click)
 *     - Validates current position is valid
 *     - Initializes building with recipe
 *     - Clears ghost reference (building is now permanent)
 *     - Broadcasts OnGhostPlaced
 *
 *   ExitPlacementMode(bCancel)
 *     - If cancelled, destroys ghost actor
 *     - Restores input context to PawnControl
 *     - Broadcasts OnPlacementModeExited
 *
 * INPUT INTEGRATION:
 *   HandlePlacementPrimaryAction() - Called from PlayerController on left click
 *   HandlePlacementSecondaryAction() - Called on right click/escape
 *   RotateGhostZ/X/Y() - Called from rotation input actions
 *
 * =============================================================================
 */

#include "MOBuildingComponent.h"
#include "MOBuildableActor.h"
#include "MOContainerActor.h"
#include "MOCraftingStationActor.h"
#include "MOPlayerController.h"
#include "MORecipeDatabaseSettings.h"
#include "MOFramework.h"
#include "MOQuestSubsystem.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace MOBuildingComponentInternal
{
	/**
	 * Fire a tutorial event via the quest subsystem on this actor's game
	 * instance. Used to surface "you just opened build mode / rotated /
	 * placed / cancelled" hints to the tutorial popup widget.
	 *
	 * Gated on IsAnyQuestListeningForEvent so once the build tutorials
	 * complete, the discrete-action firers (RotateGhostZ, TryPlaceGhost,
	 * etc.) cost a single set lookup and skip the iteration. Same pattern
	 * as the player controller's BroadcastTutorialEvent.
	 */
	void FireBuildingTutorialEvent(UObject* WorldContext, FName EventName)
	{
		if (!WorldContext) return;
		UWorld* World = WorldContext->GetWorld();
		if (!World) return;
		UGameInstance* GI = World->GetGameInstance();
		if (!GI) return;
		UMOQuestSubsystem* Quest = GI->GetSubsystem<UMOQuestSubsystem>();
		if (!Quest) return;
		if (!Quest->IsAnyQuestListeningForEvent(EventName)) return;
		Quest->FireGameEvent(EventName);
	}
}

UMOBuildingComponent::UMOBuildingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMOBuildingComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UMOBuildingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bInPlacementMode && IsValid(CurrentGhost))
	{
		UpdateGhostPosition();
	}
}

void UMOBuildingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clean up ghost if we're destroyed while in placement mode
	if (bInPlacementMode)
	{
		DestroyGhostActor();
	}

	Super::EndPlay(EndPlayReason);
}

// ============================================================================
// PLACEMENT MODE
// ============================================================================

bool UMOBuildingComponent::EnterPlacementMode(FName RecipeId)
{
	if (bInPlacementMode)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingComponent] Already in placement mode"));
		return false;
	}

	// Get recipe definition
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingComponent] Recipe not found: %s"), *RecipeId.ToString());
		return false;
	}

	if (!Recipe->bIsBuilding)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingComponent] Recipe %s is not a building"), *RecipeId.ToString());
		return false;
	}

	// Store current recipe info
	CurrentRecipeId = RecipeId;
	CurrentPlacementData = Recipe->PlacementData;
	UserRotationOffset = FRotator::ZeroRotator;
	SnapEdgeIndex = 0;
	UserFlipYawDeg = 0.0f;

	// Spawn ghost actor
	if (!SpawnGhostActor(*Recipe))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingComponent] Failed to spawn ghost for recipe: %s"), *RecipeId.ToString());
		CurrentRecipeId = NAME_None;
		return false;
	}

	bInPlacementMode = true;
	bCurrentPlacementValid = false;

	// Enable tick
	SetComponentTickEnabled(true);

	// Switch input context to building mode
	if (AMOPlayerController* PC = GetOwningPlayerController())
	{
		PC->SetInputContext(EMOInputContext::BaseBuilding, false);
	}

	OnPlacementModeEntered.Broadcast(RecipeId);
	MOBuildingComponentInternal::FireBuildingTutorialEvent(this, TEXT("OpenedBuildMenu"));

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingComponent] Entered placement mode for: %s"), *RecipeId.ToString());
	return true;
}

void UMOBuildingComponent::ExitPlacementMode(bool bCancel)
{
	if (!bInPlacementMode)
	{
		return;
	}

	// Clean up ghost
	DestroyGhostActor();

	// Disable tick
	SetComponentTickEnabled(false);

	// Reset state
	bInPlacementMode = false;
	FName ExitedRecipeId = CurrentRecipeId;
	CurrentRecipeId = NAME_None;
	bCurrentPlacementValid = false;

	// Switch input context back to pawn control
	if (AMOPlayerController* PC = GetOwningPlayerController())
	{
		PC->SetInputContext(EMOInputContext::PawnControl, true);
	}

	OnPlacementModeExited.Broadcast(!bCancel);
	MOBuildingComponentInternal::FireBuildingTutorialEvent(this, TEXT("ExitedBuildMode"));

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingComponent] Exited placement mode (cancelled: %s) for: %s"),
		bCancel ? TEXT("yes") : TEXT("no"), *ExitedRecipeId.ToString());
}

// ============================================================================
// GHOST CONTROL
// ============================================================================

void UMOBuildingComponent::UpdateGhostPosition()
{
	if (!IsValid(CurrentGhost))
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Get camera location and direction
	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
	FVector CameraDirection = CameraRotation.Vector();

	// Perform line trace
	FVector TraceEnd = CameraLocation + CameraDirection * MaxPlacementDistance;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(CurrentGhost);
	QueryParams.AddIgnoredActor(PC->GetPawn());

	FHitResult Hit;
	bool bHit = World->LineTraceSingleByChannel(Hit, CameraLocation, TraceEnd, PlacementTraceChannel, QueryParams);

	if (bHit)
	{
		LastPlacementHit = Hit;

		// Determine surface orientation
		const FVector SurfaceNormal = Hit.ImpactNormal;

		// Validate surface placement
		bool bValidSurface = ValidateSurfacePlacement(SurfaceNormal);

		// Calculate raw ghost rotation from surface + user offset.
		const FRotator RawRotation = CalculateGhostRotation(SurfaceNormal);

		// Edge-snap pulls the placement onto a neighbor's grid AND overrides
		// rotation to the neighbor's so adjacent pieces share an axis.
		// Returns raw location+rotation untouched if no neighbor in range —
		// feature is transparent for free placement.
		bool bDidSnap = false;
		FTransform SnappedXform = TryEdgeSnap(Hit.ImpactPoint, RawRotation, &bDidSnap);

		// User-applied in-place flip yaw: applied AFTER the snap result,
		// rotated around the ghost's CUBE CENTER so position stays
		// invariant. This lets the player mirror/cycle placement
		// orientation without disturbing the snap-aligned position
		// (e.g. swap which face of a wall points inward, flip a
		// triangular wall's slant side, or cycle a floor's grain
		// direction).
		if (!FMath::IsNearlyZero(UserFlipYawDeg))
		{
			const FBox GhostLocalBox = GetActorLocalBox(CurrentGhost);
			const FVector GhostLocalCenter = GhostLocalBox.IsValid
				? GhostLocalBox.GetCenter() : FVector::ZeroVector;
			const FQuat Flip(FVector::UpVector, FMath::DegreesToRadians(UserFlipYawDeg));
			const FQuat OldRot = SnappedXform.GetRotation();
			const FVector OldLoc = SnappedXform.GetLocation();
			const FVector CubeCenterWorld = OldLoc + OldRot.RotateVector(GhostLocalCenter);
			const FQuat NewRot = OldRot * Flip;
			const FVector NewLoc = CubeCenterWorld - NewRot.RotateVector(GhostLocalCenter);
			SnappedXform.SetRotation(NewRot);
			SnappedXform.SetLocation(NewLoc);
		}

		// Set ghost transform
		CurrentGhost->SetActorLocation(SnappedXform.GetLocation());
		CurrentGhost->SetActorRotation(SnappedXform.GetRotation());

		// Validity gating:
		//   - If the snap fired, the cursor's hit-surface type is irrelevant.
		//     The snap already vouched geometrically (cursor is near a known
		//     buildable, position computed accordingly). Otherwise placing a
		//     wall by aiming at another wall's vertical face would fail the
		//     ground-only ValidateSurfacePlacement, even though the snap
		//     correctly puts the new wall above/below.
		//   - If no snap, fall back to the surface-type validator (free
		//     placement still requires hitting an appropriate surface).
		bool bValidPlacement = bDidSnap || bValidSurface;
		if (CurrentPlacementData.bRequiresNoCollision && bValidPlacement)
		{
			bValidPlacement = !CheckGhostCollision();
		}

		// Update validity and visual
		if (bCurrentPlacementValid != bValidPlacement)
		{
			bCurrentPlacementValid = bValidPlacement;
			ApplyGhostMaterial(bValidPlacement);
		}
	}
	else
	{
		// No hit - place at max distance but invalid
		FVector PlacementPoint = CameraLocation + CameraDirection * MaxPlacementDistance;
		CurrentGhost->SetActorLocation(PlacementPoint);

		if (bCurrentPlacementValid)
		{
			bCurrentPlacementValid = false;
			ApplyGhostMaterial(false);
		}
	}
}

void UMOBuildingComponent::RotateGhostZ(float DeltaDegrees)
{
	if (!bInPlacementMode || !CurrentPlacementData.bAllowZRotation)
	{
		return;
	}

	UserRotationOffset.Yaw += DeltaDegrees;
	UserRotationOffset.Yaw = FMath::Fmod(UserRotationOffset.Yaw, 360.0f);

	MOBuildingComponentInternal::FireBuildingTutorialEvent(this, TEXT("RotatedGhost"));

	// Each Q/E press also cycles the snap-edge selection. Stack-snap uses
	// this to pick which of the target's 4 cardinal edges the ghost lands
	// against. We can't infer this from yaw because cube-symmetric ghosts
	// have no asymmetric "facing" to map from, and we can't infer it from
	// camera direction reliably (degenerates when the player looks down).
	// Each call to RotateGhostZ = one logical key press, so this advances
	// one edge per press regardless of the per-press rotation increment.
	if (DeltaDegrees > 0.0f)
	{
		SnapEdgeIndex = (SnapEdgeIndex + 1) % 4;
	}
	else if (DeltaDegrees < 0.0f)
	{
		SnapEdgeIndex = (SnapEdgeIndex + 3) % 4;
	}

	UE_LOG(LogMOFramework, Log,
		TEXT("[MOBuildingComponent] RotateGhostZ delta=%.2f -> UserYaw=%.2f SnapEdgeIndex=%d"),
		DeltaDegrees, UserRotationOffset.Yaw, SnapEdgeIndex);
}

void UMOBuildingComponent::ToggleGhostFlipYaw()
{
	if (!bInPlacementMode)
	{
		return;
	}

	MOBuildingComponentInternal::FireBuildingTutorialEvent(this, TEXT("FlippedGhost"));

	// Pick increment by ghost shape: flat (Z << min(X,Y)) = 90°, else 180°.
	// Falls back to 180° if the ghost isn't valid or has no bounds yet.
	float Increment = 180.0f;
	if (IsValid(CurrentGhost))
	{
		const FBox LocalBox = GetActorLocalBox(CurrentGhost);
		if (LocalBox.IsValid)
		{
			const FVector Extent = LocalBox.GetExtent();
			const float MinXY = FMath::Min(Extent.X, Extent.Y);
			if (Extent.Z < 0.5f * MinXY)
			{
				Increment = 90.0f;
			}
		}
	}

	UserFlipYawDeg = FMath::Fmod(UserFlipYawDeg + Increment, 360.0f);
	UE_LOG(LogMOFramework, Log,
		TEXT("[MOBuildingComponent] ToggleGhostFlipYaw +%.0f° -> total=%.0f°"),
		Increment, UserFlipYawDeg);
}

void UMOBuildingComponent::RotateGhostX(float DeltaDegrees)
{
	if (!bInPlacementMode || !CurrentPlacementData.bAllowXRotation)
	{
		return;
	}

	UserRotationOffset.Pitch += DeltaDegrees;
	UserRotationOffset.Pitch = FMath::Clamp(UserRotationOffset.Pitch, -90.0f, 90.0f);
}

void UMOBuildingComponent::RotateGhostY(float DeltaDegrees)
{
	if (!bInPlacementMode || !CurrentPlacementData.bAllowYRotation)
	{
		return;
	}

	UserRotationOffset.Roll += DeltaDegrees;
	UserRotationOffset.Roll = FMath::Clamp(UserRotationOffset.Roll, -90.0f, 90.0f);
}

// ============================================================================
// PLACEMENT
// ============================================================================

bool UMOBuildingComponent::TryPlaceGhost()
{
	if (!bInPlacementMode || !IsValid(CurrentGhost))
	{
		return false;
	}

	if (!bCurrentPlacementValid)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingComponent] Cannot place - invalid placement"));
		return false;
	}

	// Client: the ghost is a client-only preview and cannot be promoted across the wire.
	// Forward the resolved placement to the server (which spawns the real replicated
	// building), then respawn a fresh local ghost so the player can chain placements (H19).
	if (!GetOwner()->HasAuthority())
	{
		ServerPlaceBuilding(CurrentRecipeId, CurrentGhost->GetActorTransform());

		const FMORecipeDefinitionRow* ChainRecipe = UMORecipeDatabaseSettings::GetRecipeDefinition(CurrentRecipeId);
		CurrentGhost->Destroy();
		CurrentGhost = nullptr;
		if (ChainRecipe && SpawnGhostActor(*ChainRecipe))
		{
			bCurrentPlacementValid = false;
		}
		else
		{
			ExitPlacementMode(false);
		}
		return true;
	}

	// The ghost becomes the actual building - just finalize its placement.
	AMOBuildableActor* PlacedBuilding = CurrentGhost;
	PlacedBuilding->InitializeBuilding(CurrentRecipeId);

	// Detach: the actor we just finalized is now a permanent building, not
	// our ghost. Clear the reference so the spawn below creates a fresh ghost.
	CurrentGhost = nullptr;

	OnGhostPlaced.Broadcast(PlacedBuilding);
	MOBuildingComponentInternal::FireBuildingTutorialEvent(this, TEXT("PlacedGhost"));
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingComponent] Placed building: %s"), *PlacedBuilding->GetName());

	// Stay in placement mode and spawn a fresh ghost for the same recipe so
	// the player can chain placements. CurrentRecipeId, CurrentPlacementData,
	// and UserRotationOffset are deliberately preserved — players typically
	// place a run of identical pieces (a wall of floors, a row of walls) and
	// resetting rotation between placements would feel hostile.
	//
	// Player still has Escape / right-click to leave the mode explicitly
	// (HandlePlacementSecondaryAction → ExitPlacementMode(true)).
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(CurrentRecipeId);
	if (Recipe && SpawnGhostActor(*Recipe))
	{
		// New ghost not yet positioned — next Tick's UpdateGhostPosition
		// will trace + place it. Mark invalid until then.
		bCurrentPlacementValid = false;
	}
	else
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOBuildingComponent] Could not respawn ghost after placement (recipe=%s); exiting placement mode"),
			*CurrentRecipeId.ToString());
		ExitPlacementMode(false);
	}

	return true;
}

void UMOBuildingComponent::ServerPlaceBuilding_Implementation(FName InRecipeId, FTransform PlacementTransform)
{
	UWorld* World = GetWorld();
	if (!World || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(InRecipeId);
	if (!Recipe || !Recipe->bIsBuilding)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingComponent] ServerPlaceBuilding: invalid building recipe %s"), *InRecipeId.ToString());
		return;
	}

	// Resolve the buildable class the same way SpawnGhostActor does.
	TSubclassOf<AMOBuildableActor> ActorClass = Recipe->PlacementData.BuildableActorClass;
	if (!ActorClass)
	{
		if (Recipe->ContainerSlotCount > 0)
		{
			ActorClass = AMOContainerActor::StaticClass();
		}
		else if (Recipe->ProvidedStationType != EMOCraftingStation::None)
		{
			ActorClass = AMOCraftingStationActor::StaticClass();
		}
		else
		{
			ActorClass = AMOBuildableActor::StaticClass();
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AMOBuildableActor* Placed = World->SpawnActor<AMOBuildableActor>(ActorClass, PlacementTransform, SpawnParams);
	if (!Placed)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingComponent] ServerPlaceBuilding: spawn failed for recipe %s"), *InRecipeId.ToString());
		return;
	}

	Placed->InitializeBuilding(InRecipeId);
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingComponent] ServerPlaceBuilding: spawned %s for recipe %s"), *Placed->GetName(), *InRecipeId.ToString());
}

void UMOBuildingComponent::HandlePlacementPrimaryAction()
{
	if (bInPlacementMode)
	{
		TryPlaceGhost();
	}
}

void UMOBuildingComponent::HandlePlacementSecondaryAction()
{
	if (bInPlacementMode)
	{
		ExitPlacementMode(true);
	}
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

AMOPlayerController* UMOBuildingComponent::GetOwningPlayerController() const
{
	return Cast<AMOPlayerController>(GetOwner());
}

bool UMOBuildingComponent::SpawnGhostActor(const FMORecipeDefinitionRow& Recipe)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Get the actor class to spawn
	TSubclassOf<AMOBuildableActor> ActorClass = Recipe.PlacementData.BuildableActorClass;

	// If no class specified, infer from recipe properties
	if (!ActorClass)
	{
		if (Recipe.ContainerSlotCount > 0)
		{
			// Container building
			ActorClass = AMOContainerActor::StaticClass();
			UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingComponent] Inferred AMOContainerActor for recipe: %s (slots: %d)"),
				*Recipe.RecipeId.ToString(), Recipe.ContainerSlotCount);
		}
		else if (Recipe.ProvidedStationType != EMOCraftingStation::None)
		{
			// Crafting station building
			ActorClass = AMOCraftingStationActor::StaticClass();
			UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingComponent] Inferred AMOCraftingStationActor for recipe: %s (station: %d)"),
				*Recipe.RecipeId.ToString(), (int32)Recipe.ProvidedStationType);
		}
		else
		{
			// Default to base buildable actor
			ActorClass = AMOBuildableActor::StaticClass();
			UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingComponent] Using default AMOBuildableActor for recipe: %s"),
				*Recipe.RecipeId.ToString());
		}
	}

	// Spawn the ghost actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	CurrentGhost = World->SpawnActor<AMOBuildableActor>(ActorClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!CurrentGhost)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingComponent] Failed to spawn actor of class: %s"), *ActorClass->GetName());
		return false;
	}

	// Apply preview mesh from recipe if specified
	if (!Recipe.PlacementData.PreviewMesh.IsNull())
	{
		UStaticMesh* Mesh = Recipe.PlacementData.PreviewMesh.LoadSynchronous();
		if (Mesh && CurrentGhost->MeshComponent)
		{
			CurrentGhost->MeshComponent->SetStaticMesh(Mesh);
			CurrentGhost->MeshComponent->SetRelativeScale3D(Recipe.PlacementData.PreviewScale);
			UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingComponent] Applied PreviewMesh: %s"), *Mesh->GetName());
		}
		else if (!Mesh)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingComponent] Failed to load PreviewMesh for recipe: %s"), *Recipe.RecipeId.ToString());
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingComponent] No PreviewMesh specified for recipe: %s"), *Recipe.RecipeId.ToString());
	}

	// Set as ghost state
	CurrentGhost->SetGhostMode(true);

	// Apply initial ghost material (invalid until positioned)
	ApplyGhostMaterial(false);

	return true;
}

void UMOBuildingComponent::DestroyGhostActor()
{
	if (IsValid(CurrentGhost))
	{
		CurrentGhost->Destroy();
		CurrentGhost = nullptr;
	}
}

void UMOBuildingComponent::ApplyGhostMaterial(bool bValid)
{
	if (!IsValid(CurrentGhost))
	{
		return;
	}

	CurrentGhost->SetGhostVisual(bValid ? ValidPlacementColor : InvalidPlacementColor);
}

bool UMOBuildingComponent::CheckGhostCollision() const
{
	if (!IsValid(CurrentGhost))
	{
		return false;
	}

	// Use the ghost's own collision checking
	return CurrentGhost->IsOverlappingBlockingActors();
}

bool UMOBuildingComponent::ValidateSurfacePlacement(const FVector& SurfaceNormal) const
{
	// Determine surface type based on normal
	const float GroundThreshold = 0.7f;   // Normal Z > 0.7 = ground
	const float WallThreshold = 0.3f;     // |Normal Z| < 0.3 = wall
	const float CeilingThreshold = -0.7f; // Normal Z < -0.7 = ceiling

	bool bIsGround = SurfaceNormal.Z > GroundThreshold;
	bool bIsWall = FMath::Abs(SurfaceNormal.Z) < WallThreshold;
	bool bIsCeiling = SurfaceNormal.Z < CeilingThreshold;

	// Check against placement preferences
	if (bIsGround && CurrentPlacementData.bPrefersGroundPlacement)
	{
		return true;
	}
	if (bIsWall && CurrentPlacementData.bPrefersWallPlacement)
	{
		return true;
	}
	if (bIsCeiling && CurrentPlacementData.bPrefersCeilingPlacement)
	{
		return true;
	}

	return false;
}

FRotator UMOBuildingComponent::CalculateGhostRotation(const FVector& SurfaceNormal) const
{
	// Get player's forward direction (ignoring pitch)
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	FVector PlayerForward = FVector::ForwardVector;

	if (PC)
	{
		FRotator ViewRotation;
		FVector ViewLocation;
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
		PlayerForward = FRotationMatrix(FRotator(0.0f, ViewRotation.Yaw, 0.0f)).GetUnitAxis(EAxis::X);
	}

	// Calculate base rotation from surface normal
	// For ground placement, align Z up and face toward player's forward
	FRotator BaseRotation;

	if (SurfaceNormal.Z > 0.7f)
	{
		// Ground - stand upright, face player direction
		BaseRotation = FRotator(0.0f, PlayerForward.Rotation().Yaw, 0.0f);
	}
	else if (SurfaceNormal.Z < -0.7f)
	{
		// Ceiling - hang down, face player direction
		BaseRotation = UKismetMathLibrary::MakeRotFromZX(-FVector::UpVector, PlayerForward);
	}
	else
	{
		// Wall - align with wall normal
		BaseRotation = UKismetMathLibrary::MakeRotFromXZ(SurfaceNormal, FVector::UpVector);
	}

	// Apply user rotation offset
	FRotator FinalRotation = BaseRotation;
	FinalRotation.Yaw += UserRotationOffset.Yaw;
	FinalRotation.Pitch += UserRotationOffset.Pitch;
	FinalRotation.Roll += UserRotationOffset.Roll;

	return FinalRotation;
}

// ============================================================================
// SNAPPING
// ============================================================================

FVector UMOBuildingComponent::GetActorXYExtents(const AActor* Actor) const
{
	const FBox LocalBox = GetActorLocalBox(Actor);
	if (!LocalBox.IsValid) return FVector::ZeroVector;
	return LocalBox.GetExtent();
}

FBox UMOBuildingComponent::GetActorLocalBox(const AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return FBox(EForceInit::ForceInit);
	}

	TArray<UStaticMeshComponent*> AllMeshes;
	Actor->GetComponents<UStaticMeshComponent>(AllMeshes);

	// Helper: take a mesh component's local bounds and express them in the
	// ACTOR's local frame regardless of nesting depth. GetRelativeTransform()
	// returns transform relative to the IMMEDIATE PARENT, which is wrong for
	// nested components (e.g. a "Cube" child of MeshComponent — its relative
	// transform is in MeshComponent's frame, not the actor's). Using
	// GetComponentTransform().GetRelativeTransform(ActorXform) walks the
	// full chain correctly. Actor scale is factored out by this call and
	// re-applied at the bottom of this function.
	auto MeshBoxInActorLocal = [Actor](const UStaticMeshComponent* Mesh) -> FBox
	{
		if (!IsValid(Mesh)) return FBox(EForceInit::ForceInit);
		const UStaticMesh* SM = Mesh->GetStaticMesh();
		if (!SM) return FBox(EForceInit::ForceInit);
		const FTransform LocalXform = Mesh->GetComponentTransform()
			.GetRelativeTransform(Actor->GetActorTransform());
		return SM->GetBoundingBox().TransformBy(LocalXform);
	};

	FBox LocalBounds(EForceInit::ForceInit);
	const TCHAR* MatchedPriority = TEXT("none");

	// Priority 1: any component tagged "MO_SnapBounds". Explicit BP-level
	// "this is the structural authority" flag — survives renames and is
	// indifferent to component nesting.
	for (UStaticMeshComponent* Mesh : AllMeshes)
	{
		if (!IsValid(Mesh) || !Mesh->ComponentHasTag(TEXT("MO_SnapBounds"))) continue;
		LocalBounds = MeshBoxInActorLocal(Mesh);
		if (LocalBounds.IsValid) { MatchedPriority = TEXT("tag"); break; }
	}

	// Priority 2: the C++ MeshComponent's SUBTREE — MeshComponent itself
	// plus any UStaticMeshComponent attached underneath it. Matches the
	// convention "structural cube lives in or under MeshComponent, while
	// decoration components are siblings under RootScene." Captures BPs
	// where the cube is a CHILD of MeshComponent (not directly assigned to
	// MeshComponent's static mesh slot) without including unrelated
	// decoration meshes at the root level.
	if (!LocalBounds.IsValid)
	{
		if (const AMOBuildableActor* Buildable = Cast<AMOBuildableActor>(Actor))
		{
			if (IsValid(Buildable->MeshComponent))
			{
				TArray<USceneComponent*> SubComps;
				SubComps.Add(Buildable->MeshComponent);
				Buildable->MeshComponent->GetChildrenComponents(/*bIncludeAllDescendants=*/true, SubComps);
				for (USceneComponent* SC : SubComps)
				{
					if (UStaticMeshComponent* SMComp = Cast<UStaticMeshComponent>(SC))
					{
						const FBox B = MeshBoxInActorLocal(SMComp);
						if (B.IsValid) LocalBounds += B;
					}
				}
				if (LocalBounds.IsValid) MatchedPriority = TEXT("MeshComponent-subtree");
			}
		}
	}

	// Priority 3: any component whose name contains "Cube" (legacy fallback).
	if (!LocalBounds.IsValid)
	{
		for (UStaticMeshComponent* Mesh : AllMeshes)
		{
			if (!IsValid(Mesh)) continue;
			if (!Mesh->GetName().Contains(TEXT("Cube"), ESearchCase::IgnoreCase)) continue;
			LocalBounds = MeshBoxInActorLocal(Mesh);
			if (LocalBounds.IsValid) { MatchedPriority = TEXT("name-Cube"); break; }
		}
	}

	// Priority 4 (last resort): union of all static meshes. Hits when none
	// of the explicit signals matched — e.g. ad-hoc actors with random
	// mesh components and no naming/tagging convention.
	if (!LocalBounds.IsValid)
	{
		for (UStaticMeshComponent* Mesh : AllMeshes)
		{
			const FBox B = MeshBoxInActorLocal(Mesh);
			if (B.IsValid) LocalBounds += B;
		}
		if (LocalBounds.IsValid) MatchedPriority = TEXT("union-all");
	}

	if (!LocalBounds.IsValid)
	{
		return FBox(EForceInit::ForceInit);
	}

	// Apply actor scale — GetRelativeTransform factored it out, but we
	// want world-scale extents.
	const FVector Scale = Actor->GetActorScale3D();
	const FVector Min(LocalBounds.Min.X * Scale.X,
	                  LocalBounds.Min.Y * Scale.Y,
	                  LocalBounds.Min.Z * Scale.Z);
	const FVector Max(LocalBounds.Max.X * Scale.X,
	                  LocalBounds.Max.Y * Scale.Y,
	                  LocalBounds.Max.Z * Scale.Z);
	const FBox Final(Min, Max);

	UE_LOG(LogMOFramework, VeryVerbose,
		TEXT("[MOBuildingComponent] GetActorLocalBox(%s): priority=%s extent=%s center=%s"),
		*Actor->GetName(), MatchedPriority,
		*Final.GetExtent().ToString(), *Final.GetCenter().ToString());

	return Final;
}

FTransform UMOBuildingComponent::TryEdgeSnap(const FVector& RawLocation, const FRotator& RawRotation, bool* bOutDidSnap) const
{
	if (bOutDidSnap) { *bOutDidSnap = false; }

	// Pass-through default: caller's location + rotation, untouched.
	FTransform Result;
	Result.SetLocation(RawLocation);
	Result.SetRotation(RawRotation.Quaternion());
	Result.SetScale3D(FVector::OneVector);

	if (!bEnableEdgeSnapping || !IsValid(CurrentGhost))
	{
		return Result;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return Result;
	}

	const FBox GhostLocalBox = GetActorLocalBox(CurrentGhost);
	if (!GhostLocalBox.IsValid)
	{
		UE_LOG(LogMOFramework, Verbose,
			TEXT("[MOBuildingComponent] TryEdgeSnap: ghost has no valid bounds, skipping snap"));
		return Result;
	}
	const FVector GhostExtent = GhostLocalBox.GetExtent();
	const FVector GhostLocalCenter = GhostLocalBox.GetCenter();
	if (GhostExtent.IsNearlyZero())
	{
		return Result;
	}

	// Force-stack flag from the ghost's BP — needed when the actor's
	// bounding box can't disambiguate "wall" from "floor" (same slab shape).
	const bool bGhostForcesStack = IsValid(CurrentGhost) && CurrentGhost->bSnapAsWall;
	// Roof flag — always edge-snaps, never vertical-stacks; works on any
	// target (floor / wall / another roof).
	const bool bGhostIsRoof = IsValid(CurrentGhost) && CurrentGhost->bSnapAsRoof;

	// Determine the ghost's "thin" XY axis (whichever of X/Y is smaller).
	// Used by the stack case to figure out how much to rotate the ghost so
	// its thin face ends up facing along the snap-target's edge normal.
	// For a square ghost (e.g. 1m cube wall) X == Y; tie-break picks +X.
	const bool bGhostThinIsX = GhostExtent.X <= GhostExtent.Y;
	const float GhostThinAngleDeg = bGhostThinIsX ? 0.0f : 90.0f;

	// Which of the target's 4 cardinal edges to stack against.
	// Player-controlled via Q/E (see RotateGhostZ).
	const int32 StackEdge = ((SnapEdgeIndex % 4) + 4) % 4;

	// Camera forward direction in world (used for camera-facing edge selection
	// in horizontal placement modes — natural UX is "snap onto the side I'm
	// looking at").
	FVector CameraForward = FVector::ForwardVector;
	if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		FVector ViewLoc; FRotator ViewRot;
		PC->GetPlayerViewPoint(ViewLoc, ViewRot);
		CameraForward = ViewRot.Vector();
	}

	const float SnapThresholdSq = EdgeSnapDistance * EdgeSnapDistance;
	const float SearchRadiusSq = EdgeSnapSearchRadius * EdgeSnapSearchRadius;

	float BestDistSq = SnapThresholdSq;
	FTransform BestXform = Result;
	bool bAnySnap = false;

	// Floor-on-wall (wall-target) is a FALLBACK for floor ghosts — if there's
	// already a floor in range, the user almost always wants to extend it
	// rather than create a new floor adjacent to the wall on the opposite
	// side of the existing floor. Track wall-target snaps separately so
	// floor-target snaps win priority.
	float BestWallFallbackDistSq = SnapThresholdSq;
	FTransform BestWallFallbackXform = Result;
	bool bHasWallFallback = false;

	// Local-frame edge directions and the corresponding "edge yaw".
	// Order matters: Q/E cycle SnapEdgeIndex by ±1, so consecutive indices
	// MUST be 90° apart in yaw — otherwise a single press jumps by 180°
	// (invisible on a symmetric ghost mesh) and the user has to press
	// twice to see any visual change. Cycle: +X → +Y → -X → -Y → +X
	// (always a clean quarter-turn).
	static const FVector EdgeLocalDirs[4] = {
		FVector(1.0f, 0.0f, 0.0f),    // 0: +X (East),  yaw 0°
		FVector(0.0f, 1.0f, 0.0f),    // 1: +Y (North), yaw 90°
		FVector(-1.0f, 0.0f, 0.0f),   // 2: -X (West),  yaw 180°
		FVector(0.0f, -1.0f, 0.0f),   // 3: -Y (South), yaw 270° (= -90°)
	};
	static const float EdgeYawDegs[4] = { 0.0f, 90.0f, 180.0f, -90.0f };

	for (TActorIterator<AMOBuildableActor> It(World); It; ++It)
	{
		AMOBuildableActor* Other = *It;
		if (!IsValid(Other) || Other == CurrentGhost) continue;

		const FVector OtherLoc = Other->GetActorLocation();

		// 2D range reject.
		const float DX = OtherLoc.X - RawLocation.X;
		const float DY = OtherLoc.Y - RawLocation.Y;
		if (DX * DX + DY * DY > SearchRadiusSq) continue;

		const FBox OtherLocalBox = GetActorLocalBox(Other);
		if (!OtherLocalBox.IsValid) continue;
		const FVector OtherExtent = OtherLocalBox.GetExtent();
		const FVector OtherLocalCenter = OtherLocalBox.GetCenter();
		if (OtherExtent.IsNearlyZero()) continue;

		const FQuat OtherQuat = Other->GetActorQuat();

		// Detect the "stack" case: target is flat (thin in Z compared to
		// its XY footprint, like a floor) AND ghost is at least cubic in
		// its tallest dimension (a wall, post, or even a 1m-cube placeholder
		// wall). These get placed STANDING on the target's edge:
		//   - vertical lift so ghost's bottom sits at target's top
		//   - XY position at the target's edge midpoint (ghost straddles it)
		//   - rotation aligned so ghost's thin face faces along edge normal
		//   - which edge picked by CAMERA angle (player aims at the edge)
		// Anything else (floor-on-floor, wall-on-wall, etc.) falls back to
		// coplanar edge-touching snap.
		const float OtherMaxXY = FMath::Max(OtherExtent.X, OtherExtent.Y);
		const float GhostMaxXY = FMath::Max(GhostExtent.X, GhostExtent.Y);
		const bool bTargetIsFlat = OtherMaxXY > KINDA_SMALL_NUMBER
			&& (OtherExtent.Z / OtherMaxXY) < 0.5f;
		const bool bGhostIsTall = GhostMaxXY > KINDA_SMALL_NUMBER
			&& (GhostExtent.Z / GhostMaxXY) >= 0.5f;
		const bool bGhostIsFlat = GhostMaxXY > KINDA_SMALL_NUMBER
			&& (GhostExtent.Z / GhostMaxXY) < 0.5f;
		const bool bTargetIsWall = Other->bSnapAsWall;
		const bool bTargetIsRoof = Other->bSnapAsRoof;
		const bool bGhostIsFloor = !bGhostForcesStack && bGhostIsFlat;

		// Five snap modes:
		//   Vertical stack:    wall ghost  +  wall target  → directly above/below
		//   Floor-on-wall:     floor ghost +  wall target  → new level above/below
		//   Horizontal stack:  wall ghost  +  flat non-wall target (floor) → edge
		//   Roof:              roof ghost  +  ANY target   → edge with auto-Z
		//   Coplanar (else):   floor-on-floor and any other case
		//
		// Roof always edge-snaps and never vertical-stacks, so it pre-empts
		// VerticalStack and ignores the target-flatness check that HorizStack
		// normally uses.
		const bool bVerticalStackCase = bGhostForcesStack && bTargetIsWall && !bGhostIsRoof;
		const bool bFloorOnWallCase   = bGhostIsFloor   && bTargetIsWall;
		const bool bHorizontalStackCase = !bVerticalStackCase && !bFloorOnWallCase && !bGhostIsRoof
			&& bTargetIsFlat && (bGhostIsTall || bGhostForcesStack);
		const bool bRoofCase = bGhostIsRoof;

		// Verbose (not Log): this line fires for EVERY buildable in the world,
		// every tick, while in placement mode. With 30+ buildables and 60fps
		// that's 2000+ formatted lines/sec — enough to cause periodic hitches
		// when UE's log buffer flushes. The actual snap-fire logs further
		// below stay at Log level because they only fire for matching cases.
		// Re-enable with console: Log LogMOFramework Verbose
		UE_LOG(LogMOFramework, Verbose,
			TEXT("[MOBuildingComponent] Snap eval: target=%s otherExt=%s ghostExt=%s flat=%d tall=%d forceStack=%d ghostFlat=%d targetWall=%d vert=%d floorOnWall=%d horiz=%d"),
			*Other->GetName(), *OtherExtent.ToString(), *GhostExtent.ToString(),
			bTargetIsFlat, bGhostIsTall, bGhostForcesStack, bGhostIsFlat, bTargetIsWall,
			bVerticalStackCase, bFloorOnWallCase, bHorizontalStackCase);

		// 3D closest-point-on-AABB eligibility. Replaces the old XY-only
		// version so vertical stack (wall above floor) can correctly
		// disambiguate by Z proximity — if cursor is on the floor, the
		// floor's AABB closest point is 0cm away; the wall above is at
		// least its half-height away in Z, so floor wins. Aim higher and
		// the wall wins.
		const FVector CursorRel = RawLocation - OtherLoc;
		const FVector CursorInLocal = OtherQuat.Inverse().RotateVector(CursorRel);
		const FVector LocalMin = OtherLocalCenter - OtherExtent;
		const FVector LocalMax = OtherLocalCenter + OtherExtent;
		const FVector ClampedLocal(
			FMath::Clamp(CursorInLocal.X, LocalMin.X, LocalMax.X),
			FMath::Clamp(CursorInLocal.Y, LocalMin.Y, LocalMax.Y),
			FMath::Clamp(CursorInLocal.Z, LocalMin.Z, LocalMax.Z));
		const FVector ClosestWorld = OtherLoc + OtherQuat.RotateVector(ClampedLocal);
		const float Closest3DDistSq = FVector::DistSquared(ClosestWorld, RawLocation);
		if ((bVerticalStackCase || bFloorOnWallCase || bHorizontalStackCase || bRoofCase) && Closest3DDistSq >= SnapThresholdSq)
		{
			continue;  // cursor not close enough to this target's 3D AABB
		}

		// Approach direction in target's local frame — which side of the
		// target the player is aiming at. Cursor-relative position is the
		// primary signal (cursor on +X face / +X half of target → extend
		// in +X). Camera direction is fallback for when the cursor lands
		// exactly at the target center (no clear side).
		//
		// Cursor-relative > camera-direction because camera direction can
		// invert based on the player's viewing angle (e.g., looking at a
		// floor from one side vs. the other gives opposite approach dirs),
		// while cursor position is directly under the player's control.
		FVector LocalApproachDir(1.0f, 0.0f, 0.0f);  // default +X if both fail
		{
			FVector2D Candidate(CursorInLocal.X, CursorInLocal.Y);
			if (Candidate.SizeSquared() < KINDA_SMALL_NUMBER)
			{
				// Cursor exactly at target center — fall back to camera dir.
				const FVector CamFwdLocal = OtherQuat.Inverse().RotateVector(CameraForward);
				Candidate = FVector2D(-CamFwdLocal.X, -CamFwdLocal.Y);
			}
			if (Candidate.SizeSquared() > KINDA_SMALL_NUMBER)
			{
				if (FMath::Abs(Candidate.X) >= FMath::Abs(Candidate.Y))
				{
					LocalApproachDir = FVector(Candidate.X >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
				}
				else
				{
					LocalApproachDir = FVector(0.0f, Candidate.Y >= 0.0f ? 1.0f : -1.0f, 0.0f);
				}
			}
		}

		if (bVerticalStackCase)
		{
			// Wall-on-wall: three sub-modes based on cursor position relative
			// to the target wall's cube:
			//   - Cursor PAST the wall's long-axis end (|cube-local along-long|
			//     > extent): COPLANAR end-to-end — extend the wall in line.
			//   - Cursor CLEARLY BELOW the cube (cursor.Z < cube bottom world Z):
			//     PRIMARY stack-below. (Cursor in lower half but still inside
			//     cube Z range = FALLBACK, lets floor-edge HorizStack win when
			//     user is on a floor.)
			//   - Cursor in cube's upper half: PRIMARY stack-above.
			//   - Cursor in cube's lower half but inside cube Z range: FALLBACK
			//     stack-below.
			const FVector CursorInCubeLocal = CursorInLocal - OtherLocalCenter;
			const bool bWallThinIsX = OtherExtent.X <= OtherExtent.Y;
			const float WallLongExtent   = bWallThinIsX ? OtherExtent.Y : OtherExtent.X;
			const float GhostLongExtent  = bWallThinIsX ? GhostExtent.Y : GhostExtent.X;
			const float CursorAlongLong  = bWallThinIsX ? CursorInCubeLocal.Y : CursorInCubeLocal.X;

			const bool bEndToEnd = FMath::Abs(CursorAlongLong) > WallLongExtent;

			if (bEndToEnd)
			{
				// End-to-end Coplanar: extend the wall along its long axis.
				// New wall has identical rotation; cube center offset by
				// (WallExt + GhostExt) along the long axis on the cursor's
				// side.
				const float DirSign = CursorAlongLong > 0.0f ? 1.0f : -1.0f;
				const FVector LongAxisLocal = bWallThinIsX
					? FVector(0.0f, DirSign, 0.0f)
					: FVector(DirSign, 0.0f, 0.0f);
				const float TotalOffset = WallLongExtent + GhostLongExtent;

				// LocalOffset = target_cube_center_local + offset_along_long_axis.
				// Setting Z to OtherLocalCenter.Z keeps the ghost coplanar with
				// the target (same cube Z), even if the cursor is at a
				// different Z within the target's vertical span.
				FVector LocalOffset = OtherLocalCenter + LongAxisLocal * TotalOffset;
				LocalOffset.Z = OtherLocalCenter.Z;
				FVector SnapPos = OtherLoc + OtherQuat.RotateVector(LocalOffset);
				const FQuat FinalQuat = OtherQuat;

				// Cube-pivot adjustment: SnapPos is currently the GHOST's CUBE
				// CENTER world position. Subtract the ghost's local cube
				// offset (rotated to world) to get the actor pivot position.
				const FVector GhostCubeOffsetWorld = FinalQuat.RotateVector(GhostLocalCenter);
				SnapPos -= GhostCubeOffsetWorld;

				UE_LOG(LogMOFramework, Log,
					TEXT("[MOBuildingComponent] WallEndToEnd: target=%s cursorCubeLocal=%s longAxis=%s totOff=%.1f snapPos=%s"),
					*Other->GetName(), *CursorInCubeLocal.ToString(),
					*LongAxisLocal.ToString(), TotalOffset, *SnapPos.ToString());

				if (Closest3DDistSq < BestDistSq)
				{
					BestDistSq = Closest3DDistSq;
					BestXform.SetLocation(SnapPos);
					BestXform.SetRotation(FinalQuat);
					bAnySnap = true;
				}
				continue;  // handled this target; skip the vertical-stack path
			}

			// Vertical stacking onto another wall:
			//   - Position: EXACTLY the target's XY (same column).
			//   - Rotation: EXACTLY the target's rotation (no yaw offset).
			//   - Z: cube-edge-to-cube-edge — ghost cube BOTTOM meets target
			//     cube TOP (ABOVE), or ghost cube TOP meets target cube BOTTOM
			//     (BELOW). Works for any wall height and any cube-pivot offset,
			//     and supports mixed-height stacking (e.g. full wall on top of
			//     half wall) automatically because it uses the actual cube
			//     extents of both pieces.
			//
			// PREVIOUS BUG (fixed 2026-05): used ZDelta = SnapStackZOffset * 2,
			// which assumed SnapStackZOffset equals the wall's half-height.
			// That's true for full walls (cube centered on pivot), but for
			// half-walls — where SnapStackZOffset is tuned to the BP's actual
			// pivot-to-floor distance (which includes any cube-pivot Z offset),
			// not just half-height — the 2x multiplier produced a 50cm gap
			// between stacked half-walls.
			//
			// Priority: cursor in UPPER half of the CUBE = PRIMARY (clear
			// "stack above" intent). Cursor in LOWER half = FALLBACK
			// (ambiguous — wall's bottom face shares Z with the floor
			// surface beneath it, so floor-edge HorizStack should win when
			// the user is just aiming near a perimeter wall while standing
			// on the floor). Stack-below still fires when the wall is
			// FLOATING (no floor below it) — that's the genuine "build down"
			// case.
			//
			// CUBE CENTER, not actor pivot: half-wall BPs have the cube
			// offset within the actor (e.g. cube center at local Z = -25,
			// so the actor pivot sits at the cube's TOP). Comparing against
			// the pivot would only fire ABOVE when the cursor is at the
			// very top edge — natural hover lands in the lower half of the
			// cube, always BELOW. Cube center makes the partition the cube's
			// actual horizontal midline, which is the user's mental model.
			const float TgtCubeBottomZ = OtherLoc.Z + OtherLocalCenter.Z - OtherExtent.Z;
			const float TgtCubeTopZ    = OtherLoc.Z + OtherLocalCenter.Z + OtherExtent.Z;
			const float TgtCubeCenterZ = OtherLoc.Z + OtherLocalCenter.Z;
			const bool bAbove = (RawLocation.Z >= TgtCubeCenterZ);

			FVector SnapPos = OtherLoc;
			if (bAbove)
			{
				// Ghost cube bottom = target cube top
				// → ghost.Z + GhostLocalCenter.Z - GhostExtent.Z = TgtCubeTopZ
				SnapPos.Z = TgtCubeTopZ + GhostExtent.Z - GhostLocalCenter.Z;
			}
			else
			{
				// Ghost cube top = target cube bottom
				// → ghost.Z + GhostLocalCenter.Z + GhostExtent.Z = TgtCubeBottomZ
				SnapPos.Z = TgtCubeBottomZ - GhostExtent.Z - GhostLocalCenter.Z;
			}
			const FQuat FinalQuat = OtherQuat;

			// Cube-pivot adjustment so the ghost's cube (not actor pivot)
			// lands at the desired stack position. XY only — Z is set
			// explicitly above. Net zero for centered cubes; corrects for
			// BP-level cube offset otherwise.
			const FVector CubeOffsetWorld = FinalQuat.RotateVector(GhostLocalCenter) - OtherQuat.RotateVector(OtherLocalCenter);
			SnapPos.X -= CubeOffsetWorld.X;
			SnapPos.Y -= CubeOffsetWorld.Y;

			UE_LOG(LogMOFramework, Log,
				TEXT("[MOBuildingComponent] VertStack: %s target=%s otherLoc=%s otherCenter=%s otherExtZ=%.1f ghostCenter=%s ghostExtZ=%.1f cursorZ=%.1f snapPos=%s"),
				bAbove ? TEXT("ABOVE") : TEXT("BELOW"),
				*Other->GetName(), *OtherLoc.ToString(), *OtherLocalCenter.ToString(), OtherExtent.Z,
				*GhostLocalCenter.ToString(), GhostExtent.Z, RawLocation.Z,
				*SnapPos.ToString());

			// PRIMARY vs FALLBACK distinction for stack-below:
			//   - bAbove (upper half of cube): PRIMARY stack-above
			//   - !bAbove AND cursor below (cube bottom + tolerance):
			//     PRIMARY stack-below — the cursor is clearly aimed at the
			//     wall's lower portion, "build down" intent. Tolerance extends
			//     UP INTO the cube so aiming AT the wall's bottom edge (cursor
			//     lands a few cm inside the cube) still counts as primary.
			//   - !bAbove AND cursor between (cube bottom + tolerance) and
			//     cube center: FALLBACK stack-below — ambiguous; if HorizStack
			//     on a floor is also eligible, the floor wins.
			//
			// Tolerance picked to cover the typical "lower lip" of a wall
			// (~25cm — half a half-wall, quarter of a full wall) so aiming
			// is forgiving without swallowing the entire bottom half.
			const float BelowToleranceCm = 25.0f;
			const bool bCursorClearlyBelow = !bAbove
				&& (RawLocation.Z < TgtCubeBottomZ + BelowToleranceCm);
			const bool bPrimary = bAbove || bCursorClearlyBelow;

			if (bPrimary)
			{
				if (Closest3DDistSq < BestDistSq)
				{
					BestDistSq = Closest3DDistSq;
					BestXform.SetLocation(SnapPos);
					BestXform.SetRotation(FinalQuat);
					bAnySnap = true;
				}
			}
			else
			{
				// Fallback (stack-below stored, used only if no floor snap fires)
				if (Closest3DDistSq < BestWallFallbackDistSq)
				{
					BestWallFallbackDistSq = Closest3DDistSq;
					BestWallFallbackXform.SetLocation(SnapPos);
					BestWallFallbackXform.SetRotation(FinalQuat);
					bHasWallFallback = true;
				}
			}
		}
		else if (bFloorOnWallCase)
		{
			// Floor placed onto a wall — creates a new level of flooring at
			// the top of the wall (or below). XY extends in the direction
			// of the wall face the player is looking at: looking at the
			// wall's +X face = floor extends in wall-local +X, etc. Z is
			// the wall's own SnapStackZOffset (= half wall height); puts
			// the floor at the wall's top face (above) or bottom face
			// (below) depending on cursor Z.
			const bool bAbove = (RawLocation.Z >= OtherLoc.Z);
			const float ZDelta = Other->SnapStackZOffset;

			// Horizontal offset: full distance past the wall (wall_extent +
			// floor_extent) so the floor sits ENTIRELY past the wall's
			// camera-facing edge — no overlap with the wall in XY.
			const bool bAlongX = !FMath::IsNearlyZero(LocalApproachDir.X);
			const float TgtExtAlongDir   = bAlongX ? OtherExtent.X : OtherExtent.Y;
			const float GhostExtAlongDir = bAlongX ? GhostExtent.X : GhostExtent.Y;
			const float HorizOffset = TgtExtAlongDir + GhostExtAlongDir;

			FVector LocalOffset = OtherLocalCenter + LocalApproachDir * HorizOffset;
			LocalOffset.Z = 0.0f;
			FVector SnapPos = OtherLoc + OtherQuat.RotateVector(LocalOffset);
			SnapPos.Z = OtherLoc.Z + (bAbove ? +ZDelta : -ZDelta);
			const FQuat FinalQuat = OtherQuat;

			// Cube-pivot adjustment (XY only). LocalOffset already included
			// OtherLocalCenter via the OtherLocalCenter + LocalApproachDir
			// expression — so the target side is already accounted for in
			// SnapPos. Only the GHOST cube offset needs subtracting.
			const FVector FoCubeOffsetWorld = FinalQuat.RotateVector(GhostLocalCenter);
			SnapPos.X -= FoCubeOffsetWorld.X;
			SnapPos.Y -= FoCubeOffsetWorld.Y;

			UE_LOG(LogMOFramework, Log,
				TEXT("[MOBuildingComponent] FloorOnWall: %s target=%s approachDir=%s otherLoc=%s snapPos=%s zDelta=%.1f"),
				bAbove ? TEXT("ABOVE") : TEXT("BELOW"),
				*Other->GetName(), *LocalApproachDir.ToString(),
				*OtherLoc.ToString(), *SnapPos.ToString(), ZDelta);

			// Stored as a fallback — if any FLOOR-target snap also exists,
			// that one wins (user almost certainly wants to extend the
			// existing floor, not create one on the opposite side of the wall).
			if (Closest3DDistSq < BestWallFallbackDistSq)
			{
				BestWallFallbackDistSq = Closest3DDistSq;
				BestWallFallbackXform.SetLocation(SnapPos);
				BestWallFallbackXform.SetRotation(FinalQuat);
				bHasWallFallback = true;
			}
		}
		else if (bHorizontalStackCase)
		{
			// Position: ghost's CUBE CENTER lands AT the target's edge midpoint.
			// Offset = TgtExtAlongEdge only — for a 1m floor that's 50cm.
			// Targets the GHOST'S CUBE (the structural mesh component used
			// for snap bounds), not the actor pivot — so if the BP has its
			// cube offset from the actor's pivot, the snap still aligns the
			// VISIBLE cube to the floor edge.
			//
			// The cube straddles the edge by half its thickness on each
			// side. For thin walls that's a small overlap.
			const bool bAlongX = (StackEdge % 2 == 0);
			const float TgtExtAlongEdge = bAlongX ? OtherExtent.X : OtherExtent.Y;
			const float TotalOffset = TgtExtAlongEdge;
			FVector LocalEdgeMid = OtherLocalCenter + EdgeLocalDirs[StackEdge] * TotalOffset;
			LocalEdgeMid.Z = 0.0f;  // Z set explicitly below
			FVector SnapPos = OtherLoc + OtherQuat.RotateVector(LocalEdgeMid);

			// Z lift: fixed offset above target.Z, set per-BP via
			// SnapStackZOffset.
			SnapPos.Z = OtherLoc.Z + CurrentGhost->SnapStackZOffset;

			// Rotation: target rotation + yaw offset so ghost's thin axis
			// faces along the chosen edge normal.
			const float YawOffsetDeg = EdgeYawDegs[StackEdge] - GhostThinAngleDeg;
			const FQuat YawOffset(FVector::UpVector, FMath::DegreesToRadians(YawOffsetDeg));
			const FQuat FinalQuat = OtherQuat * YawOffset;

			// Cube-pivot adjustment (XY only). LocalEdgeMid already includes
			// OtherLocalCenter so the target side is accounted for. We only
			// subtract the GHOST'S cube offset (rotated by ghost's final
			// quat) so the ghost cube — not actor pivot — lands at the
			// computed snap target.
			const FVector HsCubeOffsetWorld = FinalQuat.RotateVector(GhostLocalCenter);
			SnapPos.X -= HsCubeOffsetWorld.X;
			SnapPos.Y -= HsCubeOffsetWorld.Y;

			UE_LOG(LogMOFramework, Log,
				TEXT("[MOBuildingComponent] HorizStack: edge=%d target=%s otherLoc=%s otherCenter=%s ghostCenter=%s snapPos=%s deltaXY=(%.1f,%.1f) yawOff=%.1f"),
				StackEdge, *Other->GetName(),
				*OtherLoc.ToString(), *OtherLocalCenter.ToString(), *GhostLocalCenter.ToString(),
				*SnapPos.ToString(),
				SnapPos.X - OtherLoc.X, SnapPos.Y - OtherLoc.Y, YawOffsetDeg);

			if (Closest3DDistSq < BestDistSq)
			{
				BestDistSq = Closest3DDistSq;
				BestXform.SetLocation(SnapPos);
				BestXform.SetRotation(FinalQuat);
				bAnySnap = true;
			}
		}
		else if (bRoofCase)
		{
			// Roof ghost snap. Two distinct sub-cases:
			//   A) Target is wall or floor: roof sits on top, edge-snapped to
			//      target's perimeter, slope direction picked by cursor side.
			//   B) Target is another roof: tile continuation. New roof matches
			//      target's orientation (continuing the same slope plane).
			//      Approach along slope axis = end-to-end (with Z rise/fall),
			//      approach along eave axis = side-to-side (coplanar).
			//
			// Wall-target axis override: LocalApproachDir's "dominant cursor
			// axis" rule picks ±Y for a wall (because the wall is much longer
			// along Y than X) — which would snap the roof along the wall's
			// LENGTH instead of perpendicular to its face. For walls, force
			// the approach direction along the wall's THIN axis with sign
			// from the cursor's local-frame position.
			//
			// Roof-target override: use NORMALIZED cursor dominance (|X|/Xext
			// vs |Y|/Yext) instead of raw dominance. For tilted-cube roofs,
			// the X bounding-box extent is inflated by the tilt projection
			// (e.g. 53.5 vs 50 for our test BPs). Raw |X| > |Y| would pick X
			// even when the cursor is on a side face — making side-to-side
			// snap require the cursor to hit the side face within a narrow
			// strip. Normalization fixes this so side faces always pick Y.
			FVector RoofApproachDir = LocalApproachDir;
			if (bTargetIsWall)
			{
				const bool bWallThinIsX = OtherExtent.X <= OtherExtent.Y;
				if (bWallThinIsX)
					RoofApproachDir = FVector(CursorInLocal.X >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
				else
					RoofApproachDir = FVector(0.0f, CursorInLocal.Y >= 0.0f ? 1.0f : -1.0f, 0.0f);
			}
			else
			{
				// Cube-relative dominance: subtract OtherLocalCenter so we
				// measure cursor offset from the CUBE CENTER, not the actor
				// pivot. For BPs where the cube is offset within the actor
				// (e.g. OtherLocalCenter = (-50, 0, 0)), the actor-pivot
				// measurement is biased toward whichever side the cube is on.
				const FVector CursorInCubeLocal = CursorInLocal - OtherLocalCenter;
				FVector2D RoofCand(CursorInCubeLocal.X, CursorInCubeLocal.Y);
				if (RoofCand.SizeSquared() > KINDA_SMALL_NUMBER)
				{
					const float NormX = (OtherExtent.X > KINDA_SMALL_NUMBER)
						? FMath::Abs(RoofCand.X) / OtherExtent.X : 0.0f;
					const float NormY = (OtherExtent.Y > KINDA_SMALL_NUMBER)
						? FMath::Abs(RoofCand.Y) / OtherExtent.Y : 0.0f;
					if (NormX >= NormY)
						RoofApproachDir = FVector(RoofCand.X >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
					else
						RoofApproachDir = FVector(0.0f, RoofCand.Y >= 0.0f ? 1.0f : -1.0f, 0.0f);
				}
			}

			const bool bAlongX = !FMath::IsNearlyZero(RoofApproachDir.X);

			FVector LocalOffset;
			FQuat FinalQuat;
			FVector SnapPos;

			bool bRoofEndToEnd = false;  // diagnostic — logged below
			if (bTargetIsRoof)
			{
				// --- Sub-case B: roof tiling ---
				// Slope axis = LONG horizontal axis (the mesh tilt projects
				// onto it, making it longer than the eave/short axis).
				const bool bTargetSlopeIsX = OtherExtent.X >= OtherExtent.Y;
				// Tile size = full mesh dimension along the EAVE axis (the
				// untilted one), which equals the underlying cube edge length
				// (e.g. 100cm for our test roofs). This matches the user's
				// intuitive "tile size" — bounding-box extent on the slope
				// axis is inflated by the tilt projection and shouldn't be
				// used as tile pitch.
				const float TileSize = 2.0f * (bTargetSlopeIsX ? OtherExtent.Y : OtherExtent.X);

				// End-to-end if approach axis matches target's slope axis,
				// else side-to-side.
				const bool bEndToEnd = (bTargetSlopeIsX && bAlongX)
					|| (!bTargetSlopeIsX && !bAlongX);
				bRoofEndToEnd = bEndToEnd;

				// Sign for end-to-end Z direction.
				//
				// Counterintuitive: the +180° yaw flip applied during
				// wall-target placement (see sub-case A) means the visual
				// PEAK of the roof is on the OPPOSITE side of the actor's
				// local +slope-axis. So when the cursor hits the +slope-axis
				// side of the actor's local frame, the user is visually
				// pointing at the EAVE (low side) — they want a roof BELOW.
				// And -slope-axis side = visual peak → roof ABOVE.
				const float DirSign = bAlongX
					? (RoofApproachDir.X > 0.0f ? -1.0f : 1.0f)
					: (RoofApproachDir.Y > 0.0f ? -1.0f : 1.0f);

				// LocalOffset starts at the target's cube center (so the new
				// roof's cube center will be positioned RELATIVE to the target
				// cube center, including the target's pivot offset). Then add
				// horizontal tile-pitch in the approach direction, and Z rise
				// for end-to-end.
				//
				// PREVIOUS BUG: LocalOffset.Z was being set to 0 (side-to-side)
				// or DirSign * TileSize (end-to-end), overwriting OtherLocalCenter.Z.
				// For BPs where the actor pivot is offset from the cube center
				// in Z (which most are — the cube is usually centered above the
				// floor pivot), this dropped the cube-center-Z contribution and
				// the new roof landed at the target's actor Z instead of the
				// target's cube center Z. Fix: ADD the Z rise to OtherLocalCenter.Z
				// instead of replacing it.
				LocalOffset = OtherLocalCenter + RoofApproachDir * TileSize;
				if (bEndToEnd)
				{
					// 45° slope: rise = run. Adds to existing OtherLocalCenter.Z.
					LocalOffset.Z = OtherLocalCenter.Z + DirSign * TileSize;
				}
				// (For side-to-side, LocalOffset.Z stays as OtherLocalCenter.Z
				// from the initial assignment — coplanar with target cube.)

				SnapPos = OtherLoc + OtherQuat.RotateVector(LocalOffset);
				// Match target's rotation exactly — continuing the slope plane.
				FinalQuat = OtherQuat;
			}
			else
			{
				// --- Sub-case A: roof on wall or floor ---
				// Per-target-type horizontal offset:
				//   - Wall:  cube edge at wall's CENTERLINE (wall thickness
				//            ignored). TotalOffset = GhostExt only.
				//   - Floor: cube edge at floor's perimeter edge (overhang
				//            outward). TotalOffset = TgtExt + GhostExt.
				const float TgtExtAlongDir   = bAlongX ? OtherExtent.X : OtherExtent.Y;
				const float GhostExtAlongDir = bAlongX ? GhostExtent.X : GhostExtent.Y;
				const float TotalOffset = bTargetIsWall
					? GhostExtAlongDir
					: (TgtExtAlongDir + GhostExtAlongDir);

				LocalOffset = OtherLocalCenter + RoofApproachDir * TotalOffset;
				LocalOffset.Z = 0.0f;  // Z set explicitly below
				SnapPos = OtherLoc + OtherQuat.RotateVector(LocalOffset);

				// Sit on top: ghost cube bottom = target cube top.
				const float TargetCubeTopZ = OtherLoc.Z + OtherLocalCenter.Z + OtherExtent.Z;
				SnapPos.Z = TargetCubeTopZ + GhostExtent.Z - GhostLocalCenter.Z;

				// Rotation: roof's LONG axis (slope direction) faces along
				// RoofApproachDir. +180° flip puts the EAVE toward the target
				// (not the peak), so the slope rises away from the wall.
				float DirYawDeg = 0.0f;
				if (bAlongX) DirYawDeg = (RoofApproachDir.X > 0.0f) ? 0.0f  : 180.0f;
				else         DirYawDeg = (RoofApproachDir.Y > 0.0f) ? 90.0f : -90.0f;
				const float GhostLongAngleDeg = bGhostThinIsX ? 90.0f : 0.0f;
				const float YawOffsetDeg = DirYawDeg - GhostLongAngleDeg + 180.0f;
				const FQuat YawOffset(FVector::UpVector, FMath::DegreesToRadians(YawOffsetDeg));
				FinalQuat = OtherQuat * YawOffset;
			}

			// Cube-pivot adjustment — LocalOffset already baked OtherLocalCenter
			// (the target's cube center), so only the GHOST cube offset needs
			// subtracting. XYZ all matter for roof-on-roof end-to-end (Z offset
			// is non-zero).
			const FVector RfCubeOffsetWorld = FinalQuat.RotateVector(GhostLocalCenter);
			SnapPos.X -= RfCubeOffsetWorld.X;
			SnapPos.Y -= RfCubeOffsetWorld.Y;
			if (bTargetIsRoof)
			{
				SnapPos.Z -= RfCubeOffsetWorld.Z;
			}
			// (For wall/floor target, SnapPos.Z was set explicitly above using
			// GhostLocalCenter.Z, so no double-subtract.)

			const FVector CursorInCubeLocalDbg = CursorInLocal - OtherLocalCenter;
			UE_LOG(LogMOFramework, Log,
				TEXT("[MOBuildingComponent] RoofSnap: target=%s targetWall=%d targetIsRoof=%d endToEnd=%d cursorLocal=%s cursorCubeLocal=%s approachDir=%s otherLoc=%s otherLocalCenter=%s snapPos=%s localOff=%s"),
				*Other->GetName(), bTargetIsWall ? 1 : 0, bTargetIsRoof ? 1 : 0,
				bRoofEndToEnd ? 1 : 0,
				*CursorInLocal.ToString(), *CursorInCubeLocalDbg.ToString(),
				*RoofApproachDir.ToString(), *OtherLoc.ToString(),
				*OtherLocalCenter.ToString(),
				*SnapPos.ToString(), *LocalOffset.ToString());

			if (Closest3DDistSq < BestDistSq)
			{
				BestDistSq = Closest3DDistSq;
				BestXform.SetLocation(SnapPos);
				BestXform.SetRotation(FinalQuat);
				bAnySnap = true;
			}
		}
		else
		{
			// Coplanar edge-touching snap (floor-to-floor, wall-to-wall).
			// Uses the same 3D AABB eligibility as the stack cases, so it
			// fires when the cursor is near the target from below or the
			// side, not just from above.
			if (Closest3DDistSq >= SnapThresholdSq) continue;

			// Direction picked by camera (looking at the target's east face
			// → extend east), with cursor-position fallback if the camera
			// is too vertical. Same LocalApproachDir used by all horizontal
			// modes so the UX is consistent.
			const bool bAlongX = !FMath::IsNearlyZero(LocalApproachDir.X);
			const float TgtExtAlongDir   = bAlongX ? OtherExtent.X : OtherExtent.Y;
			const float GhostExtAlongDir = bAlongX ? GhostExtent.X : GhostExtent.Y;
			const float TotalOffset = TgtExtAlongDir + GhostExtAlongDir;

			FVector LocalOffset = OtherLocalCenter + LocalApproachDir * TotalOffset;
			LocalOffset.Z = 0.0f;
			FVector SnapPos = OtherLoc + OtherQuat.RotateVector(LocalOffset);

			// Cube-pivot adjustment (XY only) — LocalOffset already includes
			// OtherLocalCenter, so only the GHOST cube offset needs subtracting.
			// Coplanar uses target's rotation directly (no yaw offset).
			const FVector CpCubeOffsetWorld = OtherQuat.RotateVector(GhostLocalCenter);
			SnapPos.X -= CpCubeOffsetWorld.X;
			SnapPos.Y -= CpCubeOffsetWorld.Y;

			UE_LOG(LogMOFramework, Log,
				TEXT("[MOBuildingComponent] Coplanar: target=%s approachDir=%s snapPos=%s"),
				*Other->GetName(), *LocalApproachDir.ToString(), *SnapPos.ToString());

			if (Closest3DDistSq < BestDistSq)
			{
				BestDistSq = Closest3DDistSq;
				BestXform.SetLocation(SnapPos);
				BestXform.SetRotation(OtherQuat);
				bAnySnap = true;
			}
		}
	}

	// Fallback: if no primary snap fired but we have a stored FloorOnWall
	// candidate, use it. This makes wall-targets a second-class snap for
	// floor ghosts — coplanar floor-on-floor wins whenever both are eligible.
	if (!bAnySnap && bHasWallFallback)
	{
		BestXform = BestWallFallbackXform;
		bAnySnap = true;
		UE_LOG(LogMOFramework, Log,
			TEXT("[MOBuildingComponent] Using FloorOnWall fallback (no floor target in range)"));
	}

	if (bOutDidSnap) { *bOutDidSnap = bAnySnap; }
	return BestXform;
}
