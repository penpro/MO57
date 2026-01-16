#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "MOInventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOInventorySlotsChangedSignature);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOInventoryChangedSignature);

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory")
	FGuid ItemGuid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory")
	FName ItemDefinitionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory", meta=(ClampMin="1"))
	int32 Quantity = 1;
};

USTRUCT()
struct MOFRAMEWORK_API FMOInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMOInventoryEntry> Entries;

	// Not replicated, runtime pointer.
	class UMOInventoryComponent* OwnerComponent = nullptr;

	void SetOwner(UMOInventoryComponent* InOwnerComponent)
	{
		OwnerComponent = InOwnerComponent;
	}

	// FFastArraySerializer interface
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FMOInventoryEntry, FMOInventoryList>(Entries, DeltaParams, *this);
	}

	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	void PostReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);
};

template<>
struct TStructOpsTypeTraits<FMOInventoryList> : public TStructOpsTypeTraitsBase2<FMOInventoryList>
{
	enum { WithNetDeltaSerializer = true };
};

UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOInventoryComponent();

	// Replicated inventory list.
	UPROPERTY(Replicated)
	FMOInventoryList Inventory;

	UPROPERTY(BlueprintAssignable, Category="MO|Inventory")
	FMOInventoryChangedSignature OnInventoryChanged;

	// Server authoritative add, keyed by stable GUID.
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory|Slots", meta=(ClampMin="1"))
	int32 SlotCount = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory|Slots")
	bool bAutoAssignNewItemsToSlots = true;

	UPROPERTY(ReplicatedUsing=OnRep_SlotItemGuids, VisibleAnywhere, BlueprintReadOnly, Category="MO|Inventory|Slots")
	TArray<FGuid> SlotItemGuids;

	UPROPERTY(BlueprintAssignable, Category="MO|Inventory|Slots")
	FMOInventorySlotsChangedSignature OnSlotsChanged;

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

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Slots")
	bool FindFirstEmptySlot(int32& OutSlotIndex) const;

	UFUNCTION(BlueprintCallable, Category="MO|Inventory|Slots")
	bool FindSlotForGuid(const FGuid& ItemGuid, int32& OutSlotIndex) const;


protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	int32 FindEntryIndexByGuid(const FGuid& ItemGuid) const;
	void BroadcastInventoryChanged();

	UFUNCTION()
	void OnRep_SlotItemGuids();

	void EnsureSlotsInitialized();
	bool IsSlotIndexValid(int32 SlotIndex) const;
	bool IsGuidInSlots(const FGuid& ItemGuid) const;
	void RemoveGuidFromSlotsInternal(const FGuid& ItemGuid);
	bool TryAutoAssignGuidToEmptySlot(const FGuid& ItemGuid);
};
