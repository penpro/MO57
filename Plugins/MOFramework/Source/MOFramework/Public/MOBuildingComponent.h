/**
 * =============================================================================
 * MOBuildingComponent.h - Building Placement Controller
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] INPUT CONTEXT: Placement mode switches to BaseBuilding input
 *   context. Must restore previous context on exit. See ExitPlacementMode().
 *
 * [2024-02] GHOST CLEANUP: If placement cancelled, ghost must be destroyed.
 *   Check DestroyGhostActor() is called in all exit paths.
 *
 * [2024-02] ROTATION: Ghost rotation uses Q/E keys. Rotation snap configurable.
 *   Rotation stored in PlacementRotation, applied to ghost each tick.
 *
 * =============================================================================
 * RELATED FILES: MOBuildableActor.h, MOBuildProgressComponent.h, MOBuildingTypes.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

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
	// SNAPPING
	// ============================================================================
	//
	// Edge snap pulls the ghost into alignment with nearby placed buildables so
	// pieces don't have to be eyeballed. MVP behavior: compute axis-aligned
	// XY bounding boxes for the ghost and each nearby AMOBuildableActor, and
	// snap the ghost to the nearest "edges touching" position if within range.
	//
	// Z is also snapped to the neighbor's Z when an edge snap fires — the
	// expectation is "these two pieces are coplanar." Player can still place
	// pieces at different heights by aiming somewhere with no neighbor in
	// range, or by disabling snap entirely (bEnableEdgeSnapping).
	//
	// Not yet implemented (future): socket-based snap points, corner snap,
	// stacking (top-of-wall to bottom-of-floor), rotation snap to neighbor.

	/** Master toggle. Disable to free-place. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Snapping")
	bool bEnableEdgeSnapping = true;

	/**
	 * Snap radius (cm). If the raw placement position lands within this
	 * distance of an edge-aligned position relative to a nearby buildable,
	 * the ghost is pulled to that aligned position.
	 *
	 * Too small = snap feels unresponsive. Too large = snap fires when the
	 * player wants to free-place near a structure. 50cm is a good default
	 * (half a meter).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Snapping", meta=(ClampMin="0.0"))
	float EdgeSnapDistance = 50.0f;

	/**
	 * Maximum 2D distance (cm) from the ghost to consider another buildable
	 * as a snap candidate. Skip buildables farther than this — keeps the
	 * search cheap and avoids snapping to something across the map.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Snapping", meta=(ClampMin="0.0"))
	float EdgeSnapSearchRadius = 500.0f;

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

	/**
	 * Increment the ghost's "in-place flip yaw" — adds a fixed amount on top
	 * of whatever the snap chose, rotating around the ghost's CUBE CENTER
	 * (not the actor pivot) so position stays invariant. Use this to mirror
	 * placement orientation without disturbing the snap-aligned position —
	 * e.g. swap which face of a wall points inward, or flip a triangular
	 * wall's slant side without losing the floor-edge snap.
	 *
	 * Increment size depends on ghost shape:
	 *   - FLAT pieces (floor-like, Z extent < ½ × min(X,Y) extent): 90°
	 *     — four cycle states, matches the floor's 4-fold rotational symmetry
	 *   - TALL pieces (walls, roofs): 180°
	 *     — two cycle states, only meaningful flip is "face other direction"
	 *
	 * Unlike RotateGhostZ (which cycles the snap-edge index), this is a pure
	 * cosmetic rotation overlay that doesn't change which edge of the target
	 * the ghost snaps against. Accumulates mod 360°.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void ToggleGhostFlipYaw();

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

	// Client->server transport for building placement (H19). A remote client forwards the
	// resolved placement here; the server spawns the real replicated building.
	UFUNCTION(Server, Reliable)
	void ServerPlaceBuilding(FName InRecipeId, FTransform PlacementTransform);

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

	/**
	 * In-place flip yaw (degrees, 0..360). Applied AFTER the snap math
	 * runs, rotated around the ghost's cube center so position stays put.
	 * Incremented by ToggleGhostFlipYaw() — wired to mouse wheel. Increment
	 * size is 90° for flat ghosts, 180° otherwise, so wheel-clicks always
	 * advance through the meaningful orientations and never land on a
	 * "no visible change" state.
	 */
	float UserFlipYawDeg = 0.0f;

	/** Last valid hit result from placement trace. */
	FHitResult LastPlacementHit;

	/**
	 * Which of the snap target's 4 cardinal edges to stack against, when in
	 * the stack-on-flat snap case (wall on floor). Q/E cycles this (+1/-1
	 * per press). 0=+X, 1=-X, 2=+Y, 3=-Y in the target's local frame.
	 *
	 * Decoupled from UserRotationOffset because cube-shaped ghosts have
	 * symmetric extents and yaw-based selection can't disambiguate; the
	 * camera-direction approach also degenerates when looking down. Explicit
	 * Q/E control sidesteps both.
	 */
	int32 SnapEdgeIndex = 0;

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

	/**
	 * If bEnableEdgeSnapping is on, look for a nearby AMOBuildableActor whose
	 * axis-aligned edge would line up with the ghost at a position close to
	 * RawLocation. Returns a transform with:
	 *   - location pulled to the neighbor's edge (XY) at the neighbor's Z
	 *   - rotation overridden to match the neighbor's yaw (so snapped pieces
	 *     line up to a consistent grid orientation, not the user's free Q/E
	 *     rotation)
	 * If no neighbor is within EdgeSnapDistance, returns RawLocation +
	 * RawRotation unchanged.
	 *
	 * Sets bOutDidSnap if the caller wants to know whether a snap happened
	 * (e.g. for visual feedback). Pass nullptr if not needed.
	 */
	FTransform TryEdgeSnap(const FVector& RawLocation, const FRotator& RawRotation, bool* bOutDidSnap = nullptr) const;

	/**
	 * Return half-extents (cm) of an actor's XY footprint by walking its
	 * static mesh components' bounds. Z is included but unused by the
	 * edge-snap logic. Returns zero vector if no meshes found.
	 */
	FVector GetActorXYExtents(const AActor* Actor) const;

	/**
	 * Return the actor's mesh-bounding box in actor-LOCAL space (after each
	 * mesh component's relative transform, then actor scale). The box can
	 * be off-center from the actor origin — e.g. a floor BP that offsets
	 * its mesh +50cm in Z so it sits at standing-foot level. Use
	 * box.GetCenter() to know where the mesh actually is relative to the
	 * actor's pivot.
	 *
	 * Returns an invalid box (FBox::IsValid==false) if no static meshes.
	 */
	FBox GetActorLocalBox(const AActor* Actor) const;
};
