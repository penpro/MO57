#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MOGameMode.generated.h"

class APawn;

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

	/** Minimum spawn height above water level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn")
	float MinSpawnHeightAboveWater = 100.0f;

	/** Height offset above detected ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn")
	float SpawnHeightOffset = 100.0f;

	/** Center point to search for spawn locations around. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn")
	FVector SpawnSearchCenter = FVector::ZeroVector;

	/** Radius to search for spawn points around SpawnSearchCenter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn")
	float SpawnSearchRadius = 50000.0f;

	/** Maximum attempts to find a valid spawn point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn")
	int32 MaxSpawnAttempts = 100;

	/** Pawn class to spawn for new games. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Spawn")
	TSubclassOf<APawn> DefaultNewGamePawnClass;

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

private:
	/** Register all configured tag mappings with the PCG interaction subsystem. */
	void RegisterPCGTagMappings();

	/** Handle pending new game - spawn initial pawn if coming from main menu. */
	void HandlePendingNewGame();

	/** Spawn the initial pawn for a new game. */
	void SpawnInitialPawn();

	/** Wait for voxel world to be ready, then spawn pawn. */
	void WaitForVoxelWorldAndSpawn();

	/** Whether we're waiting for voxel world to spawn. */
	bool bPendingSpawnAfterVoxelReady = false;
};
