#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOWorldItemFactory.generated.h"

class AMOWorldItem;
class UMOItemComponent;

/**
 * Parameters for spawning a world item.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWorldItemSpawnParams
{
	GENERATED_BODY()

	/** Item definition ID from datatable */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ItemDefinitionId = NAME_None;

	/** Quantity of items in the stack */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1"))
	int32 Quantity = 1;

	/** Optional GUID for the item (for persistence). If invalid, a new one is generated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid ItemGuid;

	/** World location to spawn at */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Location = FVector::ZeroVector;

	/** World rotation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator Rotation = FRotator::ZeroRotator;

	/** If true, enables physics simulation after spawn (for dropped items) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnableDropPhysics = false;

	/** Optional owner actor for spawn params */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TWeakObjectPtr<AActor> Owner;

	/** Optional instigator for spawn params */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TWeakObjectPtr<APawn> Instigator;

	/** Spawn collision handling */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESpawnActorCollisionHandlingMethod CollisionHandling = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
};

/**
 * World subsystem for spawning AMOWorldItem actors.
 * Consolidates duplicate item spawning logic from:
 * - MOForagingSubsystem::SpawnWorldItem()
 * - MOInventoryComponent::DropItemByGuid()
 * - MOInventoryComponent::SpawnWorldItem()
 *
 * Provides consistent item spawning with:
 * - Item definition lookup and visual application
 * - Optional physics enabling for dropped items
 * - GUID management for persistence
 * - Configurable spawn parameters
 */
UCLASS()
class MOFRAMEWORK_API UMOWorldItemFactory : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	// ============================================================================
	// SPAWNING
	// ============================================================================

	/**
	 * Spawn a world item with full parameters.
	 * @param Params Spawn parameters including item ID, location, physics settings
	 * @return Spawned world item, or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category="MO|WorldItem")
	AMOWorldItem* SpawnWorldItem(const FMOWorldItemSpawnParams& Params);

	/**
	 * Spawn a world item with minimal parameters.
	 * @param ItemDefinitionId Item ID from datatable
	 * @param Quantity Stack quantity
	 * @param Location World location
	 * @param Rotation World rotation (optional)
	 * @return Spawned world item, or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category="MO|WorldItem")
	AMOWorldItem* SpawnWorldItemSimple(
		FName ItemDefinitionId,
		int32 Quantity,
		FVector Location,
		FRotator Rotation = FRotator::ZeroRotator);

	/**
	 * Spawn a world item from a transform (useful for PCG/HISM conversions).
	 * @param ItemDefinitionId Item ID from datatable
	 * @param Quantity Stack quantity
	 * @param Transform World transform
	 * @return Spawned world item, or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category="MO|WorldItem")
	AMOWorldItem* SpawnWorldItemAtTransform(
		FName ItemDefinitionId,
		int32 Quantity,
		const FTransform& Transform);

	/**
	 * Spawn a dropped item with physics enabled.
	 * Used when dropping items from inventory.
	 * @param ItemDefinitionId Item ID from datatable
	 * @param Quantity Stack quantity
	 * @param ItemGuid GUID for persistence
	 * @param DropLocation World location
	 * @param DropRotation World rotation
	 * @param Owner Actor that owns this drop
	 * @return Spawned world item, or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category="MO|WorldItem")
	AMOWorldItem* SpawnDroppedItem(
		FName ItemDefinitionId,
		int32 Quantity,
		const FGuid& ItemGuid,
		FVector DropLocation,
		FRotator DropRotation,
		AActor* Owner = nullptr);

	// ============================================================================
	// BATCH SPAWNING
	// ============================================================================

	/**
	 * Spawn multiple world items at once.
	 * @param ParamsArray Array of spawn parameters
	 * @return Array of spawned items (may contain nullptrs for failed spawns)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|WorldItem")
	TArray<AMOWorldItem*> SpawnWorldItemsBatch(const TArray<FMOWorldItemSpawnParams>& ParamsArray);

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/**
	 * Set the default world item class to spawn.
	 * If not set, uses AMOWorldItem::StaticClass().
	 */
	void SetDefaultWorldItemClass(TSubclassOf<AMOWorldItem> InClass);

	/** Get the current default world item class */
	TSubclassOf<AMOWorldItem> GetDefaultWorldItemClass() const;

protected:
	/** Default class to spawn. Can be overridden per-spawn via Params. */
	UPROPERTY()
	TSubclassOf<AMOWorldItem> DefaultWorldItemClass;

	/**
	 * Internal spawn implementation.
	 * @param Params Spawn parameters
	 * @param WorldItemClass Class to spawn (uses default if null)
	 * @return Spawned world item
	 */
	AMOWorldItem* SpawnWorldItemInternal(
		const FMOWorldItemSpawnParams& Params,
		TSubclassOf<AMOWorldItem> WorldItemClass = nullptr);

	/**
	 * Apply item definition data to the spawned item.
	 * Sets ItemComponent data and calls ApplyItemDefinitionToWorldMesh().
	 */
	void ApplyItemData(AMOWorldItem* WorldItem, FName ItemDefinitionId, int32 Quantity, const FGuid& ItemGuid);
};
