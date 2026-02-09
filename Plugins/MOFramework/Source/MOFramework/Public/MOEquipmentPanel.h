#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOEquipmentComponent.h"
#include "MOEquipmentPanel.generated.h"

class UMOEquipmentComponent;
class UMOInventoryComponent;
class UImage;
class UTextBlock;
class UProgressBar;
class UBorder;

/**
 * Delegate for when an item is dropped onto an equipment slot.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOOnEquipmentSlotDropReceived, EMOEquipmentSlot, TargetSlot, const FGuid&, ItemGuid, UMOInventoryComponent*, SourceInventory);

/**
 * Delegate for equipment slot clicks.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnEquipmentSlotClicked, EMOEquipmentSlot, Slot, const FMOEquippedItem&, Item);

/**
 * Delegate for equipment slot right-clicks.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOOnEquipmentSlotRightClicked, EMOEquipmentSlot, Slot, const FMOEquippedItem&, Item, FVector2D, ScreenPosition);

/**
 * Panel widget displaying left/right hand equipment slots.
 *
 * Supports:
 * - Drag/drop from inventory to equip items
 * - Click to unequip back to inventory
 * - Visual feedback for swap progress
 * - Durability display
 */
UCLASS()
class MOFRAMEWORK_API UMOEquipmentPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOEquipmentPanel(const FObjectInitializer& ObjectInitializer);

	// ============================================================================
	// INITIALIZATION
	// ============================================================================

	/**
	 * Initialize the panel with equipment and inventory components.
	 * @param InEquipment The equipment component to display/control
	 * @param InInventory The inventory for drag-drop operations
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Equipment|UI")
	void InitializePanel(UMOEquipmentComponent* InEquipment, UMOInventoryComponent* InInventory);

	/** Refresh the display from current equipment state. */
	UFUNCTION(BlueprintCallable, Category="MO|Equipment|UI")
	void RefreshDisplay();

	/** Clear the panel and unbind from equipment. */
	UFUNCTION(BlueprintCallable, Category="MO|Equipment|UI")
	void ClearPanel();

	// ============================================================================
	// ACCESSORS
	// ============================================================================

	/** Get the equipment component. */
	UFUNCTION(BlueprintPure, Category="MO|Equipment|UI")
	UMOEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent.Get(); }

	/** Get the inventory component. */
	UFUNCTION(BlueprintPure, Category="MO|Equipment|UI")
	UMOInventoryComponent* GetInventoryComponent() const { return InventoryComponent.Get(); }

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Broadcast when an item is dropped onto a slot. */
	UPROPERTY(BlueprintAssignable, Category="MO|Equipment|UI")
	FMOOnEquipmentSlotDropReceived OnSlotDropReceived;

	/** Broadcast when a slot is clicked. */
	UPROPERTY(BlueprintAssignable, Category="MO|Equipment|UI")
	FMOOnEquipmentSlotClicked OnSlotClicked;

	/** Broadcast when a slot is right-clicked. */
	UPROPERTY(BlueprintAssignable, Category="MO|Equipment|UI")
	FMOOnEquipmentSlotRightClicked OnSlotRightClicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// ============================================================================
	// BIND WIDGETS
	// ============================================================================

	// --- Left Hand Slot ---

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UBorder> LeftHandSlotBorder;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UImage> LeftHandIcon;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LeftHandLabel;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> LeftHandSwapProgress;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> LeftHandDurability;

	// --- Right Hand Slot ---

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UBorder> RightHandSlotBorder;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UImage> RightHandIcon;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RightHandLabel;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> RightHandSwapProgress;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> RightHandDurability;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Default icon when slot is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment|UI")
	TSoftObjectPtr<UTexture2D> EmptySlotIcon;

	/** Tint for empty slots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment|UI")
	FLinearColor EmptySlotTint = FLinearColor(0.3f, 0.3f, 0.3f, 0.5f);

	/** Tint for occupied slots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment|UI")
	FLinearColor OccupiedSlotTint = FLinearColor::White;

	/** Tint for slots during swap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment|UI")
	FLinearColor SwappingSlotTint = FLinearColor(1.0f, 0.8f, 0.2f, 1.0f);

private:
	// ============================================================================
	// STATE
	// ============================================================================

	UPROPERTY()
	TWeakObjectPtr<UMOEquipmentComponent> EquipmentComponent;

	UPROPERTY()
	TWeakObjectPtr<UMOInventoryComponent> InventoryComponent;

	// ============================================================================
	// INTERNAL
	// ============================================================================

	void BindEquipmentEvents();
	void UnbindEquipmentEvents();

	UFUNCTION()
	void HandleEquipmentChanged(EMOEquipmentSlot EquipSlot, const FMOEquippedItem& Item);

	UFUNCTION()
	void HandleSwapStarted(EMOEquipmentSlot EquipSlot);

	UFUNCTION()
	void HandleSwapCompleted(EMOEquipmentSlot EquipSlot);

	void UpdateSlotDisplay(EMOEquipmentSlot EquipSlot);
	void UpdateSwapProgress();

	EMOEquipmentSlot GetSlotAtPosition(const FGeometry& Geometry, const FVector2D& LocalPosition) const;
};
