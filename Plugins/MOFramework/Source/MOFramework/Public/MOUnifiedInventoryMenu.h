#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MOEquipmentComponent.h"
#include "MOUnifiedInventoryMenu.generated.h"

class UMOInventoryGrid;
class UMOInventoryComponent;
class UMOEquipmentComponent;
class UMOEquipmentPanel;
class UMOItemInfoPanel;
class UMONearbyItemsPanel;
class UMOCommonButton;
class UWidgetSwitcher;
class AMOWorldItem;

/**
 * Delegate broadcast when the unified inventory menu requests close.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnUnifiedInventoryMenuClosed);

/**
 * Delegate for quick transfer requests from slots.
 * @param ItemGuid The item to transfer
 * @param SourceInventory The inventory the item is coming from
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnQuickTransferRequested, const FGuid&, ItemGuid, UMOInventoryComponent*, SourceInventory);

/**
 * Delegate for context menu requests from slots.
 * @param InventoryComponent The inventory containing the item
 * @param ItemGuid The item GUID
 * @param SlotIndex The slot index
 * @param ScreenPosition Screen position for menu placement
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FMOOnContextMenuRequested, UMOInventoryComponent*, InventoryComponent, const FGuid&, ItemGuid, int32, SlotIndex, FVector2D, ScreenPosition);

/**
 * Unified inventory menu with dual-panel layout.
 *
 * Left Panel: Always shows player inventory
 * Right Panel: Tabbed switcher with:
 *   - Other Inventory (container/pawn)
 *   - Item Details
 *   - Nearby World Items
 *
 * Supports:
 *   - Drag/drop between panels
 *   - Shift+Click quick transfer
 *   - Bulk operations (Store All, Take All, Loot All)
 *   - Stack management (organize, fill stacks)
 */
UCLASS()
class MOFRAMEWORK_API UMOUnifiedInventoryMenu : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UMOUnifiedInventoryMenu();

	// ============================================================================
	// INITIALIZATION
	// ============================================================================

	/**
	 * Initialize the menu with player inventory and optional secondary source.
	 * @param PlayerInventory The player's inventory component
	 * @param SecondarySource Optional container or other pawn actor
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Menu")
	void InitializeMenu(UMOInventoryComponent* PlayerInventory, AActor* SecondarySource = nullptr);

	// ============================================================================
	// TAB SWITCHING
	// ============================================================================

	/** Switch to the Other Inventory tab. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Menu")
	void ShowOtherInventoryTab();

	/** Switch to the Details tab. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Menu")
	void ShowDetailsTab();

	/** Switch to the Nearby Items tab. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Menu")
	void ShowNearbyTab();

	/** Switch to the Equipment tab. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Menu")
	void ShowEquipmentTab();

	/** Get the current tab index (0=OtherInv, 1=Details, 2=Nearby, 3=Equipment). */
	UFUNCTION(BlueprintPure, Category="MO|Inventory|Menu")
	int32 GetCurrentTabIndex() const { return CurrentTabIndex; }

	// ============================================================================
	// SECONDARY INVENTORY (Container/Pawn)
	// ============================================================================

	/**
	 * Set the secondary inventory source (container or other pawn).
	 * @param Source Actor implementing IMOInventoryHolderInterface
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Menu")
	void SetSecondaryInventorySource(AActor* Source);

	/** Clear the secondary inventory source. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Menu")
	void ClearSecondaryInventorySource();

	/** Check if a secondary inventory is currently set. */
	UFUNCTION(BlueprintPure, Category="MO|Inventory|Menu")
	bool HasSecondaryInventory() const;

	/** Get the secondary inventory source actor. */
	UFUNCTION(BlueprintPure, Category="MO|Inventory|Menu")
	AActor* GetSecondarySource() const;

	// ============================================================================
	// ITEM SELECTION
	// ============================================================================

	/**
	 * Set the selected item and update the details panel.
	 * @param ItemGuid The item to select
	 * @param SourceInventory The inventory containing the item
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Menu")
	void SetSelectedItem(const FGuid& ItemGuid, UMOInventoryComponent* SourceInventory);

	/** Clear the current item selection. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Menu")
	void ClearSelectedItem();

	/** Get the currently selected item GUID. */
	UFUNCTION(BlueprintPure, Category="MO|Inventory|Menu")
	FGuid GetSelectedItemGuid() const { return SelectedItemGuid; }

	// ============================================================================
	// BULK OPERATIONS
	// ============================================================================

	/** Transfer all items from player inventory to secondary inventory. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Menu")
	void StoreAllToSecondary();

	/** Transfer all items from secondary inventory to player inventory. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Menu")
	void TakeAllFromSecondary();

	/** Pick up all nearby world items into player inventory. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Menu")
	void LootAllNearbyToPlayer();

	/** Pick up all nearby world items into secondary inventory. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Menu")
	void LootAllNearbyToSecondary();

	/** Drop all items from player inventory into the world. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Menu")
	void DropAllToWorld();

	/** Refresh the nearby items panel by querying for world items. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Menu")
	void RefreshNearbyPanel();

	// ============================================================================
	// STACK MANAGEMENT
	// ============================================================================

	/**
	 * Consolidate stacks and defragment the specified inventory.
	 * @param Inventory The inventory to organize
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Menu")
	void StackAndOrganize(UMOInventoryComponent* Inventory);

	/** Organize player inventory. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Menu")
	void StackAndOrganizePlayer();

	/** Organize secondary inventory. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Menu")
	void StackAndOrganizeSecondary();

	/**
	 * Fill partial stacks in player inventory from secondary inventory.
	 * Items are NOT transferred, only existing stacks are topped up.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Menu")
	void FillStacksLeftToRight();

	/**
	 * Fill partial stacks in secondary inventory from player inventory.
	 * Items are NOT transferred, only existing stacks are topped up.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Menu")
	void FillStacksRightToLeft();

	// ============================================================================
	// NEARBY ITEMS
	// ============================================================================

	/**
	 * Refresh the nearby items panel with current world items.
	 * @param NearbyItems Array of world items within range
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Menu")
	void RefreshNearbyItems(const TArray<AMOWorldItem*>& NearbyItems);

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Broadcast when the menu requests to close. */
	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|Menu")
	FMOOnUnifiedInventoryMenuClosed OnRequestClose;

	/** Broadcast when a quick transfer is requested (Shift+Click). */
	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|Menu")
	FMOOnQuickTransferRequested OnQuickTransferRequested;

	/** Broadcast when a context menu is requested (right-click). */
	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|Menu")
	FMOOnContextMenuRequested OnContextMenuRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	// ============================================================================
	// BIND WIDGETS
	// ============================================================================

	/** Left panel: Player inventory grid. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UMOInventoryGrid> PlayerInventoryGrid;

	/** Right panel: Widget switcher for tabs. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UWidgetSwitcher> RightPanelSwitcher;

	/** Right panel tab 0: Secondary inventory grid (container/pawn). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UMOInventoryGrid> SecondaryInventoryGrid;

	/** Right panel tab 1: Item details panel. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UMOItemInfoPanel> DetailsPanel;

	/** Right panel tab 2: Nearby items panel. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UMONearbyItemsPanel> NearbyPanel;

	/** Right panel tab 3: Equipment panel. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOEquipmentPanel> EquipmentPanel;

	// --- Tab Buttons ---

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UMOCommonButton> OtherInventoryTabButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UMOCommonButton> DetailsTabButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UMOCommonButton> NearbyTabButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> EquipmentTabButton;

	// --- Action Buttons ---

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> StoreAllButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> TakeAllButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> StackOrganizePlayerButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> StackOrganizeSecondaryButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> FillLeftButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> FillRightButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> LootNearbyToPlayerButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> LootNearbyToSecondaryButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> CloseButton;

private:
	// ============================================================================
	// INTERNAL HANDLERS
	// ============================================================================

	void BindButtonEvents();
	void UnbindButtonEvents();

	UFUNCTION()
	void HandleOtherInventoryTabClicked();

	UFUNCTION()
	void HandleDetailsTabClicked();

	UFUNCTION()
	void HandleNearbyTabClicked();

	UFUNCTION()
	void HandleEquipmentTabClicked();

	UFUNCTION()
	void HandleStoreAllClicked();

	UFUNCTION()
	void HandleTakeAllClicked();

	UFUNCTION()
	void HandleStackOrganizePlayerClicked();

	UFUNCTION()
	void HandleStackOrganizeSecondaryClicked();

	UFUNCTION()
	void HandleFillLeftClicked();

	UFUNCTION()
	void HandleFillRightClicked();

	UFUNCTION()
	void HandleLootNearbyToPlayerClicked();

	UFUNCTION()
	void HandleLootNearbyToSecondaryClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandlePlayerSlotRightClicked(int32 SlotIndex, const FGuid& ItemGuid, FVector2D ScreenPosition);

	UFUNCTION()
	void HandleSecondarySlotRightClicked(int32 SlotIndex, const FGuid& ItemGuid, FVector2D ScreenPosition);

	UFUNCTION()
	void HandlePlayerSlotShiftClicked(int32 SlotIndex, const FGuid& ItemGuid);

	UFUNCTION()
	void HandleSecondarySlotShiftClicked(int32 SlotIndex, const FGuid& ItemGuid);

	UFUNCTION()
	void HandleNearbyItemsChanged();

	UFUNCTION()
	void HandlePlayerSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory);

	UFUNCTION()
	void HandleEquipmentSlotDropReceived(EMOEquipmentSlot TargetSlot, const FGuid& ItemGuid, UMOInventoryComponent* SourceInventory);

	void UpdateTabButtonStates();
	void UpdateActionButtonStates();

	// ============================================================================
	// STATE
	// ============================================================================

	UPROPERTY()
	TWeakObjectPtr<UMOInventoryComponent> CachedPlayerInventory;

	UPROPERTY()
	TWeakObjectPtr<UMOEquipmentComponent> CachedEquipmentComponent;

	UPROPERTY()
	TWeakObjectPtr<AActor> CachedSecondarySource;

	UPROPERTY()
	TWeakObjectPtr<UMOInventoryComponent> CachedSecondaryInventory;

	FGuid SelectedItemGuid;
	int32 CurrentTabIndex = 3;  // Default to Equipment tab (0=OtherInv, 1=Details, 2=Nearby, 3=Equipment)
};
