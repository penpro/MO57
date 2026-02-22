#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "MOEquipmentComponent.h"
#include "MOInventorySlot.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UWidget;
class UBorder;
class UMOInventoryComponent;
class UMOEquipmentComponent;
class UMODragVisualWidget;
class AMOWorldItem;

/**
 * Drag-drop operation payload for inventory slot drag.
 */
UCLASS()
class MOFRAMEWORK_API UMOInventorySlotDragOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	/** Source inventory component (for inventory-to-inventory transfers). May be null for world item or equipment drags. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	TWeakObjectPtr<UMOInventoryComponent> SourceInventoryComponent;

	/** Source world item (for nearby items panel drags). May be null for inventory or equipment drags. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	TWeakObjectPtr<AMOWorldItem> SourceWorldItem;

	/** Source equipment component (for equipment slot drags). May be null for inventory or world item drags. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	TWeakObjectPtr<UMOEquipmentComponent> SourceEquipmentComponent;

	/** Source equipment slot (valid only if SourceEquipmentComponent is set). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	EMOEquipmentSlot SourceEquipmentSlot = EMOEquipmentSlot::MAX;

	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	int32 SourceSlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	FGuid ItemGuid;

	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	FName ItemDefinitionId;

	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	int32 Quantity = 0;

	/** Returns true if this drag originated from a world item (nearby items panel). */
	bool IsFromWorldItem() const { return SourceWorldItem.IsValid(); }

	/** Returns true if this drag originated from an inventory. */
	bool IsFromInventory() const { return SourceInventoryComponent.IsValid(); }

	/** Returns true if this drag originated from an equipment slot. */
	bool IsFromEquipment() const { return SourceEquipmentComponent.IsValid() && SourceEquipmentSlot != EMOEquipmentSlot::MAX; }
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

	/** Optional: Source world item for nearby items display. Used for drag operations. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	TWeakObjectPtr<AMOWorldItem> SourceWorldItem;

	/** Optional: Source equipment component for equipment slot display. Used for drag operations. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	TWeakObjectPtr<UMOEquipmentComponent> SourceEquipmentComponent;

	/** Optional: Source equipment slot type. Valid only if SourceEquipmentComponent is set. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	EMOEquipmentSlot SourceEquipmentSlot = EMOEquipmentSlot::MAX;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOInventorySlotClickedSignature, int32, SlotIndex, const FGuid&, ItemGuid);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOInventorySlotShiftClickedSignature, int32, SlotIndex, const FGuid&, ItemGuid);
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

	/**
	 * Set visual data directly without an inventory component.
	 * Used for displaying items that aren't in a standard inventory (e.g., nearby world items).
	 * @param InVisualData The visual data to display
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void SetVisualData(const FMOInventorySlotVisualData& InVisualData);

	/** Clear the slot's visual data, showing empty state. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void ClearVisualData();

	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|UI")
	FMOInventorySlotClickedSignature OnSlotClicked;

	/** Called when slot is Shift+Clicked for quick transfer. */
	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|UI")
	FMOInventorySlotShiftClickedSignature OnSlotShiftClicked;

	/** Called when slot is right-clicked (context menu). Provides screen position for menu placement. */
	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|UI")
	FMOInventorySlotRightClickedSignature OnSlotRightClicked;

	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|UI")
	FMOInventorySlotDroppedSignature OnSlotDropReceived;

	/** Enable/disable drag-drop for this slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory|UI")
	bool bEnableDragDrop = true;

	/**
	 * Set a custom empty slot icon for this specific slot.
	 * Overrides the default EmptySlotIcon when this slot is empty.
	 * @param InIcon The custom empty icon to use (nullptr to use default)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void SetCustomEmptyIcon(UTexture2D* InIcon);

	/** Get the current empty slot icon (custom if set, otherwise default). */
	UFUNCTION(BlueprintPure, Category="MO|Inventory|UI")
	UTexture2D* GetEffectiveEmptyIcon() const;

	/** If true, dropping outside inventory slots will drop the item into the world. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory|UI")
	bool bEnableWorldDrop = true;

	/**
	 * Set the source world item for this slot (used by nearby items panel).
	 * When set, drag operations will use this world item as the source instead of an inventory.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void SetSourceWorldItem(AMOWorldItem* InWorldItem);

	/** Get the source world item (if any). */
	UFUNCTION(BlueprintPure, Category="MO|Inventory|UI")
	AMOWorldItem* GetSourceWorldItem() const;

	/**
	 * Set the source equipment for this slot (used by equipment panel).
	 * When set, drag operations will use this equipment slot as the source.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void SetEquipmentSource(UMOEquipmentComponent* InEquipment, EMOEquipmentSlot InSlot);

	/** Get the source equipment component (if any). */
	UFUNCTION(BlueprintPure, Category="MO|Inventory|UI")
	UMOEquipmentComponent* GetSourceEquipmentComponent() const;

	/** Get the source equipment slot type. */
	UFUNCTION(BlueprintPure, Category="MO|Inventory|UI")
	EMOEquipmentSlot GetSourceEquipmentSlot() const;

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

	/** Source world item for nearby items panel (alternative to InventoryComponent). */
	UPROPERTY()
	TWeakObjectPtr<AMOWorldItem> SourceWorldItem;

	/** Source equipment component for equipment panel (alternative to InventoryComponent). */
	UPROPERTY()
	TWeakObjectPtr<UMOEquipmentComponent> SourceEquipmentComponent;

	/** Source equipment slot type. */
	UPROPERTY()
	EMOEquipmentSlot SourceEquipmentSlot = EMOEquipmentSlot::MAX;

	UPROPERTY(EditDefaultsOnly, Category="MO|Inventory|UI")
	TObjectPtr<UTexture2D> DefaultItemIcon;

	/** Default empty slot icon (used when no custom icon is set). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Inventory|UI")
	TObjectPtr<UTexture2D> EmptySlotIcon;

	/** Custom empty slot icon (overrides EmptySlotIcon when set). */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CustomEmptyIcon;

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

	/** Time of last click for double-click detection. */
	double LastClickTime = 0.0;

	/** Slot index of last click for double-click detection (must be same slot). */
	int32 LastClickSlotIndex = INDEX_NONE;

	/** Threshold in seconds for double-click detection (500ms is standard Windows default). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Inventory|UI")
	float DoubleClickThreshold = 0.5f;

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
