#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MOInventoryHolderInterface.generated.h"

class UMOInventoryComponent;

UINTERFACE(MinimalAPI, Blueprintable)
class UMOInventoryHolderInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for actors that hold inventory.
 * Provides consistent access to inventory operations without FindComponentByClass.
 *
 * Implementers: AMOCharacter, AMOContainerActor, AMOCraftingStationActor
 */
class MOFRAMEWORK_API IMOInventoryHolderInterface
{
	GENERATED_BODY()

public:
	/**
	 * Get the inventory component held by this actor.
	 * @return The inventory component, or nullptr if not available
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Inventory")
	UMOInventoryComponent* GetInventory() const;

	/**
	 * Check if this holder has a specific item.
	 * @param ItemDefinitionId The item to check for
	 * @param Quantity Minimum quantity required (default 1)
	 * @return True if the holder has at least the specified quantity
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Inventory")
	bool HasInventoryItem(FName ItemDefinitionId, int32 Quantity = 1) const;

	/**
	 * Get the count of a specific item in this holder's inventory.
	 * @param ItemDefinitionId The item to count
	 * @return Total quantity of the item
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Inventory")
	int32 GetInventoryItemCount(FName ItemDefinitionId) const;
};
