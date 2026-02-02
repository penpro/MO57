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
#include "MOPlayerController.h"
#include "MORecipeDatabaseSettings.h"
#include "MOFramework.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

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

		// Calculate ghost rotation
		FRotator FinalRotation = CalculateGhostRotation(SurfaceNormal);

		// Set ghost transform
		CurrentGhost->SetActorLocation(Hit.ImpactPoint);
		CurrentGhost->SetActorRotation(FinalRotation);

		// Check collision if required
		bool bValidPlacement = bValidSurface;
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

	// The ghost becomes the actual building - just finalize its placement
	AMOBuildableActor* PlacedBuilding = CurrentGhost;

	// Initialize the building with the recipe
	PlacedBuilding->InitializeBuilding(CurrentRecipeId);

	// Clear our reference (don't destroy it)
	CurrentGhost = nullptr;

	// Broadcast the placement
	OnGhostPlaced.Broadcast(PlacedBuilding);

	// Exit placement mode (not cancelled since we placed)
	ExitPlacementMode(false);

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingComponent] Placed building: %s"), *PlacedBuilding->GetName());
	return true;
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
	if (!ActorClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingComponent] No BuildableActorClass set for recipe: %s"), *Recipe.RecipeId.ToString());
		return false;
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
