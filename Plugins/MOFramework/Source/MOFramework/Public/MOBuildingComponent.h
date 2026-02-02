#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOBuildingTypes.h"

#include "MOBuildingComponent.generated.h"

class AMOBuildableActor;
class AMOPlayerController;

/**
 * =============================================================================
 * UMOBuildingComponent - Building Placement Controller
 * =============================================================================
 *
 * PURPOSE:
 * Manages building placement mode on the player controller. Handles ghost
 * preview spawning, positioning via camera trace, rotation, validation,
 * and final placement.
 *
 * OWNERSHIP:
 * - Owner: AMOPlayerController (created in constructor as default subobject)
 * - Lifespan: Exists for duration of PlayerController
 *
 * -----------------------------------------------------------------------------
 * PLACEMENT MODE FLOW
 * -----------------------------------------------------------------------------
 *
 * ENTER PLACEMENT MODE:
 *   1. UIManager->ToggleBuildingMenu() opens building selection
 *   2. Player selects building recipe
 *   3. UIManager->HandleBuildingSelected(RecipeId)
 *   4. BuildingComponent->EnterPlacementMode(RecipeId)
 *   5. Recipe validated (bIsBuilding, BuildableActorClass set)
 *   6. Ghost actor spawned via SpawnGhostActor()
 *   7. Ghost set to ghost mode (translucent material)
 *   8. Input context switched to BaseBuilding
 *   9. Component tick enabled for UpdateGhostPosition()
 *   10. OnPlacementModeEntered broadcast
 *
 * DURING PLACEMENT:
 *   - Every tick: UpdateGhostPosition() called
 *   - Camera trace determines placement location
 *   - Surface validation checks ground/wall/ceiling preferences
 *   - Collision validation if bRequiresNoCollision
 *   - Ghost material updated (green=valid, red=invalid)
 *   - Q/E keys call RotateGhostZ() for yaw rotation
 *   - Click calls TryPlaceGhost() -> places if valid
 *   - Escape/RightClick calls ExitPlacementMode(cancel=true)
 *
 * EXIT PLACEMENT MODE (success):
 *   1. TryPlaceGhost() validates placement
 *   2. Ghost->InitializeBuilding(RecipeId) called
 *   3. Ghost transitions from placement ghost to placed ghost
 *   4. OnGhostPlaced broadcast with placed actor
 *   5. ExitPlacementMode(cancel=false) called
 *   6. Input context restored to PawnControl
 *   7. OnPlacementModeExited(bPlaced=true) broadcast
 *
 * EXIT PLACEMENT MODE (cancel):
 *   1. ExitPlacementMode(cancel=true) called
 *   2. Ghost actor destroyed
 *   3. Input context restored to PawnControl
 *   4. OnPlacementModeExited(bPlaced=false) broadcast
 *
 * -----------------------------------------------------------------------------
 * CAMERA TRACE PLACEMENT
 * -----------------------------------------------------------------------------
 *
 * UpdateGhostPosition() performs this each tick:
 *
 *   1. Get camera location and direction from PlayerController
 *   2. Trace from camera along direction (MaxPlacementDistance)
 *   3. If hit:
 *      a. Get surface normal from hit
 *      b. Validate surface (ground/wall/ceiling preferences)
 *      c. Calculate rotation from surface + user offset
 *      d. Set ghost location to hit point
 *      e. Set ghost rotation
 *      f. Check collision overlap if required
 *      g. Update ghost material (green/red)
 *   4. If no hit:
 *      a. Place ghost at max distance
 *      b. Mark as invalid (red material)
 *
 * -----------------------------------------------------------------------------
 * INPUT ACTIONS HANDLED
 * -----------------------------------------------------------------------------
 *
 * From PlayerController when in BaseBuilding context:
 *   HandlePlacementPrimaryAction() -> TryPlaceGhost()
 *   HandlePlacementSecondaryAction() -> ExitPlacementMode(cancel=true)
 *
 * From rotation input actions:
 *   RotateGhostZ(+degrees) -> Q key (clockwise from above)
 *   RotateGhostZ(-degrees) -> E key (counter-clockwise)
 *   RotateGhostX(degrees) -> W/S keys (if allowed by recipe)
 *   RotateGhostY(degrees) -> A/D keys (if allowed by recipe)
 *
 * -----------------------------------------------------------------------------
 * DELEGATE BROADCASTS
 * -----------------------------------------------------------------------------
 *
 * OnPlacementModeEntered(FName RecipeId)
 *   -> When: EnterPlacementMode() succeeds
 *   -> Listeners: UI for mode indicator, HUD updates
 *
 * OnPlacementModeExited(bool bPlaced)
 *   -> When: ExitPlacementMode() called
 *   -> bPlaced: true if building placed, false if cancelled
 *   -> Listeners: UI for mode indicator
 *
 * OnGhostPlaced(AMOBuildableActor* Ghost)
 *   -> When: TryPlaceGhost() succeeds
 *   -> Listeners: Persistence system, tutorial system
 *
 * =============================================================================
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOBuildingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOBuildingComponent();

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Broadcast when placement mode is entered. */
	UPROPERTY(BlueprintAssignable, Category="MO|Building")
	FMOOnPlacementModeEnteredSignature OnPlacementModeEntered;

	/** Broadcast when placement mode is exited. */
	UPROPERTY(BlueprintAssignable, Category="MO|Building")
	FMOOnPlacementModeExitedSignature OnPlacementModeExited;

	/** Broadcast when a ghost building is successfully placed. */
	UPROPERTY(BlueprintAssignable, Category="MO|Building")
	FMOOnGhostPlacedSignature OnGhostPlaced;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Maximum distance for placement line trace. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Placement")
	float MaxPlacementDistance = 1000.0f;

	/** Collision channel for placement line trace. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Placement")
	TEnumAsByte<ECollisionChannel> PlacementTraceChannel = ECC_Visibility;

	/** Ghost material to apply to placement preview. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Visual")
	TSoftObjectPtr<UMaterialInterface> GhostMaterialBase;

	/** Color for valid placement ghost. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Visual")
	FLinearColor ValidPlacementColor = FLinearColor(0.2f, 1.0f, 0.2f, 0.5f);

	/** Color for invalid placement ghost. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Visual")
	FLinearColor InvalidPlacementColor = FLinearColor(1.0f, 0.2f, 0.2f, 0.5f);

	// ============================================================================
	// PLACEMENT MODE
	// ============================================================================

	/**
	 * Enter placement mode for a specific building recipe.
	 * @param RecipeId - The recipe ID for the building to place
	 * @return True if placement mode was entered successfully
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	bool EnterPlacementMode(FName RecipeId);

	/**
	 * Exit placement mode.
	 * @param bCancel - If true, the placement was cancelled (no building placed)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void ExitPlacementMode(bool bCancel = false);

	/**
	 * Check if currently in placement mode.
	 * @return True if in placement mode
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	bool IsInPlacementMode() const { return bInPlacementMode; }

	/**
	 * Get the current placement recipe ID.
	 * @return The recipe ID being placed, or NAME_None if not in placement mode
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	FName GetCurrentRecipeId() const { return CurrentRecipeId; }

	// ============================================================================
	// GHOST CONTROL
	// ============================================================================

	/**
	 * Update ghost position based on camera trace.
	 * Called automatically during Tick when in placement mode.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void UpdateGhostPosition();

	/**
	 * Rotate the ghost around the Z axis (yaw).
	 * @param DeltaDegrees - Degrees to rotate (positive = clockwise from above)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void RotateGhostZ(float DeltaDegrees);

	/**
	 * Rotate the ghost around the X axis (pitch).
	 * @param DeltaDegrees - Degrees to rotate
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void RotateGhostX(float DeltaDegrees);

	/**
	 * Rotate the ghost around the Y axis (roll).
	 * @param DeltaDegrees - Degrees to rotate
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void RotateGhostY(float DeltaDegrees);

	// ============================================================================
	// PLACEMENT
	// ============================================================================

	/**
	 * Check if the ghost can be placed at the current location.
	 * @return True if placement is valid
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	bool CanPlaceAtCurrentLocation() const { return bCurrentPlacementValid; }

	/**
	 * Attempt to place the ghost at the current location.
	 * @return True if the building was placed successfully
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	bool TryPlaceGhost();

	/**
	 * Handle primary action during placement mode (attempt to place).
	 */
	void HandlePlacementPrimaryAction();

	/**
	 * Handle secondary action during placement mode (cancel).
	 */
	void HandlePlacementSecondaryAction();

protected:
	// ============================================================================
	// OVERRIDES
	// ============================================================================

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// ============================================================================
	// STATE
	// ============================================================================

	/** Whether currently in placement mode. */
	bool bInPlacementMode = false;

	/** Current recipe being placed. */
	FName CurrentRecipeId = NAME_None;

	/** Cached placement data for current recipe. */
	FMOBuildingPlacementData CurrentPlacementData;

	/** Current ghost actor being previewed. */
	UPROPERTY()
	TObjectPtr<AMOBuildableActor> CurrentGhost;

	/** Whether current ghost position is valid for placement. */
	bool bCurrentPlacementValid = false;

	/** User-applied rotation offset. */
	FRotator UserRotationOffset = FRotator::ZeroRotator;

	/** Last valid hit result from placement trace. */
	FHitResult LastPlacementHit;

	// ============================================================================
	// INTERNAL METHODS
	// ============================================================================

	/** Get the owning player controller. */
	AMOPlayerController* GetOwningPlayerController() const;

	/** Spawn the ghost preview actor. */
	bool SpawnGhostActor(const FMORecipeDefinitionRow& Recipe);

	/** Destroy the current ghost actor. */
	void DestroyGhostActor();

	/** Apply ghost material to the ghost actor. */
	void ApplyGhostMaterial(bool bValid);

	/** Check if ghost overlaps any blocking actors. */
	bool CheckGhostCollision() const;

	/** Validate placement against surface preferences. */
	bool ValidateSurfacePlacement(const FVector& SurfaceNormal) const;

	/** Calculate final rotation for ghost based on surface and user input. */
	FRotator CalculateGhostRotation(const FVector& SurfaceNormal) const;
};
