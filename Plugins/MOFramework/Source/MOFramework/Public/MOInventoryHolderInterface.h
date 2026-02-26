/**
 * =============================================================================
 * MOInventoryHolderInterface.h - Inventory Access Contract
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Standardized interface for accessing inventory without FindComponentByClass.
 * Use this to get inventory from any actor type without knowing its class.
 *
 * WHO IMPLEMENTS THIS:
 * - AMOCharacter - Player/NPC pawns
 * - AMOContainerActor - Storage containers
 * - AMOCraftingStationActor - Crafting stations with storage
 *
 * WHO USES THIS:
 * - UMOCraftingSubsystem - Check ingredient availability
 * - Building system - Gather materials
 * - UI widgets - Display inventory contents
 *
 * =============================================================================
 * IMPLEMENTATION REQUIREMENTS
 * =============================================================================
 *
 * 1. GetInventory: Return the UMOInventoryComponent, or nullptr if none.
 *
 * 2. HasInventoryItem: Check if item exists with quantity.
 *    Default impl: GetInventory()->HasItem(ItemId, Quantity)
 *
 * 3. GetInventoryItemCount: Return total quantity of item.
 *    Default impl: GetInventory()->GetItemCount(ItemId)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-01] NULL INVENTORY: GetInventory() may return nullptr if component
 *   hasn't been created yet (during construction) or was destroyed.
 *   Always null-check the result.
 *
 * [2024-02] INTERFACE CAST: Use Cast<IMOInventoryHolderInterface>(Actor) or
 *   Actor->Implements<UMOInventoryHolderInterface>(). Both work.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOInventoryComponent.h - The actual inventory storage
 * - MOMaterialSourceInterface.h - Related interface for material gathering
 * - MOCraftingSubsystem.h - Uses this for ingredient checks
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

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
 * See file header for implementation requirements and pitfalls.
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
