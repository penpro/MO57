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
						*static_cast<FGuid*>(Dest) = ItemGuid;
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
	UE_LOG(LogMOFramework, Error, TEXT(">>>>> [MOInventory] DropItemFromSlot CALLED: Slot=%d, Location=%s <<<<<"),
		SlotIndex, *DropLocation.ToString());
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, FString::Printf(TEXT("DropItemFromSlot slot=%d loc=%s"), SlotIndex, *DropLocation.ToString()));
	}

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

	UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] DropItemFromSlot: Calling DropItemByGuid with GUID=%s"),
		*ItemGuid.ToString(EGuidFormats::Short));

	return DropItemByGuid(ItemGuid, DropLocation, DropRotation);
}

AActor* UMOInventoryComponent::DropItemByGuid(const FGuid& ItemGuid, const FVector& DropLocation, const FRotator& DropRotation)
{
	UE_LOG(LogMOFramework, Error, TEXT(">>>>> [MOInventory] DropItemByGuid CALLED Location=%s <<<<<"), *DropLocation.ToString());
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Magenta, FString::Printf(TEXT("DropItemByGuid loc=%s"), *DropLocation.ToString()));
	}

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
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] DropItemByGuid: no drop actor class for ItemDefinitionId=%s"), *Entry.ItemDefinitionId.ToString());
		return nullptr;
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] DropItemByGuid: Spawning %s for ItemDefinitionId=%s at Location=%s"),
		*DropActorClass->GetName(), *Entry.ItemDefinitionId.ToString(), *DropLocation.ToString());

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
				UE_LOG(LogMOFramework, Log, TEXT("[MOInventory] DropItemByGuid: Clearing GUID %s from destroyed list"), *Entry.ItemGuid.ToString(EGuidFormats::Short));
				PersistenceSubsystem->ClearDestroyedGuid(Entry.ItemGuid);
			}
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerActor;
	SpawnParams.Instigator = OwnerActor->GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	const FTransform SpawnTransform(DropRotation, DropLocation);
	UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] DropItemByGuid: SpawnTransform Location=%s, Rotation=%s"),
		*SpawnTransform.GetLocation().ToString(), *SpawnTransform.GetRotation().Rotator().ToString());

	AActor* SpawnedActor = World->SpawnActor<AActor>(DropActorClass, SpawnTransform, SpawnParams);
	if (!IsValid(SpawnedActor))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] DropItemByGuid: spawn failed for ItemDefinitionId=%s"), *Entry.ItemDefinitionId.ToString());
		return nullptr;
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOInventory] DropItemByGuid: Actor spawned! Name=%s, ActualLocation=%s"),
		*SpawnedActor->GetName(), *SpawnedActor->GetActorLocation().ToString());

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

		UE_LOG(LogMOFramework, Log, TEXT("[MOInventory] DropItemByGuid: Applied item definition visuals, restored location to %s"),
			*PreservedLocation.ToString());
	}

	// Remove the entire stack so the same GUID does not exist in two places.
	RemoveItemByGuid(ItemGuid, Entry.Quantity);

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventory] DropItemByGuid: Successfully dropped item GUID=%s"), *ItemGuid.ToString(EGuidFormats::Short));

	return SpawnedActor;
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
