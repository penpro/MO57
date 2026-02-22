#include "MONearbyItemsPanel.h"
#include "MOFramework.h"
#include "MOInventoryGrid.h"
#include "MOInventorySlot.h"
#include "MOInventoryComponent.h"
#include "MOWorldItem.h"
#include "MOItemComponent.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"

UMONearbyItemsPanel::UMONearbyItemsPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMONearbyItemsPanel::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind grid events
	if (NearbyGrid)
	{
		NearbyGrid->OnGridSlotClicked.RemoveDynamic(this, &UMONearbyItemsPanel::HandleSlotClicked);
		NearbyGrid->OnGridSlotClicked.AddDynamic(this, &UMONearbyItemsPanel::HandleSlotClicked);

		NearbyGrid->OnGridSlotShiftClicked.RemoveDynamic(this, &UMONearbyItemsPanel::HandleSlotShiftClicked);
		NearbyGrid->OnGridSlotShiftClicked.AddDynamic(this, &UMONearbyItemsPanel::HandleSlotShiftClicked);

		NearbyGrid->OnGridSlotRightClicked.RemoveDynamic(this, &UMONearbyItemsPanel::HandleSlotRightClicked);
		NearbyGrid->OnGridSlotRightClicked.AddDynamic(this, &UMONearbyItemsPanel::HandleSlotRightClicked);

		NearbyGrid->OnGridSlotDropReceived.RemoveDynamic(this, &UMONearbyItemsPanel::HandleSlotDropReceived);
		NearbyGrid->OnGridSlotDropReceived.AddDynamic(this, &UMONearbyItemsPanel::HandleSlotDropReceived);
	}

	UpdateEmptyState();
}

void UMONearbyItemsPanel::NativeDestruct()
{
	if (NearbyGrid)
	{
		NearbyGrid->OnGridSlotClicked.RemoveDynamic(this, &UMONearbyItemsPanel::HandleSlotClicked);
		NearbyGrid->OnGridSlotShiftClicked.RemoveDynamic(this, &UMONearbyItemsPanel::HandleSlotShiftClicked);
		NearbyGrid->OnGridSlotRightClicked.RemoveDynamic(this, &UMONearbyItemsPanel::HandleSlotRightClicked);
		NearbyGrid->OnGridSlotDropReceived.RemoveDynamic(this, &UMONearbyItemsPanel::HandleSlotDropReceived);
	}

	Super::NativeDestruct();
}

void UMONearbyItemsPanel::SetQuickPickupTarget(UMOInventoryComponent* TargetInventory)
{
	QuickPickupTargetInventory = TargetInventory;
}

// ============================================================================
// NEARBY ITEMS MANAGEMENT
// ============================================================================

void UMONearbyItemsPanel::RefreshNearbyItems(const TArray<AMOWorldItem*>& NearbyItems)
{
	UE_LOG(LogMOFramework, Log, TEXT("[NearbyItemsPanel] RefreshNearbyItems called with %d items"), NearbyItems.Num());

	CachedNearbyItems.Empty();

	for (AMOWorldItem* WorldItem : NearbyItems)
	{
		if (IsValid(WorldItem))
		{
			CachedNearbyItems.Add(WorldItem);
			UE_LOG(LogMOFramework, Log, TEXT("[NearbyItemsPanel] Added world item: %s"), *WorldItem->GetName());
		}
	}

	// Update the grid display
	// Note: Since the grid is designed for UMOInventoryComponent, we need to
	// manually update slots with world item data. For now, we'll use a simplified
	// approach that creates visual slot data without a backing inventory.
	if (NearbyGrid)
	{
		// Build visual data for each world item
		TArray<FMOInventorySlotVisualData> VisualData;
		for (int32 i = 0; i < CachedNearbyItems.Num(); ++i)
		{
			AMOWorldItem* WorldItem = CachedNearbyItems[i].Get();
			if (!WorldItem)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[NearbyItemsPanel] WorldItem at index %d is invalid"), i);
				continue;
			}

			UMOItemComponent* ItemComp = WorldItem->GetItemComponent();
			if (!ItemComp)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[NearbyItemsPanel] WorldItem %s has no ItemComponent"), *WorldItem->GetName());
				continue;
			}

			FMOInventorySlotVisualData SlotData;
			SlotData.bHasItem = true;
			SlotData.ItemGuid = FGuid::NewGuid();  // Temporary GUID for display
			SlotData.ItemDefinitionId = ItemComp->ItemDefinitionId;
			SlotData.Quantity = ItemComp->Quantity;
			SlotData.SourceWorldItem = WorldItem;  // Store reference for drag operations
			VisualData.Add(SlotData);

			UE_LOG(LogMOFramework, Log, TEXT("[NearbyItemsPanel] Created visual data for %s (ID=%s, Qty=%d)"),
				*WorldItem->GetName(), *ItemComp->ItemDefinitionId.ToString(), ItemComp->Quantity);
		}

		UE_LOG(LogMOFramework, Log, TEXT("[NearbyItemsPanel] Calling SetSlotVisualData with %d entries"), VisualData.Num());
		NearbyGrid->SetSlotVisualData(VisualData);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[NearbyItemsPanel] NearbyGrid is NULL!"));
	}

	UpdateEmptyState();

	UE_LOG(LogMOFramework, Log, TEXT("[NearbyItemsPanel] Refresh complete. CachedNearbyItems=%d"), CachedNearbyItems.Num());
}

void UMONearbyItemsPanel::ClearNearbyItems()
{
	CachedNearbyItems.Empty();

	if (NearbyGrid)
	{
		NearbyGrid->ClearGrid();
	}

	UpdateEmptyState();
}

int32 UMONearbyItemsPanel::GetNearbyItemCount() const
{
	return CachedNearbyItems.Num();
}

// ============================================================================
// PICKUP OPERATIONS
// ============================================================================

bool UMONearbyItemsPanel::PickupItem(AMOWorldItem* WorldItem, UMOInventoryComponent* TargetInventory)
{
	if (!IsValid(WorldItem) || !IsValid(TargetInventory))
	{
		return false;
	}

	UMOItemComponent* ItemComp = WorldItem->GetItemComponent();
	if (!ItemComp)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[NearbyItemsPanel] PickupItem: WorldItem has no ItemComponent"));
		return false;
	}

	// Try to add item to inventory
	FGuid NewItemGuid = FGuid::NewGuid();
	bool bSuccess = TargetInventory->AddItemByGuid(NewItemGuid, ItemComp->ItemDefinitionId, ItemComp->Quantity);

	if (bSuccess)
	{
		// Remove from cached list
		CachedNearbyItems.Remove(WorldItem);

		// Broadcast pickup event
		OnItemPickedUp.Broadcast(WorldItem, TargetInventory);

		// Destroy or hide the world item
		if (WorldItem->bDestroyAfterPickup)
		{
			WorldItem->Destroy();
		}
		else
		{
			WorldItem->SetActorHiddenInGame(true);
			WorldItem->SetActorEnableCollision(false);
		}

		UpdateEmptyState();

		UE_LOG(LogMOFramework, Log, TEXT("[NearbyItemsPanel] Picked up %s x%d"),
			*ItemComp->ItemDefinitionId.ToString(), ItemComp->Quantity);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[NearbyItemsPanel] Failed to add item to inventory (full?)"));
	}

	return bSuccess;
}

int32 UMONearbyItemsPanel::LootAllToInventory(UMOInventoryComponent* TargetInventory)
{
	if (!IsValid(TargetInventory))
	{
		return 0;
	}

	int32 PickedUpCount = 0;

	// Copy the array since we'll be modifying it during iteration
	TArray<TWeakObjectPtr<AMOWorldItem>> ItemsToLoot = CachedNearbyItems;

	for (const TWeakObjectPtr<AMOWorldItem>& WeakItem : ItemsToLoot)
	{
		AMOWorldItem* WorldItem = WeakItem.Get();
		if (WorldItem && PickupItem(WorldItem, TargetInventory))
		{
			++PickedUpCount;
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[NearbyItemsPanel] Looted %d items"), PickedUpCount);

	return PickedUpCount;
}

// ============================================================================
// INTERNAL
// ============================================================================

void UMONearbyItemsPanel::UpdateEmptyState()
{
	const bool bIsEmpty = CachedNearbyItems.Num() == 0;

	UE_LOG(LogMOFramework, Log, TEXT("[NearbyItemsPanel] UpdateEmptyState: bIsEmpty=%s, CachedItems=%d"),
		bIsEmpty ? TEXT("true") : TEXT("false"), CachedNearbyItems.Num());

	if (EmptyMessageText)
	{
		EmptyMessageText->SetVisibility(bIsEmpty ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		UE_LOG(LogMOFramework, Log, TEXT("[NearbyItemsPanel] EmptyMessageText visibility set to %s"),
			bIsEmpty ? TEXT("Visible") : TEXT("Collapsed"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[NearbyItemsPanel] EmptyMessageText is NULL"));
	}

	if (NearbyGrid)
	{
		NearbyGrid->SetVisibility(bIsEmpty ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		UE_LOG(LogMOFramework, Log, TEXT("[NearbyItemsPanel] NearbyGrid visibility set to %s"),
			bIsEmpty ? TEXT("Collapsed") : TEXT("Visible"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[NearbyItemsPanel] NearbyGrid is NULL"));
	}
}

void UMONearbyItemsPanel::HandleSlotClicked(int32 SlotIndex, const FGuid& ItemGuid)
{
	if (SlotIndex >= 0 && SlotIndex < CachedNearbyItems.Num())
	{
		AMOWorldItem* WorldItem = CachedNearbyItems[SlotIndex].Get();
		if (WorldItem)
		{
			OnItemSelected.Broadcast(WorldItem);
		}
	}
}

void UMONearbyItemsPanel::HandleSlotShiftClicked(int32 SlotIndex, const FGuid& ItemGuid)
{
	// Shift+Click or Double-Click on nearby item = quick pickup to target inventory
	if (SlotIndex >= 0 && SlotIndex < CachedNearbyItems.Num())
	{
		AMOWorldItem* WorldItem = CachedNearbyItems[SlotIndex].Get();
		UMOInventoryComponent* TargetInv = QuickPickupTargetInventory.Get();

		if (WorldItem && TargetInv)
		{
			if (PickupItem(WorldItem, TargetInv))
			{
				UE_LOG(LogMOFramework, Log, TEXT("[NearbyItemsPanel] Shift+Click: Picked up item to target inventory"));

				// Refresh the grid display
				TArray<AMOWorldItem*> RemainingItems;
				for (const TWeakObjectPtr<AMOWorldItem>& WeakItem : CachedNearbyItems)
				{
					if (AMOWorldItem* Item = WeakItem.Get())
					{
						RemainingItems.Add(Item);
					}
				}
				RefreshNearbyItems(RemainingItems);
			}
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[NearbyItemsPanel] Shift+Click: No target inventory set or world item invalid"));
		}
	}
}

void UMONearbyItemsPanel::HandleSlotRightClicked(int32 SlotIndex, const FGuid& ItemGuid, FVector2D ScreenPosition)
{
	// For now, right-click on nearby item does the same as left-click (select)
	// Could add a context menu later for "Pickup", "Inspect", etc.
	HandleSlotClicked(SlotIndex, ItemGuid);
}

void UMONearbyItemsPanel::HandleSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory)
{
	UE_LOG(LogMOFramework, Verbose, TEXT("[NearbyItemsPanel] HandleSlotDropReceived: slot %d from slot %d"), TargetSlotIndex, SourceSlotIndex);

	// When an item is dropped on the nearby panel, it gets dropped to the world.
	// Broadcast event so parent can trigger a refresh of nearby items.
	OnNearbyItemsChanged.Broadcast();
}
