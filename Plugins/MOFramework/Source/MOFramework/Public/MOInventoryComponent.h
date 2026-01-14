#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "MOInventoryComponent.generated.h"

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

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	int32 FindEntryIndexByGuid(const FGuid& ItemGuid) const;
	void BroadcastInventoryChanged();
};
