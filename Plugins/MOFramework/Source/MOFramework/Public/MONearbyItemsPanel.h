/**
 * =============================================================================
 * MONearbyItemsPanel.h - World Items Pickup Panel
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Panel displaying world items within pickup range. Uses inventory grid
 * display but sources from AMOWorldItem actors instead of inventory.
 *
 * FEATURES:
 * - Grid display of nearby world items
 * - Click to select/inspect
 * - Shift+Click to quick pickup to target inventory
 * - Right-click for context menu
 * - Drag items to inventory grids
 * - LootAllToInventory() for bulk pickup
 *
 * USAGE:
 * - Parent widget calls RefreshNearbyItems() with world items in range
 * - SetQuickPickupTarget() sets destination for Shift+Click
 * - Delegates broadcast for selection/pickup events
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] WORLD ITEM CACHING: CachedNearbyItems parallel to grid slots.
 *   Index in cache matches slot index for lookup.
 *
 * [2024-02] SLOT VISUAL DATA: Slots set via SetVisualData() with
 *   SourceWorldItem, not via InitializeSlot().
 *
 * [2024-02] EMPTY STATE: UpdateEmptyState() shows/hides EmptyMessageText
 *   based on CachedNearbyItems count.
 *
 * =============================================================================
 * RELATED FILES: MOUnifiedInventoryMenu.h, MOInventoryGrid.h, MOWorldItem.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MONearbyItemsPanel.generated.h"

class UMOInventoryGrid;
class UMOInventoryComponent;
class UTextBlock;
class AMOWorldItem;

/**
 * Delegate broadcast when a nearby item is selected.
 * @param WorldItem The selected world item
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnNearbyItemSelected, AMOWorldItem*, WorldItem);

/**
 * Delegate broadcast when a nearby item is picked up.
 * @param WorldItem The picked up world item
 * @param TargetInventory The inventory it was added to
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnNearbyItemPickedUp, AMOWorldItem*, WorldItem, UMOInventoryComponent*, TargetInventory);

/**
 * Delegate broadcast when the nearby items list changes (item dropped to world, etc.).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnNearbyItemsChanged);

/**
 * Delegate broadcast when a nearby item is right-clicked.
 * @param WorldItem The right-clicked world item
 * @param ScreenPosition The screen position for context menu placement
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnNearbyItemRightClicked, AMOWorldItem*, WorldItem, FVector2D, ScreenPosition);

/**
 * Panel displaying nearby world items that can be picked up.
 *
 * Shows world items within range using an inventory grid display.
 * Supports:
 *   - Single item pickup
 *   - Loot all to specified inventory
 *   - Visual feedback for empty state
 */
UCLASS()
class MOFRAMEWORK_API UMONearbyItemsPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	UMONearbyItemsPanel(const FObjectInitializer& ObjectInitializer);

	// ============================================================================
	// NEARBY ITEMS MANAGEMENT
	// ============================================================================

	/**
	 * Refresh the display with current nearby items.
	 * @param NearbyItems Array of world items within pickup range
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Nearby")
	void RefreshNearbyItems(const TArray<AMOWorldItem*>& NearbyItems);

	/** Clear all displayed nearby items. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Nearby")
	void ClearNearbyItems();

	/** Get the count of currently displayed nearby items. */
	UFUNCTION(BlueprintPure, Category="MO|Inventory|Nearby")
	int32 GetNearbyItemCount() const;

	/**
	 * Set the target inventory for Shift+Click quick pickups.
	 * @param TargetInventory The inventory to transfer items to on shift+click
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Nearby")
	void SetQuickPickupTarget(UMOInventoryComponent* TargetInventory);

	// ============================================================================
	// PICKUP OPERATIONS
	// ============================================================================

	/**
	 * Pick up a single nearby item into the target inventory.
	 * @param WorldItem The world item to pick up
	 * @param TargetInventory The inventory to add the item to
	 * @return True if successfully picked up
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Nearby")
	bool PickupItem(AMOWorldItem* WorldItem, UMOInventoryComponent* TargetInventory);

	/**
	 * Pick up all nearby items into the target inventory.
	 * @param TargetInventory The inventory to add items to
	 * @return Number of items successfully picked up
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Nearby")
	int32 LootAllToInventory(UMOInventoryComponent* TargetInventory);

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Broadcast when a nearby item is selected (clicked). */
	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|Nearby")
	FMOOnNearbyItemSelected OnItemSelected;

	/** Broadcast when a nearby item is picked up. */
	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|Nearby")
	FMOOnNearbyItemPickedUp OnItemPickedUp;

	/** Broadcast when nearby items list changes (item dropped to world, etc.). */
	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|Nearby")
	FMOOnNearbyItemsChanged OnNearbyItemsChanged;

	/** Broadcast when a nearby item is right-clicked (for context menu). */
	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|Nearby")
	FMOOnNearbyItemRightClicked OnNearbyItemRightClicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ============================================================================
	// BIND WIDGETS
	// ============================================================================

	/** Grid displaying nearby items. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UMOInventoryGrid> NearbyGrid;

	/** Text shown when no nearby items are found. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyMessageText;

private:
	void UpdateEmptyState();

	UFUNCTION()
	void HandleSlotClicked(int32 SlotIndex, const FGuid& ItemGuid);

	UFUNCTION()
	void HandleSlotShiftClicked(int32 SlotIndex, const FGuid& ItemGuid);

	UFUNCTION()
	void HandleSlotRightClicked(int32 SlotIndex, const FGuid& ItemGuid, FVector2D ScreenPosition);

	UFUNCTION()
	void HandleSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory);

	/** Cached list of nearby world items (parallel to grid slots). */
	UPROPERTY()
	TArray<TWeakObjectPtr<AMOWorldItem>> CachedNearbyItems;

	/** Target inventory for Shift+Click quick pickups. */
	UPROPERTY()
	TWeakObjectPtr<UMOInventoryComponent> QuickPickupTargetInventory;
};
