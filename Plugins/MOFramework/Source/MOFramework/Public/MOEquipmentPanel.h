/**
 * =============================================================================
 * MOEquipmentPanel.h - Character Equipment Display
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Visual display of equipped items (weapons, armor, accessories). Shows slots
 * for each equipment position with drag-drop support for equipping items.
 *
 * EQUIPMENT SLOTS:
 * - Head, Chest, Legs, Feet (armor)
 * - MainHand, OffHand (weapons/tools)
 * - Back (backpack/quiver)
 * - Accessory slots
 *
 * INTERACTIONS:
 * - Left-click: Select slot, show item details
 * - Right-click: Context menu (unequip, inspect)
 * - Drag-drop: Equip item from inventory
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] SLOT MAPPING: EMOEquipmentSlot enum (in MOEquipmentComponent.h)
 *   values must match GetSlotWidget() switch cases. Enum: Head=0, Chest=1,
 *   Hands=2, Legs=3, Feet=4, Back=5, LeftHand=6, RightHand=7.
 *
 * [2024-02] EQUIP VALIDATION: OnSlotDropReceived broadcasts ItemGuid +
 *   SourceInventory. Caller (UIController) must call MOEquipmentComponent::
 *   TryEquipItem() which validates slot compatibility.
 *
 * [2024-02] WEAK POINTER SAFETY: EquipmentComponent and InventoryComponent
 *   are TWeakObjectPtr. All handlers check .IsValid() before access.
 *   InitializePanel() with null is safe (display remains empty).
 *
 * [2024-02] DELEGATE BINDING: BindSlotEvents() calls RemoveAll(this) before
 *   binding slot click/drop handlers. Safe to call InitializePanel() multiple
 *   times (e.g., on possession change).
 *
 * [2024-02] SWAP PROGRESS: UpdateSwapProgress() runs in NativeTick. Only
 *   active when LeftHandSwapProgress or RightHandSwapProgress widgets exist.
 *   Progress bar shows equipment component's swap delay countdown.
 *
 * [2024-02] EMPTY ICONS: GetEmptyIconForSlot() returns slot-specific empty
 *   icons (configurable in Blueprint). Falls back to nullptr if not set.
 *
 * [2024-02] SLOT HANDLERS: 8 separate handlers per slot (HandleHeadSlotClicked,
 *   etc.) because UMOInventorySlot delegate doesn't include slot type. Each
 *   handler calls HandleSlotClickedInternal(EMOEquipmentSlot) for unified logic.
 *
 * =============================================================================
 * RELATED FILES: MOEquipmentComponent.h, MOInventorySlot.h, MOInventoryMenu.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOEquipmentComponent.h"
#include "MOEquipmentPanel.generated.h"

class UMOEquipmentComponent;
class UMOInventoryComponent;
class UMOInventorySlot;
class UProgressBar;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOOnEquipmentSlotDropReceived, EMOEquipmentSlot, TargetSlot, const FGuid&, ItemGuid, UMOInventoryComponent*, SourceInventory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnEquipmentSlotClicked, EMOEquipmentSlot, EquipSlot, const FMOEquippedItem&, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOOnEquipmentSlotRightClicked, EMOEquipmentSlot, EquipSlot, const FMOEquippedItem&, Item, FVector2D, ScreenPosition);

/**
 * Panel widget displaying equipment slots.
 * See file header for slot layout and pitfalls.
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
	virtual void NativePreConstruct() override;
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

	// ============================================================================
	// EMPTY SLOT ICONS (Blueprint-configurable per slot)
	// ============================================================================

	/** Empty icon for head slot (helmet silhouette, etc). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment|UI|Icons")
	TObjectPtr<UTexture2D> HeadEmptyIcon;

	/** Empty icon for chest slot (armor silhouette, etc). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment|UI|Icons")
	TObjectPtr<UTexture2D> ChestEmptyIcon;

	/** Empty icon for hands slot (gloves silhouette, etc). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment|UI|Icons")
	TObjectPtr<UTexture2D> HandsEmptyIcon;

	/** Empty icon for legs slot (pants silhouette, etc). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment|UI|Icons")
	TObjectPtr<UTexture2D> LegsEmptyIcon;

	/** Empty icon for feet slot (boots silhouette, etc). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment|UI|Icons")
	TObjectPtr<UTexture2D> FeetEmptyIcon;

	/** Empty icon for back slot (backpack silhouette, etc). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment|UI|Icons")
	TObjectPtr<UTexture2D> BackEmptyIcon;

	/** Empty icon for left hand slot (hand/weapon silhouette, etc). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment|UI|Icons")
	TObjectPtr<UTexture2D> LeftHandEmptyIcon;

	/** Empty icon for right hand slot (hand/weapon silhouette, etc). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment|UI|Icons")
	TObjectPtr<UTexture2D> RightHandEmptyIcon;

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
	UTexture2D* GetEmptyIconForSlot(EMOEquipmentSlot EquipSlot) const;
	void ApplyEmptyIconsToSlots();
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
