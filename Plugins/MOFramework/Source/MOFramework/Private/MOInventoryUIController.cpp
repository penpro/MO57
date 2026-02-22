#include "MOInventoryUIController.h"
#include "MOUIManagerComponent.h"
#include "MOInventoryMenu.h"
#include "MOUnifiedInventoryMenu.h"
#include "MOItemContextMenu.h"
#include "MOInventoryComponent.h"
#include "MOWorldItem.h"
#include "MOItemComponent.h"
#include "MOSurvivalStatsComponent.h"
#include "MOInventoryHolderInterface.h"
#include "MOIdentifiableInterface.h"
#include "MOFramework.h"
#include "EngineUtils.h"
#include "Blueprint/UserWidget.h"

UMOInventoryUIController::UMOInventoryUIController()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMOInventoryUIController::BeginPlay()
{
	Super::BeginPlay();
}

void UMOInventoryUIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clean up widgets - unbind delegates first to prevent callbacks during destruction
	if (UMOInventoryMenu* InvMenu = InventoryMenuWidget.Get())
	{
		InvMenu->OnRequestClose.RemoveDynamic(this, &UMOInventoryUIController::HandleInventoryMenuRequestClose);
		InvMenu->OnSlotRightClicked.RemoveDynamic(this, &UMOInventoryUIController::HandleInventoryMenuSlotRightClicked);
		InvMenu->RemoveFromParent();
	}
	InventoryMenuWidget.Reset();

	if (UMOUnifiedInventoryMenu* UnifiedMenu = UnifiedInventoryWidget.Get())
	{
		UnifiedMenu->OnRequestClose.RemoveDynamic(this, &UMOInventoryUIController::HandleUnifiedInventoryMenuRequestClose);
		UnifiedMenu->OnContextMenuRequested.RemoveDynamic(this, &UMOInventoryUIController::HandleUnifiedInventoryMenuContextMenuRequested);
		UnifiedMenu->OnWorldItemContextMenuRequested.RemoveDynamic(this, &UMOInventoryUIController::HandleUnifiedInventoryMenuWorldItemContextMenuRequested);
		UnifiedMenu->RemoveFromParent();
	}
	UnifiedInventoryWidget.Reset();

	if (UMOItemContextMenu* CtxMenu = ItemContextMenuWidget.Get())
	{
		CtxMenu->OnMenuClosed.RemoveDynamic(this, &UMOInventoryUIController::HandleContextMenuClosed);
		CtxMenu->OnActionSelected.RemoveDynamic(this, &UMOInventoryUIController::HandleContextMenuAction);
		CtxMenu->RemoveFromParent();
	}
	ItemContextMenuWidget.Reset();

	CurrentContainerActor.Reset();

	Super::EndPlay(EndPlayReason);
}

// =============================================================================
// Inventory Menu
// =============================================================================

bool UMOInventoryUIController::IsInventoryMenuOpen() const
{
	// Check unified inventory menu first
	const UMOUnifiedInventoryMenu* UnifiedMenu = UnifiedInventoryWidget.Get();
	if (IsValid(UnifiedMenu) && UnifiedMenu->IsInViewport())
	{
		return true;
	}

	// Fall back to legacy inventory menu
	const UMOInventoryMenu* MenuWidget = InventoryMenuWidget.Get();
	return IsValid(MenuWidget) && MenuWidget->IsInViewport();
}

void UMOInventoryUIController::ToggleInventoryMenu()
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	UMOUIManagerComponent* UIManager = GetUIManager();
	if (!UIManager)
	{
		return;
	}

	// Don't allow opening while in-game menu is open
	if (UIManager->IsInGameMenuOpen())
	{
		return;
	}

	// If already open, just close it
	if (IsInventoryMenuOpen())
	{
		CloseInventoryMenu();
		return;
	}

	// Close other switchable menus and open this one
	UIManager->CloseAllSwitchableMenus();
	OpenInventoryMenu();
}

void UMOInventoryUIController::OpenInventoryMenu()
{
	// If unified inventory menu is configured, use it instead
	if (UnifiedInventoryMenuClass)
	{
		// Open unified menu with current active container (may be null)
		OpenInventoryWithContainer(GetActiveContainer());
		return;
	}

	// Legacy: Fall back to old inventory menu if unified menu class not set
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	// Check for valid pawn first
	if (!HasValidPawn())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] OpenInventoryMenu - No valid pawn, showing notification"));
		ShowNoPawnNotification();
		return;
	}

	if (!InventoryMenuClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] InventoryMenuClass not set on controller."));
		return;
	}

	UMOInventoryComponent* InventoryComponent = GetCachedInventory();
	if (!IsValid(InventoryComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] No UMOInventoryComponent found on current pawn."));
		return;
	}

	UMOInventoryMenu* MenuWidget = InventoryMenuWidget.Get();
	if (!IsValid(MenuWidget))
	{
		MenuWidget = CreateWidget<UMOInventoryMenu>(PlayerController, InventoryMenuClass);
		InventoryMenuWidget = MenuWidget;

		if (!IsValid(MenuWidget))
		{
			return;
		}

		// Bind Tab close (widget broadcasts, manager closes).
		MenuWidget->OnRequestClose.AddDynamic(this, &UMOInventoryUIController::HandleInventoryMenuRequestClose);

		// Bind right-click for context menu
		MenuWidget->OnSlotRightClicked.AddDynamic(this, &UMOInventoryUIController::HandleInventoryMenuSlotRightClicked);
	}

	// Always re-initialize on open in case pawn changed.
	MenuWidget->InitializeMenu(InventoryComponent);

	if (!MenuWidget->IsInViewport())
	{
		ShowModalBackground();
		MenuWidget->AddToViewport(InventoryMenuZOrder);
	}

	UpdateReticleVisibility();
	ApplyInputModeForMenuOpen(MenuWidget);
}

void UMOInventoryUIController::CloseInventoryMenu()
{
	APlayerController* PlayerController = ResolveOwningPlayerController();

	UMOInventoryMenu* MenuWidget = InventoryMenuWidget.Get();
	if (IsValid(MenuWidget))
	{
		if (MenuWidget->IsInViewport())
		{
			MenuWidget->RemoveFromParent();
		}
	}

	// Also close unified inventory menu if open
	UMOUnifiedInventoryMenu* UnifiedMenu = UnifiedInventoryWidget.Get();
	if (IsValid(UnifiedMenu) && UnifiedMenu->IsInViewport())
	{
		UnifiedMenu->RemoveFromParent();
	}

	UpdateReticleVisibility();

	UMOUIManagerComponent* UIManager = GetUIManager();
	if (UIManager && !UIManager->IsAnyMenuOpen())
	{
		HideModalBackground();
		if (IsValid(PlayerController) && PlayerController->IsLocalController())
		{
			ApplyInputModeForMenuClosed();
		}
	}
}

// =============================================================================
// Unified Inventory Menu (Container Support)
// =============================================================================

void UMOInventoryUIController::OpenInventoryWithContainer(AActor* ContainerActor)
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	// Check for valid pawn first
	if (!HasValidPawn())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] OpenInventoryWithContainer - No valid pawn, showing notification"));
		ShowNoPawnNotification();
		return;
	}

	if (!UnifiedInventoryMenuClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] UnifiedInventoryMenuClass not set, falling back to regular inventory menu."));
		// Fall back to regular inventory menu
		SetActiveContainer(ContainerActor);
		OpenInventoryMenu();
		return;
	}

	UMOInventoryComponent* InventoryComponent = GetCachedInventory();
	if (!IsValid(InventoryComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] No UMOInventoryComponent found on current pawn."));
		return;
	}

	// Close other switchable menus
	UMOUIManagerComponent* UIManager = GetUIManager();
	if (UIManager)
	{
		UIManager->CloseAllSwitchableMenus();
	}

	// Set the active container
	CurrentContainerActor = ContainerActor;

	// Create or get the unified inventory widget
	UMOUnifiedInventoryMenu* MenuWidget = UnifiedInventoryWidget.Get();
	if (!IsValid(MenuWidget))
	{
		MenuWidget = CreateWidget<UMOUnifiedInventoryMenu>(PlayerController, UnifiedInventoryMenuClass);
		UnifiedInventoryWidget = MenuWidget;

		if (!IsValid(MenuWidget))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] Failed to create unified inventory menu widget."));
			return;
		}

		// Bind delegates
		MenuWidget->OnRequestClose.RemoveAll(this);
		MenuWidget->OnRequestClose.AddDynamic(this, &UMOInventoryUIController::HandleUnifiedInventoryMenuRequestClose);

		MenuWidget->OnContextMenuRequested.RemoveAll(this);
		MenuWidget->OnContextMenuRequested.AddDynamic(this, &UMOInventoryUIController::HandleUnifiedInventoryMenuContextMenuRequested);

		MenuWidget->OnWorldItemContextMenuRequested.RemoveAll(this);
		MenuWidget->OnWorldItemContextMenuRequested.AddDynamic(this, &UMOInventoryUIController::HandleUnifiedInventoryMenuWorldItemContextMenuRequested);
	}

	// Initialize with player inventory and container
	MenuWidget->InitializeMenu(InventoryComponent, ContainerActor);

	// If container is provided, show the Other Inventory tab
	if (IsValid(ContainerActor))
	{
		MenuWidget->ShowOtherInventoryTab();
	}

	if (!MenuWidget->IsInViewport())
	{
		ShowModalBackground();
		MenuWidget->AddToViewport(UnifiedInventoryMenuZOrder);
	}

	UpdateReticleVisibility();
	ApplyInputModeForMenuOpen(MenuWidget);

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] Unified inventory menu opened with container: %s"),
		IsValid(ContainerActor) ? *ContainerActor->GetName() : TEXT("None"));
}

void UMOInventoryUIController::SetActiveContainer(AActor* ContainerActor)
{
	CurrentContainerActor = ContainerActor;

	// If unified menu is open, update it
	UMOUnifiedInventoryMenu* MenuWidget = UnifiedInventoryWidget.Get();
	if (IsValid(MenuWidget) && MenuWidget->IsInViewport())
	{
		MenuWidget->SetSecondaryInventorySource(ContainerActor);
		if (IsValid(ContainerActor))
		{
			MenuWidget->ShowOtherInventoryTab();
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] Active container set to: %s"),
		IsValid(ContainerActor) ? *ContainerActor->GetName() : TEXT("None"));
}

void UMOInventoryUIController::ClearActiveContainer()
{
	CurrentContainerActor.Reset();

	// If unified menu is open, clear its container
	UMOUnifiedInventoryMenu* MenuWidget = UnifiedInventoryWidget.Get();
	if (IsValid(MenuWidget) && MenuWidget->IsInViewport())
	{
		MenuWidget->ClearSecondaryInventorySource();
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] Active container cleared"));
}

AActor* UMOInventoryUIController::GetActiveContainer() const
{
	return CurrentContainerActor.Get();
}

bool UMOInventoryUIController::HasActiveContainer() const
{
	return CurrentContainerActor.IsValid();
}

void UMOInventoryUIController::HandleUnifiedInventoryMenuRequestClose()
{
	UMOUnifiedInventoryMenu* MenuWidget = UnifiedInventoryWidget.Get();
	if (IsValid(MenuWidget) && MenuWidget->IsInViewport())
	{
		MenuWidget->RemoveFromParent();
	}

	UpdateReticleVisibility();

	UMOUIManagerComponent* UIManager = GetUIManager();
	if (UIManager && !UIManager->IsAnyMenuOpen())
	{
		HideModalBackground();
		APlayerController* PlayerController = ResolveOwningPlayerController();
		if (IsValid(PlayerController) && PlayerController->IsLocalController())
		{
			ApplyInputModeForMenuClosed();
		}
	}
}

void UMOInventoryUIController::HandleUnifiedInventoryMenuContextMenuRequested(UMOInventoryComponent* InventoryComponent, const FGuid& ItemGuid, int32 SlotIndex, FVector2D ScreenPosition)
{
	// Show the context menu for the item
	ShowItemContextMenu(InventoryComponent, ItemGuid, SlotIndex, ScreenPosition);
}

void UMOInventoryUIController::HandleUnifiedInventoryMenuWorldItemContextMenuRequested(AMOWorldItem* WorldItem, FVector2D ScreenPosition)
{
	// Show the context menu for the world item
	ShowWorldItemContextMenu(WorldItem, ScreenPosition);
}

// =============================================================================
// Nearby World Items
// =============================================================================

TArray<AMOWorldItem*> UMOInventoryUIController::QueryNearbyWorldItems() const
{
	TArray<AMOWorldItem*> Result;

	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return Result;
	}

	APawn* Pawn = PlayerController->GetPawn();
	if (!IsValid(Pawn))
	{
		return Result;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return Result;
	}

	const FVector Origin = Pawn->GetActorLocation();
	const float RadiusSquared = NearbyItemsQueryRadius * NearbyItemsQueryRadius;

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] QueryNearbyWorldItems starting from origin %s, radius=%.0f"),
		*Origin.ToString(), NearbyItemsQueryRadius);

	int32 TotalItemsInWorld = 0;
	int32 ItemsInRange = 0;
	int32 ItemsPickupable = 0;

	// Iterate through all world items and check distance
	for (TActorIterator<AMOWorldItem> It(World); It; ++It)
	{
		AMOWorldItem* WorldItem = *It;
		TotalItemsInWorld++;

		if (!IsValid(WorldItem))
		{
			continue;
		}

		// Check distance
		const FVector ItemLocation = WorldItem->GetActorLocation();
		const float DistanceSquared = FVector::DistSquared(Origin, ItemLocation);

		if (DistanceSquared > RadiusSquared)
		{
			continue;
		}

		ItemsInRange++;

		if (WorldItem->IsPickupable())
		{
			ItemsPickupable++;
			Result.AddUnique(WorldItem);
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] QueryNearbyWorldItems: TotalInWorld=%d, InRange=%d, Pickupable=%d"),
		TotalItemsInWorld, ItemsInRange, ItemsPickupable);

	return Result;
}

int32 UMOInventoryUIController::LootAllNearbyItems()
{
	UMOInventoryComponent* InventoryComponent = GetCachedInventory();
	if (!IsValid(InventoryComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] LootAllNearbyItems - No inventory component on pawn"));
		return 0;
	}

	TArray<AMOWorldItem*> NearbyItems = QueryNearbyWorldItems();
	int32 LootedCount = 0;

	for (AMOWorldItem* WorldItem : NearbyItems)
	{
		if (!IsValid(WorldItem))
		{
			continue;
		}

		// Get item info from the world item
		UMOItemComponent* ItemComp = WorldItem->GetItemComponent();
		if (!IsValid(ItemComp))
		{
			continue;
		}

		// Get identity for GUID
		FGuid ItemGuid;
		if (WorldItem->Implements<UMOIdentifiableInterface>())
		{
			ItemGuid = IMOIdentifiableInterface::Execute_GetPersistentGuid(WorldItem);
		}

		if (!ItemGuid.IsValid())
		{
			// Generate a new GUID for this item
			ItemGuid = FGuid::NewGuid();
		}

		// Add to inventory
		bool bAdded = InventoryComponent->AddItemByGuid(
			ItemGuid,
			ItemComp->ItemDefinitionId,
			ItemComp->Quantity
		);

		if (bAdded)
		{
			LootedCount++;

			// Hide or destroy the world item based on its settings
			if (WorldItem->bDestroyAfterPickup)
			{
				WorldItem->Destroy();
			}
			else if (WorldItem->bHideOnPickup)
			{
				WorldItem->SetActorHiddenInGame(true);
				WorldItem->SetActorEnableCollision(false);
			}
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] LootAllNearbyItems - Looted %d/%d items"),
		LootedCount, NearbyItems.Num());

	return LootedCount;
}

void UMOInventoryUIController::HandleQuickTransfer(const FGuid& ItemGuid, UMOInventoryComponent* SourceInventory)
{
	if (!ItemGuid.IsValid() || !IsValid(SourceInventory))
	{
		return;
	}

	UMOInventoryComponent* PlayerInventory = GetCachedInventory();
	if (!IsValid(PlayerInventory))
	{
		return;
	}

	// Determine target inventory (opposite of source)
	UMOInventoryComponent* TargetInventory = nullptr;

	if (SourceInventory == PlayerInventory)
	{
		// Source is player, target is container (if any)
		AActor* Container = GetActiveContainer();
		if (IsValid(Container) && Container->Implements<UMOInventoryHolderInterface>())
		{
			TargetInventory = IMOInventoryHolderInterface::Execute_GetInventory(Container);
		}
	}
	else
	{
		// Source is container, target is player
		TargetInventory = PlayerInventory;
	}

	if (!IsValid(TargetInventory))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] QuickTransfer - No valid target inventory"));
		return;
	}

	// Perform transfer
	bool bSuccess = SourceInventory->TransferItem(ItemGuid, TargetInventory);

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] QuickTransfer %s: %s"),
		bSuccess ? TEXT("succeeded") : TEXT("failed"),
		*ItemGuid.ToString(EGuidFormats::DigitsWithHyphens));
}

void UMOInventoryUIController::HandleLootAllNearby()
{
	int32 LootedCount = LootAllNearbyItems();

	if (LootedCount > 0)
	{
		// Show notification through UIManager
		UMOUIManagerComponent* UIManager = GetUIManager();
		if (UIManager)
		{
			FText Message = FText::Format(
				NSLOCTEXT("MO", "LootedItemsFormat", "Looted {0} item(s)"),
				FText::AsNumber(LootedCount)
			);
			UIManager->ShowNotification(Message, 2.0f);
		}
	}
}

// =============================================================================
// Item Context Menu
// =============================================================================

void UMOInventoryUIController::ShowItemContextMenu(UMOInventoryComponent* InventoryComponent, const FGuid& ItemGuid, int32 SlotIndex, FVector2D ScreenPosition)
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	if (!ItemContextMenuClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] ItemContextMenuClass not set on controller."));
		return;
	}

	// Close existing context menu if any
	CloseItemContextMenu();

	UMOItemContextMenu* MenuWidget = CreateWidget<UMOItemContextMenu>(PlayerController, ItemContextMenuClass);
	if (!IsValid(MenuWidget))
	{
		return;
	}

	ItemContextMenuWidget = MenuWidget;

	MenuWidget->OnMenuClosed.AddDynamic(this, &UMOInventoryUIController::HandleContextMenuClosed);
	MenuWidget->OnActionSelected.AddDynamic(this, &UMOInventoryUIController::HandleContextMenuAction);

	MenuWidget->InitializeForItem(InventoryComponent, ItemGuid, SlotIndex);

	// Add to viewport first, then position
	MenuWidget->AddToViewport(ItemContextMenuZOrder);

	// Position at mouse cursor using viewport slot positioning
	MenuWidget->SetMenuPosition(ScreenPosition);
}

void UMOInventoryUIController::ShowWorldItemContextMenu(AMOWorldItem* WorldItem, FVector2D ScreenPosition)
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	if (!ItemContextMenuClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] ItemContextMenuClass not set on controller."));
		return;
	}

	if (!IsValid(WorldItem))
	{
		return;
	}

	// Close existing context menu if any
	CloseItemContextMenu();

	// Store the world item for pickup action
	ContextMenuWorldItem = WorldItem;

	UMOItemContextMenu* MenuWidget = CreateWidget<UMOItemContextMenu>(PlayerController, ItemContextMenuClass);
	if (!IsValid(MenuWidget))
	{
		return;
	}

	ItemContextMenuWidget = MenuWidget;

	MenuWidget->OnMenuClosed.AddDynamic(this, &UMOInventoryUIController::HandleContextMenuClosed);
	MenuWidget->OnActionSelected.AddDynamic(this, &UMOInventoryUIController::HandleContextMenuAction);

	// Initialize for world item (shows Pickup instead of Drop)
	MenuWidget->InitializeForWorldItem(WorldItem);

	// Add to viewport first, then position
	MenuWidget->AddToViewport(ItemContextMenuZOrder);

	// Position at mouse cursor
	MenuWidget->SetMenuPosition(ScreenPosition);

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] Showing context menu for world item: %s"), *WorldItem->GetName());
}

void UMOInventoryUIController::CloseItemContextMenu()
{
	UMOItemContextMenu* MenuWidget = ItemContextMenuWidget.Get();
	if (IsValid(MenuWidget))
	{
		if (MenuWidget->IsInViewport())
		{
			MenuWidget->RemoveFromParent();
		}
	}
	ItemContextMenuWidget.Reset();
	ContextMenuWorldItem.Reset();
}

bool UMOInventoryUIController::IsItemContextMenuOpen() const
{
	const UMOItemContextMenu* MenuWidget = ItemContextMenuWidget.Get();
	return IsValid(MenuWidget) && MenuWidget->IsInViewport();
}

void UMOInventoryUIController::HandleContextMenuClosed()
{
	ItemContextMenuWidget.Reset();
}

void UMOInventoryUIController::HandleContextMenuAction(FName ActionId, const FGuid& ItemGuid)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] Context menu action: %s for item %s"),
		*ActionId.ToString(), *ItemGuid.ToString(EGuidFormats::DigitsWithHyphens));

	UMOInventoryComponent* InventoryComponent = GetCachedInventory();
	if (!IsValid(InventoryComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] No inventory component for context menu action"));
		return;
	}

	UMOUIManagerComponent* UIManager = GetUIManager();

	if (ActionId == FName("Use"))
	{
		// Consume item - apply nutrition to survival stats (use cached component)
		UMOSurvivalStatsComponent* SurvivalStats = GetCachedSurvivalStats();
		if (IsValid(SurvivalStats))
		{
			if (SurvivalStats->ConsumeItem(InventoryComponent, ItemGuid))
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] Item consumed successfully"));
			}
			else
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] Failed to consume item"));
			}
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] No SurvivalStatsComponent cached for pawn"));
		}
	}
	else if (ActionId == FName("Drop1"))
	{
		// Close context menu first to ensure clean UI state
		CloseItemContextMenu();
		// Drop single item into world
		DropItemToWorldByGuid(InventoryComponent, ItemGuid);
	}
	else if (ActionId == FName("DropAll"))
	{
		// Close context menu first to ensure clean UI state
		CloseItemContextMenu();
		// Drop entire stack into world
		DropItemToWorldByGuid(InventoryComponent, ItemGuid);
	}
	else if (ActionId == FName("Inspect"))
	{
		// Cache world item BEFORE closing context menu (which resets ContextMenuWorldItem)
		AMOWorldItem* WorldItem = ContextMenuWorldItem.Get();

		// Close context menu first to ensure IsAnyMenuOpen() returns correct state
		CloseItemContextMenu();

		FName ItemDefinitionId;

		// Check if this is a world item or inventory item
		if (IsValid(WorldItem))
		{
			// World item inspection
			UMOItemComponent* ItemComp = WorldItem->GetItemComponent();
			if (IsValid(ItemComp))
			{
				ItemDefinitionId = ItemComp->ItemDefinitionId;
			}
		}
		else if (IsValid(InventoryComponent))
		{
			// Inventory item inspection
			FMOInventoryEntry Entry;
			if (InventoryComponent->TryGetEntryByGuid(ItemGuid, Entry))
			{
				ItemDefinitionId = Entry.ItemDefinitionId;
			}
		}

		if (!ItemDefinitionId.IsNone())
		{
			if (UIManager)
			{
				UIManager->StartItemInspection(ItemDefinitionId, ItemGuid);
			}
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] Inspect action - could not find item definition"));
		}
	}
	else if (ActionId == FName("SplitStack"))
	{
		// Cache world item BEFORE closing context menu (which resets ContextMenuWorldItem)
		AMOWorldItem* WorldItem = ContextMenuWorldItem.Get();

		// Close context menu first
		CloseItemContextMenu();
		if (IsValid(WorldItem))
		{
			// World item split: take half into player inventory, leave rest in world
			UMOItemComponent* ItemComp = WorldItem->GetItemComponent();
			if (!IsValid(ItemComp))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] SplitStack (world) - item has no ItemComponent"));
				return;
			}

			// Can't split single items
			if (ItemComp->Quantity <= 1)
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] SplitStack (world) - nothing to split (quantity=%d)"), ItemComp->Quantity);
				return;
			}

			// Split in half
			const int32 SplitAmount = ItemComp->Quantity / 2;
			const int32 RemainingAmount = ItemComp->Quantity - SplitAmount;
			const FName ItemDefId = ItemComp->ItemDefinitionId;

			// Add split portion to player inventory
			const FGuid NewGuid = FGuid::NewGuid();
			bool bAdded = InventoryComponent->AddItemByGuidNoStack(NewGuid, ItemDefId, SplitAmount);

			if (bAdded)
			{
				// Update world item quantity
				ItemComp->Quantity = RemainingAmount;
				UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] SplitStack (world) - picked up %d, left %d in world"),
					SplitAmount, RemainingAmount);

				// Refresh nearby panel to show updated quantity
				UMOUnifiedInventoryMenu* MenuWidget = UnifiedInventoryWidget.Get();
				if (IsValid(MenuWidget))
				{
					MenuWidget->RefreshNearbyPanel();
				}
			}
			else
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] SplitStack (world) - failed to add to inventory (full?)"));
				if (UIManager)
				{
					UIManager->ShowNotification(NSLOCTEXT("MO", "InventoryFull", "Inventory full"), 2.0f);
				}
			}
		}
		else
		{
			// Inventory item split (original logic)
			FMOInventoryEntry Entry;
			if (!InventoryComponent->TryGetEntryByGuid(ItemGuid, Entry))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] SplitStack - item not found"));
				return;
			}

			// Can't split single items
			if (Entry.Quantity <= 1)
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] SplitStack - nothing to split (quantity=%d)"), Entry.Quantity);
				return;
			}

			// Split in half (floor division)
			const int32 SplitAmount = Entry.Quantity / 2;
			const FName ItemDefId = Entry.ItemDefinitionId;

			// Remove split amount from original stack
			if (!InventoryComponent->RemoveItemByGuid(ItemGuid, SplitAmount))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] SplitStack - failed to remove from original stack"));
				return;
			}

			// Generate new GUID for split portion
			const FGuid NewGuid = FGuid::NewGuid();

			// Check if there's room in inventory (an empty slot)
			if (InventoryComponent->HasEmptySlot())
			{
				// Add split portion to inventory - use NoStack to prevent it from merging back
				InventoryComponent->AddItemByGuidNoStack(NewGuid, ItemDefId, SplitAmount);
				UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] SplitStack - split %d items into new stack"), SplitAmount);
			}
			else
			{
				// No room in inventory - drop to world
				APlayerController* PC = ResolveOwningPlayerController();
				APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;

				if (IsValid(PlayerPawn))
				{
					// Calculate drop location in front of player
					const FVector PlayerLocation = PlayerPawn->GetActorLocation();
					const FVector ForwardVector = PlayerPawn->GetActorForwardVector();
					const FVector DropLocation = PlayerLocation + ForwardVector * 100.0f;
					const FRotator DropRotation(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f);

					// Spawn world item for the split portion
					InventoryComponent->SpawnWorldItem(ItemDefId, SplitAmount, NewGuid, DropLocation, DropRotation);
					UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] SplitStack - dropped %d items to world (no room in inventory)"), SplitAmount);
				}
				else
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] SplitStack - no room and no pawn to drop items"));
				}
			}
		}
	}
	else if (ActionId == FName("Craft"))
	{
		// Close context menu first to ensure IsAnyMenuOpen() returns correct state
		CloseItemContextMenu();
		// Close inventory and open crafting menu
		CloseInventoryMenu();
		if (UIManager)
		{
			UIManager->OpenCraftingMenu();
		}
		UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] Craft action - opened crafting menu"));
	}
	else if (ActionId == FName("Details"))
	{
		// Cache world item BEFORE closing context menu (which resets ContextMenuWorldItem)
		AMOWorldItem* WorldItem = ContextMenuWorldItem.Get();

		// Close context menu first
		CloseItemContextMenu();

		// Switch to details tab in unified inventory menu
		UMOUnifiedInventoryMenu* MenuWidget = UnifiedInventoryWidget.Get();
		if (!IsValid(MenuWidget))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] Details action - unified inventory menu not available"));
			return;
		}

		// Check if this is a world item or inventory item
		if (IsValid(WorldItem))
		{
			// World item details
			UMOItemComponent* ItemComp = WorldItem->GetItemComponent();
			if (IsValid(ItemComp))
			{
				MenuWidget->SetItemByDefinitionId(ItemComp->ItemDefinitionId, ItemComp->Quantity);
				MenuWidget->ShowDetailsTab();
				UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] Details action - showing details for world item %s x%d"),
					*ItemComp->ItemDefinitionId.ToString(), ItemComp->Quantity);
			}
			else
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] Details action - world item has no ItemComponent"));
			}
		}
		else
		{
			// Inventory item details
			MenuWidget->SetSelectedItem(ItemGuid, InventoryComponent);
			MenuWidget->ShowDetailsTab();
			UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] Details action - switched to details panel for item %s"),
				*ItemGuid.ToString(EGuidFormats::DigitsWithHyphens));
		}
	}
	else if (ActionId == FName("Transfer"))
	{
		// Close context menu first
		CloseItemContextMenu();
		// Quick transfer - handled by HandleQuickTransfer
		HandleQuickTransfer(ItemGuid, InventoryComponent);
		UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] Transfer action - quick transferred item %s"), *ItemGuid.ToString(EGuidFormats::DigitsWithHyphens));
	}
	else if (ActionId == FName("Pickup"))
	{
		// Cache world item BEFORE closing context menu (which resets ContextMenuWorldItem)
		AMOWorldItem* WorldItem = ContextMenuWorldItem.Get();

		// Close context menu first
		CloseItemContextMenu();

		if (!IsValid(WorldItem))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] Pickup action - world item no longer valid"));
			return;
		}

		UMOItemComponent* ItemComp = WorldItem->GetItemComponent();
		if (!IsValid(ItemComp))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] Pickup action - world item has no item component"));
			return;
		}

		// Add to player inventory
		FGuid NewItemGuid = FGuid::NewGuid();
		bool bAdded = InventoryComponent->AddItemByGuid(NewItemGuid, ItemComp->ItemDefinitionId, ItemComp->Quantity);

		if (bAdded)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] Pickup action - picked up %s x%d"),
				*ItemComp->ItemDefinitionId.ToString(), ItemComp->Quantity);

			// Hide or destroy the world item
			if (WorldItem->bDestroyAfterPickup)
			{
				WorldItem->Destroy();
			}
			else
			{
				WorldItem->SetActorHiddenInGame(true);
				WorldItem->SetActorEnableCollision(false);
			}
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] Pickup action - failed to add item to inventory (full?)"));
			if (UIManager)
			{
				UIManager->ShowNotification(NSLOCTEXT("MO", "InventoryFull", "Inventory full"), 2.0f);
			}
		}
	}
}

void UMOInventoryUIController::HandleInventoryMenuRequestClose()
{
	CloseInventoryMenu();
}

void UMOInventoryUIController::HandleInventoryMenuSlotRightClicked(int32 SlotIndex, const FGuid& ItemGuid, FVector2D ScreenPosition)
{
	// Only show context menu if there's an item
	if (!ItemGuid.IsValid())
	{
		return;
	}

	UMOInventoryMenu* MenuWidget = InventoryMenuWidget.Get();
	if (!IsValid(MenuWidget))
	{
		return;
	}

	UMOInventoryComponent* InventoryComponent = MenuWidget->GetInventoryComponent();
	if (!IsValid(InventoryComponent))
	{
		return;
	}

	ShowItemContextMenu(InventoryComponent, ItemGuid, SlotIndex, ScreenPosition);
}

// =============================================================================
// Item Drop
// =============================================================================

void UMOInventoryUIController::DropItemToWorldByGuid(UMOInventoryComponent* InventoryComponent, const FGuid& ItemGuid)
{
	if (!IsValid(InventoryComponent) || !ItemGuid.IsValid())
	{
		return;
	}

	APlayerController* PC = ResolveOwningPlayerController();
	if (!IsValid(PC))
	{
		return;
	}

	APawn* PlayerPawn = PC->GetPawn();
	if (!IsValid(PlayerPawn))
	{
		return;
	}

	// Calculate drop location in front of player
	FVector PlayerLocation = PlayerPawn->GetActorLocation();
	FRotator PlayerRotation = PlayerPawn->GetActorRotation();
	PlayerRotation.Pitch = 0.0f; // Flatten to horizontal

	// Random offset in front of player
	const float ForwardDistance = FMath::RandRange(150.0f, 250.0f);
	const float SideOffset = FMath::RandRange(-50.0f, 50.0f);

	FVector ForwardDir = PlayerRotation.Vector();
	FVector RightDir = FRotationMatrix(PlayerRotation).GetScaledAxis(EAxis::Y);
	FVector DropLocation = PlayerLocation + (ForwardDir * ForwardDistance) + (RightDir * SideOffset);

	// Trace down to find ground - use WorldStatic channel for reliable floor detection
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		FHitResult HitResult;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(PlayerPawn);
		QueryParams.bTraceComplex = false; // Use simple collision for speed

		const FVector TraceStart = DropLocation + FVector(0.0f, 0.0f, 200.0f);
		const FVector TraceEnd = DropLocation - FVector(0.0f, 0.0f, 500.0f);

		// Try WorldStatic first (floors, terrain), then Visibility as fallback
		bool bHitGround = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams);
		if (!bHitGround)
		{
			bHitGround = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
		}

		if (bHitGround)
		{
			// Place 30cm above ground - high enough to not clip, low enough for reliable physics
			DropLocation = HitResult.Location + FVector(0.0f, 0.0f, 30.0f);
		}
		else
		{
			// No ground found - place at player height as fallback
			DropLocation.Z = PlayerLocation.Z;
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryUI] DropItem: No ground found at drop location, using player height"));
		}
	}

	const FRotator DropRotation(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f);

	// Drop the item by GUID
	AActor* DroppedActor = InventoryComponent->DropItemByGuid(ItemGuid, DropLocation, DropRotation);
	if (IsValid(DroppedActor))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryUI] Dropped item at %s"), *DropLocation.ToString());

		// Enable physics for dropped item
		if (AMOWorldItem* WorldItem = Cast<AMOWorldItem>(DroppedActor))
		{
			WorldItem->EnableDropPhysics();
		}
	}
}
