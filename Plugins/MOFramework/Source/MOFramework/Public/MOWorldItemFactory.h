/**
 * =============================================================================
 * MOWorldItemFactory.h - Centralized World Item Spawning
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * WorldSubsystem consolidating all world item spawning logic. Replaces
 * duplicate spawning code in MOForagingSubsystem, MOInventoryComponent, etc.
 * Handles item definition lookup, mesh application, physics, and GUID setup.
 *
 * KEY RESPONSIBILITIES:
 * 1. Spawn AMOWorldItem actors with consistent configuration
 * 2. Apply item definition data to spawned items
 * 3. Handle drop physics for dropped items
 * 4. Manage GUIDs for persistence
 * 5. Support batch spawning for performance
 *
 * ARCHITECTURE NOTES:
 * - WorldSubsystem: One per UWorld
 * - FMOWorldItemSpawnParams encapsulates all spawn options
 * - DefaultWorldItemClass can be overridden for custom item actors
 * - SpawnWorldItemInternal is the core implementation
 *
 * SPAWN METHODS:
 * - SpawnWorldItem(Params) - Full control with params struct
 * - SpawnWorldItemSimple() - Minimal params for quick spawning
 * - SpawnWorldItemAtTransform() - For PCG/HISM conversions
 * - SpawnDroppedItem() - With physics enabled for inventory drops
 * - SpawnWorldItemsBatch() - Multiple items efficiently
 *
 * CRITICAL PATTERNS:
 * 1. Spawn Flow:
 *    SpawnWorldItem(Params) -> SpawnWorldItemInternal()
 *    -> SpawnActorDeferred() -> ApplyItemData() -> FinishSpawning()
 *
 * 2. Data Application:
 *    ApplyItemData() -> Set ItemComponent fields
 *    -> ApplyItemDefinitionToWorldMesh() -> Load/set mesh
 *
 * KNOWN PITFALLS:
 * 1. MESH LOADING: Item definition meshes load synchronously. For spawning
 *    many items at once, consider async loading or preloading.
 *
 * 2. GUID COLLISION: If ItemGuid is not provided, a new one is generated.
 *    Ensure GUIDs from save data are passed correctly for persistence.
 *
 * 3. PHYSICS STATE: EnableDropPhysics only works if mesh has collision.
 *    Items without proper collision will fall through world.
 *
 * RELATED FILES:
 * - MOWorldItem.h - Actor class being spawned
 * - MOItemComponent.h - Item data component
 * - MOInventoryComponent.h - Uses this for drops
 * - MOForagingSubsystem.h - Uses this for forage spawns
 *
 * TESTING CHECKLIST:
 * [ ] SpawnWorldItemSimple creates item at correct location
 * [ ] SpawnDroppedItem enables physics and item falls
 * [ ] Batch spawning works without performance issues
 * [ ] GUID is preserved through spawn process
 * [ ] Item mesh displays correctly from definition
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

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
