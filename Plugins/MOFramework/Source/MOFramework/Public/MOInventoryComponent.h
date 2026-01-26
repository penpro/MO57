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

	// If true, newly added items auto-assign into the first empty slot if they are not already in any slot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory|Slots")
	bool bAutoAssignNewItemsToSlots = true;

	// Actual slot contents (Guid per slot, invalid = empty)
	UPROPERTY(ReplicatedUsing=OnRep_SlotItemGuids)
	TArray<FGuid> SlotItemGuids;

	// Basic inventory operations
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	bool AddItemByGuid(const FGuid& ItemGuid, const FName ItemDefinitionId, int32 QuantityToAdd);

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

	// Slots API
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Slots")
	int32 GetSlotCount() const;

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

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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
