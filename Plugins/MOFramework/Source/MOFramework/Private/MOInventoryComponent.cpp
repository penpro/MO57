#include "MOInventoryComponent.h"

#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

UMOInventoryComponent::UMOInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMOInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	Inventory.SetOwner(this);
	EnsureSlotsInitialized();
}

void UMOInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMOInventoryComponent, Inventory);
	DOREPLIFETIME(UMOInventoryComponent, SlotItemGuids);
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

	EnsureSlotsInitialized();

	const int32 ExistingIndex = FindEntryIndexByGuid(ItemGuid);
	if (ExistingIndex != INDEX_NONE)
	{
		FMOInventoryEntry& ExistingEntry = Inventory.Entries[ExistingIndex];
		ExistingEntry.Quantity += QuantityToAdd;

		Inventory.MarkItemDirty(ExistingEntry);
		BroadcastInventoryChanged();

		// Slot array unchanged.
		return true;
	}

	FMOInventoryEntry NewEntry;
	NewEntry.ItemGuid = ItemGuid;
	NewEntry.ItemDefinitionId = ItemDefinitionId;
	NewEntry.Quantity = QuantityToAdd;

	const int32 NewIndex = Inventory.Entries.Add(NewEntry);
	Inventory.MarkItemDirty(Inventory.Entries[NewIndex]);
	BroadcastInventoryChanged();

	// Optionally slot into first empty slot.
	if (TryAutoAssignGuidToEmptySlot(ItemGuid))
	{
		MarkSlotItemGuidsDirty();
		OnSlotsChanged.Broadcast();
	}

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

	// Remove entirely
	if (ExistingEntry.Quantity <= QuantityToRemove)
	{
		RemoveGuidFromSlotsInternal(ItemGuid);
		MarkSlotItemGuidsDirty();
		OnSlotsChanged.Broadcast();

		Inventory.Entries.RemoveAt(ExistingIndex);
		Inventory.MarkArrayDirty();
		BroadcastInventoryChanged();
		return true;
	}

	// Subtract quantity, keep entry
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

void UMOInventoryComponent::GetInventoryEntries(TArray<FMOInventoryEntry>& OutEntries) const
{
	OutEntries = Inventory.Entries;
}

FString UMOInventoryComponent::GetInventoryDebugString() const
{
	FString Result;

	for (const FMOInventoryEntry& Entry : Inventory.Entries)
	{
		Result += FString::Printf(
			TEXT("Guid=%s Def=%s Qty=%d\n"),
			*Entry.ItemGuid.ToString(EGuidFormats::Short),
			*Entry.ItemDefinitionId.ToString(),
			Entry.Quantity
		);
	}

	if (Result.IsEmpty())
	{
		Result = TEXT("(empty)");
	}

	return Result;
}

void UMOInventoryComponent::BroadcastInventoryChanged()
{
	OnInventoryChanged.Broadcast();
}

/*
 * Fast array replication notifications.
 */
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

/*
 * Slots
 */
void UMOInventoryComponent::EnsureSlotsInitialized()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	// Only the server should authoritatively size the array.
	if (OwnerActor->HasAuthority())
	{
		if (SlotCount < 1)
		{
			SlotCount = 1;
		}

		if (SlotItemGuids.Num() != SlotCount)
		{
			SlotItemGuids.SetNum(SlotCount);
			MarkSlotItemGuidsDirty();
		}
	}
}

bool UMOInventoryComponent::IsSlotIndexValid(int32 SlotIndex) const
{
	return SlotIndex >= 0 && SlotIndex < SlotItemGuids.Num();
}

int32 UMOInventoryComponent::GetSlotCount() const
{
	// Clients may not have SlotItemGuids yet, but do have the design-time SlotCount.
	return (SlotItemGuids.Num() > 0) ? SlotItemGuids.Num() : FMath::Max(1, SlotCount);
}

bool UMOInventoryComponent::TryGetSlotGuid(int32 SlotIndex, FGuid& OutGuid) const
{
	OutGuid.Invalidate();

	if (!IsSlotIndexValid(SlotIndex))
	{
		return false;
	}

	OutGuid = SlotItemGuids[SlotIndex];
	return OutGuid.IsValid();
}

bool UMOInventoryComponent::TryGetSlotEntry(int32 SlotIndex, FMOInventoryEntry& OutEntry) const
{
	OutEntry = FMOInventoryEntry();

	FGuid SlotGuid;
	if (!TryGetSlotGuid(SlotIndex, SlotGuid))
	{
		return false;
	}

	return TryGetEntryByGuid(SlotGuid, OutEntry);
}

bool UMOInventoryComponent::IsGuidInSlots(const FGuid& ItemGuid) const
{
	if (!ItemGuid.IsValid())
	{
		return false;
	}

	for (const FGuid& SlotGuid : SlotItemGuids)
	{
		if (SlotGuid == ItemGuid)
		{
			return true;
		}
	}

	return false;
}

bool UMOInventoryComponent::FindFirstEmptySlot(int32& OutSlotIndex) const
{
	OutSlotIndex = INDEX_NONE;

	for (int32 SlotIndex = 0; SlotIndex < SlotItemGuids.Num(); ++SlotIndex)
	{
		if (!SlotItemGuids[SlotIndex].IsValid())
		{
			OutSlotIndex = SlotIndex;
			return true;
		}
	}

	return false;
}

bool UMOInventoryComponent::TryAutoAssignGuidToEmptySlot(const FGuid& ItemGuid)
{
	if (!bAutoAssignNewItemsToSlots)
	{
		return false;
	}

	if (!ItemGuid.IsValid())
	{
		return false;
	}

	if (IsGuidInSlots(ItemGuid))
	{
		return false;
	}

	int32 EmptySlotIndex = INDEX_NONE;
	if (!FindFirstEmptySlot(EmptySlotIndex))
	{
		return false;
	}

	SlotItemGuids[EmptySlotIndex] = ItemGuid;
	return true;
}

void UMOInventoryComponent::RemoveGuidFromSlotsInternal(const FGuid& ItemGuid)
{
	if (!ItemGuid.IsValid())
	{
		return;
	}

	bool bChanged = false;
	for (FGuid& SlotGuid : SlotItemGuids)
	{
		if (SlotGuid == ItemGuid)
		{
			SlotGuid.Invalidate();
			bChanged = true;
		}
	}

	if (bChanged)
	{
		MarkSlotItemGuidsDirty();
	}
}

bool UMOInventoryComponent::SetSlotGuid(int32 SlotIndex, const FGuid& ItemGuid)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInventory] SetSlotGuid requires authority"));
		return false;
	}

	EnsureSlotsInitialized();

	if (!IsSlotIndexValid(SlotIndex))
	{
		return false;
	}

	// Clearing is allowed by passing invalid guid.
	if (!ItemGuid.IsValid())
	{
		SlotItemGuids[SlotIndex].Invalidate();
		MarkSlotItemGuidsDirty();
		OnSlotsChanged.Broadcast();
		return true;
	}

	// Must exist in inventory to be slotted.
	FMOInventoryEntry ExistingEntry;
	if (!TryGetEntryByGuid(ItemGuid, ExistingEntry))
	{
		return false;
	}

	// Enforce uniqueness: remove the guid from any other slot first.
	RemoveGuidFromSlotsInternal(ItemGuid);

	SlotItemGuids[SlotIndex] = ItemGuid;
	MarkSlotItemGuidsDirty();
	OnSlotsChanged.Broadcast();
	return true;
}

bool UMOInventoryComponent::ClearSlot(int32 SlotIndex)
{
	return SetSlotGuid(SlotIndex, FGuid());
}

bool UMOInventoryComponent::SwapSlots(int32 SlotIndexA, int32 SlotIndexB)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInventory] SwapSlots requires authority"));
		return false;
	}

	EnsureSlotsInitialized();

	if (!IsSlotIndexValid(SlotIndexA) || !IsSlotIndexValid(SlotIndexB))
	{
		return false;
	}

	if (SlotIndexA == SlotIndexB)
	{
		return true;
	}

	const FGuid TempGuid = SlotItemGuids[SlotIndexA];
	SlotItemGuids[SlotIndexA] = SlotItemGuids[SlotIndexB];
	SlotItemGuids[SlotIndexB] = TempGuid;

	MarkSlotItemGuidsDirty();
	OnSlotsChanged.Broadcast();
	return true;
}

void UMOInventoryComponent::MarkSlotItemGuidsDirty()
{
#if defined(MARK_PROPERTY_DIRTY_FROM_NAME)
	MARK_PROPERTY_DIRTY_FROM_NAME(UMOInventoryComponent, SlotItemGuids, this);
#endif
}

void UMOInventoryComponent::OnRep_SlotItemGuids()
{
	OnSlotsChanged.Broadcast();
}

/*
 * SAVE / RESTORE HELPERS
 */
void UMOInventoryComponent::ClearInventoryAndSlots()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInventory] ClearInventoryAndSlots requires authority"));
		return;
	}

	// Clear entries
	Inventory.Entries.Reset();
	Inventory.MarkArrayDirty();
	BroadcastInventoryChanged();

	// Clear slots
	EnsureSlotsInitialized();
	for (FGuid& SlotGuid : SlotItemGuids)
	{
		SlotGuid.Invalidate();
	}

	MarkSlotItemGuidsDirty();
	OnSlotsChanged.Broadcast();
}

bool UMOInventoryComponent::SetSlotCountAuthority(int32 NewSlotCount)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInventory] SetSlotCountAuthority requires authority"));
		return false;
	}

	SlotCount = FMath::Max(1, NewSlotCount);
	SlotItemGuids.SetNum(SlotCount);

	MarkSlotItemGuidsDirty();
	OnSlotsChanged.Broadcast();
	return true;
}

bool UMOInventoryComponent::AddItemByGuidWithoutSlotAutoAssign(const FGuid& ItemGuid, const FName ItemDefinitionId, int32 QuantityToAdd)
{
	const bool bPreviousAutoAssign = bAutoAssignNewItemsToSlots;
	bAutoAssignNewItemsToSlots = false;

	const bool bResult = AddItemByGuid(ItemGuid, ItemDefinitionId, QuantityToAdd);

	bAutoAssignNewItemsToSlots = bPreviousAutoAssign;
	return bResult;
}

void UMOInventoryComponent::BuildSaveData(FMOInventorySaveData& OutSaveData) const
{
	OutSaveData = FMOInventorySaveData();

	OutSaveData.SlotCount = GetSlotCount();
	OutSaveData.SlotItemGuids = SlotItemGuids;
	if (OutSaveData.SlotItemGuids.Num() != OutSaveData.SlotCount)
	{
		OutSaveData.SlotItemGuids.SetNum(OutSaveData.SlotCount);
	}

	OutSaveData.Items.Reserve(Inventory.Entries.Num());
	for (const FMOInventoryEntry& Entry : Inventory.Entries)
	{
		if (!Entry.ItemGuid.IsValid() || Entry.ItemDefinitionId.IsNone() || Entry.Quantity <= 0)
		{
			continue;
		}

		FMOInventoryItemSaveEntry SaveEntry;
		SaveEntry.ItemGuid = Entry.ItemGuid;
		SaveEntry.ItemDefinitionId = Entry.ItemDefinitionId;
		SaveEntry.Quantity = Entry.Quantity;
		OutSaveData.Items.Add(SaveEntry);
	}
}

bool UMOInventoryComponent::ApplySaveDataAuthority(const FMOInventorySaveData& InSaveData)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInventory] ApplySaveDataAuthority requires authority"));
		return false;
	}

	const int32 DesiredSlotCount = FMath::Max(1, InSaveData.SlotCount);

	const bool bPreviousAutoAssign = bAutoAssignNewItemsToSlots;
	bAutoAssignNewItemsToSlots = false;

	// Reset inventory entries
	Inventory.Entries.Reset();

	// Restore entries
	for (const FMOInventoryItemSaveEntry& ItemSaveEntry : InSaveData.Items)
	{
		if (!ItemSaveEntry.ItemGuid.IsValid() || ItemSaveEntry.ItemDefinitionId.IsNone() || ItemSaveEntry.Quantity <= 0)
		{
			continue;
		}

		FMOInventoryEntry NewEntry;
		NewEntry.ItemGuid = ItemSaveEntry.ItemGuid;
		NewEntry.ItemDefinitionId = ItemSaveEntry.ItemDefinitionId;
		NewEntry.Quantity = ItemSaveEntry.Quantity;
		Inventory.Entries.Add(NewEntry);
	}

	Inventory.MarkArrayDirty();
	BroadcastInventoryChanged();

	// Restore slots
	SlotCount = DesiredSlotCount;
	SlotItemGuids = InSaveData.SlotItemGuids;
	SlotItemGuids.SetNum(SlotCount);

	MarkSlotItemGuidsDirty();
	OnSlotsChanged.Broadcast();

	bAutoAssignNewItemsToSlots = bPreviousAutoAssign;
	return true;
}
