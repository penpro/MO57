#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOEquipmentComponent.h"
#include "MOEquipmentPanel.generated.h"

class UMOEquipmentComponent;
class UMOInventoryComponent;
class UMOInventorySlot;
class UProgressBar;

/**
 * Delegate for when an item is dropped onto an equipment slot.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOOnEquipmentSlotDropReceived, EMOEquipmentSlot, TargetSlot, const FGuid&, ItemGuid, UMOInventoryComponent*, SourceInventory);

/**
 * Delegate for equipment slot clicks.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnEquipmentSlotClicked, EMOEquipmentSlot, EquipSlot, const FMOEquippedItem&, Item);

/**
 * Delegate for equipment slot right-clicks.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOOnEquipmentSlotRightClicked, EMOEquipmentSlot, EquipSlot, const FMOEquippedItem&, Item, FVector2D, ScreenPosition);

/**
 * Panel widget displaying equipment slots using UMOInventorySlot widgets.
 *
 * Body Slots: Head, Chest, Hands, Legs, Feet, Back
 * Hand Slots: LeftHand, RightHand (with swap progress)
 *
 * Supports:
 * - Drag/drop from inventory to equip items
 * - Click to unequip back to inventory
 * - Visual feedback for swap progress (hand slots only)
 * - Reuses existing UMOInventorySlot widget for consistent look/feel
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
	 * @param InInventory The inventory for unequip operations
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

	// ============================================================================
	// BIND WIDGETS - Equipment Slots (use existing UMOInventorySlot widget)
	// ============================================================================

	/** Head slot (helmet, hat). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOInventorySlot> HeadSlot;

	/** Chest slot (armor, tunic). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOInventorySlot> ChestSlot;

	/** Hands slot (gloves). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOInventorySlot> HandsSlot;

	/** Legs slot (pants, greaves). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOInventorySlot> LegsSlot;

	/** Feet slot (boots, shoes). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOInventorySlot> FeetSlot;

	/** Back slot (backpack, sack - container). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOInventorySlot> BackSlot;

	/** Left hand slot (held item). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOInventorySlot> LeftHandSlot;

	/** Right hand slot (held item). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOInventorySlot> RightHandSlot;

	// ============================================================================
	// BIND WIDGETS - Swap Progress (optional, for hand slots)
	// ============================================================================

	/** Progress bar for left hand swap delay. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> LeftHandSwapProgress;

	/** Progress bar for right hand swap delay. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> RightHandSwapProgress;

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
	void BindSlotEvents();
	void UnbindSlotEvents();

	UMOInventorySlot* GetSlotWidget(EMOEquipmentSlot EquipSlot) const;
	void UpdateSlotDisplay(EMOEquipmentSlot EquipSlot);
	void UpdateSwapProgress();

	UFUNCTION()
	void HandleEquipmentChanged(EMOEquipmentSlot EquipSlot, const FMOEquippedItem& Item);

	UFUNCTION()
	void HandleSwapStarted(EMOEquipmentSlot EquipSlot);

	UFUNCTION()
	void HandleSwapCompleted(EMOEquipmentSlot EquipSlot);

	// Slot event handlers (one per slot to know which slot triggered)
	UFUNCTION()
	void HandleHeadSlotClicked(int32 SlotIndex, const FGuid& ItemGuid);
	UFUNCTION()
	void HandleChestSlotClicked(int32 SlotIndex, const FGuid& ItemGuid);
	UFUNCTION()
	void HandleHandsSlotClicked(int32 SlotIndex, const FGuid& ItemGuid);
	UFUNCTION()
	void HandleLegsSlotClicked(int32 SlotIndex, const FGuid& ItemGuid);
	UFUNCTION()
	void HandleFeetSlotClicked(int32 SlotIndex, const FGuid& ItemGuid);
	UFUNCTION()
	void HandleBackSlotClicked(int32 SlotIndex, const FGuid& ItemGuid);
	UFUNCTION()
	void HandleLeftHandSlotClicked(int32 SlotIndex, const FGuid& ItemGuid);
	UFUNCTION()
	void HandleRightHandSlotClicked(int32 SlotIndex, const FGuid& ItemGuid);

	UFUNCTION()
	void HandleHeadSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory);
	UFUNCTION()
	void HandleChestSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory);
	UFUNCTION()
	void HandleHandsSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory);
	UFUNCTION()
	void HandleLegsSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory);
	UFUNCTION()
	void HandleFeetSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory);
	UFUNCTION()
	void HandleBackSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory);
	UFUNCTION()
	void HandleLeftHandSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory);
	UFUNCTION()
	void HandleRightHandSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory);

	void HandleSlotClickedInternal(EMOEquipmentSlot EquipSlot);
	void HandleSlotDropReceivedInternal(EMOEquipmentSlot EquipSlot, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory);
};
