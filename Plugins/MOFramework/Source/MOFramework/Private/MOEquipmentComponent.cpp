#include "MOEquipmentComponent.h"
#include "MOFramework.h"
#include "MOInventoryComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "Net/UnrealNetwork.h"

namespace MOEquipmentTransactions
{
	bool ReturnItem(UMOInventoryComponent* Inventory, const FMOEquippedItem& Item)
	{
		FMOInventoryAddOptions Options;
		Options.State.Durability = Item.CurrentDurability;
		Options.State.Liquid = Item.Liquid;
		Options.bAllowStacking = false;
		Options.bReportPickup = false;
		return Inventory->TryAddItem(Item.ItemGuid, Item.ItemDefinitionId, 1, Options).IsComplete();
	}
}

UMOEquipmentComponent::UMOEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UMOEquipmentComponent::BeginPlay()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UMOEquipmentComponent_BeginPlay);   // spawn-cost breakdown in traces (-statnamedevents)
	Super::BeginPlay();
	EnsureSlotsInitialized();
}

void UMOEquipmentComponent::EnsureSlotsInitialized()
{
	const int32 NumSlots = static_cast<int32>(EMOEquipmentSlot::MAX);
	if (EquippedSlots.Num() != NumSlots)
	{
		EquippedSlots.SetNum(NumSlots);
	}
}

bool UMOEquipmentComponent::IsHandSlot(EMOEquipmentSlot EquipSlot) const
{
	return EquipSlot == EMOEquipmentSlot::LeftHand || EquipSlot == EMOEquipmentSlot::RightHand;
}

void UMOEquipmentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		SetComponentTickEnabled(false);
		return;
	}
	TArray<EMOEquipmentSlot, TInlineAllocator<2>> Finished;
	for (FMOEquipmentPendingSwap& Pending : PendingSwaps)
	{
		Pending.RemainingSeconds = FMath::Max(0.0f, Pending.RemainingSeconds - DeltaTime);
		if (Pending.RemainingSeconds <= 0.0f)
		{
			Finished.Add(Pending.Slot);
		}
	}
	// Completion broadcasts can start another swap; never keep references across them.
	for (EMOEquipmentSlot Slot : Finished)
	{
		CompleteSwap(Slot);
	}
	SetComponentTickEnabled(PendingSwaps.Num() > 0);
}

// ============================================================================
// EQUIPMENT OPERATIONS
// ============================================================================

namespace
{
	/** The slot an item's EquipmentSlotType naturally occupies. Held items get
	 *  the right hand. None and unknown types map to MAX (not equippable). */
	EMOEquipmentSlot NaturalSlotForType(EMOEquipmentSlotType SlotType)
	{
		switch (SlotType)
		{
		case EMOEquipmentSlotType::Head:  return EMOEquipmentSlot::Head;
		case EMOEquipmentSlotType::Chest: return EMOEquipmentSlot::Chest;
		case EMOEquipmentSlotType::Hands: return EMOEquipmentSlot::Hands;
		case EMOEquipmentSlotType::Legs:  return EMOEquipmentSlot::Legs;
		case EMOEquipmentSlotType::Feet:  return EMOEquipmentSlot::Feet;
		case EMOEquipmentSlotType::Back:  return EMOEquipmentSlot::Back;
		case EMOEquipmentSlotType::Held:  return EMOEquipmentSlot::RightHand;
		default:                          return EMOEquipmentSlot::MAX;
		}
	}
}

bool UMOEquipmentComponent::EquipFromInventoryToNaturalSlot(UMOInventoryComponent* Inventory, const FGuid& ItemGuid)
{
	if (!IsValid(Inventory) || !ItemGuid.IsValid())
	{
		return false;
	}

	FMOInventoryEntry Entry;
	if (!Inventory->TryGetEntryByGuid(ItemGuid, Entry))
	{
		return false;
	}

	FMOItemDefinitionRow ItemDef;
	if (!UMOItemDatabaseSettings::GetItemDefinition(Entry.ItemDefinitionId, ItemDef))
	{
		return false;
	}

	const EMOEquipmentSlot Slot = NaturalSlotForType(ItemDef.EquipmentSlotType);
	if (Slot == EMOEquipmentSlot::MAX)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOEquipmentComponent] EquipFromInventoryToNaturalSlot: %s has no equip slot (EquipmentSlotType=%d)"),
			*Entry.ItemDefinitionId.ToString(), static_cast<int32>(ItemDef.EquipmentSlotType));
		return false;
	}

	return EquipFromInventory(Inventory, ItemGuid, Slot);
}

bool UMOEquipmentComponent::EquipFromInventory(UMOInventoryComponent* Inventory, const FGuid& ItemGuid, EMOEquipmentSlot EquipSlot)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bEquipmentMutationInProgress
		|| !IsValid(Inventory) || !ItemGuid.IsValid() || !IsValidEquipmentSlot(EquipSlot) || IsSlotSwapping(EquipSlot))
	{
		return false;
	}
	FMOInventoryEntry Entry;
	if (!Inventory->TryGetEntryByGuid(ItemGuid, Entry) || !CanEquipToSlot(Entry.ItemDefinitionId, EquipSlot))
	{
		return false;
	}
	EnsureSlotsInitialized();
	const int32 SlotIndex = static_cast<int32>(EquipSlot);
	const FMOEquippedItem Previous = EquippedSlots[SlotIndex];
	bool bFreesSlot = false;
	for (int32 Index = 0; Entry.Quantity == 1 && Index < Inventory->GetSlotCount(); ++Index)
	{
		FGuid Slotted;
		if (Inventory->TryGetSlotGuid(Index, Slotted) && Slotted == ItemGuid)
		{
			bFreesSlot = true;
			break;
		}
	}
	if (Previous.IsValid() && !bFreesSlot && !Inventory->CanAddItemByDefinitionId(Previous.ItemDefinitionId, 1))
	{
		return false;
	}
	TGuardValue<bool> MutationGuard(bEquipmentMutationInProgress, true);
	FMOInventoryMutationScope InventoryMutation(Inventory);
	FMOEquipmentPendingSwap Pending;
	Pending.Slot = EquipSlot;
	Pending.Item.ItemGuid = Entry.Quantity == 1 ? Entry.ItemGuid : FGuid::NewGuid();
	Pending.Item.ItemDefinitionId = Entry.ItemDefinitionId;
	Pending.Item.CurrentDurability = Entry.CurrentDurability;
	Pending.Item.Liquid = Entry.Liquid;
	Pending.TotalSeconds = IsHandSlot(EquipSlot) ? FMath::Max(0.0f, GetSwapTimeForItem(Entry.ItemDefinitionId)) : 0.0f;
	Pending.RemainingSeconds = Pending.TotalSeconds;
	// Escrow is installed before Remove broadcasts: a synchronous save sees who owns the incoming item.
	PendingSwaps.Add(Pending);
	if (!Inventory->RemoveItemByGuid(ItemGuid, 1))
	{
		PendingSwaps.RemoveAll([EquipSlot](const FMOEquipmentPendingSwap& P) { return P.Slot == EquipSlot; });
		return false;
	}
	EquippedSlots[SlotIndex].Clear();
	if (Previous.IsValid() && !MOEquipmentTransactions::ReturnItem(Inventory, Previous))
	{
		EquippedSlots[SlotIndex] = Previous;
		// The incoming item was just removed, so restoring it uses the same slot/stack room.
		PendingSwaps.RemoveAll([EquipSlot](const FMOEquipmentPendingSwap& P) { return P.Slot == EquipSlot; });
		FMOInventoryAddOptions Restore;
		Restore.State.Durability = Entry.CurrentDurability;
		Restore.State.Liquid = Entry.Liquid;
		Restore.bReportPickup = false;
		const bool bRestored = Inventory->TryAddItem(Entry.ItemGuid, Entry.ItemDefinitionId, 1, Restore).IsComplete();
		if (!bRestored)
		{
			PendingSwaps.Add(Pending);
			UE_LOG(LogMOFramework, Error, TEXT("[MOEquipment] Inventory disappeared during equip rollback; incoming item retained in saved escrow"));
		}
		return false;
	}
	OnEquipmentChanged.Broadcast(EquipSlot, EquippedSlots[SlotIndex]);
	if (Pending.TotalSeconds > 0.0f)
	{
		SetComponentTickEnabled(true);
		OnSwapStarted.Broadcast(EquipSlot);
	}
	else
	{
		CompleteSwap(EquipSlot);
	}
	return true;
}

bool UMOEquipmentComponent::UnequipToInventory(EMOEquipmentSlot EquipSlot, UMOInventoryComponent* Inventory)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bEquipmentMutationInProgress
		|| !IsValid(Inventory) || !IsValidEquipmentSlot(EquipSlot) || IsSlotSwapping(EquipSlot))
	{
		return false;
	}

	EnsureSlotsInitialized();
	const int32 SlotIndex = static_cast<int32>(EquipSlot);
	const FMOEquippedItem Item = EquippedSlots[SlotIndex];

	if (!Item.IsValid())
	{
		return false; // Nothing to unequip
	}

	TGuardValue<bool> MutationGuard(bEquipmentMutationInProgress, true);
	FMOInventoryMutationScope InventoryMutation(Inventory);
	EquippedSlots[SlotIndex].Clear();
	if (!MOEquipmentTransactions::ReturnItem(Inventory, Item))
	{
		EquippedSlots[SlotIndex] = Item;
		return false;
	}
	OnEquipmentChanged.Broadcast(EquipSlot, EquippedSlots[SlotIndex]);

	return true;
}

void UMOEquipmentComponent::SwapSlots(EMOEquipmentSlot SlotA, EMOEquipmentSlot SlotB)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bEquipmentMutationInProgress || SlotA == SlotB
		|| !IsValidEquipmentSlot(SlotA) || !IsValidEquipmentSlot(SlotB) || IsSlotSwapping(SlotA) || IsSlotSwapping(SlotB))
	{
		return;
	}

	EnsureSlotsInitialized();
	const int32 IndexA = static_cast<int32>(SlotA);
	const int32 IndexB = static_cast<int32>(SlotB);
	if ((EquippedSlots[IndexA].IsValid() && !CanEquipToSlot(EquippedSlots[IndexA].ItemDefinitionId, SlotB))
		|| (EquippedSlots[IndexB].IsValid() && !CanEquipToSlot(EquippedSlots[IndexB].ItemDefinitionId, SlotA)))
	{
		return;
	}
	TGuardValue<bool> MutationGuard(bEquipmentMutationInProgress, true);

	FMOEquippedItem Temp = EquippedSlots[IndexA];
	EquippedSlots[IndexA] = EquippedSlots[IndexB];
	EquippedSlots[IndexB] = Temp;

	OnEquipmentChanged.Broadcast(SlotA, EquippedSlots[IndexA]);
	OnEquipmentChanged.Broadcast(SlotB, EquippedSlots[IndexB]);
}

bool UMOEquipmentComponent::ReduceEquippedDurability(EMOEquipmentSlot Slot, int32 Amount, bool& bOutDestroyed)
{
	bOutDestroyed = false;
	if (!GetOwner() || !GetOwner()->HasAuthority() || bEquipmentMutationInProgress || Amount <= 0
		|| !IsValidEquipmentSlot(Slot) || IsSlotSwapping(Slot)) return false;
	EnsureSlotsInitialized();
	auto& Item = EquippedSlots[static_cast<int32>(Slot)];
	if (!Item.IsValid() || Item.CurrentDurability < 0) return false;
	TGuardValue<bool> Mutation(bEquipmentMutationInProgress, true);
	Item.CurrentDurability = FMath::Max(0, Item.CurrentDurability - Amount);
	if (Item.CurrentDurability == 0)
	{
		Item.Clear();
		bOutDestroyed = true;
		OnEquipmentChanged.Broadcast(Slot, Item);
	}
	else OnEquipmentDurabilityChanged.Broadcast(Slot, Item.CurrentDurability);
	return true;
}

bool UMOEquipmentComponent::IsSlotSwapping(EMOEquipmentSlot EquipSlot) const
{
	return IsValidEquipmentSlot(EquipSlot) && PendingSwaps.ContainsByPredicate(
		[EquipSlot](const FMOEquipmentPendingSwap& P) { return P.Slot == EquipSlot; });
}

float UMOEquipmentComponent::GetSwapProgress(EMOEquipmentSlot EquipSlot) const
{
	const FMOEquipmentPendingSwap* Pending = PendingSwaps.FindByPredicate(
		[EquipSlot](const FMOEquipmentPendingSwap& P) { return P.Slot == EquipSlot; });
	return Pending && Pending->TotalSeconds > 0.0f
		? 1.0f - FMath::Clamp(Pending->RemainingSeconds / Pending->TotalSeconds, 0.0f, 1.0f) : 1.0f;
}

// ============================================================================
// ACCESSORS
// ============================================================================

FMOEquippedItem UMOEquipmentComponent::GetEquippedItem(EMOEquipmentSlot EquipSlot) const
{
	if (!IsValidEquipmentSlot(EquipSlot))
	{
		return FMOEquippedItem();
	}

	const int32 SlotIndex = static_cast<int32>(EquipSlot);
	if (SlotIndex < EquippedSlots.Num())
	{
		return EquippedSlots[SlotIndex];
	}
	return FMOEquippedItem();
}

bool UMOEquipmentComponent::HasItemInSlot(EMOEquipmentSlot EquipSlot) const
{
	return GetEquippedItem(EquipSlot).IsValid();
}

void UMOEquipmentComponent::GetAllEquippedItems(TArray<FMOEquippedItem>& OutItems) const
{
	OutItems.Reset();
	for (const FMOEquippedItem& Item : EquippedSlots)
	{
		if (Item.IsValid())
		{
			OutItems.Add(Item);
		}
	}
}

bool UMOEquipmentComponent::CanEquipToSlot(FName ItemDefinitionId, EMOEquipmentSlot EquipSlot) const
{
	if (!IsValidEquipmentSlot(EquipSlot))
	{
		return false;
	}
	FMOItemDefinitionRow ItemDef;
	if (!UMOItemDatabaseSettings::GetItemDefinition(ItemDefinitionId, ItemDef))
	{
		return false;
	}

	if (!ItemDef.bEquippable)
	{
		return false;
	}

	// Held items fit either hand; everything else fits exactly its natural slot.
	if (ItemDef.EquipmentSlotType == EMOEquipmentSlotType::Held)
	{
		return EquipSlot == EMOEquipmentSlot::LeftHand || EquipSlot == EMOEquipmentSlot::RightHand;
	}
	const EMOEquipmentSlot Natural = NaturalSlotForType(ItemDef.EquipmentSlotType);
	return Natural != EMOEquipmentSlot::MAX && EquipSlot == Natural;
}

// ============================================================================
// BACK SLOT (CONTAINER) SPECIFIC
// ============================================================================

bool UMOEquipmentComponent::HasBackpackEquipped() const
{
	return HasItemInSlot(EMOEquipmentSlot::Back);
}

FName UMOEquipmentComponent::GetEquippedBackpackId() const
{
	FMOEquippedItem BackItem = GetEquippedItem(EMOEquipmentSlot::Back);
	return BackItem.IsValid() ? BackItem.ItemDefinitionId : NAME_None;
}

// ============================================================================
// INTERNAL
// ============================================================================

float UMOEquipmentComponent::GetSwapTimeForItem(FName ItemDefinitionId) const
{
	FMOItemDefinitionRow ItemDef;
	if (!UMOItemDatabaseSettings::GetItemDefinition(ItemDefinitionId, ItemDef))
	{
		return MediumSwapTime; // Default to medium
	}

	// Use item weight to determine swap tier
	// Light: < 1.0 kg
	// Medium: 1.0 - 3.0 kg
	// Heavy: > 3.0 kg
	if (ItemDef.Weight < 1.0f)
	{
		return LightSwapTime;
	}
	else if (ItemDef.Weight <= 3.0f)
	{
		return MediumSwapTime;
	}
	else
	{
		return HeavySwapTime;
	}
}

void UMOEquipmentComponent::CompleteSwap(EMOEquipmentSlot EquipSlot)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValidEquipmentSlot(EquipSlot))
	{
		return;
	}
	const int32 PendingIndex = PendingSwaps.IndexOfByPredicate(
		[EquipSlot](const FMOEquipmentPendingSwap& P) { return P.Slot == EquipSlot; });
	if (PendingIndex == INDEX_NONE)
	{
		return;
	}
	EnsureSlotsInitialized();
	const int32 SlotIndex = static_cast<int32>(EquipSlot);
	if (EquippedSlots[SlotIndex].IsValid())
	{
		return; // Preserve both owners if a failed rollback retained an escrow item.
	}
	TGuardValue<bool> MutationGuard(bEquipmentMutationInProgress, true);
	EquippedSlots[SlotIndex] = PendingSwaps[PendingIndex].Item;
	PendingSwaps.RemoveAt(PendingIndex);
	const FMOEquippedItem Equipped = EquippedSlots[SlotIndex];
	OnEquipmentChanged.Broadcast(EquipSlot, Equipped);
	OnSwapCompleted.Broadcast(EquipSlot);
}

// ============================================================================
// SAVE/LOAD
// ============================================================================

void UMOEquipmentComponent::BuildSaveData(FMOEquipmentSaveData& OutData) const
{
	OutData.Slots.Reset();
	OutData.PendingSwaps = PendingSwaps;
	for (int32 i = 0; i < EquippedSlots.Num(); ++i)
	{
		if (EquippedSlots[i].IsValid())
		{
			FMOEquipmentSlotSaveData SlotData;
			SlotData.Slot = static_cast<EMOEquipmentSlot>(i);
			SlotData.EquippedItem = EquippedSlots[i];
			OutData.Slots.Add(SlotData);
		}
	}
}

void UMOEquipmentComponent::ApplySaveData(const FMOEquipmentSaveData& InData)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bEquipmentMutationInProgress)
	{
		return;
	}
	TGuardValue<bool> MutationGuard(bEquipmentMutationInProgress, true);
	EnsureSlotsInitialized();
	PendingSwaps.Reset();
	SetComponentTickEnabled(false);
	TSet<FGuid> RestoredItems;

	// Clear all slots first
	for (FMOEquippedItem& Item : EquippedSlots)
	{
		Item.Clear();
	}

	// Apply saved data
	for (const FMOEquipmentSlotSaveData& SlotData : InData.Slots)
	{
		if (IsValidEquipmentSlot(SlotData.Slot) && SlotData.EquippedItem.IsValid()
			&& !RestoredItems.Contains(SlotData.EquippedItem.ItemGuid))
		{
			const int32 SlotIndex = static_cast<int32>(SlotData.Slot);
			if (SlotIndex < EquippedSlots.Num())
			{
				EquippedSlots[SlotIndex] = SlotData.EquippedItem;
				RestoredItems.Add(SlotData.EquippedItem.ItemGuid);
			}
		}
	}
	for (const FMOEquipmentPendingSwap& Saved : InData.PendingSwaps)
	{
		if (!IsValidEquipmentSlot(Saved.Slot) || !Saved.Item.IsValid() || RestoredItems.Contains(Saved.Item.ItemGuid)
			|| IsSlotSwapping(Saved.Slot) || !FMath::IsFinite(Saved.TotalSeconds) || !FMath::IsFinite(Saved.RemainingSeconds)
			|| Saved.TotalSeconds < 0.0f || Saved.RemainingSeconds < 0.0f)
		{
			continue;
		}
		FMOEquipmentPendingSwap Pending = Saved;
		Pending.RemainingSeconds = FMath::Min(Pending.RemainingSeconds, Pending.TotalSeconds);
		PendingSwaps.Add(Pending);
		RestoredItems.Add(Pending.Item.ItemGuid);
	}
	// Broadcast only after the whole state is installed, including pending ownership.
	for (int32 Index = 0; Index < EquippedSlots.Num(); ++Index)
	{
		OnEquipmentChanged.Broadcast(static_cast<EMOEquipmentSlot>(Index), EquippedSlots[Index]);
	}
	SetComponentTickEnabled(PendingSwaps.Num() > 0);
}

// ============================================================================
// REPLICATION
// ============================================================================

void UMOEquipmentComponent::OnRep_EquippedSlots(const TArray<FMOEquippedItem>& PreviousSlots)
{
	for (int32 Index = 0; Index < FMath::Max(EquippedSlots.Num(), PreviousSlots.Num()) && Index < static_cast<int32>(EMOEquipmentSlot::MAX); ++Index)
	{
		const FMOEquippedItem Item = EquippedSlots.IsValidIndex(Index) ? EquippedSlots[Index] : FMOEquippedItem();
		const FMOEquippedItem Before = PreviousSlots.IsValidIndex(Index) ? PreviousSlots[Index] : FMOEquippedItem();
		const auto Slot = static_cast<EMOEquipmentSlot>(Index);
		if (Item.ItemGuid != Before.ItemGuid || Item.ItemDefinitionId != Before.ItemDefinitionId || !(Item.Liquid == Before.Liquid))
			OnEquipmentChanged.Broadcast(Slot, Item);
		else if (Item.CurrentDurability != Before.CurrentDurability)
			OnEquipmentDurabilityChanged.Broadcast(Slot, Item.CurrentDurability);
	}
}

void UMOEquipmentComponent::OnRep_PendingSwaps()
{
	TSet<EMOEquipmentSlot> Current;
	for (const FMOEquipmentPendingSwap& Pending : PendingSwaps)
	{
		Current.Add(Pending.Slot);
	}
	const TSet<EMOEquipmentSlot> Previous = MoveTemp(NotifiedPendingSlots);
	NotifiedPendingSlots = Current;
	for (EMOEquipmentSlot Slot : Current)
	{
		if (!Previous.Contains(Slot)) OnSwapStarted.Broadcast(Slot);
	}
	for (EMOEquipmentSlot Slot : Previous)
	{
		if (!Current.Contains(Slot)) OnSwapCompleted.Broadcast(Slot);
	}
}

void UMOEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMOEquipmentComponent, EquippedSlots);
	DOREPLIFETIME(UMOEquipmentComponent, PendingSwaps);
}

// ---- IMOInsulationSource (codex review: equipment owns clothing policy) ----

float UMOEquipmentComponent::ComputeInsulationFromWarmth(const TArray<float>& ItemWarmths, float MaxClothingInsulation)
{
	float Sum = 0.0f;
	for (float W : ItemWarmths)
	{
		Sum += FMath::Max(W, 0.0f);
	}
	return FMath::Clamp(Sum, 0.0f, MaxClothingInsulation);
}

float UMOEquipmentComponent::GetClothingInsulationBonus01() const
{
	TArray<FMOEquippedItem> Equipped;
	GetAllEquippedItems(Equipped);
	TArray<float> Warmths;
	for (const FMOEquippedItem& Item : Equipped)
	{
		FMOItemDefinitionRow Def;
		if (UMOItemDatabaseSettings::GetItemDefinition(Item.ItemDefinitionId, Def))
		{
			for (const FMOItemScalarProperty& Prop : Def.ScalarProperties)
			{
				if (Prop.Key == TEXT("Warmth"))
				{
					Warmths.Add(Prop.Value);
				}
			}
		}
	}
	return ComputeInsulationFromWarmth(Warmths);
}
