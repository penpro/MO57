#include "MOInventoryComponent.h"
#include "MOWaterSourceSettings.h"
#include "MOIdentityRegistrySubsystem.h"
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
#include "MOQuestSubsystem.h"
#include "MOHarvestDebugSubsystem.h"
#include "UObject/UnrealType.h"
#include "UObject/SoftObjectPtr.h"


UMOInventoryComponent::UMOInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMOInventoryComponent::BeginPlay()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UMOInventoryComponent_BeginPlay);   // spawn-cost breakdown in traces (-statnamedevents)
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

	// COND_None, NOT COND_OwnerOnly: this component also lives on COMMUNAL
	// CONTAINERS (baskets, stations), which have no owning connection — under
	// OwnerOnly their contents replicated to NOBODY and every co-op client
	// saw shared storage as permanently empty (V0 co-op sub-gate, 2026-07-06:
	// villager deposited 4 flakes server-side, client read 0 forever).
	// Bandwidth is bounded by actor relevancy + FastArray deltas; pawn-
	// inventory privacy is a non-goal in friends-only co-op. If that changes,
	// split a shared-container subclass with its own lifetime registration.
	DOREPLIFETIME_CONDITION(UMOInventoryComponent, Inventory, COND_None);
	DOREPLIFETIME_CONDITION(UMOInventoryComponent, SlotItemGuids, COND_None);
}

int32 UMOInventoryComponent::FindEntryIndexByGuid(const FGuid& ItemGuid) const
{
	if (!ItemGuid.IsValid())
	{
		return INDEX_NONE;
	}

	const int32* Found = GuidToEntryIndex.Find(ItemGuid);
	if (Found && *Found < Inventory.Entries.Num() && Inventory.Entries[*Found].ItemGuid == ItemGuid)
	{
		return *Found;
	}

	// Fallback linear search if index is stale
	for (int32 EntryIndex = 0; EntryIndex < Inventory.Entries.Num(); ++EntryIndex)
	{
		if (Inventory.Entries[EntryIndex].ItemGuid == ItemGuid)
		{
			return EntryIndex;
		}
	}

	return INDEX_NONE;
}

void UMOInventoryComponent::RebuildGuidIndex()
{
	GuidToEntryIndex.Reset();
	GuidToEntryIndex.Reserve(Inventory.Entries.Num());
	for (int32 i = 0; i < Inventory.Entries.Num(); ++i)
	{
		if (Inventory.Entries[i].ItemGuid.IsValid())
		{
			GuidToEntryIndex.Add(Inventory.Entries[i].ItemGuid, i);
		}
	}
}

int32 UMOInventoryComponent::GetItemStackLimit(const FMOItemDefinitionRow& Definition)
{
	return (Definition.bIsTool || Definition.LiquidCapacityMl > 0.0f) ? 1 : FMath::Max(1, Definition.MaxStackSize);
}

bool UMOInventoryComponent::HasMatchingStackState(const FMOInventoryEntry& Entry, FName DefinitionId, const FMOItemInstanceState& State)
{
	return Entry.ItemDefinitionId == DefinitionId && Entry.CurrentDurability == State.Durability && Entry.Liquid == State.Liquid;
}

namespace MOInventoryAdds
{

	FMOItemInstanceState NormalizeState(const FMOItemDefinitionRow& Def, FMOItemInstanceState State)
	{
		if (State.Durability < 0 && Def.bIsTool && Def.MaxDurability > 0) State.Durability = Def.MaxDurability;
		return State;
	}

}

int64 UMOInventoryComponent::GetAddCapacity(FName DefinitionId, const FMOItemInstanceState& State, int32 MaxStack, bool bAllowStacking) const
{
	// Entries reserve capacity even during a starting-kit batch with slot assignment disabled.
	int64 Room = int64(FMath::Max(0, FMath::Max(1, SlotCount) - Inventory.Entries.Num())) * MaxStack;
	if (bAllowStacking && MaxStack > 1)
	{
		for (const FMOInventoryEntry& Entry : Inventory.Entries)
		{
			if (HasMatchingStackState(Entry, DefinitionId, State)) Room += FMath::Max(0, MaxStack - Entry.Quantity);
		}
	}
	return Room;
}

FMOInventoryAddResult UMOInventoryComponent::TryAddItem(const FGuid& ItemGuid, FName ItemDefinitionId, int32 Quantity,
	const FMOInventoryAddOptions& Options)
{
	FMOInventoryAddResult Result;
	Result.Requested = Quantity;
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		Result.Failure = EMOInventoryAddFailure::NotAuthority;
		return Result;
	}
	if (!ItemGuid.IsValid() || ItemDefinitionId.IsNone() || Quantity <= 0)
	{
		Result.Failure = EMOInventoryAddFailure::InvalidInput;
		return Result;
	}
	FMOItemDefinitionRow Def;
	if (!UMOItemDatabaseSettings::GetItemDefinition(ItemDefinitionId, Def))
	{
		Result.Failure = EMOInventoryAddFailure::UnknownDefinition;
		return Result;
	}
	const FMOItemInstanceState State = MOInventoryAdds::NormalizeState(Def, Options.State);
	const int32 MaxStack = GetItemStackLimit(Def);
	const int32 ExistingIndex = FindEntryIndexByGuid(ItemGuid);
	if (ExistingIndex != INDEX_NONE && (!Options.bAllowStacking || MaxStack == 1
		|| !HasMatchingStackState(Inventory.Entries[ExistingIndex], ItemDefinitionId, State)))
	{
		Result.Failure = EMOInventoryAddFailure::IdentityConflict;
		return Result;
	}
	const int64 Capacity = GetAddCapacity(ItemDefinitionId, State, MaxStack, Options.bAllowStacking);
	if (Capacity < Quantity)
	{
		Result.Failure = EMOInventoryAddFailure::Capacity;
		if (!Options.bAllowPartial || Capacity <= 0) return Result;
	}
	int32 Remaining = int32(FMath::Min<int64>(Capacity, Quantity));
	Result.Accepted = Remaining;
	FMOInventoryMutationScope Mutation(this);
	EnsureSlotsInitialized();
	if (Options.bAllowStacking && MaxStack > 1)
	{
		for (FMOInventoryEntry& Entry : Inventory.Entries)
		{
			if (Remaining <= 0) break;
			if (!HasMatchingStackState(Entry, ItemDefinitionId, State)) continue;
			const int32 Added = FMath::Min(Remaining, FMath::Max(0, MaxStack - Entry.Quantity));
			if (Added == 0) continue;
			Entry.Quantity += Added;
			Remaining -= Added;
			Inventory.MarkItemDirty(Entry);
		}
	}
	bool bUsedGuid = ExistingIndex != INDEX_NONE;
	while (Remaining > 0)
	{
		FMOInventoryEntry Entry;
		Entry.ItemGuid = bUsedGuid ? FGuid::NewGuid() : ItemGuid;
		bUsedGuid = true;
		Entry.ItemDefinitionId = ItemDefinitionId;
		Entry.Quantity = FMath::Min(Remaining, MaxStack);
		Entry.CurrentDurability = State.Durability;
		Entry.Liquid = State.Liquid;
		Remaining -= Entry.Quantity;
		const int32 Index = Inventory.Entries.Add(Entry);
		Inventory.MarkItemDirty(Inventory.Entries[Index]);
	}
	if (bAutoAssignNewItemsToSlots) ReconcileSlots();
	BroadcastInventoryChanged();
	PendingItemAdded.FindOrAdd(ItemDefinitionId) += Result.Accepted;
	if (Options.bReportPickup && !bApplyingStartingItems) PendingPickupCredit.FindOrAdd(ItemDefinitionId) += Result.Accepted;
	return Result;
}

bool UMOInventoryComponent::AddItemByGuid(const FGuid& ItemGuid, const FName ItemDefinitionId, int32 QuantityToAdd)
{
	return TryAddItem(ItemGuid, ItemDefinitionId, QuantityToAdd).IsComplete();
}

bool UMOInventoryComponent::AddItemByGuidWithDurability(const FGuid& ItemGuid, const FName ItemDefinitionId, int32 QuantityToAdd, int32 InitialDurability)
{
	FMOInventoryAddOptions Options;
	Options.State.Durability = InitialDurability;
	Options.bAllowStacking = false;
	Options.bReportPickup = false;
	return TryAddItem(ItemGuid, ItemDefinitionId, QuantityToAdd, Options).IsComplete();
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
		Inventory.Entries.RemoveAt(ExistingIndex);
		Inventory.MarkArrayDirty();
		RebuildGuidIndex();
		BroadcastSlotsChanged();
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

FMOInventoryMutationScope::FMOInventoryMutationScope(UMOInventoryComponent* Inventory) : Component(Inventory)
{
	if (Inventory) ++Inventory->MutationDepth;
}

FMOInventoryMutationScope::~FMOInventoryMutationScope()
{
	if (Component.IsValid()) Component->EndMutation();
}

void UMOInventoryComponent::EndMutation()
{
	check(MutationDepth > 0);
	if (--MutationDepth > 0) return;
	// Move the pending notifications out before callbacks, so a listener may start a new transaction.
	const bool bInventoryChanged = bInventoryNotificationPending;
	const bool bSlotsChanged = bSlotsNotificationPending;
	bInventoryNotificationPending = bSlotsNotificationPending = false;
	TMap<FName, int32> Added = MoveTemp(PendingItemAdded);
	TMap<FName, int32> Pickups = MoveTemp(PendingPickupCredit);
	PendingItemAdded.Reset();
	PendingPickupCredit.Reset();
	if (bSlotsChanged) OnSlotsChanged.Broadcast();
	if (bInventoryChanged) OnInventoryChanged.Broadcast();
	for (const auto& Pair : Added) OnItemAdded.Broadcast(Pair.Key, Pair.Value);
	for (const auto& Pair : Pickups) UMOQuestSubsystem::ReportForActor(GetOwner(), EMOObjectiveType::ItemPickup, Pair.Key, Pair.Value);
}

void UMOInventoryComponent::BroadcastSlotsChanged()
{
	if (MutationDepth > 0) bSlotsNotificationPending = true;
	else OnSlotsChanged.Broadcast();
}

void UMOInventoryComponent::BroadcastInventoryChanged()
{
	RebuildGuidIndex();
	if (MutationDepth > 0) bInventoryNotificationPending = true;
	else OnInventoryChanged.Broadcast();
}

/*
 * Fast array replication notifications.
 */
void FMOInventoryList::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& /*Parameters*/)
{
	if (OwnerComponent)
	{
		OwnerComponent->RebuildGuidIndex();
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
	return Inventory.Entries.Num() < FMath::Max(1, SlotCount) && FindFirstEmptySlot(EmptySlotIndex);
}

bool UMOInventoryComponent::CanAddItem(const FGuid& ItemGuid) const
{
	if (!ItemGuid.IsValid()) return false;
	const int32 Index = FindEntryIndexByGuid(ItemGuid);
	if (Index == INDEX_NONE) return Inventory.Entries.Num() < FMath::Max(1, SlotCount);
	const FMOInventoryEntry& Entry = Inventory.Entries[Index];
	FMOItemDefinitionRow Def;
	if (!UMOItemDatabaseSettings::GetItemDefinition(Entry.ItemDefinitionId, Def)) return false;
	const int32 MaxStack = GetItemStackLimit(Def);
	return MaxStack > 1 && GetAddCapacity(Entry.ItemDefinitionId, GetInstanceState(ItemGuid), MaxStack, true) > 0;
}

bool UMOInventoryComponent::CanAddItemByDefinitionId(FName ItemDefinitionId, int32 Quantity) const
{
	FMOItemDefinitionRow Def;
	if (Quantity <= 0 || !UMOItemDatabaseSettings::GetItemDefinition(ItemDefinitionId, Def)) return false;
	const FMOItemInstanceState State = MOInventoryAdds::NormalizeState(Def, FMOItemInstanceState());
	return GetAddCapacity(ItemDefinitionId, State, GetItemStackLimit(Def), true) >= Quantity;
}

bool UMOInventoryComponent::CanAddItemsByDefinition(const TMap<FName, int32>& Items) const
{
	const int64 FreeSlots = FMath::Max(0, FMath::Max(1, SlotCount) - Inventory.Entries.Num());
	int64 NeededSlots = 0;
	for (const auto& Pair : Items)
	{
		FMOItemDefinitionRow Def;
		if (Pair.Value <= 0 || !UMOItemDatabaseSettings::GetItemDefinition(Pair.Key, Def)) return false;
		const int32 MaxStack = GetItemStackLimit(Def);
		const auto State = MOInventoryAdds::NormalizeState(Def, FMOItemInstanceState());
		const int64 ExistingRoom = GetAddCapacity(Pair.Key, State, MaxStack, true) - FreeSlots * MaxStack;
		const int64 Remaining = FMath::Max<int64>(0, int64(Pair.Value) - ExistingRoom);
		NeededSlots += (Remaining + MaxStack - 1) / MaxStack;
		if (NeededSlots > FreeSlots) return false;
	}
	return true;
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

int32 UMOInventoryComponent::ReconcileSlots()
{
	EnsureSlotsInitialized();

	bool bChanged = false;

	// A slot may only hold an entry's GUID, and only once.
	TSet<FGuid> Slotted;
	for (FGuid& SlotGuid : SlotItemGuids)
	{
		if (!SlotGuid.IsValid())
		{
			continue;
		}
		bool bAlreadySlotted = false;
		Slotted.Add(SlotGuid, &bAlreadySlotted);
		if (bAlreadySlotted || FindEntryIndexByGuid(SlotGuid) == INDEX_NONE)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] %s: slot held %s GUID %s -- cleared"), *GetNameSafe(GetOwner()),
				bAlreadySlotted ? TEXT("a duplicate") : TEXT("a dangling"), *SlotGuid.ToString());
			SlotGuid.Invalidate();
			bChanged = true;
		}
	}

	// Every entry sits in a slot while one is free.
	int32 Unslotted = 0;
	for (const FMOInventoryEntry& Entry : Inventory.Entries)
	{
		if (!Entry.ItemGuid.IsValid() || Slotted.Contains(Entry.ItemGuid))
		{
			continue;
		}
		if (TryAutoAssignGuidToEmptySlot(Entry.ItemGuid))
		{
			Slotted.Add(Entry.ItemGuid);
			bChanged = true;
		}
		else
		{
			++Unslotted;
		}
	}

	if (bChanged)
	{
		MarkSlotItemGuidsDirty();
		BroadcastSlotsChanged();
	}
	if (Unslotted > 0)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] %s: %d entr%s without a slot (no free slot, or auto-assign off) -- in the inventory but not shown"),
			*GetNameSafe(GetOwner()), Unslotted, Unslotted == 1 ? TEXT("y") : TEXT("ies"));
	}
	return Unslotted;
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
		BroadcastSlotsChanged();
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
	BroadcastSlotsChanged();
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
	BroadcastSlotsChanged();
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
	BroadcastSlotsChanged();
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
	BroadcastSlotsChanged();
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
	BroadcastSlotsChanged();
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

bool UMOInventoryComponent::AddItemByGuidNoStack(const FGuid& ItemGuid, const FName ItemDefinitionId, int32 QuantityToAdd)
{
	FMOInventoryAddOptions Options;
	Options.bAllowStacking = false;
	return TryAddItem(ItemGuid, ItemDefinitionId, QuantityToAdd, Options).IsComplete();
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
		SaveEntry.Liquid = Entry.Liquid;
		OutSaveData.Items.Add(SaveEntry);
	}
}

FMOItemInstanceState UMOInventoryComponent::GetInstanceState(const FGuid& ItemGuid) const
{
	FMOItemInstanceState State;
	const int32 Index = FindEntryIndexByGuid(ItemGuid);
	if (Inventory.Entries.IsValidIndex(Index))
	{
		State.Durability = Inventory.Entries[Index].CurrentDurability;
		State.Liquid = Inventory.Entries[Index].Liquid;
	}
	return State;
}

bool UMOInventoryComponent::ApplyInstanceState(const FGuid& ItemGuid, const FMOItemInstanceState& State)
{
	const int32 Index = FindEntryIndexByGuid(ItemGuid);
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Inventory.Entries.IsValidIndex(Index))
	{
		return false;
	}
	FMOInventoryEntry& Entry = Inventory.Entries[Index];
	if (State.Durability >= 0)
	{
		Entry.CurrentDurability = State.Durability;
	}
	Entry.Liquid = State.Liquid;
	Inventory.MarkItemDirty(Entry);
	BroadcastInventoryChanged();
	return true;
}

bool UMOInventoryComponent::SetLiquid(const FGuid& ItemGuid, const FMOLiquidContents& Liquid)
{
	const int32 Index = FindEntryIndexByGuid(ItemGuid);
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Inventory.Entries.IsValidIndex(Index))
	{
		return false;
	}
	FMOInventoryEntry& Entry = Inventory.Entries[Index];
	if (Entry.Liquid == Liquid)
	{
		return true;
	}
	Entry.Liquid = Liquid;
	Inventory.MarkItemDirty(Entry);
	BroadcastInventoryChanged();
	return true;
}

bool UMOInventoryComponent::ApplySaveDataAuthority(const FMOInventorySaveData& InSaveData)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] ApplySaveDataAuthority requires authority"));
		return false;
	}

	FMOInventoryMutationScope Mutation(this);
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
		NewEntry.Liquid = ItemSaveEntry.Liquid;
		const int32 Index = Inventory.Entries.Add(NewEntry);
		Inventory.MarkItemDirty(Inventory.Entries[Index]);
	}

	Inventory.MarkArrayDirty();
	BroadcastInventoryChanged();

	// Restore slots
	SlotCount = DesiredSlotCount;
	SlotItemGuids = InSaveData.SlotItemGuids;
	SlotItemGuids.SetNum(SlotCount);

	MarkSlotItemGuidsDirty();
	BroadcastSlotsChanged();

	bAutoAssignNewItemsToSlots = bPreviousAutoAssign;

	// Saves written before the starting-kit fix carry an entry in no slot (invisible); a shrunk slot count can strand some too.
	ReconcileSlots();
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
	static void TryWriteDroppedItemPayload(AActor* SpawnedActor, const FGuid& ItemGuid, int32 Quantity, const FName& ItemDefinitionId, const FMOItemInstanceState& State)
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
			ItemComp->InstanceState = State;   // durability and contents ride along: the pickup puts them back
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
	// DEBUG (MOInventory, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOInventory] DropItemFromSlot: Slot=%d, Location=%s"),
	// 	SlotIndex, *DropLocation.ToString());

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
	// DEBUG (MOInventory, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOInventory] DropItemByGuid: Location=%s"), *DropLocation.ToString());

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

	// CRITICAL: the item keeps its GUID, which its pickup recorded as destroyed: announce that it is coming back BEFORE spawning, or the
	// destroyed ledger destroys the new actor on registration. (Through the identity registry: the inventory needs no persistence.)
	if (UMOIdentityRegistrySubsystem* Registry = World->GetSubsystem<UMOIdentityRegistrySubsystem>())
	{
		Registry->AnnounceReintroduction(Entry.ItemGuid);
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

	TryWriteDroppedItemPayload(SpawnedActor, Entry.ItemGuid, Entry.Quantity, Entry.ItemDefinitionId, GetInstanceState(ItemGuid));

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

AActor* UMOInventoryComponent::SpawnWorldItem(FName ItemDefinitionId, int32 Quantity, const FGuid& ItemGuid, const FVector& DropLocation, const FRotator& DropRotation, const FMOItemInstanceState& State)
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

	// The GUID comes back into the world: announce it before spawning (see the drop path above).
	if (UMOIdentityRegistrySubsystem* Registry = World->GetSubsystem<UMOIdentityRegistrySubsystem>())
	{
		Registry->AnnounceReintroduction(ItemGuid);
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

	TryWriteDroppedItemPayload(SpawnedActor, ItemGuid, Quantity, ItemDefinitionId, State);

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
	return TryTransferItem(ItemGuid, TargetInventory).IsComplete();
}

FMOInventoryAddResult UMOInventoryComponent::TryTransferItem(const FGuid& ItemGuid, UMOInventoryComponent* TargetInventory)
{
	FMOInventoryAddResult Result;
	FMOInventoryEntry Entry;
	if (!IsValid(TargetInventory) || TargetInventory == this || !TryGetEntryByGuid(ItemGuid, Entry))
	{
		Result.Failure = EMOInventoryAddFailure::InvalidInput;
		return Result;
	}
	Result.Requested = Entry.Quantity;
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		Result.Failure = EMOInventoryAddFailure::NotAuthority;
		return Result;
	}
	FMOInventoryEntry Existing;
	if (TargetInventory->TryGetEntryByGuid(ItemGuid, Existing))
	{
		Result.Failure = EMOInventoryAddFailure::IdentityConflict;
		return Result;
	}
	FMOInventoryMutationScope SourceMutation(this);
	FMOInventoryMutationScope TargetMutation(TargetInventory);
	FMOInventoryAddOptions Options;
	Options.State.Durability = Entry.CurrentDurability;
	Options.State.Liquid = Entry.Liquid;
	Options.bReportPickup = false;
	Result = TargetInventory->TryAddItem(ItemGuid, Entry.ItemDefinitionId, Entry.Quantity, Options);
	if (Result.IsComplete())
	{
		// Neither inventory has dispatched callbacks since the snapshot: the source still owns exactly this stack.
		const bool bRemoved = RemoveItemByGuid(ItemGuid, Entry.Quantity);
		check(bRemoved);
	}
	return Result;
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
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	FMOInventoryMutationScope Mutation(this);
	for (int32 TargetIndex = 0; TargetIndex < Inventory.Entries.Num(); ++TargetIndex)
	{
		FMOInventoryEntry& Target = Inventory.Entries[TargetIndex];
		FMOItemDefinitionRow Def;
		if (Target.Quantity <= 0 || !UMOItemDatabaseSettings::GetItemDefinition(Target.ItemDefinitionId, Def)) continue;
		const int32 MaxStack = GetItemStackLimit(Def);
		if (MaxStack <= 1) continue;
		FMOItemInstanceState State;
		State.Durability = Target.CurrentDurability;
		State.Liquid = Target.Liquid;
		for (int32 SourceIndex = TargetIndex + 1; SourceIndex < Inventory.Entries.Num() && Target.Quantity < MaxStack; ++SourceIndex)
		{
			FMOInventoryEntry& Source = Inventory.Entries[SourceIndex];
			if (!HasMatchingStackState(Source, Target.ItemDefinitionId, State)) continue;
			const int32 Moved = FMath::Min(MaxStack - Target.Quantity, Source.Quantity);
			if (Moved <= 0) continue;
			Target.Quantity += Moved;
			Source.Quantity -= Moved;
			Inventory.MarkItemDirty(Target);
			Inventory.MarkItemDirty(Source);
		}
	}
	const int32 Removed = Inventory.Entries.RemoveAll([](const FMOInventoryEntry& Entry) { return Entry.Quantity <= 0; });
	if (Removed > 0) Inventory.MarkArrayDirty();
	SlotItemGuids.Init(FGuid(), FMath::Max(1, SlotCount));
	for (int32 Index = 0; Index < Inventory.Entries.Num() && Index < SlotItemGuids.Num(); ++Index)
	{
		SlotItemGuids[Index] = Inventory.Entries[Index].ItemGuid;
	}
	MarkSlotItemGuidsDirty();
	BroadcastSlotsChanged();
	BroadcastInventoryChanged();
}

int32 UMOInventoryComponent::FillStacksFrom(UMOInventoryComponent* SourceInventory)
{
	if (!IsValid(SourceInventory) || SourceInventory == this || !GetOwner() || !GetOwner()->HasAuthority()
		|| !SourceInventory->GetOwner() || !SourceInventory->GetOwner()->HasAuthority()) return 0;
	FMOInventoryMutationScope TargetMutation(this);
	FMOInventoryMutationScope SourceMutation(SourceInventory);
	int32 TotalFilled = 0;
	for (FMOInventoryEntry& Target : Inventory.Entries)
	{
		FMOItemDefinitionRow Def;
		if (!UMOItemDatabaseSettings::GetItemDefinition(Target.ItemDefinitionId, Def)) continue;
		const int32 MaxStack = GetItemStackLimit(Def);
		if (MaxStack <= 1 || Target.Quantity >= MaxStack) continue;
		FMOItemInstanceState State;
		State.Durability = Target.CurrentDurability;
		State.Liquid = Target.Liquid;
		for (FMOInventoryEntry& Source : SourceInventory->Inventory.Entries)
		{
			if (Target.Quantity >= MaxStack) break;
			if (!HasMatchingStackState(Source, Target.ItemDefinitionId, State)) continue;
			const int32 Moved = FMath::Min(MaxStack - Target.Quantity, Source.Quantity);
			if (Moved <= 0) continue;
			Target.Quantity += Moved;
			Source.Quantity -= Moved;
			TotalFilled += Moved;
			Inventory.MarkItemDirty(Target);
			SourceInventory->Inventory.MarkItemDirty(Source);
		}
	}
	for (int32 Index = SourceInventory->Inventory.Entries.Num() - 1; Index >= 0; --Index)
	{
		if (SourceInventory->Inventory.Entries[Index].Quantity <= 0)
		{
			SourceInventory->RemoveGuidFromSlotsInternal(SourceInventory->Inventory.Entries[Index].ItemGuid);
			SourceInventory->Inventory.Entries.RemoveAt(Index);
			SourceInventory->Inventory.MarkArrayDirty();
			SourceInventory->BroadcastSlotsChanged();
		}
	}
	if (TotalFilled > 0)
	{
		BroadcastInventoryChanged();
		SourceInventory->BroadcastInventoryChanged();
	}
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

	TGuardValue<bool> KitGuard(bApplyingStartingItems, true);   // a spawn kit is not a pickup (quests)

	for (const FMOStartingInventoryItem& StartingItem : StartingItems)
	{
		if (StartingItem.ItemDefinitionId.IsNone() || StartingItem.Quantity <= 0)
		{
			continue;
		}

		// A row is not one entry carrying NewItemGuid: AddItemByGuid tops up existing stacks of the item first and splits the
		// rest at MaxStackSize, so a row makes zero, one or several entries. Placing only NewItemGuid left a "Stick01 x20" kit
		// (stack 10) with 10 sticks in no slot -- in the inventory, invisible in the UI (first-ten-minutes pass, Oct 15 2026).
		TSet<FGuid> EntriesBefore;
		for (const FMOInventoryEntry& Entry : Inventory.Entries)
		{
			EntriesBefore.Add(Entry.ItemGuid);
		}

		// Add without auto-assign so explicit SlotIndex rows are placed before ReconcileSlots fills the rest in order.
		const bool bPreviousAutoAssign = bAutoAssignNewItemsToSlots;
		bAutoAssignNewItemsToSlots = false;

		const bool bAdded = AddItemByGuid(FGuid::NewGuid(), StartingItem.ItemDefinitionId, StartingItem.Quantity);

		bAutoAssignNewItemsToSlots = bPreviousAutoAssign;

		if (!bAdded)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] Failed to add starting item: %s"), *StartingItem.ItemDefinitionId.ToString());
			continue;
		}

		// The requested slot gets the row's first new entry (if the slot is still free).
		if (StartingItem.SlotIndex >= 0 && IsSlotIndexValid(StartingItem.SlotIndex) && !SlotItemGuids[StartingItem.SlotIndex].IsValid())
		{
			for (const FMOInventoryEntry& Entry : Inventory.Entries)
			{
				if (!EntriesBefore.Contains(Entry.ItemGuid))
				{
					SlotItemGuids[StartingItem.SlotIndex] = Entry.ItemGuid;
					MarkSlotItemGuidsDirty();
					break;
				}
			}
		}
	}

	ReconcileSlots();
	BroadcastSlotsChanged();
}

// ============================================================================
// TOOL QUERIES
// ============================================================================

namespace MOIngredients
{
	/** DefinitionId as a liquid measure: its millilitres and whether it must be safe water. False = an ordinary item. */
	bool GetLiquidMeasure(FName DefinitionId, float& OutMl, bool& bOutSafe)
	{
		FMOItemDefinitionRow Def;
		if (DefinitionId.IsNone() || !UMOItemDatabaseSettings::GetItemDefinition(DefinitionId, Def)
			|| !FMath::IsFinite(Def.LiquidMeasureMl) || Def.LiquidMeasureMl <= 0.0f)
		{
			return false;
		}
		OutMl = Def.LiquidMeasureMl;
		bOutSafe = Def.bMeasureNeedsSafeWater;
		return true;
	}

	float CapacityOf(FName DefinitionId)
	{
		FMOItemDefinitionRow Def;
		return UMOItemDatabaseSettings::GetItemDefinition(DefinitionId, Def) ? Def.LiquidCapacityMl : 0.0f;
	}

}

int32 UMOInventoryComponent::CountIngredient(FName DefinitionId) const
{
	const int32 Items = GetItemCountByDefinitionId(DefinitionId);
	float MeasureMl = 0.0f;
	bool bSafe = false;
	if (!MOIngredients::GetLiquidMeasure(DefinitionId, MeasureMl, bSafe))
	{
		return Items;
	}
	float Ml = 0.0f;
	for (const FMOInventoryEntry& Entry : Inventory.Entries)
	{
		if (Entry.Quantity > 0 && FMath::IsFinite(Entry.Liquid.Millilitres) && MOIngredients::CapacityOf(Entry.ItemDefinitionId) > 0.0f
			&& Entry.Liquid.IsSuitableMeasure(bSafe))
		{
			Ml += Entry.Liquid.Millilitres;
		}
	}
	return Items + FMath::FloorToInt((Ml + MOIngredientPlans::MeasureToleranceMl(MeasureMl)) / MeasureMl);
}

void UMOInventoryComponent::InstallIngredientPlan(TArray<FMOInventoryEntry>&& Entries)
{
	bool bChanged = Entries.Num() != Inventory.Entries.Num();
	bool bRemoved = false;
	TMap<FName, int32> Added;
	for (const auto& Before : Inventory.Entries)
	{
		Added.FindOrAdd(Before.ItemDefinitionId) -= Before.Quantity;
		if (!Entries.ContainsByPredicate([&Before](const auto& Entry) { return Entry.ItemGuid == Before.ItemGuid; })) bRemoved = true;
	}
	for (auto& Entry : Entries)
	{
		Added.FindOrAdd(Entry.ItemDefinitionId) += Entry.Quantity;
		const int32 OldIndex = FindEntryIndexByGuid(Entry.ItemGuid);
		// FFastArraySerializerItem deliberately clears replication metadata on copy.
		// Snapshot planning must preserve the live serializer identity of every survivor,
		// including unchanged entries, before replacing the array's storage.
		if (OldIndex != INDEX_NONE)
		{
			const auto& Before = Inventory.Entries[OldIndex];
			Entry.ReplicationID = Before.ReplicationID;
			Entry.ReplicationKey = Before.ReplicationKey;
			Entry.MostRecentArrayReplicationKey = Before.MostRecentArrayReplicationKey;
		}
		else
		{
			Entry.ReplicationID = INDEX_NONE;
			Entry.ReplicationKey = INDEX_NONE;
			Entry.MostRecentArrayReplicationKey = INDEX_NONE;
		}
		if (OldIndex == INDEX_NONE || Inventory.Entries[OldIndex].Quantity != Entry.Quantity
			|| Inventory.Entries[OldIndex].ItemDefinitionId != Entry.ItemDefinitionId
			|| Inventory.Entries[OldIndex].CurrentDurability != Entry.CurrentDurability
			|| !(Inventory.Entries[OldIndex].Liquid == Entry.Liquid))
		{
			Inventory.MarkItemDirty(Entry);
			bChanged = true;
		}
	}
	if (!bChanged) return;
	Inventory.Entries = MoveTemp(Entries);
	if (bRemoved) Inventory.MarkArrayDirty();
	RebuildGuidIndex();
	ReconcileSlots();
	for (const auto& Pair : Added) if (Pair.Value > 0) PendingItemAdded.FindOrAdd(Pair.Key) += Pair.Value;
	BroadcastInventoryChanged();
}

FMOIngredientOperationResult UMOInventoryComponent::ReserveIngredients(const TMap<FName, int32>& Requirements, FMOIngredientReservation& OutReservation)
{
	FMOIngredientOperationResult Result;
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		Result.Failure = EMOIngredientFailure::NotAuthority;
		return Result;
	}
	TArray<FMOInventoryEntry> Plan = Inventory.Entries;
	Result = MOIngredientPlans::Reserve(Plan, Requirements, OutReservation, UMOItemDatabaseSettings::GetItemDefinition);
	if (Result.IsComplete())
	{
		FMOInventoryMutationScope Mutation(this);
		InstallIngredientPlan(MoveTemp(Plan));
	}
	return Result;
}

FMOIngredientOperationResult UMOInventoryComponent::ReturnReservedIngredients(FMOIngredientReservation& Reservation)
{
	FMOIngredientOperationResult Result;
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		Result.Failure = EMOIngredientFailure::NotAuthority;
		return Result;
	}
	TArray<FMOInventoryEntry> Plan = Inventory.Entries;
	FMOIngredientReservation Remaining = Reservation;
	Result = MOIngredientPlans::Return(Plan, SlotCount, Remaining, UMOItemDatabaseSettings::GetItemDefinition);
	FMOInventoryMutationScope Mutation(this);
	Reservation = MoveTemp(Remaining);
	InstallIngredientPlan(MoveTemp(Plan));
	return Result;
}

FMOIngredientOperationResult UMOInventoryComponent::TransferIngredientsTo(UMOInventoryComponent* Target, const TMap<FName, int32>& Requirements,
	const FMOIngredientTransferOptions& Options, TArray<FGuid>* OutCarriedContainers)
{
	FMOIngredientOperationResult Result;
	if (!IsValid(Target) || Target == this || !GetOwner() || !Target->GetOwner() || GetWorld() != Target->GetWorld())
	{
		Result.Failure = EMOIngredientFailure::InvalidInput;
		return Result;
	}
	if (!GetOwner()->HasAuthority() || !Target->GetOwner()->HasAuthority())
	{
		Result.Failure = EMOIngredientFailure::NotAuthority;
		return Result;
	}
	TArray<FMOInventoryEntry> SourcePlan = Inventory.Entries, TargetPlan = Target->Inventory.Entries;
	FMOIngredientReservation Transfer;
	TSet<FGuid> AdditionalItems;
	for (const auto& Guid : Options.AlsoCarryItemGuids)
	{
		if (!Guid.IsValid() || AdditionalItems.Contains(Guid) || !Inventory.Entries.ContainsByPredicate([&Guid](const auto& Entry) { return Entry.ItemGuid == Guid; }))
		{
			Result.Failure = EMOIngredientFailure::IdentityConflict;
			return Result;
		}
		AdditionalItems.Add(Guid);
	}
	Result = MOIngredientPlans::Reserve(SourcePlan, Requirements, Transfer, UMOItemDatabaseSettings::GetItemDefinition, &AdditionalItems);
	if (!Result.IsComplete()) return Result;
	TArray<FGuid> CarriedContainers;
	if (Options.bCarrySourceContainers)
	{
		Result = MOIngredientPlans::CarrySourceContainers(SourcePlan, Transfer, CarriedContainers, UMOItemDatabaseSettings::GetItemDefinition);
		if (!Result.IsComplete()) return Result;
	}
	Result = MOIngredientPlans::CarryItems(SourcePlan, Transfer, Options.AlsoCarryItemGuids);
	if (!Result.IsComplete()) return Result;
	Result = MOIngredientPlans::Return(TargetPlan, Target->SlotCount, Transfer, UMOItemDatabaseSettings::GetItemDefinition);
	if (!Result.IsComplete()) return Result;
	FMOInventoryMutationScope SourceMutation(this);
	FMOInventoryMutationScope TargetMutation(Target);
	InstallIngredientPlan(MoveTemp(SourcePlan));
	Target->InstallIngredientPlan(MoveTemp(TargetPlan));
	if (OutCarriedContainers) *OutCarriedContainers = MoveTemp(CarriedContainers);
	return Result;
}

bool UMOInventoryComponent::ConsumeIngredient(FName DefinitionId, int32 Quantity)
{
	if (Quantity <= 0) return Quantity == 0;
	FMOIngredientReservation Consumed;
	return ReserveIngredients({{DefinitionId, Quantity}}, Consumed).IsComplete();
}

int32 UMOInventoryComponent::AddIngredient(FName DefinitionId, int32 Quantity)
{
	if (Quantity <= 0 || !GetOwner() || !GetOwner()->HasAuthority()) return 0;
	FMOIngredientReservation Products;
	if (!MOIngredientPlans::CreateProducts({{DefinitionId, Quantity}}, Products, UMOItemDatabaseSettings::GetItemDefinition).IsComplete()) return 0;
	const float Measure = Products.Lots.IsEmpty() ? 0.0f : Products.Lots[0].MeasureMl;
	ReturnReservedIngredients(Products);
	double RemainingUnits = 0.0;
	for (const auto& Lot : Products.Lots)
	{
		RemainingUnits += Lot.IsLiquid() ? double(Lot.State.Liquid.Millilitres) / Lot.MeasureMl : Lot.Quantity;
	}
	// This compatibility API reports placed whole measures; queue products keep the exact remainder instead.
	const double Tolerance = Measure > 0 ? MOIngredientPlans::MeasureToleranceMl(Measure) / Measure : 0.0;
	const int32 Unplaced = FMath::CeilToInt(FMath::Max(0.0, RemainingUnits - Tolerance));
	return FMath::Clamp(Quantity - Unplaced, 0, Quantity);
}

float UMOInventoryComponent::GetBestToolEffectiveness(EMOToolType ToolType) const
{
	float Best = 0.0f;
	if (ToolType == EMOToolType::None)
	{
		return Best;
	}
	for (const FMOInventoryEntry& Entry : Inventory.Entries)
	{
		if (Entry.ItemDefinitionId.IsNone() || Entry.Quantity <= 0 || Entry.CurrentDurability == 0)
		{
			continue;   // a broken tool (durability 0; -1 = not tracked) does no work
		}
		FMOItemDefinitionRow ItemDef;
		if (UMOItemDatabaseSettings::GetItemDefinition(Entry.ItemDefinitionId, ItemDef))
		{
			Best = FMath::Max(Best, ItemDef.GetToolEffectiveness(ToolType));
		}
	}
	return Best;
}

bool UMOInventoryComponent::HasToolOfType(EMOToolType ToolType) const
{
	if (ToolType == EMOToolType::None)
	{
		return true;
	}

	// Check all inventory entries for a matching tool capability
	for (const FMOInventoryEntry& Entry : Inventory.Entries)
	{
		if (Entry.ItemDefinitionId.IsNone() || Entry.Quantity <= 0)
		{
			continue;
		}

		FMOItemDefinitionRow ItemDef;
		if (UMOItemDatabaseSettings::GetItemDefinition(Entry.ItemDefinitionId, ItemDef))
		{
			if (ItemDef.HasToolCapability(ToolType))
			{
				return true;
			}
		}
	}

	return false;
}

bool UMOInventoryComponent::FindToolOfType(EMOToolType ToolType, FGuid& OutItemGuid) const
{
	OutItemGuid = FGuid();

	if (ToolType == EMOToolType::None)
	{
		return false;
	}

	// Find the first inventory entry with a matching tool capability
	for (const FMOInventoryEntry& Entry : Inventory.Entries)
	{
		if (Entry.ItemDefinitionId.IsNone() || Entry.Quantity <= 0)
		{
			continue;
		}

		FMOItemDefinitionRow ItemDef;
		if (UMOItemDatabaseSettings::GetItemDefinition(Entry.ItemDefinitionId, ItemDef))
		{
			if (ItemDef.HasToolCapability(ToolType))
			{
				OutItemGuid = Entry.ItemGuid;
				return true;
			}
		}
	}

	return false;
}
