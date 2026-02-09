#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "MOInventoryDragDropOperation.generated.h"

class UMOInventoryComponent;

/**
 * Drag-drop operation for inventory items.
 * Carries information about the dragged item and its source.
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
