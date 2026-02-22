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
	 * and call CreateRuntime() on new game start.
	 * Requires VoxelWorld->bCreateRuntimeOnBeginPlay = false.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Voxel")
	bool bAutoInitializeVoxelWithSeed = false;

private:
	/** Register all configured tag mappings with the PCG interaction subsystem. */
	void RegisterPCGTagMappings();

	/** Handle pending new game - spawn initial pawn if coming from main menu. */
	void HandlePendingNewGame();

	/** Spawn the initial pawn for a new game. */
	void SpawnInitialPawn();

	/** Wait for voxel world to be ready, then spawn pawn. */
	void WaitForVoxelWorldAndSpawn();

	/** Timer callback to check voxel readiness. */
	void CheckVoxelReadyAndSpawn();

	/** Timer callback after collision delay - actually spawn the pawn. */
	void OnCollisionDelayComplete();

	/** Whether we're waiting for voxel world to spawn. */
	bool bPendingSpawnAfterVoxelReady = false;

	/** Timer handle for polling voxel world readiness. */
	FTimerHandle VoxelReadyTimerHandle;

	/** Timer handle for collision generation delay. */
	FTimerHandle CollisionDelayTimerHandle;

	/** Timer handle for re-grounding loaded pawns after voxel regeneration. */
	FTimerHandle RegroundPawnsTimerHandle;

	/** Wait for voxel world to be ready, then re-ground all loaded pawns. */
	void WaitForVoxelAndRegroundPawns();

	/** Timer callback to check voxel readiness for re-grounding. */
	void CheckVoxelReadyAndReground();

	/** Actually re-ground all MOCharacters to terrain level. */
	void RegroundAllPawns();

	/** Whether we need to re-ground pawns after voxel is ready. */
	bool bPendingRegroundAfterVoxel = false;

	/** Delay in seconds after voxel ready before searching for land (allows collision generation). */
	static constexpr float CollisionGenerationDelay = 3.0f;

	// ============================================================================
	// PAWN LANDING DETECTION (for loading screen dismissal)
	// ============================================================================

	/** Check if possessed pawn has landed on the ground. */
	void CheckPawnLanded();

	/** Called when pawn has safely landed on ground - dismisses loading screen. */
	void OnPawnLandedSafely();

	/** Timer handle for pawn landing check. */
	FTimerHandle PawnLandingTimerHandle;

	/** Track the pawn we're waiting to land. */
	TWeakObjectPtr<APawn> PendingLandingPawn;
};
