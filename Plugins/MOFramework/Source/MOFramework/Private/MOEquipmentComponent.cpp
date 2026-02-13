#include "MOEquipmentComponent.h"
#include "MOFramework.h"
#include "MOInventoryComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "Net/UnrealNetwork.h"

UMOEquipmentComponent::UMOEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UMOEquipmentComponent::BeginPlay()
{
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

	bool bStillSwapping = false;

	// Update left hand swap
	if (LeftHandSwapTimeRemaining > 0.0f)
	{
		LeftHandSwapTimeRemaining -= DeltaTime;
		if (LeftHandSwapTimeRemaining <= 0.0f)
		{
			CompleteSwap(EMOEquipmentSlot::LeftHand);
		}
		else
		{
			bStillSwapping = true;
		}
	}

	// Update right hand swap
	if (RightHandSwapTimeRemaining > 0.0f)
	{
		RightHandSwapTimeRemaining -= DeltaTime;
		if (RightHandSwapTimeRemaining <= 0.0f)
		{
			CompleteSwap(EMOEquipmentSlot::RightHand);
		}
		else
		{
			bStillSwapping = true;
		}
	}

	// Disable tick when not swapping
	if (!bStillSwapping)
	{
		SetComponentTickEnabled(false);
	}
}

// ============================================================================
// EQUIPMENT OPERATIONS
// ============================================================================

bool UMOEquipmentComponent::EquipFromInventory(UMOInventoryComponent* Inventory, const FGuid& ItemGuid, EMOEquipmentSlot EquipSlot)
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOEquipmentComponent] EquipFromInventory: ItemGuid=%s, EquipSlot=%d"),
		*ItemGuid.ToString(EGuidFormats::Short), static_cast<int32>(EquipSlot));

	if (!IsValid(Inventory) || !ItemGuid.IsValid() || EquipSlot == EMOEquipmentSlot::MAX)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOEquipmentComponent] EquipFromInventory: Basic validation failed - Inventory=%s, GuidValid=%s, SlotValid=%s"),
			IsValid(Inventory) ? TEXT("valid") : TEXT("NULL"),
			ItemGuid.IsValid() ? TEXT("yes") : TEXT("no"),
			EquipSlot != EMOEquipmentSlot::MAX ? TEXT("yes") : TEXT("no"));
		return false;
	}

	EnsureSlotsInitialized();
	const int32 SlotIndex = static_cast<int32>(EquipSlot);

	// Get the item from inventory
	FMOInventoryEntry Entry;
	if (!Inventory->TryGetEntryByGuid(ItemGuid, Entry))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOEquipmentComponent] EquipFromInventory: Item not found in inventory: %s"), *ItemGuid.ToString());
		return false;
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOEquipmentComponent] EquipFromInventory: Found item '%s' in inventory"), *Entry.ItemDefinitionId.ToString());

	// Get item definition for swap timing and slot validation
	FMOItemDefinitionRow ItemDef;
	if (!UMOItemDatabaseSettings::GetItemDefinition(Entry.ItemDefinitionId, ItemDef))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOEquipmentComponent] EquipFromInventory: Item definition not found: %s"), *Entry.ItemDefinitionId.ToString());
		return false;
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOEquipmentComponent] EquipFromInventory: Item def found - bEquippable=%s, EquipmentSlotType=%d"),
		ItemDef.bEquippable ? TEXT("true") : TEXT("false"),
		static_cast<int32>(ItemDef.EquipmentSlotType));

	// Check if item can be equipped to this slot
	if (!CanEquipToSlot(Entry.ItemDefinitionId, EquipSlot))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOEquipmentComponent] EquipFromInventory: Item %s cannot be equipped to slot %d"), *Entry.ItemDefinitionId.ToString(), static_cast<int32>(EquipSlot));
		return false;
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOEquipmentComponent] EquipFromInventory: CanEquipToSlot passed"));

	// Check if slot is currently swapping (only for hand slots)
	if (IsHandSlot(EquipSlot) && IsSlotSwapping(EquipSlot))
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOEquipmentComponent] Slot is currently swapping, rejecting equip"));
		return false;
	}

	// If slot already has an item, return it to inventory first
	FMOEquippedItem& SlotRef = EquippedSlots[SlotIndex];
	if (SlotRef.IsValid())
	{
		// Return current item to inventory
		const FGuid NewGuid = FGuid::NewGuid();
		if (!Inventory->AddItemByGuidWithDurability(NewGuid, SlotRef.ItemDefinitionId, 1, SlotRef.CurrentDurability))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOEquipmentComponent] Failed to return current item to inventory"));
			return false;
		}
	}

	// Remove item from inventory (only 1 - equipment is single items)
	if (!Inventory->RemoveItemByGuid(ItemGuid, 1))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOEquipmentComponent] Failed to remove item from inventory"));
		return false;
	}

	// Build equipped item data
	FMOEquippedItem NewEquipped;
	NewEquipped.ItemGuid = FGuid::NewGuid(); // New GUID for equipped instance
	NewEquipped.ItemDefinitionId = Entry.ItemDefinitionId;
	NewEquipped.CurrentDurability = Entry.CurrentDurability;

	// Hand slots have swap delay, other slots are instant
	if (IsHandSlot(EquipSlot))
	{
		float SwapTime = GetSwapTimeForItem(Entry.ItemDefinitionId);

		if (SwapTime > 0.0f)
		{
			// Start swap with delay
			if (EquipSlot == EMOEquipmentSlot::LeftHand)
			{
				PendingLeftHandItem = NewEquipped;
				bHasPendingLeftHand = true;
			}
			else
			{
				PendingRightHandItem = NewEquipped;
				bHasPendingRightHand = true;
			}
			StartSwap(EquipSlot, SwapTime);
		}
		else
		{
			// Instant equip
			SlotRef = NewEquipped;
			OnEquipmentChanged.Broadcast(EquipSlot, SlotRef);
		}
	}
	else
	{
		// Non-hand slots are always instant
		SlotRef = NewEquipped;
		OnEquipmentChanged.Broadcast(EquipSlot, SlotRef);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOEquipmentComponent] Equipping %s to slot %d"),
		*Entry.ItemDefinitionId.ToString(), SlotIndex);

	return true;
}

bool UMOEquipmentComponent::UnequipToInventory(EMOEquipmentSlot EquipSlot, UMOInventoryComponent* Inventory)
{
	if (!IsValid(Inventory) || EquipSlot == EMOEquipmentSlot::MAX)
	{
		return false;
	}

	EnsureSlotsInitialized();
	const int32 SlotIndex = static_cast<int32>(EquipSlot);
	FMOEquippedItem& SlotRef = EquippedSlots[SlotIndex];

	if (!SlotRef.IsValid())
	{
		return false; // Nothing to unequip
	}

	// Add back to inventory
	const FGuid NewGuid = FGuid::NewGuid();
	if (!Inventory->AddItemByGuidWithDurability(NewGuid, SlotRef.ItemDefinitionId, 1, SlotRef.CurrentDurability))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOEquipmentComponent] Failed to add item back to inventory"));
		return false;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOEquipmentComponent] Unequipped %s from slot %d"),
		*SlotRef.ItemDefinitionId.ToString(), SlotIndex);

	// Clear the slot
	SlotRef.Clear();
	OnEquipmentChanged.Broadcast(EquipSlot, SlotRef);

	return true;
}

void UMOEquipmentComponent::SwapSlots(EMOEquipmentSlot SlotA, EMOEquipmentSlot SlotB)
{
	if (SlotA == SlotB || SlotA == EMOEquipmentSlot::MAX || SlotB == EMOEquipmentSlot::MAX)
	{
		return;
	}

	EnsureSlotsInitialized();
	const int32 IndexA = static_cast<int32>(SlotA);
	const int32 IndexB = static_cast<int32>(SlotB);

	FMOEquippedItem Temp = EquippedSlots[IndexA];
	EquippedSlots[IndexA] = EquippedSlots[IndexB];
	EquippedSlots[IndexB] = Temp;

	OnEquipmentChanged.Broadcast(SlotA, EquippedSlots[IndexA]);
	OnEquipmentChanged.Broadcast(SlotB, EquippedSlots[IndexB]);
}

bool UMOEquipmentComponent::IsSlotSwapping(EMOEquipmentSlot EquipSlot) const
{
	if (EquipSlot == EMOEquipmentSlot::LeftHand)
	{
		return LeftHandSwapTimeRemaining > 0.0f;
	}
	if (EquipSlot == EMOEquipmentSlot::RightHand)
	{
		return RightHandSwapTimeRemaining > 0.0f;
	}
	return false;
}

float UMOEquipmentComponent::GetSwapProgress(EMOEquipmentSlot EquipSlot) const
{
	if (EquipSlot == EMOEquipmentSlot::LeftHand)
	{
		if (LeftHandSwapTimeTotal <= 0.0f) return 1.0f;
		return 1.0f - FMath::Clamp(LeftHandSwapTimeRemaining / LeftHandSwapTimeTotal, 0.0f, 1.0f);
	}
	else if (EquipSlot == EMOEquipmentSlot::RightHand)
	{
		if (RightHandSwapTimeTotal <= 0.0f) return 1.0f;
		return 1.0f - FMath::Clamp(RightHandSwapTimeRemaining / RightHandSwapTimeTotal, 0.0f, 1.0f);
	}
	return 1.0f;
}

// ============================================================================
// ACCESSORS
// ============================================================================

FMOEquippedItem UMOEquipmentComponent::GetEquippedItem(EMOEquipmentSlot EquipSlot) const
{
	if (EquipSlot == EMOEquipmentSlot::MAX)
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
	FMOItemDefinitionRow ItemDef;
	if (!UMOItemDatabaseSettings::GetItemDefinition(ItemDefinitionId, ItemDef))
	{
		return false;
	}

	if (!ItemDef.bEquippable)
	{
		return false;
	}

	// Check if the item's equipment slot type matches the target slot
	switch (ItemDef.EquipmentSlotType)
	{
	case EMOEquipmentSlotType::None:
		return false;
	case EMOEquipmentSlotType::Head:
		return EquipSlot == EMOEquipmentSlot::Head;
	case EMOEquipmentSlotType::Chest:
		return EquipSlot == EMOEquipmentSlot::Chest;
	case EMOEquipmentSlotType::Hands:
		return EquipSlot == EMOEquipmentSlot::Hands;
	case EMOEquipmentSlotType::Legs:
		return EquipSlot == EMOEquipmentSlot::Legs;
	case EMOEquipmentSlotType::Feet:
		return EquipSlot == EMOEquipmentSlot::Feet;
	case EMOEquipmentSlotType::Back:
		return EquipSlot == EMOEquipmentSlot::Back;
	case EMOEquipmentSlotType::Held:
		return EquipSlot == EMOEquipmentSlot::LeftHand || EquipSlot == EMOEquipmentSlot::RightHand;
	default:
		return false;
	}
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

void UMOEquipmentComponent::StartSwap(EMOEquipmentSlot EquipSlot, float Duration)
{
	if (EquipSlot == EMOEquipmentSlot::LeftHand)
	{
		LeftHandSwapTimeRemaining = Duration;
		LeftHandSwapTimeTotal = Duration;
	}
	else if (EquipSlot == EMOEquipmentSlot::RightHand)
	{
		RightHandSwapTimeRemaining = Duration;
		RightHandSwapTimeTotal = Duration;
	}
	else
	{
		return; // Only hand slots have swap delays
	}

	SetComponentTickEnabled(true);
	OnSwapStarted.Broadcast(EquipSlot);
}

void UMOEquipmentComponent::CompleteSwap(EMOEquipmentSlot EquipSlot)
{
	EnsureSlotsInitialized();

	if (EquipSlot == EMOEquipmentSlot::LeftHand)
	{
		LeftHandSwapTimeRemaining = 0.0f;
		if (bHasPendingLeftHand)
		{
			EquippedSlots[static_cast<int32>(EMOEquipmentSlot::LeftHand)] = PendingLeftHandItem;
			bHasPendingLeftHand = false;
			OnEquipmentChanged.Broadcast(EquipSlot, EquippedSlots[static_cast<int32>(EMOEquipmentSlot::LeftHand)]);
		}
	}
	else if (EquipSlot == EMOEquipmentSlot::RightHand)
	{
		RightHandSwapTimeRemaining = 0.0f;
		if (bHasPendingRightHand)
		{
			EquippedSlots[static_cast<int32>(EMOEquipmentSlot::RightHand)] = PendingRightHandItem;
			bHasPendingRightHand = false;
			OnEquipmentChanged.Broadcast(EquipSlot, EquippedSlots[static_cast<int32>(EMOEquipmentSlot::RightHand)]);
		}
	}

	OnSwapCompleted.Broadcast(EquipSlot);

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOEquipmentComponent] Swap completed for slot %d"),
		static_cast<int32>(EquipSlot));
}

// ============================================================================
// SAVE/LOAD
// ============================================================================

void UMOEquipmentComponent::BuildSaveData(FMOEquipmentSaveData& OutData) const
{
	OutData.Slots.Reset();
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
	EnsureSlotsInitialized();

	// Clear all slots first
	for (FMOEquippedItem& Item : EquippedSlots)
	{
		Item.Clear();
	}

	// Apply saved data
	for (const FMOEquipmentSlotSaveData& SlotData : InData.Slots)
	{
		if (SlotData.Slot != EMOEquipmentSlot::MAX)
		{
			const int32 SlotIndex = static_cast<int32>(SlotData.Slot);
			if (SlotIndex < EquippedSlots.Num())
			{
				EquippedSlots[SlotIndex] = SlotData.EquippedItem;
				OnEquipmentChanged.Broadcast(SlotData.Slot, EquippedSlots[SlotIndex]);
			}
		}
	}
}

// ============================================================================
// REPLICATION
// ============================================================================

void UMOEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMOEquipmentComponent, EquippedSlots);
}
