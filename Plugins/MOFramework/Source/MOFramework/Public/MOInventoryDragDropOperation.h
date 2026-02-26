/**
 * =============================================================================
 * MOInventoryDragDropOperation.h - Inventory Drag & Drop Data
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * UDragDropOperation subclass carrying item data during inventory drag-drop.
 * Created when player starts dragging an item, consumed when dropped on
 * valid target (slot, ground, equipment).
 *
 * DRAG-DROP WORKFLOW:
 * 1. Player starts drag on inventory slot
 * 2. MOInventoryGrid creates UMOInventoryDragDropOperation
 * 3. Populates: DraggedItemGuid, SourceInventory, SourceSlotIndex, etc.
 * 4. Visual: MODragVisualWidget shows item icon following cursor
 * 5. Drop: Target widget receives OnDrop, extracts operation, processes
 *
 * DROP TARGETS:
 * - Another inventory slot → Move/swap/stack items
 * - Equipment slot → Equip item
 * - Ground (no target) → Drop item to world
 * - Crafting slot → Use as ingredient
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] GUID STABILITY: DraggedItemGuid is stable across the drag
 *   operation. Even if inventory changes, GUID identifies the item.
 *
 * [2024-02] SOURCE VALIDATION: On drop, verify SourceInventory still valid
 *   and item still exists. Player might have died or item consumed mid-drag.
 *
 * [2024-02] SLOT INDEX: SourceSlotIndex is -1 if item not from slot grid
 *   (e.g., dragged from context menu). Handle this case.
 *
 * [2024-02] QUANTITY: Quantity is copied at drag start. If partial stack
 *   drag is implemented, this represents the dragged amount, not total.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOInventoryGrid.h - Creates this operation on drag start
 * - MODragVisualWidget.h - Visual feedback during drag
 * - MOInventoryMenu.h - Handles drop on inventory grid
 * - MOEquipmentPanel.h - Handles drop on equipment slots
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "MOInventoryDragDropOperation.generated.h"

class UMOInventoryComponent;

/**
 * Drag-drop operation for inventory items.
 * See file header for drag-drop workflow details.
 */
UCLASS()
class MOFRAMEWORK_API UMOInventoryDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	/** The GUID of the item being dragged. */
	UPROPERTY(BlueprintReadWrite, Category="MO|Inventory|DragDrop")
	FGuid DraggedItemGuid;

	/** The source inventory component. */
	UPROPERTY(BlueprintReadWrite, Category="MO|Inventory|DragDrop")
	TObjectPtr<UMOInventoryComponent> SourceInventory;

	/** The source slot index (-1 if not from a slot). */
	UPROPERTY(BlueprintReadWrite, Category="MO|Inventory|DragDrop")
	int32 SourceSlotIndex = -1;

	/** Item definition ID for display purposes. */
	UPROPERTY(BlueprintReadWrite, Category="MO|Inventory|DragDrop")
	FName ItemDefinitionId = NAME_None;

	/** Quantity being dragged. */
	UPROPERTY(BlueprintReadWrite, Category="MO|Inventory|DragDrop")
	int32 Quantity = 1;
};
