#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "MOInventorySlot.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UWidget;
class UBorder;
class UMOInventoryComponent;
class UMODragVisualWidget;

/**
 * Drag-drop operation payload for inventory slot drag.
 */
UCLASS()
class MOFRAMEWORK_API UMOInventorySlotDragOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	TWeakObjectPtr<UMOInventoryComponent> SourceInventoryComponent;

	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	int32 SourceSlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	FGuid ItemGuid;

	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	FName ItemDefinitionId;

	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	int32 Quantity = 0;
};

USTRUCT(BlueprintType)
struct FMOInventorySlotVisualData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	bool bHasItem = false;

	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	FGuid ItemGuid;

	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	FName ItemDefinitionId;

	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	int32 Quantity = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOInventorySlotClickedSignature, int32, SlotIndex, const FGuid&, ItemGuid);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOInventorySlotDroppedSignature, int32, TargetSlotIndex, int32, SourceSlotIndex, UMOInventoryComponent*, SourceInventory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOInventorySlotRightClickedSignature, int32, SlotIndex, const FGuid&, ItemGuid, FVector2D, ScreenPosition);

UCLASS()
class MOFRAMEWORK_API UMOInventorySlot : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOInventorySlot(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void InitializeSlot(UMOInventoryComponent* InInventoryComponent, int32 InSlotIndex);

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void RefreshFromInventory();

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	int32 GetSlotIndex() const { return SlotIndex; }

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	FGuid GetItemGuid() const { return CachedVisualData.ItemGuid; }

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	UMOInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|UI")
	FMOInventorySlotClickedSignature OnSlotClicked;

	/** Called when slot is right-clicked (context menu). Provides screen position for menu placement. */
	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|UI")
	FMOInventorySlotRightClickedSignature OnSlotRightClicked;

	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|UI")
	FMOInventorySlotDroppedSignature OnSlotDropReceived;

	/** Enable/disable drag-drop for this slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory|UI")
	bool bEnableDragDrop = true;

	/** If true, dropping outside inventory slots will drop the item into the world. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory|UI")
	bool bEnableWorldDrop = true;

protected:
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	// Optional Blueprint hook to customize visuals.
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Inventory|UI")
	void OnVisualDataUpdated(const FMOInventorySlotVisualData& NewVisualData);

private:
	UFUNCTION()
	void HandleSlotButtonClicked();

	UFUNCTION()
	void HandleSlotButtonPressed();

	UFUNCTION()
	void HandleSlotButtonReleased();

	void ApplyVisualDataToWidget();
	void UpdateQuantityBoxVisibility(int32 InQuantity);

private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> SlotButton;

	/** Optional: Add a Border named "DragHandle" on TOP of the button (last in hierarchy) to enable drag-drop.
	 *  Set its Visibility to "Visible" in the designer. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> DragHandle;

	/** Optional: Add a Border named "SlotBorder" to show hover/selection outline. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<class UBorder> SlotBorder;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> ItemIconImage;

	// Text inside QuantityBox (optional to touch, but helpful to keep correct when shown)
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;

	// Debug only
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DebugItemIdText;

	// You said you named this in the WBP. Keep exact name: QuantityBox
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> QuantityBox;

	UPROPERTY()
	TObjectPtr<UMOInventoryComponent> InventoryComponent;

	UPROPERTY()
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY()
	FMOInventorySlotVisualData CachedVisualData;

	UPROPERTY(EditDefaultsOnly, Category="MO|Inventory|UI")
	TObjectPtr<UTexture2D> DefaultItemIcon;

	UPROPERTY(EditDefaultsOnly, Category="MO|Inventory|UI")
	TObjectPtr<UTexture2D> EmptySlotIcon;

	/** Visual feedback when dragging over this slot. */
	UPROPERTY(Transient)
	bool bIsDragHovered = false;

	/** Track if we started a drag (to differentiate from click). */
	UPROPERTY(Transient)
	bool bDragStarted = false;

	/** Track if button is currently pressed. */
	UPROPERTY(Transient)
	bool bButtonPressed = false;

	/** Mouse position when button was pressed. */
	UPROPERTY(Transient)
	FVector2D PressedMousePosition = FVector2D::ZeroVector;

	/** Border color when slot is in normal state. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Inventory|UI|Colors")
	FLinearColor NormalBorderColor = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);

	/** Border color when hovering during drag. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Inventory|UI|Colors")
	FLinearColor HoverBorderColor = FLinearColor(0.2f, 0.8f, 0.2f, 1.0f);

	/** Border color when this slot is being dragged. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Inventory|UI|Colors")
	FLinearColor DraggingBorderColor = FLinearColor(0.8f, 0.8f, 0.2f, 1.0f);

	UUserWidget* CreateDragVisual();
	void SetDragHoverVisual(bool bHovered);
	void TryDropIntoWorld();
};
