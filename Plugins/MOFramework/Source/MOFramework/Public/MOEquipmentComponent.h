/**
 * =============================================================================
 * MOEquipmentComponent.h - Body Slot Equipment System
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Pawn component managing equipped items in body slots. Items are equipped
 * FROM inventory (moved out, not copied). Hand slots have swap delays based
 * on item weight tier. Back slot is special container slot.
 *
 * KEY RESPONSIBILITIES:
 * 1. Manage 8 equipment slots (Head, Chest, Hands, Legs, Feet, Back, L/R Hand)
 * 2. Handle equip/unequip with inventory integration
 * 3. Implement hand slot swap delays by weight tier
 * 4. Support back slot as container (backpack)
 * 5. Broadcast equipment change events
 *
 * EQUIPMENT SLOTS:
 * - Head, Chest, Hands, Legs, Feet: Armor/clothing (instant equip)
 * - Back: Container slot - contents only accessible when placed on ground
 * - LeftHand, RightHand: Held items with swap delays
 *
 * SWAP TIMING (hand slots only):
 * - Light (knife, torch): 400ms
 * - Medium (hatchet, bow): 650ms
 * - Heavy (rifle, two-handed): 900ms
 *
 * ARCHITECTURE NOTES:
 * - EquippedSlots array indexed by EMOEquipmentSlot enum
 * - Items move out of inventory when equipped (not duplicated)
 * - Swap timing uses Tick with countdown timers
 * - Pending items have one equipment-owned escrow entry until the swap completes;
 *   the previous item is returned and its slot cleared before the delay starts.
 * - Escrow items and remaining swap time are replicated and saved.
 *
 * CRITICAL PATTERNS:
 * 1. Equip Flow:
 *    EquipFromInventory() -> Check CanEquipToSlot() -> Remove from inventory
 *    -> Escrow the incoming item and clear the returned item's slot
 *    -> For hand slots: tick the saved/replicated remaining delay
 *    -> On complete: Set slot data -> Broadcast OnEquipmentChanged
 *
 * 2. Unequip Flow:
 *    UnequipToInventory() -> Check inventory has space -> Add to inventory
 *    -> Clear slot -> Broadcast OnEquipmentChanged
 *
 * 3. Swap Timing:
 *    Escrow item + remaining time -> Tick decrements
 *    -> Timer hits 0 -> CompleteSwap() -> Apply pending item
 *
 * KNOWN PITFALLS:
 * 1. ITEM MOVE NOT COPY: Equipping removes item from inventory. If equip
 *    fails mid-way, item could be lost. Ensure atomic operations.
 *
 * 2. BACKPACK CONTENTS: Back slot container contents only accessible when
 *    placed on ground. Don't allow direct access to backpack inventory.
 *
 * 3. SWAP OWNERSHIP: Never leave a returned item in its old slot. Conflicting
 *    slot operations are refused while its replacement is in escrow.
 *
 * 4. SLOT VALIDATION: CanEquipToSlot() must check item's EquipSlot field.
 *    Don't allow equipping weapons to armor slots or vice versa.
 *
 * RELATED FILES:
 * - MOInventoryComponent.h - Source/target for equip/unequip
 * - MOItemDefinitionRow.h - Item's EquipSlot and WeightTier
 * - MOEquipmentPanel.h - UI for equipment display
 *
 * TESTING CHECKLIST:
 * [ ] Equip from inventory removes item from inventory
 * [ ] Unequip returns item to inventory
 * [ ] Hand slot swap has correct delay based on weight
 * [ ] IsSlotSwapping returns true during swap
 * [ ] OnEquipmentChanged fires correctly
 * [ ] Save/load preserves equipment state
 * [ ] Cannot equip incompatible items to slots
 *
 * LAST UPDATED: 2026-10-16 - Slot validation and persisted swap ownership
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOInsulationSourceInterface.h"
#include "MOItemInstanceState.h"
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

	/** Contents travel with an equipped container just as durability does. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment")
	FMOLiquidContents Liquid;

	/** Check if this slot has an item equipped. */
	bool IsValid() const { return ItemGuid.IsValid() && !ItemDefinitionId.IsNone(); }

	/** Clear this slot. */
	void Clear()
	{
		ItemGuid.Invalidate();
		ItemDefinitionId = NAME_None;
		CurrentDurability = -1;
		Liquid = FMOLiquidContents();
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

/** One item owned by equipment while it is being put into a slot. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOEquipmentPendingSwap
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment")
	EMOEquipmentSlot Slot = EMOEquipmentSlot::MAX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment")
	FMOEquippedItem Item;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment")
	float RemainingSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Equipment")
	float TotalSeconds = 0.0f;
};

/** Save data for full equipment state, including in-flight item ownership. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOEquipmentSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
	TArray<FMOEquipmentSlotSaveData> Slots;

	/** Empty in older saves. Restoring a timed swap does not skip its remaining work. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
	TArray<FMOEquipmentPendingSwap> PendingSwaps;
};

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnEquipmentChanged, EMOEquipmentSlot, EquipSlot, const FMOEquippedItem&, EquippedItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnEquipmentDurabilityChanged, EMOEquipmentSlot, EquipSlot, int32, NewDurability);
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
class MOFRAMEWORK_API UMOEquipmentComponent : public UActorComponent, public IMOInsulationSource
{
	GENERATED_BODY()

public:
	//~ Begin IMOInsulationSource (vitals reads clothing warmth through Core)
	virtual float GetClothingInsulationBonus01() const override;
	//~ End IMOInsulationSource

	/** THE clothing-warmth math — pure static for headless tests. Sum of
	 *  per-item Warmth scalars, clamped to the wearable ceiling. */
	static float ComputeInsulationFromWarmth(const TArray<float>& ItemWarmths, float MaxClothingInsulation = 0.4f);

	UMOEquipmentComponent();

	/** Shared range rule for native callers and untrusted request bytes; MAX is not a slot. */
	static bool IsValidEquipmentSlot(EMOEquipmentSlot Slot)
	{
		return static_cast<uint8>(Slot) < static_cast<uint8>(EMOEquipmentSlot::MAX);
	}

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
	 * Equip an item to the slot its own definition declares (EquipmentSlotType).
	 * Held items default to the right hand. Callers that don't care which slot
	 * an item belongs in (spawn loadouts, AI dressing) use this instead of
	 * duplicating the slot-type mapping.
	 * @return True if equip started; false if the item isn't equippable.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Equipment")
	bool EquipFromInventoryToNaturalSlot(UMOInventoryComponent* Inventory, const FGuid& ItemGuid);

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

	/** Server-only wear. Infinite durability and in-flight swaps refuse. Depletion clears ownership and emits OnEquipmentChanged;
	 * ordinary wear emits only OnEquipmentDurabilityChanged, including on replicated clients. */
	bool ReduceEquippedDurability(EMOEquipmentSlot Slot, int32 Amount, bool& bOutDestroyed);

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

	UPROPERTY(BlueprintAssignable, Category="MO|Equipment")
	FMOOnEquipmentDurabilityChanged OnEquipmentDurabilityChanged;

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

	UPROPERTY(ReplicatedUsing=OnRep_EquippedSlots)
	TArray<FMOEquippedItem> EquippedSlots;

	// ============================================================================
	// SWAP TIMING STATE (for hand slots only)
	// ============================================================================

	/** These items are not also in an inventory or EquippedSlots. */
	UPROPERTY(ReplicatedUsing=OnRep_PendingSwaps)
	TArray<FMOEquipmentPendingSwap> PendingSwaps;

	TSet<EMOEquipmentSlot> NotifiedPendingSlots;

	UFUNCTION()
	void OnRep_EquippedSlots(const TArray<FMOEquippedItem>& PreviousSlots);

	UFUNCTION()
	void OnRep_PendingSwaps();

	/** Inventory delegates are synchronous: reject re-entrant equipment operations. */
	bool bEquipmentMutationInProgress = false;

	// ============================================================================
	// INTERNAL
	// ============================================================================

	void EnsureSlotsInitialized();
	bool IsHandSlot(EMOEquipmentSlot EquipSlot) const;
	float GetSwapTimeForItem(FName ItemDefinitionId) const;
	void CompleteSwap(EMOEquipmentSlot EquipSlot);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
