/**
 * =============================================================================
 * MOInventoryGrid.h - Slot-Based Inventory Display Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Visual grid display for inventory slots. Creates UMOInventorySlot widgets
 * in a UniformGridPanel, handles click/drag interactions, and broadcasts
 * events for item manipulation.
 *
 * USAGE:
 * 1. Create in Blueprint (WBP_InventoryGrid)
 * 2. Call InitializeGrid(InventoryComponent) to bind
 * 3. Listen to delegate events for interactions
 * 4. Call RebuildGrid() when inventory size changes
 *
 * DELEGATES:
 * - OnSlotClicked: Left-click on slot (select item)
 * - OnSlotShiftClicked: Shift+click (quick transfer)
 * - OnSlotRightClicked: Right-click (context menu)
 * - OnSlotDrop: Drag-drop completed
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] SLOT POOLING: Pooling is implemented via AcquireSlotWidget() and
 *   ReleaseSlotWidget(). Slots are reset and reused, not destroyed/recreated.
 *
 * [2024-02] GUID VS INDEX: Delegates pass both SlotIndex and ItemGuid. Use
 *   GUID for item operations (stable across slot moves), index for UI only.
 *
 * [2024-02] DRAG VISUAL: Drag-drop creates MODragVisualWidget. Grid does NOT
 *   destroy it - MOInventoryDragDropOperation owns and cleans up the visual.
 *
 * [2024-02] INITIALIZATION ORDER: Call InitializeGrid() before RebuildGrid().
 *   InitializeGrid() binds to inventory events, RebuildGrid() creates slots.
 *   Calling RebuildGrid() with null InventoryComponent uses MinimumVisibleSlotCount.
 *
 * [2024-02] DELEGATE BINDING: BindInventoryDelegates() uses RemoveAll(this)
 *   before binding. Safe to call InitializeGrid() multiple times (e.g., on
 *   possession change to different pawn).
 *
 * [2024-02] VISUAL DATA MODE: SetSlotVisualData() populates grid without an
 *   inventory component. Used for "nearby items" display. Clears any existing
 *   inventory binding when called.
 *
 * [2024-02] SLOT WIDGET CLASS: SlotWidgetClass must be set in Blueprint
 *   defaults or RebuildGrid() will fail silently (no slots created).
 *
 * [2024-02] STRUCT REFERENCE: FMOInventorySlotVisualData defined in
 *   MOInventoryTypes.h. Contains: Icon, Quantity, bIsEmpty, ItemGuid, etc.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOInventorySlot.h - Individual slot widget (not in git, may be Blueprint)
 * - MOInventoryMenu.h - Parent container that hosts this grid
 * - MOInventoryDragDropOperation.h - Drag data payload
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOInventoryGrid.generated.h"

class UUniformGridPanel;
class UMOInventoryComponent;
class UMOInventorySlot;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOInventoryGridSlotClickedSignature, int32, SlotIndex, const FGuid&, ItemGuid);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOInventoryGridSlotShiftClickedSignature, int32, SlotIndex, const FGuid&, ItemGuid);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOInventoryGridSlotRightClickedSignature, int32, SlotIndex, const FGuid&, ItemGuid, FVector2D, ScreenPosition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOInventoryGridSlotDropSignature, int32, TargetSlotIndex, int32, SourceSlotIndex, UMOInventoryComponent*, SourceInventory);

UCLASS()
class MOFRAMEWORK_API UMOInventoryGrid : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOInventoryGrid(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void InitializeGrid(UMOInventoryComponent* InInventoryComponent);

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void RebuildGrid();

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void RefreshAllSlots();

	/** Clear the grid and remove all slot widgets. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void ClearGrid();

	/**
	 * Set visual data directly without an inventory component.
	 * Used for displaying items that aren't in an inventory (e.g., nearby world items).
	 * @param VisualData Array of slot visual data to display
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void SetSlotVisualData(const TArray<FMOInventorySlotVisualData>& VisualData);

	/** Get the inventory component this grid is displaying. */
	UFUNCTION(BlueprintPure, Category="MO|Inventory|UI")
	UMOInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|UI")
	FMOInventoryGridSlotClickedSignature OnGridSlotClicked;

	/** Called when a slot is Shift+Clicked for quick transfer. */
	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|UI")
	FMOInventoryGridSlotShiftClickedSignature OnGridSlotShiftClicked;

	/** Called when a slot is right-clicked. Use for context menu. */
	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|UI")
	FMOInventoryGridSlotRightClickedSignature OnGridSlotRightClicked;

	/** Called when an item is dropped onto a slot in this grid. */
	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|UI")
	FMOInventoryGridSlotDropSignature OnGridSlotDropReceived;

protected:
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void HandleSlotClicked(int32 SlotIndex, const FGuid& ItemGuid);

	UFUNCTION()
	void HandleSlotShiftClicked(int32 SlotIndex, const FGuid& ItemGuid);

	UFUNCTION()
	void HandleSlotRightClicked(int32 SlotIndex, const FGuid& ItemGuid, FVector2D ScreenPosition);

	UFUNCTION()
	void HandleSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory);

	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void HandleSlotsChanged();

	void BindInventoryDelegates();
	void UnbindInventoryDelegates();

	int32 GetDesiredSlotCount() const;

private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UUniformGridPanel> SlotsUniformGrid;

	UPROPERTY(EditDefaultsOnly, Category="MO|Inventory|UI")
	TSubclassOf<UMOInventorySlot> SlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="MO|Inventory|UI", meta=(ClampMin="1"))
	int32 Columns = 5;

	/**
	 * If the inventory reports 0 slots (or is not yet available), we still build this many slots
	 * so you can see the intended inventory size in the UI.
	 *
	 * Set this in your WBP_InventoryGrid defaults (recommended), or leave the default.
	 */
	UPROPERTY(EditDefaultsOnly, Category="MO|Inventory|UI", meta=(ClampMin="1"))
	int32 MinimumVisibleSlotCount = 20;

	UPROPERTY()
	TObjectPtr<UMOInventoryComponent> InventoryComponent;

	/** Active slot widgets currently displayed in the grid. */
	UPROPERTY()
	TArray<TObjectPtr<UMOInventorySlot>> SlotWidgets;

	// ============================================================================
	// WIDGET POOLING
	// ============================================================================

	/** Pool of inactive slot widgets available for reuse. */
	UPROPERTY()
	TArray<TObjectPtr<UMOInventorySlot>> PooledSlotWidgets;

	/** Get a slot widget from the pool or create a new one. */
	UMOInventorySlot* AcquireSlotWidget();

	/** Release a slot widget back to the pool. */
	void ReleaseSlotWidget(UMOInventorySlot* SlotWidget);

	/** Release all active slots to the pool. */
	void ReleaseAllSlotsToPool();
};
