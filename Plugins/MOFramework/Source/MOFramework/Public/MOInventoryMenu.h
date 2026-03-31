/**
 * =============================================================================
 * MOInventoryMenu.h - Main Inventory Interface Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Main inventory interface showing item grid and item details. Handles item
 * selection, context menus, and drag-drop operations.
 *
 * COMPOSITION:
 * - InventoryGrid: Grid of inventory slots
 * - ItemInfoPanel: Selected item details (optional)
 *
 * INTERACTIONS:
 * - Left-click: Select item, show details
 * - Right-click: Open context menu (use, drop, split)
 * - Shift-click: Quick transfer to other inventory
 * - Drag-drop: Move/swap items between slots
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] LEGACY DELEGATES: FMOInventoryMenuRequestCloseSignature is legacy.
 *   New code should use FMOUIRequestClose from MOUIDelegates.h.
 *
 * [2024-02] CONTEXT MENU POSITION: OnSlotRightClicked includes ScreenPosition
 *   (viewport coordinates). Use for MOContextMenuBase::SetPopupPosition().
 *
 * [2024-02] COMPONENT REBIND: If inventory component changes (e.g., possession
 *   of different pawn), call InitializeMenu() again with new component.
 *   InitializeMenu() handles unbinding old delegates before rebinding.
 *
 * [2024-02] TAB KEY CLOSE: NativeOnKeyDown() handles Tab key to close menu.
 *   Broadcasts OnCloseRequested (new) and OnRequestClose (legacy).
 *
 * [2024-02] SELECTION STATE: SelectedItemGuid tracks current selection.
 *   HandleGridSlotClicked() updates selection and refreshes ItemInfoPanel.
 *   Selection persists across RefreshAll() calls.
 *
 * [2024-02] REQUIRED WIDGETS: InventoryGrid and ItemInfoPanel are required
 *   (BindWidget, not optional). Blueprint must include both or compile fails.
 *
 * [2024-02] ITEM INFO PANEL: ItemInfoPanel is UMOItemInfoPanel - shows item
 *   details when selected. Null-check before calling SetItem() internally.
 *
 * [2024-02] DELEGATE BINDING: NativeConstruct() binds grid delegates with
 *   RemoveAll(this) first. Safe for widget reuse/reinitialization.
 *
 * [2024-02] NULL COMPONENT HANDLING: InitializeMenu(nullptr) is SAFE but results
 *   in empty grid. InventoryComponent stored as raw TObjectPtr, not validated.
 *   SYMPTOM: Empty slots, no items displayed, no crash.
 *   Always pass valid component or check before calling.
 *
 * [2024-02] WIDGET DESTRUCTION TIMING: If menu destroyed while drag operation
 *   active, drag visual orphaned but cleaned up by UE garbage collection.
 *   No crash, but visual may linger for one frame. Avoid destroying menu
 *   during active drag - call CancelDrag() on grid first if needed.
 *
 * [2024-02] SELECTION AFTER ITEM REMOVED: If selected item is removed from
 *   inventory (dropped, consumed), SelectedItemGuid becomes stale.
 *   HandleInventoryChanged() clears selection if GUID no longer valid.
 *   ItemInfoPanel shows empty state. This is expected behavior.
 *
 * [2024-02] CONTEXT MENU COORDINATES: OnSlotRightClicked provides ScreenPosition
 *   in viewport coordinates (0,0 = top-left of game window).
 *   SYMPTOM if passed to world-space UI: Menu appears off-screen or at origin.
 *   Use SetPositionInViewport() or convert via GetViewportSize().
 *
 * [SEVERITY GUIDE]
 *   REQUIRED WIDGETS: Compile-time failure (won't package)
 *   NULL COMPONENT: Runtime graceful degradation (empty display)
 *   LEGACY DELEGATES: Code smell only (both work, prefer new)
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOInventoryGrid.h - Grid subwidget
 * - MOInventoryUIController.h - Manages this menu's lifecycle
 * - MOInventoryComponent.h - Data source
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOMenuWidgetBase.h"
#include "MOInventoryMenu.generated.h"

class UMOInventoryComponent;
class UMOInventoryGrid;
class UMOItemInfoPanel;

// Legacy delegates - prefer standard delegates from MOUIDelegates.h for new code
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOInventoryMenuRequestCloseSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOInventoryMenuSlotRightClickedSignature, int32, SlotIndex, const FGuid&, ItemGuid, FVector2D, ScreenPosition);

/**
 * Inherits from UMOMenuWidgetBase for standardized CommonUI input handling.
 * Tab/Escape close is handled by base class via bIsBackHandler.
 */
UCLASS()
class MOFRAMEWORK_API UMOInventoryMenu : public UMOMenuWidgetBase
{
	GENERATED_BODY()

public:
	UMOInventoryMenu(const FObjectInitializer& ObjectInitializer);

	// Call this right after CreateWidget in your PlayerController.
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void InitializeMenu(UMOInventoryComponent* InInventoryComponent);

	/** Get the inventory component this menu is displaying. */
	UFUNCTION(BlueprintPure, Category="MO|Inventory|UI")
	UMOInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	/** @deprecated Use OnRequestClose (from base class) instead. Broadcast for legacy code. */
	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|UI")
	FMOInventoryMenuRequestCloseSignature OnLegacyRequestClose;

	/** Called when a slot is right-clicked. UIManager uses this to show context menu. */
	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|UI")
	FMOInventoryMenuSlotRightClickedSignature OnSlotRightClicked;

	// Override RequestClose to also broadcast legacy delegate
	virtual void RequestClose() override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void HandleSlotsChanged();

	UFUNCTION()
	void HandleGridSlotClicked(int32 SlotIndex, const FGuid& ItemGuid);

	UFUNCTION()
	void HandleGridSlotRightClicked(int32 SlotIndex, const FGuid& ItemGuid, FVector2D ScreenPosition);

	void RefreshAll();

private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOInventoryGrid> InventoryGrid;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOItemInfoPanel> ItemInfoPanel;

	UPROPERTY()
	TObjectPtr<UMOInventoryComponent> InventoryComponent;

	UPROPERTY()
	FGuid SelectedItemGuid;
};
