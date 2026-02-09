#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOEquipmentComponent.generated.h"

class UMOInventoryComponent;

/**
 * Equipment slot identifiers.
 */
UENUM(BlueprintType)
enum class EMOEquipmentSlot : uint8
{
	LeftHand UMETA(DisplayName="Left Hand"),
	RightHand UMETA(DisplayName="Right Hand"),
};

/**
 * Data for a single equipped item.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOEquippedItem
{
	GENERATED_BODY()

	/** Unique identifier for this equipped item instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment")
	FGuid ItemGuid;

	/** Item definition ID. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment")
	FName ItemDefinitionId = NAME_None;

	/** Current durability of the equipped item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment")
	int32 CurrentDurability = -1;

	/** Check if this slot has an item equipped. */
	bool IsValid() const { return ItemGuid.IsValid() && !ItemDefinitionId.IsNone(); }

	/** Clear this slot. */
	void Clear()
	{
		ItemGuid.Invalidate();
		ItemDefinitionId = NAME_None;
		CurrentDurability = -1;
	}
};

/**
 * Save data for equipment state.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOEquipmentSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
	FMOEquippedItem LeftHand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
	FMOEquippedItem RightHand;
};

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnEquipmentChanged, EMOEquipmentSlot, Slot, const FMOEquippedItem&, EquippedItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnSwapStarted, EMOEquipmentSlot, Slot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnSwapCompleted, EMOEquipmentSlot, Slot);

/**
 * Component that manages equipped items (left hand, right hand).
 *
 * Items are equipped FROM inventory - equipping moves the item out of inventory
 * into the equipment slot. Unequipping returns it to inventory.
 *
 * Swap delays are based on item weight tier:
 * - Light (knife, torch): 400ms
 * - Medium (hatchet, bow): 650ms
 * - Heavy (rifle, two-handed): 900ms
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOEquipmentComponent();

	// ============================================================================
	// EQUIPMENT OPERATIONS
	// ============================================================================

	/**
	 * Equip an item from inventory to a slot.
	 * @param Inventory Source inventory containing the item
	 * @param ItemGuid The item to equip
	 * @param Slot Which hand slot to equip to
	 * @return True if equip started (may have swap delay)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Equipment")
	bool EquipFromInventory(UMOInventoryComponent* Inventory, const FGuid& ItemGuid, EMOEquipmentSlot Slot);

	/**
	 * Unequip an item back to inventory.
	 * @param Slot The slot to unequip
	 * @param Inventory Target inventory to receive the item
	 * @return True if unequip succeeded
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Equipment")
	bool UnequipToInventory(EMOEquipmentSlot Slot, UMOInventoryComponent* Inventory);

	/**
	 * Swap items between two slots.
	 * @param SlotA First slot
	 * @param SlotB Second slot
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Equipment")
	void SwapSlots(EMOEquipmentSlot SlotA, EMOEquipmentSlot SlotB);

	/**
	 * Check if a slot is currently swapping (in cooldown).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Equipment")
	bool IsSlotSwapping(EMOEquipmentSlot Slot) const;

	/**
	 * Get the swap progress for a slot (0-1, 1 = ready).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Equipment")
	float GetSwapProgress(EMOEquipmentSlot Slot) const;

	// ============================================================================
	// ACCESSORS
	// ============================================================================

	/** Get equipped item in a slot. */
	UFUNCTION(BlueprintPure, Category="MO|Equipment")
	FMOEquippedItem GetEquippedItem(EMOEquipmentSlot Slot) const;

	/** Check if a slot has an item. */
	UFUNCTION(BlueprintPure, Category="MO|Equipment")
	bool HasItemInSlot(EMOEquipmentSlot Slot) const;

	/** Get the left hand item. */
	UFUNCTION(BlueprintPure, Category="MO|Equipment")
	FMOEquippedItem GetLeftHandItem() const { return LeftHandSlot; }

	/** Get the right hand item. */
	UFUNCTION(BlueprintPure, Category="MO|Equipment")
	FMOEquippedItem GetRightHandItem() const { return RightHandSlot; }

	// ============================================================================
	// SWAP TIMING CONFIGURATION
	// ============================================================================

	/** Swap time for light items (knife, torch) in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment|Timing")
	float LightSwapTime = 0.4f;

	/** Swap time for medium items (hatchet, bow) in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment|Timing")
	float MediumSwapTime = 0.65f;

	/** Swap time for heavy items (rifle, two-handed) in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment|Timing")
	float HeavySwapTime = 0.9f;

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Broadcast when equipment changes in any slot. */
	UPROPERTY(BlueprintAssignable, Category="MO|Equipment")
	FMOOnEquipmentChanged OnEquipmentChanged;

	/** Broadcast when a swap animation/delay starts. */
	UPROPERTY(BlueprintAssignable, Category="MO|Equipment")
	FMOOnSwapStarted OnSwapStarted;

	/** Broadcast when a swap animation/delay completes. */
	UPROPERTY(BlueprintAssignable, Category="MO|Equipment")
	FMOOnSwapCompleted OnSwapCompleted;

	// ============================================================================
	// SAVE/LOAD
	// ============================================================================

	/** Build save data for persistence. */
	UFUNCTION(BlueprintCallable, Category="MO|Equipment|Save")
	void BuildSaveData(FMOEquipmentSaveData& OutData) const;

	/** Apply save data to restore state. */
	UFUNCTION(BlueprintCallable, Category="MO|Equipment|Save")
	void ApplySaveData(const FMOEquipmentSaveData& InData);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// ============================================================================
	// SLOT DATA
	// ============================================================================

	UPROPERTY(Replicated)
	FMOEquippedItem LeftHandSlot;

	UPROPERTY(Replicated)
	FMOEquippedItem RightHandSlot;

	// ============================================================================
	// SWAP TIMING STATE
	// ============================================================================

	/** Time remaining on left hand swap. */
	float LeftHandSwapTimeRemaining = 0.0f;
	float LeftHandSwapTimeTotal = 0.0f;

	/** Time remaining on right hand swap. */
	float RightHandSwapTimeRemaining = 0.0f;
	float RightHandSwapTimeTotal = 0.0f;

	/** Pending equip data during swap. */
	FMOEquippedItem PendingLeftHandItem;
	FMOEquippedItem PendingRightHandItem;
	bool bHasPendingLeftHand = false;
	bool bHasPendingRightHand = false;

	// ============================================================================
	// INTERNAL
	// ============================================================================

	FMOEquippedItem& GetSlotRef(EMOEquipmentSlot Slot);
	const FMOEquippedItem& GetSlotRef(EMOEquipmentSlot Slot) const;

	float GetSwapTimeForItem(FName ItemDefinitionId) const;
	void StartSwap(EMOEquipmentSlot Slot, float Duration);
	void CompleteSwap(EMOEquipmentSlot Slot);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
