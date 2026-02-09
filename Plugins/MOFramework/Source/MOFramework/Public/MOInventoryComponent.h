#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "MOworldSaveGame.h"
#include "MOInventoryComponent.generated.h"

USTRUCT(BlueprintType)
struct FMOInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory")
	FGuid ItemGuid;

	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory")
	FName ItemDefinitionId;

	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory")
	int32 Quantity = 0;

	/** Current durability for tools (-1 = not applicable/infinite). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory")
	int32 CurrentDurability = -1;
};

USTRUCT(BlueprintType)
struct FMOInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FMOInventoryEntry> Entries;

	UPROPERTY(Transient)
	TObjectPtr<class UMOInventoryComponent> OwnerComponent;

	void SetOwner(UMOInventoryComponent* InOwner) { OwnerComponent = InOwner; }

	// Replication callbacks
	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	void PostReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FMOInventoryEntry, FMOInventoryList>(Entries, DeltaParams, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FMOInventoryList> : public TStructOpsTypeTraitsBase2<FMOInventoryList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOInventoryChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOInventorySlotsChangedSignature);

class UMOInventoryComponent;

/** Editor-friendly struct for pre-populating inventory slots. */
USTRUCT(BlueprintType)
struct FMOStartingInventoryItem
{
	GENERATED_BODY()

	/** Which item definition to add (dropdown from DataTable). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory", meta=(GetOptions="GetItemDefinitionOptionsStatic"))
	FName ItemDefinitionId = NAME_None;

	/** How many of this item to add. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory", meta=(ClampMin="1"))
	int32 Quantity = 1;

	/** Which slot to place this item in (-1 = auto-assign to first empty). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory", meta=(ClampMin="-1"))
	int32 SlotIndex = -1;
};

UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOInventoryComponent();

	UPROPERTY(BlueprintAssignable, Category="MO|Inventory")
	FMOInventoryChangedSignature OnInventoryChanged;

	UPROPERTY(BlueprintAssignable, Category="MO|Inventory")
	FMOInventorySlotsChangedSignature OnSlotsChanged;

	// Inventory entries (replicated via FastArray)
	UPROPERTY(Replicated)
	FMOInventoryList Inventory;

	// Desired number of slots (authority sizes SlotItemGuids to match this)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory|Slots")
	int32 SlotCount = 16;

	/** Items to add to inventory when the game starts (server only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory|Starting")
	TArray<FMOStartingInventoryItem> StartingItems;

	// If true, newly added items auto-assign into the first empty slot if they are not already in any slot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory|Slots")
	bool bAutoAssignNewItemsToSlots = true;

	// Actual slot contents (Guid per slot, invalid = empty)
	UPROPERTY(ReplicatedUsing=OnRep_SlotItemGuids)
	TArray<FGuid> SlotItemGuids;

	// Basic inventory operations
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	bool AddItemByGuid(const FGuid& ItemGuid, const FName ItemDefinitionId, int32 QuantityToAdd);

	/**
	 * Add an item with specific durability (for restoring equipped items).
	 * @param ItemGuid The unique GUID for this item
	 * @param ItemDefinitionId The item definition row name
	 * @param QuantityToAdd Number of items to add
	 * @param InitialDurability Initial durability (-1 for infinite/default)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	bool AddItemByGuidWithDurability(const FGuid& ItemGuid, const FName ItemDefinitionId, int32 QuantityToAdd, int32 InitialDurability);

	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	bool RemoveItemByGuid(const FGuid& ItemGuid, int32 QuantityToRemove);

	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	bool TryGetEntryByGuid(const FGuid& ItemGuid, FMOInventoryEntry& OutEntry) const;

	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	int32 GetEntryCount() const;

	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	void GetInventoryEntries(TArray<FMOInventoryEntry>& OutEntries) const;

	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	FString GetInventoryDebugString() const;

	/**
	 * Get total quantity of items matching a specific definition ID.
	 * Sums across all inventory entries with matching ItemDefinitionId.
	 * @param ItemDefinitionId The item definition to count
	 * @return Total quantity across all matching entries
	 */
	UFUNCTION(BlueprintPure, Category="MO|Inventory")
	int32 GetItemCountByDefinitionId(FName ItemDefinitionId) const;

	/**
	 * Check if inventory contains at least a certain quantity of an item.
	 * @param ItemDefinitionId The item definition to check
	 * @param RequiredQuantity Minimum quantity needed
	 * @return True if inventory has at least RequiredQuantity of the item
	 */
	UFUNCTION(BlueprintPure, Category="MO|Inventory")
	bool HasItem(FName ItemDefinitionId, int32 RequiredQuantity = 1) const;

	/**
	 * Remove items by definition ID. Removes from first matching entries found.
	 * @param ItemDefinitionId The item definition to remove
	 * @param QuantityToRemove How many to remove
	 * @return True if at least some items were removed
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	bool RemoveItemByDefinitionId(FName ItemDefinitionId, int32 QuantityToRemove);

	/**
	 * Reduce durability on an item. Returns true if successful.
	 * If durability reaches 0, the item is destroyed.
	 * @param ItemGuid The item to degrade
	 * @param Amount Amount of durability to remove
	 * @param bOutDestroyed Set to true if the item was destroyed
	 * @return True if durability was reduced, false if item not found or has infinite durability
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	bool ReduceDurability(const FGuid& ItemGuid, int32 Amount, bool& bOutDestroyed);

	/**
	 * Get the current durability of an item.
	 * @param ItemGuid The item to check
	 * @param OutDurability Current durability (-1 if infinite)
	 * @return True if item found
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	bool GetItemDurability(const FGuid& ItemGuid, int32& OutDurability) const;

	// Slots API
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Slots")
	int32 GetSlotCount() const;

	/** Check if there's at least one empty slot available. */
	UFUNCTION(BlueprintPure, Category="MO|Inventory|Slots")
	bool HasEmptySlot() const;

	/**
	 * Check if this item can be added to the inventory.
	 * Returns true if the GUID already exists (will stack) or if there's an empty slot.
	 * @param ItemGuid The item's GUID to check
	 * @return True if the item can be added
	 */
	UFUNCTION(BlueprintPure, Category="MO|Inventory")
	bool CanAddItem(const FGuid& ItemGuid) const;

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Slots")
	bool TryGetSlotGuid(int32 SlotIndex, FGuid& OutGuid) const;

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Slots")
	bool TryGetSlotEntry(int32 SlotIndex, FMOInventoryEntry& OutEntry) const;

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Slots")
	bool SetSlotGuid(int32 SlotIndex, const FGuid& ItemGuid);

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Slots")
	bool ClearSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Slots")
	bool SwapSlots(int32 SlotIndexA, int32 SlotIndexB);

	/*
	 * SAVE / RESTORE HELPERS (Authority-only)
	 */

	// Clears inventory entries and clears all slots.
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Save")
	void ClearInventoryAndSlots();

	// Ensures the authoritative slot array size matches NewSlotCount (min 1).
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Save")
	bool SetSlotCountAuthority(int32 NewSlotCount);

	// Adds an entry but guarantees no slot auto-assignment happens (useful during restore).
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Save")
	bool AddItemByGuidWithoutSlotAutoAssign(const FGuid& ItemGuid, const FName ItemDefinitionId, int32 QuantityToAdd);

	// Build a save snapshot (server or client can call, but it reflects local replicated state).
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Save")
	void BuildSaveData(FMOInventorySaveData& OutSaveData) const;

	// Apply a save snapshot (authority only).
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Save")
	bool ApplySaveDataAuthority(const FMOInventorySaveData& InSaveData);

	/** Drop item from a slot into the world. Spawns the world actor and removes from inventory.
	 *  @param SlotIndex - The slot to drop from
	 *  @param DropLocation - World location to spawn the item
	 *  @param DropRotation - World rotation for the spawned item
	 *  @return The spawned actor, or nullptr on failure */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Drop")
	AActor* DropItemFromSlot(int32 SlotIndex, const FVector& DropLocation, const FRotator& DropRotation);

	/** Drop item by GUID into the world. Spawns the world actor and removes from inventory.
	 *  @param ItemGuid - The item GUID to drop
	 *  @param DropLocation - World location to spawn the item
	 *  @param DropRotation - World rotation for the spawned item
	 *  @return The spawned actor, or nullptr on failure */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Drop")
	AActor* DropItemByGuid(const FGuid& ItemGuid, const FVector& DropLocation, const FRotator& DropRotation);

	/**
	 * Spawn a world item without removing from inventory. Used for split operations.
	 * @param ItemDefinitionId The item type to spawn
	 * @param Quantity Number of items in the stack
	 * @param ItemGuid GUID for the spawned item
	 * @param DropLocation World location for spawn
	 * @param DropRotation World rotation for spawn
	 * @return The spawned actor, or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Drop")
	AActor* SpawnWorldItem(FName ItemDefinitionId, int32 Quantity, const FGuid& ItemGuid, const FVector& DropLocation, const FRotator& DropRotation);

	// ============================================================================
	// TRANSFER OPERATIONS
	// ============================================================================

	/**
	 * Transfer a single item to another inventory.
	 * @param ItemGuid The item to transfer
	 * @param TargetInventory The destination inventory
	 * @return True if the transfer succeeded
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Transfer")
	bool TransferItem(const FGuid& ItemGuid, UMOInventoryComponent* TargetInventory);

	/**
	 * Transfer all items to another inventory.
	 * @param TargetInventory The destination inventory
	 * @return Number of items successfully transferred
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Transfer")
	int32 TransferAllTo(UMOInventoryComponent* TargetInventory);

	/**
	 * Transfer all items from another inventory to this one.
	 * @param SourceInventory The source inventory to transfer from
	 * @return Number of items successfully transferred
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Transfer")
	int32 TransferAllFrom(UMOInventoryComponent* SourceInventory);

	// ============================================================================
	// STACK MANAGEMENT
	// ============================================================================

	/**
	 * Consolidate stacks and defragment inventory slots.
	 * - Merges partial stacks of the same item
	 * - Moves all items to lowest index slots
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Stack")
	void StackAndOrganize();

	/**
	 * Fill partial stacks in this inventory from another inventory.
	 * Only completes existing stacks; doesn't transfer new item types.
	 * @param SourceInventory The inventory to take items from
	 * @return Number of items moved to complete stacks
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Stack")
	int32 FillStacksFrom(UMOInventoryComponent* SourceInventory);

	/**
	 * Get all item GUIDs in this inventory.
	 * @return Array of item GUIDs
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	TArray<FGuid> GetAllItemGuids() const;

	/**
	 * Get the entry at a specific slot index (same as TryGetSlotEntry but returns bool).
	 * @param SlotIndex The slot to query
	 * @param OutEntry The entry at that slot (if any)
	 * @return True if slot has an item
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Slots")
	bool GetEntry(int32 SlotIndex, FMOInventoryEntry& OutEntry) const;

	/** Returns available ItemDefinitionIds for dropdowns. */
	UFUNCTION()
	static TArray<FName> GetItemDefinitionOptionsStatic();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Called on server at BeginPlay to add StartingItems to inventory. */
	void ApplyStartingItems();

private:
	int32 FindEntryIndexByGuid(const FGuid& ItemGuid) const;

	void BroadcastInventoryChanged();

	void EnsureSlotsInitialized();
	bool IsSlotIndexValid(int32 SlotIndex) const;

	bool IsGuidInSlots(const FGuid& ItemGuid) const;
	bool FindFirstEmptySlot(int32& OutSlotIndex) const;

	bool TryAutoAssignGuidToEmptySlot(const FGuid& ItemGuid);
	void RemoveGuidFromSlotsInternal(const FGuid& ItemGuid);

	void MarkSlotItemGuidsDirty();

	UFUNCTION()
	void OnRep_SlotItemGuids();
};
