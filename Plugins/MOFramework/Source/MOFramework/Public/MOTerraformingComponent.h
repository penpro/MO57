/**
 * =============================================================================
 * MOTerraformingComponent.h - Voxel Terrain Modification
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Component that enables voxel terrain modification (dig, raise, flatten,
 * smooth). Interfaces with Voxel Plugin Pro 2.0 sculpt actors to modify
 * the destructible terrain.
 *
 * TERRAFORMING MODES:
 * - Dig: Lower terrain at brush location
 * - Raise: Elevate terrain at brush location
 * - Flatten: Level terrain to a target height
 * - Smooth: Reduce terrain roughness
 *
 * VOXEL INTEGRATION:
 * All Voxel Plugin calls go through the MOVoxel facade (MOVoxelAlias.h) — this
 * component never names a Voxel type. The cached sculpt actors are held as plain
 * AActor* (the facade revalidates/casts them). When the Voxel API changes, only
 * MOVoxelAlias.cpp needs updating.
 *
 * BRUSH CONFIGURATION:
 * - Radius: Size of affected area (Unreal Units)
 * - Strength: Intensity of modification (0-1)
 * - Falloff: Edge softness for flatten (0-1)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] VOXEL PLUGIN DEPENDENCY: Requires Voxel Plugin Pro 2.0. If plugin
 *   not loaded, sculpt actors will be null. Check before use.
 *
 * [2024-02] SCULPT PERSISTENCE: Voxel modifications are saved via the Voxel
 *   Plugin's own save system, NOT MOPersistenceSubsystem. Ensure both save.
 *
 * [2024-02] PERFORMANCE: Large radius operations can be expensive. Consider
 *   throttling or chunking for very large modifications.
 *
 * [2024-02] UNDO SUPPORT: Voxel Plugin may have undo support. Check if
 *   modifications should integrate with it.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOPlayerController.h - Owns this component
 * - Voxel Plugin sculpt actors - External dependency
 * - MOPersistenceSubsystem.h - World save (separate from voxel save)
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOInterruptibleInterface.h"
#include "MOTerraformingComponent.generated.h"

class AMOCharacter;

/**
 * Terraforming operation mode.
 * See file header for mode descriptions.
 *
 * NOTE: When adding a new mode, also:
 *   - Add a case to UMOTerraformingComponent::ApplyTerraformAction (the dispatcher)
 *   - Add a display name to GetModeDisplayName
 *   - Update the modulo in CycleMode (it counts via (int)MAX)
 */
UENUM(BlueprintType)
enum class EMOTerraformMode : uint8
{
	Dig				UMETA(DisplayName="Dig"),
	Raise			UMETA(DisplayName="Raise"),
	Flatten			UMETA(DisplayName="Flatten"),
	Smooth			UMETA(DisplayName="Smooth"),
	RemoveFoliage	UMETA(DisplayName="Remove Foliage"),

	MAX				UMETA(Hidden)
};

/**
 * Terraforming tool configuration.
 *
 * Strength values are intentionally split per-mode-family rather than a single
 * Strength field because the tools need very different intensities:
 *  - Raise/Dig at 1.0 produce massive terrain changes per click — far too
 *    aggressive for incremental terraforming. Default 0.1 gives a tenth of
 *    that per action, so a 5-second timer feels meaningful.
 *  - Smooth at 0.1 would be visually inert; it stays near 1.0.
 *  - Flatten uses Falloff, not Strength.
 *  - RemoveFoliage uses Radius only.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOTerraformConfig
{
	GENERATED_BODY()

	/** Radius of the terraforming brush (in Unreal units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming", meta=(ClampMin="10.0"))
	float Radius = 250.0f;

	/**
	 * Strength of Raise/Dig operations (0-1). Default 0.1 — a single 5-second
	 * action makes a noticeable but not overwhelming change. If you want a
	 * dramatic crater per click, set this higher.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming", meta=(ClampMin="0.0", ClampMax="1.0"))
	float RaiseLowerStrength = 0.1f;

	/**
	 * Strength of Smooth operation (0-1). Kept near full because smooth at
	 * 0.1 is visually indistinguishable from no change at all.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming", meta=(ClampMin="0.0", ClampMax="1.0"))
	float SmoothStrength = 1.0f;

	/** Falloff for flatten operations (0-1). Higher = softer edges. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Falloff = 0.5f;

	/** For flatten: target height (if not auto-computed from character feet). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming")
	float FlattenTargetHeight = 0.0f;

	/**
	 * When true, flatten uses the character's standing-on-ground Z as the
	 * target height (the player's feet). When false, flatten uses the Z of
	 * wherever the cursor's ray-trace hit the terrain.
	 *
	 * Default is true (feet) because the alternative made flattening cliffs
	 * very confusing — you'd aim partway up a slope and the flatten target
	 * would be at that intermediate height, not at the level the player
	 * was standing on.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming")
	bool bUseCharacterFeetForFlatten = true;
};

/**
 * Holds the captured intent of a terraform action while its 5-second progress
 * timer is running. Once the timer expires, ApplyPendingTerraform() reads
 * this struct and dispatches to the underlying voxel sculpt call.
 *
 * Captured AT THE MOMENT BeginTerraform is called — so if the player moves
 * during the 5-second window, the FlattenTargetHeight (character feet Z) is
 * locked to where they were aiming. Movement would cancel the action via the
 * interrupt system anyway, so this lock is mostly defensive.
 */
// FMOTerrainModifiedZone has moved to UMOTerrainModificationSubsystem.
// This component no longer tracks zones directly — it just reports each
// successful terraform action to that subsystem, which owns the data, the
// spatial index, the periodic sweep, and save/load integration.

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOTerraformPendingAction
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|Terraforming")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category="MO|Terraforming")
	EMOTerraformMode Mode = EMOTerraformMode::Dig;

	/** Where the terraforming brush is centered (worldspace). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Terraforming")
	FVector TargetLocation = FVector::ZeroVector;

	/** Surface normal at TargetLocation. Used by volume sculpt's Flatten. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Terraforming")
	FVector TargetNormal = FVector::UpVector;

	/**
	 * For Flatten: the Z value to flatten to. Already resolved at BeginTerraform
	 * time per Config.bUseCharacterFeetForFlatten — when this struct is read,
	 * we just use the value (no more "which Z do I use" decision).
	 */
	UPROPERTY(BlueprintReadOnly, Category="MO|Terraforming")
	float FlattenTargetHeight = 0.0f;

	/**
	 * Wall-clock seconds since this action began. Advanced by the component's
	 * Tick (NOT by the widget) so completion isn't gated on UI existing.
	 *
	 * Storing the start TIMESTAMP rather than just an elapsed counter would
	 * be safer against world-time-dilation, but at the moment we don't have
	 * dilation in flight for terraforming and the simpler counter avoids
	 * extra FPlatformTime calls per tick.
	 */
	UPROPERTY(BlueprintReadOnly, Category="MO|Terraforming")
	float ElapsedTime = 0.0f;

	/** Total duration of this action — copied from TerraformDurationSeconds at start. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Terraforming")
	float TotalTime = 5.0f;

	void Reset()
	{
		bActive = false;
		Mode = EMOTerraformMode::Dig;
		TargetLocation = FVector::ZeroVector;
		TargetNormal = FVector::UpVector;
		FlattenTargetHeight = 0.0f;
		ElapsedTime = 0.0f;
		TotalTime = 5.0f;
	}

	float GetProgress01() const
	{
		return TotalTime > 0.0f ? FMath::Clamp(ElapsedTime / TotalTime, 0.0f, 1.0f) : 0.0f;
	}

	float GetTimeRemaining() const
	{
		return FMath::Max(0.0f, TotalTime - ElapsedTime);
	}
};

/**
 * Broadcast when a terraform action begins (the 5-second progress timer
 * starts). UI listens to this to spawn the progress widget.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOTerraformStartedSignature, EMOTerraformMode, Mode, float, DurationSeconds);

/**
 * Broadcast every component tick while a terraform action is in flight.
 * UI listens to this to update the progress bar — the component is the
 * source of truth for elapsed time, the widget is just a view.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOTerraformProgressSignature, float, Progress01, float, TimeRemainingSeconds);

/**
 * Broadcast when the 5-second timer completes and the sculpt actually applied.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOTerraformCompletedSignature, bool, bSuccess);

/**
 * Broadcast when a pending terraform action was cancelled — by the player,
 * by a movement interrupt, or by any other source. UI listens to this to
 * tear down the progress widget.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOTerraformCancelledSignature);

/**
 * Component that enables terrain modification using the Voxel plugin.
 * Supports both height-based (2D) and volume-based (3D) terraforming.
 *
 * Each terraform action runs a 5-second progress timer (configurable via
 * TerraformDurationSeconds) before the sculpt is applied. The component is
 * a registered IMOInterruptibleInterface listener on its owning character,
 * so movement / damage / etc. cancel the in-flight action automatically.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOTerraformingComponent : public UActorComponent,
	public IMOInterruptibleInterface
{
	GENERATED_BODY()

public:
	UMOTerraformingComponent();

	/**
	 * Interrupt policy:
	 *   Movement / Damage / Knockdown / Unconscious / Death / EnteredCombat
	 *     / LostControl / External   -> CANCEL the pending terraform
	 *   UserCancel                    -> ignored (UI calls CancelTerraform directly)
	 *
	 * Terraforming requires concentration; no partial progress is meaningful
	 * mid-action.
	 */
	virtual void NotifyInterrupt_Implementation(const FMOInterruptContext& Context) override;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Current terraforming mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming")
	EMOTerraformMode CurrentMode = EMOTerraformMode::Dig;

	/** Terraforming tool configuration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming")
	FMOTerraformConfig Config;

	/** Maximum distance to terraform. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming")
	float MaxDistance = 1000.0f;

	/** Trace channel for terrain detection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** If true, use volume sculpting (3D). If false, use height sculpting (2D). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming")
	bool bUseVolumeSculpting = false;

	// ============================================================================
	// SCULPT ACTOR REFERENCES
	// ============================================================================

	/** Height sculpt actor (Voxel) for 2D terrain modification. Held as AActor* —
	 *  the MOVoxel facade validates/casts it. Auto-found at BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming|Actors")
	TWeakObjectPtr<AActor> HeightSculptActor;

	/** Volume sculpt actor (Voxel) for 3D terrain modification. Held as AActor* —
	 *  the MOVoxel facade validates/casts it. Auto-found at BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming|Actors")
	TWeakObjectPtr<AActor> VolumeSculptActor;

	/**
	 * Duration (seconds) of the progress timer on every terraform action.
	 * Default 5s per design — applies to all modes uniformly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming", meta=(ClampMin="0.1"))
	float TerraformDurationSeconds = 5.0f;

	// ============================================================================
	// TIMED ACTION (preferred API)
	// ============================================================================

	/**
	 * Begin a terraform action at the current aim location. Captures the
	 * target into PendingAction, registers the component as an interrupt
	 * listener on the owning character, and fires OnTerraformStarted so the
	 * UI can spawn a progress widget.
	 *
	 * The widget runs the 5-second timer and calls CompleteTerraform when
	 * done. CancelTerraform aborts at any point.
	 *
	 * @return True if a valid target was found and the action started.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	bool BeginTerraform();

	/** Apply the captured PendingAction. Called by the progress widget on success. */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	bool CompleteTerraform();

	/** Cancel any pending terraform action. Idempotent. */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	void CancelTerraform();

	/** True while a 5-second terraform timer is running and not yet applied. */
	UFUNCTION(BlueprintPure, Category="MO|Terraforming")
	bool IsTerraformPending() const { return PendingAction.bActive; }

	/** Read-only access to the captured action (for UI display). */
	UFUNCTION(BlueprintPure, Category="MO|Terraforming")
	const FMOTerraformPendingAction& GetPendingAction() const { return PendingAction; }

	// ============================================================================
	// LEGACY IMMEDIATE-APPLY API
	// ============================================================================
	//
	// These bypass the 5-second progress timer and apply the sculpt
	// immediately. Kept for tooling, automated tests, and AI controllers
	// that don't need a user-facing progress bar. Player input should
	// always go through BeginTerraform / CompleteTerraform / CancelTerraform.

	/**
	 * Perform terraforming at the current aim location immediately, with no
	 * progress timer. Used by tools and AI; player input should use
	 * BeginTerraform instead.
	 * @return True if terraforming was applied.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	bool TryTerraform();

	/**
	 * Perform terraforming at a specific world location.
	 * @param WorldLocation The world position to terraform at
	 * @param Mode The terraforming operation to perform
	 * @return True if terraforming was initiated
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	bool TerraformAtLocation(const FVector& WorldLocation, EMOTerraformMode Mode);

	/**
	 * Perform a dig operation (remove terrain).
	 * @param WorldLocation The world position to dig at
	 * @return True if dig was initiated
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	bool Dig(const FVector& WorldLocation);

	/**
	 * Perform a raise operation (add terrain).
	 * @param WorldLocation The world position to raise at
	 * @return True if raise was initiated
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	bool Raise(const FVector& WorldLocation);

	/**
	 * Perform a flatten operation at the given target height. Callers
	 * (TerraformAtLocation, BeginTerraform/ApplyPendingTerraform) resolve
	 * the target Z before invoking this — see Config.bUseCharacterFeetForFlatten.
	 * @param WorldLocation The world position to center the brush on.
	 * @param TargetHeight  The Z value to flatten to.
	 * @return True if flatten was initiated.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	bool Flatten(const FVector& WorldLocation, float TargetHeight = 0.0f);

	/**
	 * Perform a smooth operation.
	 * @param WorldLocation The world position to smooth at
	 * @return True if smooth was initiated
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	bool Smooth(const FVector& WorldLocation);

	/**
	 * Remove all foliage (ISM/HISM instances) within Config.Radius of
	 * WorldLocation. Used by the RemoveFoliage terraform mode. Does NOT
	 * yield items — this is clear-the-land destruction, not harvesting.
	 *
	 * @return True if any foliage was removed.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	bool RemoveFoliage(const FVector& WorldLocation);

	// ============================================================================
	// MODE MANAGEMENT
	// ============================================================================

	/** Cycle to the next terraforming mode. */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	void CycleMode();

	/** Set the terraforming mode. */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	void SetMode(EMOTerraformMode NewMode);

	/** Get the current mode as display text. */
	UFUNCTION(BlueprintPure, Category="MO|Terraforming")
	FText GetModeDisplayName() const;

	// ============================================================================
	// EVENTS
	// ============================================================================

	/** Called when terraforming mode changes. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOTerraformModeChangedSignature, EMOTerraformMode, OldMode, EMOTerraformMode, NewMode);
	UPROPERTY(BlueprintAssignable, Category="MO|Terraforming")
	FMOTerraformModeChangedSignature OnModeChanged;

	/** Broadcast when the 5-second progress timer starts (UI hook for the progress widget). */
	UPROPERTY(BlueprintAssignable, Category="MO|Terraforming")
	FMOTerraformStartedSignature OnTerraformStarted;

	/** Broadcast every tick while in-flight so the UI can update its progress bar. */
	UPROPERTY(BlueprintAssignable, Category="MO|Terraforming")
	FMOTerraformProgressSignature OnTerraformProgress;

	/** Broadcast when the timer completes and the sculpt was applied. */
	UPROPERTY(BlueprintAssignable, Category="MO|Terraforming")
	FMOTerraformCompletedSignature OnTerraformCompleted;

	/** Broadcast when a pending action was cancelled from any source (UI / interrupt / etc). */
	UPROPERTY(BlueprintAssignable, Category="MO|Terraforming")
	FMOTerraformCancelledSignature OnTerraformCancelled;

	// ============================================================================
	// UTILITY
	// ============================================================================

	/**
	 * Find terraforming target location via trace.
	 * @param OutLocation The hit location (if found)
	 * @param OutNormal The hit normal (if found)
	 * @return True if a valid target was found
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	bool FindTerraformTarget(FVector& OutLocation, FVector& OutNormal) const;

	/**
	 * Auto-find sculpt actors in the world.
	 * Call this if sculpt actors aren't set manually.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	void AutoFindSculptActors();

	/** Check if we have valid sculpt actors for the current mode. */
	UFUNCTION(BlueprintPure, Category="MO|Terraforming")
	bool HasValidSculptActor() const;

protected:
	virtual void BeginPlay() override;

	/**
	 * Drives the in-flight terraform action's elapsed-time counter. The
	 * component owns this — completion no longer depends on the UI widget
	 * existing, so the action applies even when the progress widget hasn't
	 * been configured. Tick is only enabled while PendingAction.bActive.
	 */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Co-op transport (#132/H17 follow-on). A remote client resolves its terraform target
	 * locally (viewpoint trace in BeginTerraform) and runs the timed action for responsive
	 * progress UI, then forwards the resolved apply here so the voxel mutation + persistence
	 * happen authoritatively on the host. NOTE: a client visual re-apply (MulticastApplyTerraform)
	 * is a separate follow-on -- it depends on the voxel plugin's edit-replication model and needs
	 * 2-client PIE to verify (avoid double-apply); still tracked under #132.
	 */
	UFUNCTION(Server, Reliable)
	void ServerApplyTerraform(EMOTerraformMode Mode, FVector Location, float FlattenHeight);

private:
	bool ResolveViewpoint(FVector& OutLocation, FRotator& OutRotation) const;

	// Height sculpting operations
	bool HeightDig(const FVector2D& Location);
	bool HeightRaise(const FVector2D& Location);
	bool HeightFlatten(const FVector2D& Location, float TargetHeight);
	bool HeightSmooth(const FVector2D& Location);

	// Volume sculpting operations
	bool VolumeDig(const FVector& Location);
	bool VolumeRaise(const FVector& Location);
	bool VolumeFlatten(const FVector& Location, const FVector& Normal, float TargetHeight);
	bool VolumeSmooth(const FVector& Location);

	// ============================================================================
	// TIMED ACTION INTERNALS
	// ============================================================================

	/** Captured intent of the currently-pending action, valid only while bActive. */
	UPROPERTY()
	FMOTerraformPendingAction PendingAction;

	// Zone tracking moved to UMOTerrainModificationSubsystem. After a
	// terraform action applies, the component just calls
	// Subsystem->RegisterModifiedZone(...). The subsystem owns:
	//   - the zone list + merge logic
	//   - the spatial grid for fast queries
	//   - the periodic sweep that removes PCG-respawned foliage
	//   - save/load via UMOPersistenceSubsystem

	/**
	 * Cached AMOCharacter we registered with for interrupts. Stored separately
	 * so we can unregister even if GetOwner() changes mid-action.
	 */
	TWeakObjectPtr<AMOCharacter> RegisteredInterruptCharacter;

	/**
	 * Dispatch a captured pending action to the right sculpt method. Reads
	 * PendingAction; doesn't mutate it (the caller resets after).
	 */
	bool ApplyPendingTerraform();

	/**
	 * Compute the Z of the character's feet — i.e., the surface they're
	 * standing on. Used as the Flatten target when bUseCharacterFeetForFlatten.
	 * Returns false if the owner is not an AMOCharacter (e.g. AI pawn type).
	 */
	bool ResolveCharacterGroundZ(float& OutGroundZ) const;

	/** Register this component as an interrupt listener on the owning character. */
	void RegisterForInterrupts();

	/** Unregister from the owning character's interrupt broadcast. */
	void UnregisterFromInterrupts();
};
