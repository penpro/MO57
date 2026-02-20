#include "MOInventoryComponent.h"
#include "MOFramework.h"

#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOWorldItem.h"
#include "MOItemComponent.h"
#include "MOIdentityComponent.h"
#include "MOPersistenceSubsystem.h"
#include "UObject/UnrealType.h"
#include "UObject/SoftObjectPtr.h"


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

	// Apply starting items on server
	AActor* OwnerActor = GetOwner();
	if (IsValid(OwnerActor) && OwnerActor->HasAuthority())
	{
		ApplyStartingItems();
	}
}

void UMOInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UMOInventoryComponent, Inventory, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMOInventoryComponent, SlotItemGuids, COND_OwnerOnly);
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
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] AddItemByGuid requires authority"));
		return false;
	}

	if (!ItemGuid.IsValid() || ItemDefinitionId.IsNone() || QuantityToAdd <= 0)
	{
		return false;
	}

	EnsureSlotsInitialized();

	// Check if this exact GUID already exists (stacking onto same item)
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

	// Get item definition for max stack size
	FMOItemDefinitionRow ItemDef;
	int32 MaxStackSize = 99; // Default fallback
	bool bIsTool = false;
	int32 MaxDurability = -1;

	if (UMOItemDatabaseSettings::GetItemDefinition(ItemDefinitionId, ItemDef))
	{
		MaxStackSize = ItemDef.MaxStackSize > 0 ? ItemDef.MaxStackSize : 1;
		bIsTool = ItemDef.bIsTool;
		MaxDurability = ItemDef.MaxDurability;
	}

	int32 RemainingToAdd = QuantityToAdd;
	bool bAnyChange = false;

	// First, try to fill existing stacks of the same item type (only for stackable non-tools)
	// Tools should not stack since they have individual durability
	if (!bIsTool && MaxStackSize > 1)
	{
		for (FMOInventoryEntry& Entry : Inventory.Entries)
		{
			if (Entry.ItemDefinitionId != ItemDefinitionId)
			{
				continue;
			}

			// Check if this stack has room
			int32 SpaceInStack = MaxStackSize - Entry.Quantity;
			if (SpaceInStack <= 0)
			{
				continue;
			}

			// Add as much as we can to this stack
			int32 ToAddToThisStack = FMath::Min(RemainingToAdd, SpaceInStack);
			Entry.Quantity += ToAddToThisStack;
			RemainingToAdd -= ToAddToThisStack;

			Inventory.MarkItemDirty(Entry);
			bAnyChange = true;

			UE_LOG(LogMOFramework, Log, TEXT("[MOInventory] Stacked %d of %s into existing stack (now %d/%d)"),
				ToAddToThisStack, *ItemDefinitionId.ToString(), Entry.Quantity, MaxStackSize);

			if (RemainingToAdd <= 0)
			{
				break;
			}
		}
	}

	// If there's remaining quantity, create new entry(ies)
	while (RemainingToAdd > 0)
	{
		// Check if we have an empty slot for new entries
		int32 EmptySlotIndex = INDEX_NONE;
		bool bHasEmptySlot = bAutoAssignNewItemsToSlots && FindFirstEmptySlot(EmptySlotIndex);

		// For the first new entry, use the provided GUID; subsequent entries get new GUIDs
		FGuid NewEntryGuid = bAnyChange ? FGuid::NewGuid() : ItemGuid;

		// Make sure this GUID isn't already used
		if (FindEntryIndexByGuid(NewEntryGuid) != INDEX_NONE)
		{
			NewEntryGuid = FGuid::NewGuid();
		}

		FMOInventoryEntry NewEntry;
		NewEntry.ItemGuid = NewEntryGuid;
		NewEntry.ItemDefinitionId = ItemDefinitionId;
		NewEntry.Quantity = FMath::Min(RemainingToAdd, MaxStackSize);
		RemainingToAdd -= NewEntry.Quantity;

		// Initialize durability for tools
		if (bIsTool && MaxDurability > 0)
		{
			NewEntry.CurrentDurability = MaxDurability;
		}
		else
		{
			NewEntry.CurrentDurability = -1; // Infinite/not applicable
		}

		const int32 NewIndex = Inventory.Entries.Add(NewEntry);
		Inventory.MarkItemDirty(Inventory.Entries[NewIndex]);
		bAnyChange = true;

		UE_LOG(LogMOFramework, Log, TEXT("[MOInventory] Created new stack of %s with %d items"),
			*ItemDefinitionId.ToString(), NewEntry.Quantity);

		// Optionally slot into first empty slot
		if (bAutoAssignNewItemsToSlots && TryAutoAssignGuidToEmptySlot(NewEntry.ItemGuid))
		{
			MarkSlotItemGuidsDirty();
			OnSlotsChanged.Broadcast();
		}
	}

	if (bAnyChange)
	{
		BroadcastInventoryChanged();
	}

	return true;
}

bool UMOInventoryComponent::AddItemByGuidWithDurability(const FGuid& ItemGuid, const FName ItemDefinitionId, int32 QuantityToAdd, int32 InitialDurability)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] AddItemByGuidWithDurability requires authority"));
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

		return true;
	}

	FMOInventoryEntry NewEntry;
	NewEntry.ItemGuid = ItemGuid;
	NewEntry.ItemDefinitionId = ItemDefinitionId;
	NewEntry.Quantity = QuantityToAdd;
	NewEntry.CurrentDurability = InitialDurability;

	const int32 NewIndex = Inventory.Entries.Add(NewEntry);
	Inventory.MarkItemDirty(Inventory.Entries[NewIndex]);
	BroadcastInventoryChanged();

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
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] RemoveItemByGuid requires authority"));
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

int32 UMOInventoryComponent::GetItemCountByDefinitionId(FName ItemDefinitionId) const
{
	if (ItemDefinitionId.IsNone())
	{
		return 0;
	}

	int32 TotalQuantity = 0;
	for (const FMOInventoryEntry& Entry : Inventory.Entries)
	{
		if (Entry.ItemDefinitionId == ItemDefinitionId)
		{
			TotalQuantity += Entry.Quantity;
		}
	}
	return TotalQuantity;
}

bool UMOInventoryComponent::HasItem(FName ItemDefinitionId, int32 RequiredQuantity) const
{
	return GetItemCountByDefinitionId(ItemDefinitionId) >= RequiredQuantity;
}

bool UMOInventoryComponent::RemoveItemByDefinitionId(FName ItemDefinitionId, int32 QuantityToRemove)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] RemoveItemByDefinitionId requires authority"));
		return false;
	}

	if (ItemDefinitionId.IsNone() || QuantityToRemove <= 0)
	{
		return false;
	}

	int32 RemainingToRemove = QuantityToRemove;

	// Iterate through entries and remove from matching ones
	for (int32 i = 0; i < Inventory.Entries.Num() && RemainingToRemove > 0; ++i)
	{
		FMOInventoryEntry& Entry = Inventory.Entries[i];
		if (Entry.ItemDefinitionId == ItemDefinitionId)
		{
			int32 ToRemoveFromThis = FMath::Min(Entry.Quantity, RemainingToRemove);
			if (ToRemoveFromThis > 0)
			{
				// Use RemoveItemByGuid for proper cleanup
				RemoveItemByGuid(Entry.ItemGuid, ToRemoveFromThis);
				RemainingToRemove -= ToRemoveFromThis;
				// Entry may have been removed, decrement i to check same index again
				--i;
			}
		}
	}

	return RemainingToRemove < QuantityToRemove;
}

bool UMOInventoryComponent::ReduceDurability(const FGuid& ItemGuid, int32 Amount, bool& bOutDestroyed)
{
	bOutDestroyed = false;

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] ReduceDurability requires authority"));
		return false;
	}

	if (!ItemGuid.IsValid() || Amount <= 0)
	{
		return false;
	}

	const int32 EntryIndex = FindEntryIndexByGuid(ItemGuid);
	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}

	FMOInventoryEntry& Entry = Inventory.Entries[EntryIndex];

	// Check if item has finite durability
	if (Entry.CurrentDurability < 0)
	{
		// Item has infinite durability
		return false;
	}

	// Reduce durability
	Entry.CurrentDurability = FMath::Max(0, Entry.CurrentDurability - Amount);

	if (Entry.CurrentDurability == 0)
	{
		// Item is broken - destroy it
		bOutDestroyed = true;
		UE_LOG(LogMOFramework, Log, TEXT("[MOInventory] Tool %s broke! Removing from inventory."), *ItemGuid.ToString(EGuidFormats::Short));
		RemoveItemByGuid(ItemGuid, Entry.Quantity);
	}
	else
	{
		Inventory.MarkItemDirty(Entry);
		BroadcastInventoryChanged();
	}

	return true;
}

bool UMOInventoryComponent::GetItemDurability(const FGuid& ItemGuid, int32& OutDurability) const
{
	OutDurability = -1;

	if (!ItemGuid.IsValid())
	{
		return false;
	}

	const int32 EntryIndex = FindEntryIndexByGuid(ItemGuid);
	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}

	OutDurability = Inventory.Entries[EntryIndex].CurrentDurability;
	return true;
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

bool UMOInventoryComponent::HasEmptySlot() const
{
	int32 EmptySlotIndex = INDEX_NONE;
	return FindFirstEmptySlot(EmptySlotIndex);
}

bool UMOInventoryComponent::CanAddItem(const FGuid& ItemGuid) const
{
	if (!ItemGuid.IsValid())
	{
		return false;
	}

	// If the GUID already exists in inventory, it will stack - no new slot needed
	if (FindEntryIndexByGuid(ItemGuid) != INDEX_NONE)
	{
		return true;
	}

	// If there's an empty slot, we can always add
	if (HasEmptySlot())
	{
		return true;
	}

	// No empty slots - this check is less useful without knowing the ItemDefinitionId
	// since we can't check for stackable items. For safety, return false.
	// The caller should use CanAddItemByDefinitionId for proper stack checking.
	return false;
}

bool UMOInventoryComponent::CanAddItemByDefinitionId(FName ItemDefinitionId, int32 Quantity) const
{
	if (ItemDefinitionId.IsNone() || Quantity <= 0)
	{
		return false;
	}

	// Get item definition for max stack size
	FMOItemDefinitionRow ItemDef;
	int32 MaxStackSize = 99;
	bool bIsTool = false;

	if (UMOItemDatabaseSettings::GetItemDefinition(ItemDefinitionId, ItemDef))
	{
		MaxStackSize = ItemDef.MaxStackSize > 0 ? ItemDef.MaxStackSize : 1;
		bIsTool = ItemDef.bIsTool;
	}

	int32 RemainingQuantity = Quantity;

	// Tools don't stack, so check if we have enough empty slots
	if (bIsTool || MaxStackSize <= 1)
	{
		int32 SlotsNeeded = Quantity; // Each tool needs its own slot
		int32 EmptySlots = 0;

		for (const FGuid& SlotGuid : SlotItemGuids)
		{
			if (!SlotGuid.IsValid())
			{
				EmptySlots++;
			}
		}

		return EmptySlots >= SlotsNeeded;
	}

	// For stackable items, first check how much room exists in current stacks
	for (const FMOInventoryEntry& Entry : Inventory.Entries)
	{
		if (Entry.ItemDefinitionId != ItemDefinitionId)
		{
			continue;
		}

		int32 SpaceInStack = MaxStackSize - Entry.Quantity;
		if (SpaceInStack > 0)
		{
			RemainingQuantity -= SpaceInStack;
			if (RemainingQuantity <= 0)
			{
				return true; // Fits entirely in existing stacks
			}
		}
	}

	// Calculate how many new slots we need for remaining quantity
	int32 NewSlotsNeeded = (RemainingQuantity + MaxStackSize - 1) / MaxStackSize; // Ceiling division

	// Count empty slots
	int32 EmptySlots = 0;
	for (const FGuid& SlotGuid : SlotItemGuids)
	{
		if (!SlotGuid.IsValid())
		{
			EmptySlots++;
		}
	}

	return EmptySlots >= NewSlotsNeeded;
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
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] SetSlotGuid requires authority"));
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
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] SwapSlots requires authority"));
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
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] ClearInventoryAndSlots requires authority"));
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
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] SetSlotCountAuthority requires authority"));
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
		SaveEntry.CurrentDurability = Entry.CurrentDurability;
		OutSaveData.Items.Add(SaveEntry);
	}
}

bool UMOInventoryComponent::ApplySaveDataAuthority(const FMOInventorySaveData& InSaveData)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] ApplySaveDataAuthority requires authority"));
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
		NewEntry.CurrentDurability = ItemSaveEntry.CurrentDurability;
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

namespace
{
	static TSubclassOf<AActor> ResolveDropActorClassFromDataTable(const FName& ItemDefinitionId)
	{
		// First try using the strongly-typed item definition lookup
		FMOItemDefinitionRow ItemDef;
		if (UMOItemDatabaseSettings::GetItemDefinition(ItemDefinitionId, ItemDef))
		{
			// Check WorldVisual.WorldActorClass
			if (!ItemDef.WorldVisual.WorldActorClass.IsNull())
			{
				UClass* LoadedClass = ItemDef.WorldVisual.WorldActorClass.LoadSynchronous();
				if (LoadedClass && LoadedClass->IsChildOf(AActor::StaticClass()))
				{
					return LoadedClass;
				}
			}

			// Fallback: use AMOWorldItem since we found the item definition but no custom class
			return AMOWorldItem::StaticClass();
		}

		// If no item definition found, try generic property search for custom DataTable structures
		const UMOItemDatabaseSettings* Settings = GetDefault<UMOItemDatabaseSettings>();
		if (!Settings)
		{
			return AMOWorldItem::StaticClass();
		}

		UDataTable* DataTable = Settings->GetItemDefinitionsDataTable();
		if (!IsValid(DataTable))
		{
			return AMOWorldItem::StaticClass();
		}

		const UScriptStruct* RowStruct = DataTable->GetRowStruct();
		if (!RowStruct)
		{
			return AMOWorldItem::StaticClass();
		}

		const uint8* RowData = DataTable->FindRowUnchecked(ItemDefinitionId);
		if (!RowData)
		{
			return AMOWorldItem::StaticClass();
		}

		// Fallback generic property search for custom row types
		static const FName CandidateNames[] =
		{
			TEXT("WorldActorClass"),
			TEXT("WorldItemActorClass"),
			TEXT("PickupActorClass"),
			TEXT("DropActorClass"),
			TEXT("PickupClass"),
			TEXT("ActorClass"),
		};

		for (const FName Candidate : CandidateNames)
		{
			const FProperty* Property = RowStruct->FindPropertyByName(Candidate);
			if (!Property)
			{
				continue;
			}

			if (const FClassProperty* ClassProperty = CastField<FClassProperty>(Property))
			{
				UObject* ObjectValue = ClassProperty->GetObjectPropertyValue_InContainer(RowData);
				UClass* ValueClass = Cast<UClass>(ObjectValue);

				if (ValueClass && ValueClass->IsChildOf(AActor::StaticClass()))
				{
					return ValueClass;
				}
			}

			if (const FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(Property))
			{
				const FSoftObjectPtr SoftPtr = SoftClassProperty->GetPropertyValue_InContainer(RowData);
				UClass* LoadedClass = Cast<UClass>(SoftPtr.LoadSynchronous());
				if (LoadedClass && LoadedClass->IsChildOf(AActor::StaticClass()))
				{
					return LoadedClass;
				}
			}
		}

		// Fallback: use AMOWorldItem if no specific class is configured
		return AMOWorldItem::StaticClass();
	}

	// Configure the spawned actor with the item data via components or direct properties.
	static void TryWriteDroppedItemPayload(AActor* SpawnedActor, const FGuid& ItemGuid, int32 Quantity, const FName& ItemDefinitionId)
	{
		if (!IsValid(SpawnedActor))
		{
			return;
		}

		// First, try to find and configure MOIdentityComponent
		UMOIdentityComponent* IdentityComp = SpawnedActor->FindComponentByClass<UMOIdentityComponent>();
		if (IsValid(IdentityComp))
		{
			IdentityComp->SetGuid(ItemGuid);
			UE_LOG(LogMOFramework, Log, TEXT("[MOInventory] Drop: Set IdentityComponent GUID to %s"), *ItemGuid.ToString(EGuidFormats::Short));
		}

		// Then, try to find and configure MOItemComponent
		UMOItemComponent* ItemComp = SpawnedActor->FindComponentByClass<UMOItemComponent>();
		if (IsValid(ItemComp))
		{
			ItemComp->ItemDefinitionId = ItemDefinitionId;
			ItemComp->Quantity = Quantity;
			UE_LOG(LogMOFramework, Log, TEXT("[MOInventory] Drop: Set ItemComponent ItemDefinitionId=%s, Quantity=%d"), *ItemDefinitionId.ToString(), Quantity);
		}

		// Also try direct property access for custom actors without MO components
		UClass* ActorClass = SpawnedActor->GetClass();
		if (!ActorClass)
		{
			return;
		}

		// Only write direct properties if we didn't have the corresponding component
		if (!IsValid(IdentityComp))
		{
			if (FProperty* GuidProperty = ActorClass->FindPropertyByName(TEXT("ItemGuid")))
			{
				if (FStructProperty* StructProperty = CastField<FStructProperty>(GuidProperty))
				{
					if (StructProperty->Struct == TBaseStructure<FGuid>::Get())
					{
						void* Dest = StructProperty->ContainerPtrToValuePtr<void>(SpawnedActor);
						if (Dest)
						{
							*static_cast<FGuid*>(Dest) = ItemGuid;
						}
					}
				}
			}
		}

		if (!IsValid(ItemComp))
		{
			if (FProperty* QuantityProperty = ActorClass->FindPropertyByName(TEXT("Quantity")))
			{
				if (FIntProperty* IntProperty = CastField<FIntProperty>(QuantityProperty))
				{
					IntProperty->SetPropertyValue_InContainer(SpawnedActor, Quantity);
				}
			}

			if (FProperty* DefIdProperty = ActorClass->FindPropertyByName(TEXT("ItemDefinitionId")))
			{
				if (FNameProperty* NameProperty = CastField<FNameProperty>(DefIdProperty))
				{
					NameProperty->SetPropertyValue_InContainer(SpawnedActor, ItemDefinitionId);
				}
			}
		}
	}
}

AActor* UMOInventoryComponent::DropItemFromSlot(int32 SlotIndex, const FVector& DropLocation, const FRotator& DropRotation)
{
	UE_LOG(LogMOFramework, Verbose, TEXT("[MOInventory] DropItemFromSlot: Slot=%d, Location=%s"),
		SlotIndex, *DropLocation.ToString());

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] DropItemFromSlot requires authority"));
		return nullptr;
	}

	EnsureSlotsInitialized();

	if (!IsSlotIndexValid(SlotIndex))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] DropItemFromSlot: Invalid slot index %d"), SlotIndex);
		return nullptr;
	}

	const FGuid ItemGuid = SlotItemGuids[SlotIndex];
	if (!ItemGuid.IsValid())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] DropItemFromSlot: No valid GUID at slot %d"), SlotIndex);
		return nullptr;
	}

	return DropItemByGuid(ItemGuid, DropLocation, DropRotation);
}

AActor* UMOInventoryComponent::DropItemByGuid(const FGuid& ItemGuid, const FVector& DropLocation, const FRotator& DropRotation)
{
	UE_LOG(LogMOFramework, Verbose, TEXT("[MOInventory] DropItemByGuid: Location=%s"), *DropLocation.ToString());

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] DropItemByGuid requires authority"));
		return nullptr;
	}

	if (!ItemGuid.IsValid())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] DropItemByGuid: Invalid ItemGuid"));
		return nullptr;
	}

	FMOInventoryEntry Entry;
	if (!TryGetEntryByGuid(ItemGuid, Entry))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] DropItemByGuid: entry not found %s"), *ItemGuid.ToString(EGuidFormats::Short));
		return nullptr;
	}

	const TSubclassOf<AActor> DropActorClass = ResolveDropActorClassFromDataTable(Entry.ItemDefinitionId);
	if (!DropActorClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] DropItemByGuid: no drop actor class for %s"), *Entry.ItemDefinitionId.ToString());
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// CRITICAL: Clear this GUID from the destroyed list before spawning.
	// When items are picked up, their world actor is destroyed and the GUID is added to SessionDestroyedGuids.
	// If we don't clear it here, the newly spawned actor will be immediately destroyed by the persistence system.
	if (UGameInstance* GameInstance = World->GetGameInstance())
	{
		if (UMOPersistenceSubsystem* PersistenceSubsystem = GameInstance->GetSubsystem<UMOPersistenceSubsystem>())
		{
			if (PersistenceSubsystem->IsGuidDestroyed(Entry.ItemGuid))
			{
				PersistenceSubsystem->ClearDestroyedGuid(Entry.ItemGuid);
			}
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerActor;
	SpawnParams.Instigator = OwnerActor->GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	const FTransform SpawnTransform(DropRotation, DropLocation);

	AActor* SpawnedActor = World->SpawnActor<AActor>(DropActorClass, SpawnTransform, SpawnParams);
	if (!IsValid(SpawnedActor))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] DropItemByGuid: spawn failed for %s"), *Entry.ItemDefinitionId.ToString());
		return nullptr;
	}

	TryWriteDroppedItemPayload(SpawnedActor, Entry.ItemGuid, Entry.Quantity, Entry.ItemDefinitionId);

	// If this is an AMOWorldItem, trigger visual update from the item definition
	// NOTE: ApplyItemDefinitionToWorldMesh sets RelativeTransform which can move the actor when ItemMesh is root
	// So we preserve and restore the spawn location after applying visuals
	if (AMOWorldItem* WorldItem = Cast<AMOWorldItem>(SpawnedActor))
	{
		const FVector PreservedLocation = SpawnedActor->GetActorLocation();
		const FRotator PreservedRotation = SpawnedActor->GetActorRotation();

		WorldItem->ApplyItemDefinitionToWorldMesh();

		// Restore the spawn location (ApplyItemDefinitionToWorldMesh may have moved it via SetRelativeTransform)
		SpawnedActor->SetActorLocationAndRotation(PreservedLocation, PreservedRotation);

		// Enable drop physics so the item settles naturally on the ground
		WorldItem->EnableDropPhysics();
	}

	// Remove the entire stack so the same GUID does not exist in two places.
	RemoveItemByGuid(ItemGuid, Entry.Quantity);

	return SpawnedActor;
}

AActor* UMOInventoryComponent::SpawnWorldItem(FName ItemDefinitionId, int32 Quantity, const FGuid& ItemGuid, const FVector& DropLocation, const FRotator& DropRotation)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] SpawnWorldItem requires authority"));
		return nullptr;
	}

	if (ItemDefinitionId.IsNone() || Quantity <= 0 || !ItemGuid.IsValid())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] SpawnWorldItem: Invalid parameters"));
		return nullptr;
	}

	const TSubclassOf<AActor> DropActorClass = ResolveDropActorClassFromDataTable(ItemDefinitionId);
	if (!DropActorClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] SpawnWorldItem: no drop actor class for %s"), *ItemDefinitionId.ToString());
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Clear GUID from destroyed list if present
	if (UGameInstance* GameInstance = World->GetGameInstance())
	{
		if (UMOPersistenceSubsystem* PersistenceSubsystem = GameInstance->GetSubsystem<UMOPersistenceSubsystem>())
		{
			if (PersistenceSubsystem->IsGuidDestroyed(ItemGuid))
			{
				PersistenceSubsystem->ClearDestroyedGuid(ItemGuid);
			}
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerActor;
	SpawnParams.Instigator = OwnerActor->GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	const FTransform SpawnTransform(DropRotation, DropLocation);
	AActor* SpawnedActor = World->SpawnActor<AActor>(DropActorClass, SpawnTransform, SpawnParams);
	if (!IsValid(SpawnedActor))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] SpawnWorldItem: spawn failed for %s"), *ItemDefinitionId.ToString());
		return nullptr;
	}

	TryWriteDroppedItemPayload(SpawnedActor, ItemGuid, Quantity, ItemDefinitionId);

	if (AMOWorldItem* WorldItem = Cast<AMOWorldItem>(SpawnedActor))
	{
		const FVector PreservedLocation = SpawnedActor->GetActorLocation();
		const FRotator PreservedRotation = SpawnedActor->GetActorRotation();

		WorldItem->ApplyItemDefinitionToWorldMesh();
		SpawnedActor->SetActorLocationAndRotation(PreservedLocation, PreservedRotation);
		WorldItem->EnableDropPhysics();
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventory] SpawnWorldItem: Spawned %s x%d with GUID %s"),
		*ItemDefinitionId.ToString(), Quantity, *ItemGuid.ToString(EGuidFormats::Short));

	return SpawnedActor;
}

// ============================================================================
// TRANSFER OPERATIONS
// ============================================================================

bool UMOInventoryComponent::TransferItem(const FGuid& ItemGuid, UMOInventoryComponent* TargetInventory)
{
	if (!IsValid(TargetInventory) || !ItemGuid.IsValid())
	{
		return false;
	}

	FMOInventoryEntry Entry;
	if (!TryGetEntryByGuid(ItemGuid, Entry))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] TransferItem: Item GUID %s not found"), *ItemGuid.ToString(EGuidFormats::Short));
		return false;
	}

	// Try to add to target
	FGuid NewGuid = FGuid::NewGuid();
	if (!TargetInventory->AddItemByGuid(NewGuid, Entry.ItemDefinitionId, Entry.Quantity))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] TransferItem: Failed to add to target inventory (full?)"));
		return false;
	}

	// Remove from source
	RemoveItemByGuid(ItemGuid, Entry.Quantity);

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventory] TransferItem: Transferred %s x%d"),
		*Entry.ItemDefinitionId.ToString(), Entry.Quantity);

	return true;
}

int32 UMOInventoryComponent::TransferAllTo(UMOInventoryComponent* TargetInventory)
{
	if (!IsValid(TargetInventory))
	{
		return 0;
	}

	int32 TransferredCount = 0;

	// Get all GUIDs first (avoid modifying while iterating)
	TArray<FGuid> ItemGuids = GetAllItemGuids();

	for (const FGuid& ItemGuid : ItemGuids)
	{
		if (TransferItem(ItemGuid, TargetInventory))
		{
			++TransferredCount;
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventory] TransferAllTo: Transferred %d items"), TransferredCount);

	return TransferredCount;
}

int32 UMOInventoryComponent::TransferAllFrom(UMOInventoryComponent* SourceInventory)
{
	if (!IsValid(SourceInventory))
	{
		return 0;
	}

	return SourceInventory->TransferAllTo(this);
}

// ============================================================================
// STACK MANAGEMENT
// ============================================================================

void UMOInventoryComponent::StackAndOrganize()
{
	// Step 1: Consolidate stacks of the same item type, respecting max stack size
	TMap<FName, TArray<int32>> ItemTypeToEntryIndices;

	for (int32 i = 0; i < Inventory.Entries.Num(); ++i)
	{
		const FMOInventoryEntry& Entry = Inventory.Entries[i];
		ItemTypeToEntryIndices.FindOrAdd(Entry.ItemDefinitionId).Add(i);
	}

	// For each item type with multiple entries, consolidate respecting stack limits
	TArray<int32> IndicesToRemove;

	for (auto& Pair : ItemTypeToEntryIndices)
	{
		TArray<int32>& Indices = Pair.Value;
		if (Indices.Num() <= 1)
		{
			continue;
		}

		// Get max stack size for this item type
		FMOItemDefinitionRow ItemDef;
		int32 MaxStack = 99;  // Default fallback
		if (UMOItemDatabaseSettings::GetItemDefinition(Pair.Key, ItemDef))
		{
			MaxStack = ItemDef.MaxStackSize > 0 ? ItemDef.MaxStackSize : 1;
		}

		// Sort so we process in order
		Indices.Sort();

		// Calculate total quantity across all stacks
		int32 TotalQuantity = 0;
		for (int32 Idx : Indices)
		{
			TotalQuantity += Inventory.Entries[Idx].Quantity;
		}

		// Distribute quantity across entries, respecting max stack size
		int32 RemainingQuantity = TotalQuantity;
		int32 EntriesNeeded = (TotalQuantity + MaxStack - 1) / MaxStack;  // Ceiling division
		int32 EntriesUsed = 0;

		for (int32 j = 0; j < Indices.Num(); ++j)
		{
			if (EntriesUsed < EntriesNeeded && RemainingQuantity > 0)
			{
				// Keep this entry with proper stack amount
				int32 StackAmount = FMath::Min(RemainingQuantity, MaxStack);
				Inventory.Entries[Indices[j]].Quantity = StackAmount;
				RemainingQuantity -= StackAmount;
				EntriesUsed++;
			}
			else
			{
				// Remove excess entries
				IndicesToRemove.Add(Indices[j]);
			}
		}
	}

	// Remove merged entries (in reverse order to preserve indices)
	IndicesToRemove.Sort();
	for (int32 i = IndicesToRemove.Num() - 1; i >= 0; --i)
	{
		// Clear slot if assigned
		FGuid GuidToRemove = Inventory.Entries[IndicesToRemove[i]].ItemGuid;
		RemoveGuidFromSlotsInternal(GuidToRemove);

		Inventory.Entries.RemoveAt(IndicesToRemove[i]);
	}

	// Step 2: Reassign all items to lowest index slots
	SlotItemGuids.Init(FGuid(), SlotCount);

	for (int32 i = 0; i < Inventory.Entries.Num() && i < SlotCount; ++i)
	{
		SlotItemGuids[i] = Inventory.Entries[i].ItemGuid;
	}

	MarkSlotItemGuidsDirty();
	Inventory.MarkArrayDirty();
	BroadcastInventoryChanged();

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventory] StackAndOrganize: Complete. %d entries remaining"),
		Inventory.Entries.Num());
}

int32 UMOInventoryComponent::FillStacksFrom(UMOInventoryComponent* SourceInventory)
{
	if (!IsValid(SourceInventory))
	{
		return 0;
	}

	int32 TotalFilled = 0;

	// For each item type in this inventory, check if we have partial stacks
	for (FMOInventoryEntry& TargetEntry : Inventory.Entries)
	{
		// Get max stack size for this item
		FMOItemDefinitionRow ItemDef;
		if (!UMOItemDatabaseSettings::GetItemDefinition(TargetEntry.ItemDefinitionId, ItemDef))
		{
			continue;
		}

		int32 MaxStack = ItemDef.MaxStackSize;
		if (MaxStack <= 0)
		{
			MaxStack = 1;  // Non-stackable
		}

		if (TargetEntry.Quantity >= MaxStack)
		{
			continue;  // Already full
		}

		int32 SpaceInStack = MaxStack - TargetEntry.Quantity;

		// Find matching items in source inventory
		for (FMOInventoryEntry& SourceEntry : SourceInventory->Inventory.Entries)
		{
			if (SourceEntry.ItemDefinitionId != TargetEntry.ItemDefinitionId)
			{
				continue;
			}

			if (SourceEntry.Quantity <= 0)
			{
				continue;
			}

			int32 ToTransfer = FMath::Min(SpaceInStack, SourceEntry.Quantity);
			if (ToTransfer <= 0)
			{
				break;  // Target stack is full
			}

			TargetEntry.Quantity += ToTransfer;
			SourceEntry.Quantity -= ToTransfer;
			TotalFilled += ToTransfer;
			SpaceInStack -= ToTransfer;

			// Remove source entry if empty
			if (SourceEntry.Quantity <= 0)
			{
				SourceInventory->RemoveGuidFromSlotsInternal(SourceEntry.ItemGuid);
			}

			if (SpaceInStack <= 0)
			{
				break;  // Target stack is full
			}
		}
	}

	// Clean up empty entries in source
	for (int32 i = SourceInventory->Inventory.Entries.Num() - 1; i >= 0; --i)
	{
		if (SourceInventory->Inventory.Entries[i].Quantity <= 0)
		{
			SourceInventory->Inventory.Entries.RemoveAt(i);
		}
	}

	if (TotalFilled > 0)
	{
		Inventory.MarkArrayDirty();
		SourceInventory->Inventory.MarkArrayDirty();
		BroadcastInventoryChanged();
		SourceInventory->BroadcastInventoryChanged();
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventory] FillStacksFrom: Filled %d items into existing stacks"), TotalFilled);

	return TotalFilled;
}

TArray<FGuid> UMOInventoryComponent::GetAllItemGuids() const
{
	TArray<FGuid> Result;
	Result.Reserve(Inventory.Entries.Num());

	for (const FMOInventoryEntry& Entry : Inventory.Entries)
	{
		Result.Add(Entry.ItemGuid);
	}

	return Result;
}

bool UMOInventoryComponent::GetEntry(int32 SlotIndex, FMOInventoryEntry& OutEntry) const
{
	return TryGetSlotEntry(SlotIndex, OutEntry);
}

TArray<FName> UMOInventoryComponent::GetItemDefinitionOptionsStatic()
{
	TArray<FName> Options;
	Options.Add(NAME_None);

	const UMOItemDatabaseSettings* Settings = GetDefault<UMOItemDatabaseSettings>();
	if (!Settings)
	{
		return Options;
	}

	UDataTable* DataTable = Settings->GetItemDefinitionsDataTable();
	if (!IsValid(DataTable))
	{
		return Options;
	}

	Options.Append(DataTable->GetRowNames());
	return Options;
}

void UMOInventoryComponent::ApplyStartingItems()
{
	if (StartingItems.Num() == 0)
	{
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventory] Applying %d starting items"), StartingItems.Num());

	for (const FMOStartingInventoryItem& StartingItem : StartingItems)
	{
		if (StartingItem.ItemDefinitionId.IsNone() || StartingItem.Quantity <= 0)
		{
			continue;
		}

		// Generate a unique GUID for this starting item
		const FGuid NewItemGuid = FGuid::NewGuid();

		// Add to inventory (without auto-assign so we can control slot placement)
		const bool bPreviousAutoAssign = bAutoAssignNewItemsToSlots;
		bAutoAssignNewItemsToSlots = false;

		const bool bAdded = AddItemByGuid(NewItemGuid, StartingItem.ItemDefinitionId, StartingItem.Quantity);

		bAutoAssignNewItemsToSlots = bPreviousAutoAssign;

		if (!bAdded)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] Failed to add starting item: %s"), *StartingItem.ItemDefinitionId.ToString());
			continue;
		}

		// Assign to specific slot or auto-assign
		if (StartingItem.SlotIndex >= 0 && IsSlotIndexValid(StartingItem.SlotIndex))
		{
			SlotItemGuids[StartingItem.SlotIndex] = NewItemGuid;
			MarkSlotItemGuidsDirty();
		}
		else if (bAutoAssignNewItemsToSlots)
		{
			TryAutoAssignGuidToEmptySlot(NewItemGuid);
		}
	}

	OnSlotsChanged.Broadcast();
}
