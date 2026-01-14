#include "MOInventoryComponent.h"

#include "Net/UnrealNetwork.h"

UMOInventoryComponent::UMOInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMOInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	Inventory.SetOwner(this);
}

void UMOInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMOInventoryComponent, Inventory);
}

int32 UMOInventoryComponent::FindEntryIndexByGuid(const FGuid& ItemGuid) const
{
	if (!ItemGuid.IsValid())
	{
		return INDEX_NONE;
	}

	for (int32 EntryIndex = 0; EntryIndex < Inventory.Entries.Num(); ++EntryIndex)
	{
		if (Inventory.Entries[EntryIndex].ItemGuid == ItemGuid)
		{
			return EntryIndex;
		}
	}

	return INDEX_NONE;
}

bool UMOInventoryComponent::AddItemByGuid(const FGuid& ItemGuid, const FName ItemDefinitionId, int32 QuantityToAdd)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInventory] AddItemByGuid requires authority"));
		return false;
	}

	if (!ItemGuid.IsValid() || ItemDefinitionId.IsNone() || QuantityToAdd <= 0)
	{
		return false;
	}

	const int32 ExistingIndex = FindEntryIndexByGuid(ItemGuid);
	if (ExistingIndex != INDEX_NONE)
	{
		FMOInventoryEntry& ExistingEntry = Inventory.Entries[ExistingIndex];
		ExistingEntry.Quantity += QuantityToAdd;

		Inventory.MarkItemDirty(ExistingEntry);
		BroadcastInventoryChanged();
		return true;
	}

	FMOInventoryEntry NewEntry;
	NewEntry.ItemGuid = ItemGuid;
	NewEntry.ItemDefinitionId = ItemDefinitionId;
	NewEntry.Quantity = QuantityToAdd;

	const int32 NewIndex = Inventory.Entries.Add(NewEntry);
	Inventory.MarkItemDirty(Inventory.Entries[NewIndex]);

	BroadcastInventoryChanged();
	return true;
}

bool UMOInventoryComponent::RemoveItemByGuid(const FGuid& ItemGuid, int32 QuantityToRemove)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInventory] RemoveItemByGuid requires authority"));
		return false;
	}

	if (!ItemGuid.IsValid() || QuantityToRemove <= 0)
	{
		return false;
	}

	const int32 ExistingIndex = FindEntryIndexByGuid(ItemGuid);
	if (ExistingIndex == INDEX_NONE)
	{
		return false;
	}

	FMOInventoryEntry& ExistingEntry = Inventory.Entries[ExistingIndex];
	if (ExistingEntry.Quantity <= QuantityToRemove)
	{
		Inventory.Entries.RemoveAt(ExistingIndex);
		Inventory.MarkArrayDirty();
		BroadcastInventoryChanged();
		return true;
	}

	ExistingEntry.Quantity -= QuantityToRemove;
	Inventory.MarkItemDirty(ExistingEntry);
	BroadcastInventoryChanged();
	return true;
}

bool UMOInventoryComponent::TryGetEntryByGuid(const FGuid& ItemGuid, FMOInventoryEntry& OutEntry) const
{
	OutEntry = FMOInventoryEntry();

	const int32 ExistingIndex = FindEntryIndexByGuid(ItemGuid);
	if (ExistingIndex == INDEX_NONE)
	{
		return false;
	}

	OutEntry = Inventory.Entries[ExistingIndex];
	return true;
}

int32 UMOInventoryComponent::GetEntryCount() const
{
	return Inventory.Entries.Num();
}

void UMOInventoryComponent::BroadcastInventoryChanged()
{
	OnInventoryChanged.Broadcast();
}

// Fast array replication notifications.
void FMOInventoryList::PostReplicatedAdd(const TArrayView<int32>& /*AddedIndices*/, int32 /*FinalSize*/)
{
	if (OwnerComponent)
	{
		OwnerComponent->OnInventoryChanged.Broadcast();
	}
}

void FMOInventoryList::PostReplicatedChange(const TArrayView<int32>& /*ChangedIndices*/, int32 /*FinalSize*/)
{
	if (OwnerComponent)
	{
		OwnerComponent->OnInventoryChanged.Broadcast();
	}
}

void FMOInventoryList::PostReplicatedRemove(const TArrayView<int32>& /*RemovedIndices*/, int32 /*FinalSize*/)
{
	if (OwnerComponent)
	{
		OwnerComponent->OnInventoryChanged.Broadcast();
	}
}
