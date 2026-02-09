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

bool UMOEquipmentComponent::EquipFromInventory(UMOInventoryComponent* Inventory, const FGuid& ItemGuid, EMOEquipmentSlot Slot)
{
	if (!IsValid(Inventory) || !ItemGuid.IsValid())
	{
		return false;
	}

	// Get the item from inventory
	FMOInventoryEntry Entry;
	if (!Inventory->TryGetEntryByGuid(ItemGuid, Entry))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOEquipmentComponent] Item not found in inventory: %s"), *ItemGuid.ToString());
		return false;
	}

	// Get item definition for swap timing
	FMOItemDefinitionRow ItemDef;
	if (!UMOItemDatabaseSettings::GetItemDefinition(Entry.ItemDefinitionId, ItemDef))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOEquipmentComponent] Item definition not found: %s"), *Entry.ItemDefinitionId.ToString());
		return false;
	}

	// Check if slot is currently swapping
	if (IsSlotSwapping(Slot))
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOEquipmentComponent] Slot is currently swapping, queuing equip"));
		// Could queue here, for now just reject
		return false;
	}

	// If slot already has an item, return it to inventory first
	FMOEquippedItem& SlotRef = GetSlotRef(Slot);
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

	// Calculate swap time
	float SwapTime = GetSwapTimeForItem(Entry.ItemDefinitionId);

	if (SwapTime > 0.0f)
	{
		// Start swap with delay
		if (Slot == EMOEquipmentSlot::LeftHand)
		{
			PendingLeftHandItem = NewEquipped;
			bHasPendingLeftHand = true;
		}
		else
		{
			PendingRightHandItem = NewEquipped;
			bHasPendingRightHand = true;
		}
		StartSwap(Slot, SwapTime);
	}
	else
	{
		// Instant equip
		SlotRef = NewEquipped;
		OnEquipmentChanged.Broadcast(Slot, SlotRef);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOEquipmentComponent] Equipping %s to %s (swap time: %.2fs)"),
		*Entry.ItemDefinitionId.ToString(),
		Slot == EMOEquipmentSlot::LeftHand ? TEXT("LeftHand") : TEXT("RightHand"),
		SwapTime);

	return true;
}

bool UMOEquipmentComponent::UnequipToInventory(EMOEquipmentSlot Slot, UMOInventoryComponent* Inventory)
{
	if (!IsValid(Inventory))
	{
		return false;
	}

	FMOEquippedItem& SlotRef = GetSlotRef(Slot);
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

	UE_LOG(LogMOFramework, Log, TEXT("[MOEquipmentComponent] Unequipped %s from %s"),
		*SlotRef.ItemDefinitionId.ToString(),
		Slot == EMOEquipmentSlot::LeftHand ? TEXT("LeftHand") : TEXT("RightHand"));

	// Clear the slot
	SlotRef.Clear();
	OnEquipmentChanged.Broadcast(Slot, SlotRef);

	return true;
}

void UMOEquipmentComponent::SwapSlots(EMOEquipmentSlot SlotA, EMOEquipmentSlot SlotB)
{
	if (SlotA == SlotB)
	{
		return;
	}

	FMOEquippedItem Temp = GetSlotRef(SlotA);
	GetSlotRef(SlotA) = GetSlotRef(SlotB);
	GetSlotRef(SlotB) = Temp;

	OnEquipmentChanged.Broadcast(SlotA, GetSlotRef(SlotA));
	OnEquipmentChanged.Broadcast(SlotB, GetSlotRef(SlotB));
}

bool UMOEquipmentComponent::IsSlotSwapping(EMOEquipmentSlot Slot) const
{
	if (Slot == EMOEquipmentSlot::LeftHand)
	{
		return LeftHandSwapTimeRemaining > 0.0f;
	}
	return RightHandSwapTimeRemaining > 0.0f;
}

float UMOEquipmentComponent::GetSwapProgress(EMOEquipmentSlot Slot) const
{
	if (Slot == EMOEquipmentSlot::LeftHand)
	{
		if (LeftHandSwapTimeTotal <= 0.0f) return 1.0f;
		return 1.0f - FMath::Clamp(LeftHandSwapTimeRemaining / LeftHandSwapTimeTotal, 0.0f, 1.0f);
	}
	else
	{
		if (RightHandSwapTimeTotal <= 0.0f) return 1.0f;
		return 1.0f - FMath::Clamp(RightHandSwapTimeRemaining / RightHandSwapTimeTotal, 0.0f, 1.0f);
	}
}

// ============================================================================
// ACCESSORS
// ============================================================================

FMOEquippedItem UMOEquipmentComponent::GetEquippedItem(EMOEquipmentSlot Slot) const
{
	return GetSlotRef(Slot);
}

bool UMOEquipmentComponent::HasItemInSlot(EMOEquipmentSlot Slot) const
{
	return GetSlotRef(Slot).IsValid();
}

// ============================================================================
// INTERNAL
// ============================================================================

FMOEquippedItem& UMOEquipmentComponent::GetSlotRef(EMOEquipmentSlot Slot)
{
	return (Slot == EMOEquipmentSlot::LeftHand) ? LeftHandSlot : RightHandSlot;
}

const FMOEquippedItem& UMOEquipmentComponent::GetSlotRef(EMOEquipmentSlot Slot) const
{
	return (Slot == EMOEquipmentSlot::LeftHand) ? LeftHandSlot : RightHandSlot;
}

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

void UMOEquipmentComponent::StartSwap(EMOEquipmentSlot Slot, float Duration)
{
	if (Slot == EMOEquipmentSlot::LeftHand)
	{
		LeftHandSwapTimeRemaining = Duration;
		LeftHandSwapTimeTotal = Duration;
	}
	else
	{
		RightHandSwapTimeRemaining = Duration;
		RightHandSwapTimeTotal = Duration;
	}

	SetComponentTickEnabled(true);
	OnSwapStarted.Broadcast(Slot);
}

void UMOEquipmentComponent::CompleteSwap(EMOEquipmentSlot Slot)
{
	if (Slot == EMOEquipmentSlot::LeftHand)
	{
		LeftHandSwapTimeRemaining = 0.0f;
		if (bHasPendingLeftHand)
		{
			LeftHandSlot = PendingLeftHandItem;
			bHasPendingLeftHand = false;
			OnEquipmentChanged.Broadcast(Slot, LeftHandSlot);
		}
	}
	else
	{
		RightHandSwapTimeRemaining = 0.0f;
		if (bHasPendingRightHand)
		{
			RightHandSlot = PendingRightHandItem;
			bHasPendingRightHand = false;
			OnEquipmentChanged.Broadcast(Slot, RightHandSlot);
		}
	}

	OnSwapCompleted.Broadcast(Slot);

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOEquipmentComponent] Swap completed for %s"),
		Slot == EMOEquipmentSlot::LeftHand ? TEXT("LeftHand") : TEXT("RightHand"));
}

// ============================================================================
// SAVE/LOAD
// ============================================================================

void UMOEquipmentComponent::BuildSaveData(FMOEquipmentSaveData& OutData) const
{
	OutData.LeftHand = LeftHandSlot;
	OutData.RightHand = RightHandSlot;
}

void UMOEquipmentComponent::ApplySaveData(const FMOEquipmentSaveData& InData)
{
	LeftHandSlot = InData.LeftHand;
	RightHandSlot = InData.RightHand;

	// Broadcast changes
	OnEquipmentChanged.Broadcast(EMOEquipmentSlot::LeftHand, LeftHandSlot);
	OnEquipmentChanged.Broadcast(EMOEquipmentSlot::RightHand, RightHandSlot);
}

// ============================================================================
// REPLICATION
// ============================================================================

void UMOEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMOEquipmentComponent, LeftHandSlot);
	DOREPLIFETIME(UMOEquipmentComponent, RightHandSlot);
}
