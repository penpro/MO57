#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOForagingSubsystem.generated.h"

class UInstancedStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class AMOWorldItem;
class UMOSkillsComponent;

/**
 * Data for a single ISM/HISM instance that can be revealed/harvested.
 * Supports both UInstancedStaticMeshComponent and UHierarchicalInstancedStaticMeshComponent.
 */
USTRUCT(BlueprintType)
struct FMOHISMInstanceData
{
	GENERATED_BODY()

	/** The ISM/HISM component containing the instance. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Foraging")
	TWeakObjectPtr<UInstancedStaticMeshComponent> HISMComponent;

	/** The instance index within the HISM. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Foraging")
	int32 InstanceIndex = INDEX_NONE;

	/** World transform of the instance. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Foraging")
	FTransform InstanceTransform;

	/** Item ID this instance represents (from mesh lookup). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Foraging")
	FName ItemId = NAME_None;

	/** Distance from search origin (for sorting). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Foraging")
	float Distance = 0.0f;

	bool IsValid() const
	{
		return HISMComponent.IsValid() && InstanceIndex != INDEX_NONE && !ItemId.IsNone();
	}
};

/**
 * Entry in the dig drop table - defines what items can be found when digging.
 */
USTRUCT(BlueprintType)
struct FMODigDropEntry
{
	GENERATED_BODY()

	/** Item definition ID to spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Foraging")
	FName ItemId;

	/** Minimum quantity to spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Foraging")
	int32 MinQuantity = 1;

	/** Maximum quantity to spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Foraging")
	int32 MaxQuantity = 1;

	/** Chance to find this item (0.0 - 1.0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Foraging", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DropChance = 0.1f;

	/** Minimum foraging level required to find this item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Foraging", meta=(ClampMin="0"))
	int32 MinForagingLevel = 0;
};

/**
 * World subsystem for foraging operations.
 * Handles HISM instance detection, conversion to world items, and dig mechanics.
 */
UCLASS()
class MOFRAMEWORK_API UMOForagingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================================
	// SUBSYSTEM LIFECYCLE
	// ============================================================================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ============================================================================
	// HISM QUERY & REVEAL
	// ============================================================================

	/**
	 * Query all harvestable HISM instances within a radius.
	 * @param Origin - Center point for the search
	 * @param Radius - Search radius in world units
	 * @return Array of instance data for harvestable items
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Foraging")
	TArray<FMOHISMInstanceData> QueryHISMInstancesInRadius(FVector Origin, float Radius) const;

	/**
	 * Convert HISM instances in radius to world item actors.
	 * Spawns AMOWorldItem at each instance location and removes the HISM instance.
	 * @param Origin - Center point for the search
	 * @param Radius - Search radius in world units
	 * @param ForagingPawn - Optional pawn to award XP to
	 * @return Array of spawned world item actors
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Foraging")
	TArray<AMOWorldItem*> RevealHISMInstancesInRadius(FVector Origin, float Radius, APawn* ForagingPawn = nullptr);

	// ============================================================================
	// DIG FOR SUPPLIES
	// ============================================================================

	/**
	 * Roll dig drops based on foraging skill level.
	 * Spawns items on the ground near the dig location.
	 * @param Location - Where to spawn dug items
	 * @param ForagingLevel - Player's foraging skill level
	 * @param ForagingPawn - Optional pawn to award XP to
	 * @return Array of spawned world item actors
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Foraging")
	TArray<AMOWorldItem*> DigForSupplies(FVector Location, int32 ForagingLevel, APawn* ForagingPawn = nullptr);

	/**
	 * Roll dig drops and add items directly to a pawn's inventory.
	 * Used for NPC survivors doing dig jobs - avoids spawning world items.
	 * @param Location - Dig location (used for level checks)
	 * @param ForagingLevel - Pawn's foraging skill level
	 * @param TargetPawn - Pawn whose inventory receives the items
	 * @return Number of items added to inventory
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Foraging")
	int32 DigForSuppliesToInventory(FVector Location, int32 ForagingLevel, APawn* TargetPawn);

	/**
	 * Harvest ISM instances and add items directly to a pawn's inventory.
	 * Used for NPC survivors doing forage jobs - avoids spawning world items.
	 * @param Origin - Center point for the search
	 * @param Radius - Search radius in world units
	 * @param TargetPawn - Pawn whose inventory receives the items
	 * @return Number of items added to inventory
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Foraging")
	int32 ForageToInventory(FVector Origin, float Radius, APawn* TargetPawn);

	// ============================================================================
	// SKILL INTEGRATION
	// ============================================================================

	/**
	 * Calculate search radius based on foraging skill level.
	 * Formula: min(BaseSearchRadius + Level * RadiusPerSkillLevel, MaxSearchRadius)
	 * @param ForagingLevel - Player's foraging skill level
	 * @return Calculated search radius in world units
	 */
	UFUNCTION(BlueprintPure, Category="MO|Foraging")
	float CalculateSearchRadius(int32 ForagingLevel) const;

	/**
	 * Get the foraging skill level from a pawn.
	 * @param Pawn - The pawn to check
	 * @return Foraging skill level (0 if no skills component or skill not found)
	 */
	UFUNCTION(BlueprintPure, Category="MO|Foraging")
	int32 GetForagingLevel(APawn* Pawn) const;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Base search radius for foraging (skill level 0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Foraging|Config")
	float BaseSearchRadius = 300.0f;

	/** Additional radius per skill level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Foraging|Config")
	float RadiusPerSkillLevel = 5.0f;

	/** Maximum search radius cap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Foraging|Config")
	float MaxSearchRadius = 800.0f;

	/** Maximum items to reveal per search (prevents performance issues). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Foraging|Config")
	int32 MaxItemsPerSearch = 20;

	/** XP awarded per item revealed through searching. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Foraging|Config")
	float XPPerRevealedItem = 2.0f;

	/** XP awarded per item found through digging. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Foraging|Config")
	float XPPerDugItem = 5.0f;

	/** Dig drop table - items that can be found when digging. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Foraging|Config")
	TArray<FMODigDropEntry> DigDropTable;

	/** Spawn radius for dug items (items spawn randomly within this radius). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Foraging|Config")
	float DigSpawnRadius = 50.0f;

private:
	/**
	 * Award foraging XP to a pawn.
	 * @param Pawn - Pawn to award XP to
	 * @param Amount - XP amount to award
	 */
	void AwardForagingXP(APawn* Pawn, float Amount);
};
