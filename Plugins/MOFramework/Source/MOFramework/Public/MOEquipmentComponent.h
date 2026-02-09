#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOEquipmentComponent.generated.h"

class UMOInventoryComponent;

/**
 * Equipment slot identifiers for body locations.
 */
UENUM(BlueprintType)
enum class EMOEquipmentSlot : uint8
{
	Head UMETA(DisplayName="Head"),
	Chest UMETA(DisplayName="Chest"),
	Hands UMETA(DisplayName="Hands"),
	Legs UMETA(DisplayName="Legs"),
	Feet UMETA(DisplayName="Feet"),
	Back UMETA(DisplayName="Back"),        // Backpack/sack - container slot
	LeftHand UMETA(DisplayName="Left Hand"),
	RightHand UMETA(DisplayName="Right Hand"),

	MAX UMETA(Hidden)
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

	bool operator==(const FMOEquippedItem& Other) const
	{
		return ItemGuid == Other.ItemGuid;
	}

	bool operator!=(const FMOEquippedItem& Other) const
	{
		return !(*this == Other);
	}
};

/**
 * Save data for a single equipment slot.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOEquipmentSlotSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
	EMOEquipmentSlot Slot = EMOEquipmentSlot::MAX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
	FMOEquippedItem EquippedItem;
};

/**
 * Save data for full equipment state.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOEquipmentSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
	TArray<FMOEquipmentSlotSaveData> Slots;
};

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnEquipmentChanged, EMOEquipmentSlot, EquipSlot, const FMOEquippedItem&, EquippedItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnSwapStarted, EMOEquipmentSlot, EquipSlot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnSwapCompleted, EMOEquipmentSlot, EquipSlot);

/**
 * Component that manages equipped items on body slots.
 *
 * Body Slots:
 * - Head, Chest, Hands, Legs, Feet: Armor/clothing slots
 * - Back: Container slot (backpack/sack) - contents only accessible when placed on ground
 * - LeftHand, RightHand: Held item slots with swap delays
 *
 * Items are equipped FROM inventory - equipping moves the item out of inventory
 * into the equipment slot. Unequipping returns it to inventory.
 *
 * Hand slot swap delays are based on item weight tier:
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
	 * @param EquipSlot Which slot to equip to
	 * @return True if equip started (may have swap delay for hand slots)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Equipment")
	bool EquipFromInventory(UMOInventoryComponent* Inventory, const FGuid& ItemGuid, EMOEquipmentSlot EquipSlot);

	/**
	 * Unequip an item back to inventory.
	 * @param EquipSlot The slot to unequip
	 * @param Inventory Target inventory to receive the item
	 * @return True if unequip succeeded
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Equipment")
	bool UnequipToInventory(EMOEquipmentSlot EquipSlot, UMOInventoryComponent* Inventory);

	/**
	 * Swap items between two slots.
	 * @param SlotA First slot
	 * @param SlotB Second slot
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Equipment")
	void SwapSlots(EMOEquipmentSlot SlotA, EMOEquipmentSlot SlotB);

	/**
	 * Check if a slot is currently swapping (in cooldown).
	 * Only applies to hand slots.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Equipment")
	bool IsSlotSwapping(EMOEquipmentSlot EquipSlot) const;

	/**
	 * Get the swap progress for a slot (0-1, 1 = ready).
	 * Only applies to hand slots.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Equipment")
	float GetSwapProgress(EMOEquipmentSlot EquipSlot) const;

	// ============================================================================
	// ACCESSORS
	// ============================================================================

	/** Get equipped item in a slot. */
	UFUNCTION(BlueprintPure, Category="MO|Equipment")
	FMOEquippedItem GetEquippedItem(EMOEquipmentSlot EquipSlot) const;

	/** Check if a slot has an item. */
	UFUNCTION(BlueprintPure, Category="MO|Equipment")
	bool HasItemInSlot(EMOEquipmentSlot EquipSlot) const;

	/** Get all equipped items. */
	UFUNCTION(BlueprintPure, Category="MO|Equipment")
	void GetAllEquippedItems(TArray<FMOEquippedItem>& OutItems) const;

	/** Check if an item type can be equipped to a specific slot. */
	UFUNCTION(BlueprintPure, Category="MO|Equipment")
	bool CanEquipToSlot(FName ItemDefinitionId, EMOEquipmentSlot EquipSlot) const;

	// ============================================================================
	// BACK SLOT (CONTAINER) SPECIFIC
	// ============================================================================

	/**
	 * Check if the back slot has a container equipped.
	 * Note: Container contents are only accessible when placed on ground.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Equipment")
	bool HasBackpackEquipped() const;

	/**
	 * Get the item definition ID of the equipped backpack.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Equipment")
	FName GetEquippedBackpackId() const;

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
	// SLOT DATA (indexed by EMOEquipmentSlot)
	// ============================================================================

	UPROPERTY(Replicated)
	TArray<FMOEquippedItem> EquippedSlots;

	// ============================================================================
	// SWAP TIMING STATE (for hand slots only)
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

	void EnsureSlotsInitialized();
	bool IsHandSlot(EMOEquipmentSlot EquipSlot) const;
	float GetSwapTimeForItem(FName ItemDefinitionId) const;
	void StartSwap(EMOEquipmentSlot EquipSlot, float Duration);
	void CompleteSwap(EMOEquipmentSlot EquipSlot);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
