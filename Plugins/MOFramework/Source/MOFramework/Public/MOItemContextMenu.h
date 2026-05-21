/**
 * =============================================================================
 * MOItemContextMenu.h - Item Right-Click Context Menu
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Context menu for item interactions (right-click). Shows action buttons
 * based on item properties. Supports both inventory items and world items.
 *
 * INVENTORY MODE BUTTONS:
 * - Use: Consume/equip (if consumable/equippable)
 * - Drop 1 / Drop All: Drop items to world
 * - Inspect: Learn about item (grants XP/knowledge)
 * - Split Stack: Split stackable items
 * - Craft: Open crafting filtered to this item
 * - Transfer: Quick transfer to other inventory
 * - Details: Show in detail panel
 *
 * WORLD ITEM MODE BUTTONS:
 * - Pickup: Add to inventory
 * - Inspect: Same as above
 * - (Drop buttons hidden)
 *
 * AUTO-CLOSE:
 * Menu auto-closes when mouse leaves ButtonContainer for AutoCloseDelay.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] MOUSE TRACKING: Uses timer-based mouse check, not NativeOnMouseLeave.
 *   ButtonContainer bounds used for IsMouseOverMenu().
 *
 * [2024-02] BUTTON VISIBILITY: RefreshButtonVisibility() is BlueprintNativeEvent.
 *   Base implementation checks item flags. Override in BP for custom logic.
 *
 * [2024-02] WORLD ITEM MODE: InitializeForWorldItem() sets SourceWorldItem.
 *   Check IsWorldItemMode() before accessing InventoryComponent.
 *
 * =============================================================================
 * RELATED FILES: MOInventoryUIController.h, MOInventorySlot.h, MOUnifiedInventoryMenu.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "MOItemContextMenu.generated.h"

class UMOCommonButton;
class UMOInventoryComponent;
class UPanelWidget;
class AMOWorldItem;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOContextMenuClosedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOContextMenuActionSignature, FName, ActionId, const FGuid&, ItemGuid);

UCLASS()
class MOFRAMEWORK_API UMOItemContextMenu : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Initialize the context menu for a specific inventory item.
	 * Call this after creating the widget and before adding to viewport.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|ContextMenu")
	void InitializeForItem(UMOInventoryComponent* InInventoryComponent, const FGuid& InItemGuid, int32 InSlotIndex);

	/**
	 * Initialize the context menu for a world item (nearby items panel).
	 * Shows Pickup/Inspect/Split options instead of Drop options.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|ContextMenu")
	void InitializeForWorldItem(AMOWorldItem* InWorldItem);

	/** Returns true if this menu is showing options for a world item. */
	UFUNCTION(BlueprintPure, Category="MO|UI|ContextMenu")
	bool IsWorldItemMode() const { return SourceWorldItem.IsValid(); }

	/** Get the world item (if in world item mode). */
	UFUNCTION(BlueprintPure, Category="MO|UI|ContextMenu")
	AMOWorldItem* GetWorldItem() const { return SourceWorldItem.Get(); }

	/**
	 * Position the menu at the given screen location (typically slot position).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|ContextMenu")
	void SetMenuPosition(FVector2D ScreenPosition);

	/** Called when the menu is closed for any reason. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|ContextMenu")
	FMOContextMenuClosedSignature OnMenuClosed;

	/** Called when an action button is clicked. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|ContextMenu")
	FMOContextMenuActionSignature OnActionSelected;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Called to update button visibility based on item properties. Override in BP for custom logic. */
	UFUNCTION(BlueprintNativeEvent, Category="MO|UI|ContextMenu")
	void RefreshButtonVisibility();

private:
	void BindButtonEvents();
	void CloseMenu();
	bool IsMouseOverMenu() const;

	// Button click handlers
	UFUNCTION() void HandleUseClicked();
	UFUNCTION() void HandleDrop1Clicked();
	UFUNCTION() void HandleDropAllClicked();
	UFUNCTION() void HandleInspectClicked();
	UFUNCTION() void HandleSplitStackClicked();
	UFUNCTION() void HandleCraftClicked();
	UFUNCTION() void HandleDetailsClicked();
	UFUNCTION() void HandleTransferClicked();
	UFUNCTION() void HandlePickupClicked();

private:
	// ============================================================
	// BIND WIDGETS - Create these in your WBP_ItemContextMenu
	// ============================================================

	/** Container panel that holds all the buttons. Used for mouse-over detection. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> ButtonContainer;

	/** Use/Consume button - hidden if item is not consumable. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> UseButton;

	/** Drop single item button. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> Drop1Button;

	/** Drop entire stack button. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> DropAllButton;

	/** Inspect item button - grants knowledge/XP. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> InspectButton;

	/** Split stack button - hidden if quantity <= 1. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> SplitStackButton;

	/** Craft button - opens crafting menu filtered to this item. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> CraftButton;

	/** Details button - shows item in details panel. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> DetailsButton;

	/** Transfer button - quick transfer to other inventory (visible when container open). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> TransferButton;

	/** Pickup button - pick up world item to inventory (visible for world items). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> PickupButton;

	// ============================================================
	// State
	// ============================================================

	UPROPERTY()
	TObjectPtr<UMOInventoryComponent> InventoryComponent;

	/** Source world item (alternative to InventoryComponent for nearby items). */
	UPROPERTY()
	TWeakObjectPtr<AMOWorldItem> SourceWorldItem;

	UPROPERTY()
	FGuid ItemGuid;

	UPROPERTY()
	int32 SlotIndex = INDEX_NONE;

	/** Time since mouse left the menu (for delayed auto-close). */
	float MouseOutsideTimer = 0.0f;

	/** Delay before auto-closing when mouse leaves (seconds). */
	UPROPERTY(EditDefaultsOnly, Category="MO|UI|ContextMenu")
	float AutoCloseDelay = 0.15f;

	/** Whether the menu has been initialized. */
	bool bInitialized = false;

	/** Timer handle for auto-close check. */
	FTimerHandle MouseCheckTimerHandle;

	/** Start the mouse check timer. */
	void StartMouseCheckTimer();

	/** Stop the mouse check timer. */
	void StopMouseCheckTimer();

	/** Called by timer to check mouse position. */
	void CheckMousePosition();
};
