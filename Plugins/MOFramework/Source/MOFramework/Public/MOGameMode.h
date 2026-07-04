/**
 * =============================================================================
 * MOGameMode.h - Base Game Mode with Voxel Integration
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Base game mode for MO Framework. Handles voxel world seed initialization,
 * safe spawn point detection above terrain, PCG tag-to-item mappings, and
 * new game / save load coordination.
 *
 * VOXEL INTEGRATION:
 * - ApplySeedToVoxelStamps(): Sets seed on all stamp components
 * - ApplySeedToHeightGraphParameter(): Sets seed in voxel graph
 * - InitializeVoxelWorldWithSeed(): Full initialization sequence
 * - bAutoInitializeVoxelWithSeed: Enable auto-init on level load
 *
 * SPAWN SYSTEM:
 * - FindSafeSpawnLocation(): Raycasts for valid terrain
 * - WaterLevelZ: Spawns must be above water
 * - MinSpawnSurfaceNormalZ: Rejects steep slopes
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] VOXEL TIMING: VoxelWorld->bCreateRuntimeOnBeginPlay must be FALSE
 *   for seed to apply correctly. Game mode calls CreateRuntime() after seed.
 *
 * [2024-02] SPAWN TIMING: After voxel CreateRuntime(), wait CollisionGenerationDelay
 *   (3s default) for collision meshes to generate before spawning pawns.
 *
 * [2024-02] RE-GROUND PAWNS: On save load, voxel regenerates asynchronously.
 *   Pawns may spawn above terrain. RegroundAllPawns() adjusts Z after ready.
 *
 * [2024-02] TAG MAPPINGS: PCGTagItemMappings must be configured for ISM/HISM
 *   harvesting to return correct items.
 *
 * =============================================================================
 * RELATED FILES: MOGameSettings.h, MOPersistenceSubsystem.h, MOCharacter.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MOGameMode.generated.h"

class APawn;
class AVoxelWorld;

/**
 * Entry for mapping PCG component tags to item IDs.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOTagItemMapping
{
	GENERATED_BODY()

	/** The component/actor tag to match (e.g., "GivesStick"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG")
	FName Tag;

	/** The item definition ID to give when harvested (e.g., "Stick01"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG")
	FName ItemId;
};

/**
 * Base game mode for MO Framework.
 * Handles initialization of PCG tag-to-item mappings and other framework setup.
 */
UCLASS()
class MOFRAMEWORK_API AMOGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMOGameMode();

	// ============================================================================
	// SPAWN POINT DETECTION
	// ============================================================================

	/**
	 * Find a safe spawn location above the terrain.
	 * Uses raycasting to find ground level and ensures spawn is above water.
	 * @return A safe spawn location, or fallback if no suitable location found.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Spawn")
	FVector FindSafeSpawnLocation() const;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Water level Z coordinate (spawn must be above this). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn")
	float WaterLevelZ = 0.0f;

	/** Minimum spawn height above water level (beach floor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn")
	float MinSpawnHeightAboveWater = 100.0f;

	/** Maximum spawn height above water level (beach ceiling - anything higher is mountain). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn")
	float MaxSpawnHeightAboveWater = 3000.0f;

	/** Height offset above detected ground (higher = safer but longer fall). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn")
	float SpawnHeightOffset = 200.0f;

	/** Center point to search for spawn locations around. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn")
	FVector SpawnSearchCenter = FVector::ZeroVector;

	/** Radius to search for spawn points around SpawnSearchCenter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn")
	float SpawnSearchRadius = 50000.0f;

	/** Maximum attempts to find a valid spawn point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn")
	int32 MaxSpawnAttempts = 100;

	/**
	 * Minimum Z component of surface normal (0-1).
	 * 1.0 = perfectly flat, 0.7 = ~45 degrees, 0.5 = 60 degrees.
	 * Surfaces steeper than this are rejected for spawning.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinSpawnSurfaceNormalZ = 0.7f;

	/** Whether to only spawn on voxel terrain (rejects trees, meshes, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn")
	bool bSpawnOnlyOnVoxelTerrain = true;

	/** Pawn class to spawn for new games. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn")
	TSubclassOf<APawn> DefaultNewGamePawnClass;

	// =========================================================================
	// SPAWN RECOVERY (used when the pawn fails to land — stuck, falling forever)
	// =========================================================================

	/**
	 * Seconds to wait for the pawn to land before triggering recovery.
	 * One-shot timeout — armed when the spawn flow binds to LandedDelegate;
	 * cancelled when the delegate fires. If the timeout elapses without a
	 * landing event, RecoverStuckSpawn runs and re-arms a fresh timeout for
	 * the next recovery attempt.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn|Recovery")
	float MaxLandingWaitSeconds = 5.0f;

	/**
	 * Maximum recovery attempts before falling back to a guaranteed-safe hard location.
	 * Each attempt tries a different strategy (lift, re-search, expanded re-search).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn|Recovery")
	int32 MaxLandingRecoveryAttempts = 3;

	/**
	 * Z offset added when "lifting" the pawn during the first recovery attempt
	 * (in case it's stuck inside terrain).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn|Recovery")
	float RecoveryLiftOffset = 5000.0f;

	// =========================================================================
	// LOAD-GAME REGROUNDING
	// =========================================================================
	//
	// After a save loads + voxel terrain regenerates from the seed, persisted
	// pawns can be slightly above/below the new terrain due to imperfect seed
	// reproducibility. We trace down to find the new terrain top and re-place
	// the pawn. Defaults are TIGHT so a small voxel wiggle doesn't visibly
	// teleport the pawn — only meaningful Z mismatches get corrected.

	/**
	 * Z mismatch (cm) required to trigger regrounding. Below this, the pawn
	 * stays at its exact saved Z (saved position is authoritative). Set higher
	 * to make load feel more "exact"; set lower if pawns fall through terrain.
	 * Default 200 = 2m: covers head-stuck-in-terrain but ignores ankle-deep drift.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn|Reground",
		meta=(ClampMin="10.0", ClampMax="2000.0"))
	float RegroundTriggerThreshold = 200.0f;

	/**
	 * Z offset (cm) above the traced terrain impact point. Pawn lands here
	 * then gravity settles them. Default 30 = ~30cm — feet sit right on top of
	 * terrain, no visible drop. Capsule half-height takes care of clearance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn|Reground",
		meta=(ClampMin="0.0", ClampMax="500.0"))
	float RegroundLiftAboveTerrain = 30.0f;

	/**
	 * Trace half-height (cm) above/below the pawn used to find terrain. Keep
	 * tight so the trace doesn't accidentally grab terrain hundreds of meters
	 * away (a cliff edge or floating island). Default 3000 = 30m up/down.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn|Reground",
		meta=(ClampMin="500.0", ClampMax="50000.0"))
	float RegroundTraceHalfHeight = 3000.0f;

	// ============================================================================
	// VOXEL SEED INTEGRATION
	// ============================================================================

	/**
	 * Apply the pending world seed to all voxel stamp components in the level.
	 * Call this before VoxelWorld creates its runtime.
	 *
	 * IMPORTANT: For this to work, set VoxelWorld->bCreateRuntimeOnBeginPlay = false
	 * in your level, then call this method followed by VoxelWorld->CreateRuntime().
	 *
	 * @param WorldSeed The seed value to apply (will be converted to FVoxelExposedSeed)
	 * @return Number of stamp components updated
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Voxel")
	int32 ApplySeedToVoxelStamps(int32 WorldSeed);

	/**
	 * Convert an integer seed to the 8-character string format used by Voxel Plugin.
	 * Uses the same algorithm as FVoxelExposedSeed::Randomize().
	 * @param Seed Integer seed value
	 * @return 8-character uppercase string (A-Z)
	 */
	UFUNCTION(BlueprintPure, Category="MO|Voxel")
	static FString IntSeedToVoxelSeedString(int32 Seed);

	/**
	 * Initialize the voxel world with the pending seed and start generation.
	 * This is a convenience method that:
	 * 1. Applies the seed from MOGameSettings to all stamp components
	 * 2. Calls VoxelWorld->CreateRuntime() to start generation
	 *
	 * IMPORTANT: VoxelWorld must have bCreateRuntimeOnBeginPlay = false for this to work.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Voxel")
	void InitializeVoxelWorldWithSeed();

	/**
	 * Debug function to log all voxel stamp seeds in the level.
	 * Call this after the world is generated to verify seeds are applied correctly.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Voxel|Debug")
	void DebugLogVoxelStampSeeds();

	/**
	 * Apply the world seed to the Voxel height graph's "Seed" parameter.
	 * This sets the parameter BEFORE runtime creation so the terrain uses our seed.
	 *
	 * REQUIRES: The Voxel height graph must have a parameter named "Seed" of type FVoxelExposedSeed.
	 *
	 * @param WorldSeed Integer seed to apply
	 * @return True if parameter was successfully set
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Voxel")
	bool ApplySeedToHeightGraphParameter(int32 WorldSeed);

	/**
	 * Name of the seed parameter in the Voxel height graph.
	 * Must match the parameter name you created in the Voxel graph editor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Voxel")
	FName VoxelSeedParameterName = TEXT("Seed");

protected:
	virtual void BeginPlay() override;

	// ============================================================================
	// PCG TAG MAPPINGS
	// ============================================================================

	/**
	 * Tag-to-item mappings for PCG-spawned objects.
	 * When an ISM/HISM component has a matching tag, harvesting gives the specified item.
	 * Example: Tag="GivesStick", ItemId="Stick01"
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG")
	TArray<FMOTagItemMapping> PCGTagItemMappings;

	/**
	 * If true, automatically apply seed from MOGameSettings to voxel stamps
	 * and call CreateRuntime() on new game start / save load.
	 * IMPORTANT: This MUST be true for save/load to work correctly!
	 * The voxel world must regenerate with the saved seed for pawn positions to be correct.
	 * Requires VoxelWorld->bCreateRuntimeOnBeginPlay = false.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Voxel")
	bool bAutoInitializeVoxelWithSeed = true;

	// =========================================================================
	// CO-OP JOIN-SPAWN (pipeline S0)
	// =========================================================================
	// The initial-pawn flow above serves exactly ONE player (the host). Remote
	// players arrive through two different doors and get a survivor spawned by
	// the same safe-spawn rules:
	//  - HandleSeamlessTravelPlayer: players carried along by the host's
	//    seamless ServerTravel (menu -> generated world). NOTE: these players
	//    NEVER see Login/PostLogin.
	//  - PostLogin: direct/late joins into a running world.
	// Joins that arrive before the voxel world is ready are queued and flushed
	// from the same completion points the host flow uses.

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;

private:
	/** Common entry for both join doors: queue until world-ready, then spawn. */
	void HandleRemotePlayerJoin(APlayerController* PC);

	/** Spawn + possess pending remote joins once the world is ready. */
	void FlushPendingJoinControllers();

	/**
	 * Lean survivor spawn for a REMOTE player: safe location -> spawn ->
	 * name -> recruit -> possess. Deliberately skips the host-only landing /
	 * loading-screen machinery (PendingLandingPawn is single-slot host state).
	 */
	APawn* SpawnJoinPawnForController(APlayerController* PC);

	/** Remote PCs that joined before the world was ready for pawns. */
	TArray<TWeakObjectPtr<APlayerController>> PendingJoinControllers;

	/** Set at the completion points of the new-game and load flows. */
	bool bWorldReadyForJoins = false;
	/** Register all configured tag mappings with the PCG interaction subsystem. */
	void RegisterPCGTagMappings();

	/** Handle pending new game - spawn initial pawn if coming from main menu. */
	void HandlePendingNewGame();

	/** Spawn the initial pawn for a new game. */
	void SpawnInitialPawn();

	/** Wait for voxel world to be ready, then spawn pawn. */
	void WaitForVoxelWorldAndSpawn();

	/** Timer callback to check voxel readiness. */
	// === Voxel readiness — event-driven via UMOVoxelReadinessSubsystem ===
	// Previously this was four FTimerHandles + a CheckVoxelReadyAndX poll pair
	// for each of new-game and load. Now both subscribe to OnVoxelReady on
	// the readiness subsystem; it owns the single polling loop project-wide.

	/** OnVoxelReady handler for the new-game spawn flow. */
	void HandleVoxelReadyForNewGame();

	/** Wait for voxel world to be ready, then re-ground all loaded pawns. */
	void WaitForVoxelAndRegroundPawns();

	/** OnVoxelReady handler for the load-game regrounding flow. */
	void HandleVoxelReadyForLoad();

	/** Actually re-ground all MOCharacters to terrain level. */
	void RegroundAllPawns();

	// =========================================================================
	// LOADING-SCREEN-HELD-UNTIL-LANDED (load path)
	// =========================================================================
	//
	// After regrounding finishes, the player pawn may still be slightly in
	// the air while voxel collision settles. Instead of dismissing the
	// loading screen immediately and showing the player a few seconds of
	// falling pawns, we poll the player pawn's grounded state and only
	// dismiss once they've actually landed (or after a generous timeout).

	/** The player pawn we're waiting for to land. Cleared after dismiss. */
	TWeakObjectPtr<APawn> WaitingForLandingPawn;

	/** Timer handle for landing poll loop. */
	FTimerHandle PlayerLandingPollHandle;

	/** World time when landing poll started — used for timeout. */
	float PlayerLandingPollStartedAtSeconds = 0.0f;

	/**
	 * Hard timeout (seconds) for waiting on terrain to appear under the saved
	 * X/Y. Last-resort safe-spawn only fires after this elapses. Default 30s —
	 * voxel streaming + collision generation can be slow for saves far from
	 * world origin. Loading screen + black fade stays up for the entire wait.
	 */
	UPROPERTY(EditAnywhere, Category="MO|Spawn|Reground",
		meta=(ClampMin="1.0", ClampMax="120.0"))
	float PlayerLandingMaxWaitSeconds = 30.0f;

	/**
	 * Tiny clearance offset (cm) above saved Z (or terrain top, once found)
	 * where we place the pawn. Just enough to avoid clipping into geometry.
	 * Saved position is authoritative — we don't drop the pawn from height.
	 */
	UPROPERTY(EditAnywhere, Category="MO|Spawn|Reground",
		meta=(ClampMin="5.0", ClampMax="200.0"))
	float PlayerLandingSpawnOffset = 25.0f;

	/**
	 * Half-range (cm) for the terrain probe trace around the saved Z. Each
	 * poll, we trace from (savedZ + this) down to (savedZ - this) at the
	 * saved X/Y. 5000 = 50m above + 50m below = 100m total window. Wide
	 * enough to catch voxel terrain even if collision regen lands a bit off
	 * the saved Z, narrow enough not to pick up "wrong" terrain in caves or
	 * underground voids.
	 */
	UPROPERTY(EditAnywhere, Category="MO|Spawn|Reground",
		meta=(ClampMin="100.0", ClampMax="20000.0"))
	float PlayerLandingTerrainSearchDistance = 5000.0f;

	/** Saved Z captured at landing-poll start so the probe trace stays anchored. */
	float SavedAnchorZ = 0.0f;
	FVector SavedAnchorXY = FVector::ZeroVector;

	/**
	 * Number of consecutive polls where the detected terrain Z must stay
	 * within LandingTerrainStableThreshold of the previous reading before we
	 * declare the LOD/refinement passes finished. Default 3 = 750ms with the
	 * 250ms poll interval.
	 */
	UPROPERTY(EditAnywhere, Category="MO|Spawn|Reground",
		meta=(ClampMin="1", ClampMax="20"))
	int32 LandingTerrainStableTicks = 3;

	/**
	 * Δ in cm between consecutive terrain hits considered "stable" — LOD has
	 * settled enough to commit. 10cm = a thumb's-width wobble. Voxel LOD0
	 * is typically sub-cm precise so this is generous.
	 */
	UPROPERTY(EditAnywhere, Category="MO|Spawn|Reground",
		meta=(ClampMin="0.5", ClampMax="100.0"))
	float LandingTerrainStableThreshold = 10.0f;

	/** Internal: terrain Z from the previous successful poll, for stability check. */
	float LastTerrainProbeZ = 0.0f;

	/** Internal: consecutive count of polls where probe Z stayed stable. */
	int32 ConsecutiveStableTerrainTicks = 0;

	/** Begin polling for the player pawn's grounded state. */
	void BeginPlayerLandingPoll(APawn* PlayerPawn);

	/** Timer callback — dismisses loading screen + clears state when player lands. */
	void PollPlayerLanding();

	/** Final teardown — dismiss screen, lift suppression, clear flags. Idempotent. */
	void FinishLoadHandoff();


	/** Delay in seconds after voxel ready before searching for land (allows collision generation). */
	static constexpr float CollisionGenerationDelay = 3.0f;

	// ============================================================================
	// PAWN LANDING DETECTION (for loading screen dismissal)
	// ============================================================================
	//
	// Event-driven: bind to ACharacter::LandedDelegate after Possess(NewPawn),
	// then arm a one-shot MaxLandingWaitSeconds timeout as a safety net. If
	// the delegate fires first, the timeout is cancelled and the loading
	// screen dismisses. If the timeout fires first, RecoverStuckSpawn
	// teleports the pawn and re-arms a fresh timeout for the next attempt.
	//
	// Replaces the previous 10Hz polling loop on CheckPawnLanded — same end
	// behavior, but no per-frame timer cost across the entire spawn flow.

	/**
	 * LandedDelegate callback. Fired by ACharacter when MOVE_Falling
	 * transitions to MOVE_Walking. Forwards to OnPawnLandedSafely.
	 */
	UFUNCTION()
	void HandlePawnLanded(const FHitResult& Hit);

	/** Called when pawn has safely landed on ground - dismisses loading screen. */
	void OnPawnLandedSafely();

	/**
	 * Called after the landing timer has run for MaxLandingWaitSeconds without the pawn
	 * landing. Tries a progressive recovery — lift up, then re-search, then hard fallback.
	 * Re-arms the one-shot landing timeout at the end of each non-fallback recovery.
	 */
	void RecoverStuckSpawn();

	/**
	 * Arm the one-shot MaxLandingWaitSeconds timeout. Called by SpawnInitialPawn
	 * after binding LandedDelegate, and by RecoverStuckSpawn after each teleport
	 * recovery. Safe to call multiple times; resets the existing timer.
	 */
	void ArmLandingTimeout();

	/** Timer handle for the one-shot landing timeout (drives RecoverStuckSpawn). */
	FTimerHandle PawnLandingTimerHandle;

	/** Track the pawn we're waiting to land. */
	TWeakObjectPtr<APawn> PendingLandingPawn;

	/** Number of recovery attempts triggered for this spawn. */
	int32 LandingRecoveryAttempts = 0;
};
