#include "MOUnifiedInventoryMenu.h"
#include "MOFramework.h"
#include "MOInventoryGrid.h"
#include "MOInventoryComponent.h"
#include "MOEquipmentComponent.h"
#include "MOEquipmentPanel.h"
#include "MOItemInfoPanel.h"
#include "MONearbyItemsPanel.h"
#include "MOCommonButton.h"
#include "MOWorldItem.h"
#include "MOInventoryHolderInterface.h"
#include "MOUIManagerComponent.h"
#include "Components/WidgetSwitcher.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Engine/Engine.h"

UMOUnifiedInventoryMenu::UMOUnifiedInventoryMenu()
{
}

void UMOUnifiedInventoryMenu::NativeConstruct()
{
	Super::NativeConstruct();

	BindButtonEvents();
	UpdateTabButtonStates();
	UpdateActionButtonStates();
}

void UMOUnifiedInventoryMenu::NativeDestruct()
{
	UnbindButtonEvents();
	Super::NativeDestruct();
}

UWidget* UMOUnifiedInventoryMenu::NativeGetDesiredFocusTarget() const
{
	// Focus the close button for keyboard navigation
	if (CloseButton)
	{
		return CloseButton;
	}
	return Super::NativeGetDesiredFocusTarget();
}

void UMOUnifiedInventoryMenu::BindButtonEvents()
{
	// Tab buttons
	if (OtherInventoryTabButton)
	{
		OtherInventoryTabButton->OnClicked().RemoveAll(this);
		OtherInventoryTabButton->OnClicked().AddUObject(this, &UMOUnifiedInventoryMenu::HandleOtherInventoryTabClicked);
	}

	if (DetailsTabButton)
	{
		DetailsTabButton->OnClicked().RemoveAll(this);
		DetailsTabButton->OnClicked().AddUObject(this, &UMOUnifiedInventoryMenu::HandleDetailsTabClicked);
	}

	if (NearbyTabButton)
	{
		NearbyTabButton->OnClicked().RemoveAll(this);
		NearbyTabButton->OnClicked().AddUObject(this, &UMOUnifiedInventoryMenu::HandleNearbyTabClicked);
	}

	if (EquipmentTabButton)
	{
		EquipmentTabButton->OnClicked().RemoveAll(this);
		EquipmentTabButton->OnClicked().AddUObject(this, &UMOUnifiedInventoryMenu::HandleEquipmentTabClicked);
	}

	// Action buttons
	if (StoreAllButton)
	{
		StoreAllButton->OnClicked().RemoveAll(this);
		StoreAllButton->OnClicked().AddUObject(this, &UMOUnifiedInventoryMenu::HandleStoreAllClicked);
	}

	if (TakeAllButton)
	{
		TakeAllButton->OnClicked().RemoveAll(this);
		TakeAllButton->OnClicked().AddUObject(this, &UMOUnifiedInventoryMenu::HandleTakeAllClicked);
	}

	if (StackOrganizePlayerButton)
	{
		StackOrganizePlayerButton->OnClicked().RemoveAll(this);
		StackOrganizePlayerButton->OnClicked().AddUObject(this, &UMOUnifiedInventoryMenu::HandleStackOrganizePlayerClicked);
	}

	if (StackOrganizeSecondaryButton)
	{
		StackOrganizeSecondaryButton->OnClicked().RemoveAll(this);
		StackOrganizeSecondaryButton->OnClicked().AddUObject(this, &UMOUnifiedInventoryMenu::HandleStackOrganizeSecondaryClicked);
	}

	if (FillLeftButton)
	{
		FillLeftButton->OnClicked().RemoveAll(this);
		FillLeftButton->OnClicked().AddUObject(this, &UMOUnifiedInventoryMenu::HandleFillLeftClicked);
	}

	if (FillRightButton)
	{
		FillRightButton->OnClicked().RemoveAll(this);
		FillRightButton->OnClicked().AddUObject(this, &UMOUnifiedInventoryMenu::HandleFillRightClicked);
	}

	if (LootNearbyToPlayerButton)
	{
		LootNearbyToPlayerButton->OnClicked().RemoveAll(this);
		LootNearbyToPlayerButton->OnClicked().AddUObject(this, &UMOUnifiedInventoryMenu::HandleLootNearbyToPlayerClicked);
	}

	if (LootNearbyToSecondaryButton)
	{
		LootNearbyToSecondaryButton->OnClicked().RemoveAll(this);
		LootNearbyToSecondaryButton->OnClicked().AddUObject(this, &UMOUnifiedInventoryMenu::HandleLootNearbyToSecondaryClicked);
	}

	if (CloseButton)
	{
		CloseButton->OnClicked().RemoveAll(this);
		CloseButton->OnClicked().AddUObject(this, &UMOUnifiedInventoryMenu::HandleCloseClicked);
	}

	// Grid event handlers
	if (PlayerInventoryGrid)
	{
		PlayerInventoryGrid->OnGridSlotRightClicked.RemoveDynamic(this, &UMOUnifiedInventoryMenu::HandlePlayerSlotRightClicked);
		PlayerInventoryGrid->OnGridSlotRightClicked.AddDynamic(this, &UMOUnifiedInventoryMenu::HandlePlayerSlotRightClicked);

		PlayerInventoryGrid->OnGridSlotShiftClicked.RemoveDynamic(this, &UMOUnifiedInventoryMenu::HandlePlayerSlotShiftClicked);
		PlayerInventoryGrid->OnGridSlotShiftClicked.AddDynamic(this, &UMOUnifiedInventoryMenu::HandlePlayerSlotShiftClicked);

		PlayerInventoryGrid->OnGridSlotDropReceived.RemoveDynamic(this, &UMOUnifiedInventoryMenu::HandlePlayerSlotDropReceived);
		PlayerInventoryGrid->OnGridSlotDropReceived.AddDynamic(this, &UMOUnifiedInventoryMenu::HandlePlayerSlotDropReceived);
	}

	if (SecondaryInventoryGrid)
	{
		SecondaryInventoryGrid->OnGridSlotRightClicked.RemoveDynamic(this, &UMOUnifiedInventoryMenu::HandleSecondarySlotRightClicked);
		SecondaryInventoryGrid->OnGridSlotRightClicked.AddDynamic(this, &UMOUnifiedInventoryMenu::HandleSecondarySlotRightClicked);

		SecondaryInventoryGrid->OnGridSlotShiftClicked.RemoveDynamic(this, &UMOUnifiedInventoryMenu::HandleSecondarySlotShiftClicked);
		SecondaryInventoryGrid->OnGridSlotShiftClicked.AddDynamic(this, &UMOUnifiedInventoryMenu::HandleSecondarySlotShiftClicked);
	}

	// Nearby panel events
	if (NearbyPanel)
	{
		NearbyPanel->OnNearbyItemsChanged.RemoveDynamic(this, &UMOUnifiedInventoryMenu::HandleNearbyItemsChanged);
		NearbyPanel->OnNearbyItemsChanged.AddDynamic(this, &UMOUnifiedInventoryMenu::HandleNearbyItemsChanged);
	}
}

void UMOUnifiedInventoryMenu::UnbindButtonEvents()
{
	if (OtherInventoryTabButton)
	{
		OtherInventoryTabButton->OnClicked().RemoveAll(this);
	}
	if (DetailsTabButton)
	{
		DetailsTabButton->OnClicked().RemoveAll(this);
	}
	if (NearbyTabButton)
	{
		NearbyTabButton->OnClicked().RemoveAll(this);
	}
	if (EquipmentTabButton)
	{
		EquipmentTabButton->OnClicked().RemoveAll(this);
	}
	if (StoreAllButton)
	{
		StoreAllButton->OnClicked().RemoveAll(this);
	}
	if (TakeAllButton)
	{
		TakeAllButton->OnClicked().RemoveAll(this);
	}
	if (StackOrganizePlayerButton)
	{
		StackOrganizePlayerButton->OnClicked().RemoveAll(this);
	}
	if (StackOrganizeSecondaryButton)
	{
		StackOrganizeSecondaryButton->OnClicked().RemoveAll(this);
	}
	if (FillLeftButton)
	{
		FillLeftButton->OnClicked().RemoveAll(this);
	}
	if (FillRightButton)
	{
		FillRightButton->OnClicked().RemoveAll(this);
	}
	if (LootNearbyToPlayerButton)
	{
		LootNearbyToPlayerButton->OnClicked().RemoveAll(this);
	}
	if (LootNearbyToSecondaryButton)
	{
		LootNearbyToSecondaryButton->OnClicked().RemoveAll(this);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked().RemoveAll(this);
	}

	if (PlayerInventoryGrid)
	{
		PlayerInventoryGrid->OnGridSlotRightClicked.RemoveDynamic(this, &UMOUnifiedInventoryMenu::HandlePlayerSlotRightClicked);
		PlayerInventoryGrid->OnGridSlotShiftClicked.RemoveDynamic(this, &UMOUnifiedInventoryMenu::HandlePlayerSlotShiftClicked);
		PlayerInventoryGrid->OnGridSlotDropReceived.RemoveDynamic(this, &UMOUnifiedInventoryMenu::HandlePlayerSlotDropReceived);
	}
	if (SecondaryInventoryGrid)
	{
		SecondaryInventoryGrid->OnGridSlotRightClicked.RemoveDynamic(this, &UMOUnifiedInventoryMenu::HandleSecondarySlotRightClicked);
		SecondaryInventoryGrid->OnGridSlotShiftClicked.RemoveDynamic(this, &UMOUnifiedInventoryMenu::HandleSecondarySlotShiftClicked);
	}
	if (NearbyPanel)
	{
		NearbyPanel->OnNearbyItemsChanged.RemoveDynamic(this, &UMOUnifiedInventoryMenu::HandleNearbyItemsChanged);
	}
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void UMOUnifiedInventoryMenu::InitializeMenu(UMOInventoryComponent* PlayerInventory, AActor* SecondarySource)
{
	CachedPlayerInventory = PlayerInventory;

	// Initialize player inventory grid
	if (PlayerInventoryGrid && PlayerInventory)
	{
		PlayerInventoryGrid->InitializeGrid(PlayerInventory);
		UE_LOG(LogMOFramework, Log, TEXT("[UnifiedInventoryMenu] Initialized player inventory grid"));
	}

	// Set quick pickup target for nearby panel
	if (NearbyPanel && PlayerInventory)
	{
		NearbyPanel->SetQuickPickupTarget(PlayerInventory);
	}

	// Initialize equipment panel
	if (EquipmentPanel && PlayerInventory)
	{
		// Find equipment component on the pawn
		APlayerController* PC = GetOwningPlayer();
		if (PC && PC->GetPawn())
		{
			UMOEquipmentComponent* EquipComp = PC->GetPawn()->FindComponentByClass<UMOEquipmentComponent>();
			if (EquipComp)
			{
				CachedEquipmentComponent = EquipComp;
				EquipmentPanel->InitializePanel(EquipComp, PlayerInventory);
				UE_LOG(LogMOFramework, Log, TEXT("[UnifiedInventoryMenu] Initialized equipment panel"));
			}
		}
	}

	// Set secondary source if provided
	if (SecondarySource)
	{
		SetSecondaryInventorySource(SecondarySource);
		ShowOtherInventoryTab();
	}
	else
	{
		// Default to equipment tab when no container
		ShowEquipmentTab();
	}

	UpdateTabButtonStates();
	UpdateActionButtonStates();
}

// ============================================================================
// TAB SWITCHING
// ============================================================================

void UMOUnifiedInventoryMenu::ShowOtherInventoryTab()
{
	CurrentTabIndex = 0;

	if (RightPanelSwitcher)
	{
		RightPanelSwitcher->SetActiveWidgetIndex(0);
	}

	UpdateTabButtonStates();
	UpdateActionButtonStates();
	UE_LOG(LogMOFramework, Log, TEXT("[UnifiedInventoryMenu] Switched to Other Inventory tab"));
}

void UMOUnifiedInventoryMenu::ShowDetailsTab()
{
	CurrentTabIndex = 1;

	if (RightPanelSwitcher)
	{
		RightPanelSwitcher->SetActiveWidgetIndex(1);
	}

	UpdateTabButtonStates();
	UpdateActionButtonStates();
	UE_LOG(LogMOFramework, Log, TEXT("[UnifiedInventoryMenu] Switched to Details tab"));
}

void UMOUnifiedInventoryMenu::ShowNearbyTab()
{
	CurrentTabIndex = 2;

	if (RightPanelSwitcher)
	{
		RightPanelSwitcher->SetActiveWidgetIndex(2);
	}

	UpdateTabButtonStates();
	UpdateActionButtonStates();

	// Refresh nearby items when switching to this tab
	RefreshNearbyPanel();

	UE_LOG(LogMOFramework, Log, TEXT("[UnifiedInventoryMenu] Switched to Nearby tab"));
}

void UMOUnifiedInventoryMenu::ShowEquipmentTab()
{
	CurrentTabIndex = 3;

	if (RightPanelSwitcher)
	{
		RightPanelSwitcher->SetActiveWidgetIndex(3);
	}

	UpdateTabButtonStates();
	UpdateActionButtonStates();

	// Refresh equipment panel display
	if (EquipmentPanel)
	{
		EquipmentPanel->RefreshDisplay();
	}

	UE_LOG(LogMOFramework, Log, TEXT("[UnifiedInventoryMenu] Switched to Equipment tab"));
}

// ============================================================================
// SECONDARY INVENTORY
// ============================================================================

void UMOUnifiedInventoryMenu::SetSecondaryInventorySource(AActor* Source)
{
	CachedSecondarySource = Source;
	CachedSecondaryInventory = nullptr;

	if (Source)
	{
		// Get inventory via interface
		if (Source->Implements<UMOInventoryHolderInterface>())
		{
			CachedSecondaryInventory = IMOInventoryHolderInterface::Execute_GetInventory(Source);
		}

		// Initialize secondary grid
		if (SecondaryInventoryGrid && CachedSecondaryInventory.IsValid())
		{
			SecondaryInventoryGrid->InitializeGrid(CachedSecondaryInventory.Get());
			UE_LOG(LogMOFramework, Log, TEXT("[UnifiedInventoryMenu] Set secondary inventory from %s"), *Source->GetName());
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[UnifiedInventoryMenu] Source %s has no inventory or doesn't implement IMOInventoryHolderInterface"), *Source->GetName());
		}
	}

	UpdateTabButtonStates();
	UpdateActionButtonStates();
}

void UMOUnifiedInventoryMenu::ClearSecondaryInventorySource()
{
	CachedSecondarySource.Reset();
	CachedSecondaryInventory.Reset();

	if (SecondaryInventoryGrid)
	{
		SecondaryInventoryGrid->ClearGrid();
	}

	// Switch to details tab if we were on other inventory
	if (CurrentTabIndex == 0)
	{
		ShowDetailsTab();
	}

	UpdateTabButtonStates();
	UpdateActionButtonStates();

	UE_LOG(LogMOFramework, Log, TEXT("[UnifiedInventoryMenu] Cleared secondary inventory"));
}

bool UMOUnifiedInventoryMenu::HasSecondaryInventory() const
{
	return CachedSecondaryInventory.IsValid();
}

AActor* UMOUnifiedInventoryMenu::GetSecondarySource() const
{
	return CachedSecondarySource.Get();
}

// ============================================================================
// ITEM SELECTION
// ============================================================================

void UMOUnifiedInventoryMenu::SetSelectedItem(const FGuid& ItemGuid, UMOInventoryComponent* SourceInventory)
{
	SelectedItemGuid = ItemGuid;

	if (DetailsPanel && SourceInventory)
	{
		DetailsPanel->InitializePanel(SourceInventory);
		DetailsPanel->SetSelectedItemGuid(ItemGuid);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[UnifiedInventoryMenu] Selected item %s"), *ItemGuid.ToString(EGuidFormats::DigitsWithHyphens));
}

void UMOUnifiedInventoryMenu::ClearSelectedItem()
{
	SelectedItemGuid.Invalidate();

	if (DetailsPanel)
	{
		DetailsPanel->ClearSelection();
	}
}

// ============================================================================
// BULK OPERATIONS
// ============================================================================

void UMOUnifiedInventoryMenu::StoreAllToSecondary()
{
	UMOInventoryComponent* PlayerInv = CachedPlayerInventory.Get();

	// If on Nearby tab (no secondary inventory), drop items to world instead
	if (CurrentTabIndex == 2 || !CachedSecondaryInventory.IsValid())
	{
		DropAllToWorld();
		return;
	}

	UMOInventoryComponent* SecondaryInv = CachedSecondaryInventory.Get();

	if (!PlayerInv || !SecondaryInv)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[UnifiedInventoryMenu] StoreAllToSecondary: Missing inventory"));
		return;
	}

	int32 TransferredCount = PlayerInv->TransferAllTo(SecondaryInv);
	UE_LOG(LogMOFramework, Log, TEXT("[UnifiedInventoryMenu] Stored %d items to secondary inventory"), TransferredCount);
}

void UMOUnifiedInventoryMenu::TakeAllFromSecondary()
{
	// If on Nearby tab (no secondary inventory), loot nearby items instead
	if (CurrentTabIndex == 2 || !CachedSecondaryInventory.IsValid())
	{
		LootAllNearbyToPlayer();
		RefreshNearbyPanel();
		return;
	}

	UMOInventoryComponent* PlayerInv = CachedPlayerInventory.Get();
	UMOInventoryComponent* SecondaryInv = CachedSecondaryInventory.Get();

	if (!PlayerInv || !SecondaryInv)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[UnifiedInventoryMenu] TakeAllFromSecondary: Missing inventory"));
		return;
	}

	int32 TransferredCount = SecondaryInv->TransferAllTo(PlayerInv);
	UE_LOG(LogMOFramework, Log, TEXT("[UnifiedInventoryMenu] Took %d items from secondary inventory"), TransferredCount);
}

void UMOUnifiedInventoryMenu::LootAllNearbyToPlayer()
{
	if (NearbyPanel)
	{
		NearbyPanel->LootAllToInventory(CachedPlayerInventory.Get());
	}
}

void UMOUnifiedInventoryMenu::LootAllNearbyToSecondary()
{
	if (NearbyPanel && CachedSecondaryInventory.IsValid())
	{
		NearbyPanel->LootAllToInventory(CachedSecondaryInventory.Get());
	}
}

void UMOUnifiedInventoryMenu::DropAllToWorld()
{
	UMOInventoryComponent* PlayerInv = CachedPlayerInventory.Get();
	if (!PlayerInv)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[UnifiedInventoryMenu] DropAllToWorld: No player inventory"));
		return;
	}

	// Get player pawn location for drop position
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[UnifiedInventoryMenu] DropAllToWorld: No owning player"));
		return;
	}

	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[UnifiedInventoryMenu] DropAllToWorld: No pawn"));
		return;
	}

	// Get all item GUIDs before we start dropping (since dropping modifies the array)
	TArray<FGuid> ItemGuids = PlayerInv->GetAllItemGuids();
	int32 DroppedCount = 0;

	// Drop location is in front of the player
	const FVector PawnLocation = Pawn->GetActorLocation();
	const FVector ForwardVector = Pawn->GetActorForwardVector();
	const FVector BaseDropLocation = PawnLocation + ForwardVector * 150.0f; // 150cm in front

	for (const FGuid& ItemGuid : ItemGuids)
	{
		// Random rotation for variety
		const FRotator DropRotation(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f);

		AActor* DroppedActor = PlayerInv->DropItemByGuid(ItemGuid, BaseDropLocation, DropRotation);
		if (DroppedActor)
		{
			DroppedCount++;
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[UnifiedInventoryMenu] DropAllToWorld: Dropped %d items"), DroppedCount);

	// Refresh nearby panel to show dropped items
	RefreshNearbyPanel();
}

void UMOUnifiedInventoryMenu::RefreshNearbyPanel()
{
	if (!NearbyPanel)
	{
		return;
	}

	// Get the UI manager to query nearby items
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	// Find the UI manager component
	UMOUIManagerComponent* UIManager = PC->FindComponentByClass<UMOUIManagerComponent>();
	if (UIManager)
	{
		TArray<AMOWorldItem*> NearbyItems = UIManager->QueryNearbyWorldItems();
		NearbyPanel->RefreshNearbyItems(NearbyItems);
		UE_LOG(LogMOFramework, Log, TEXT("[UnifiedInventoryMenu] RefreshNearbyPanel: Found %d nearby items"), NearbyItems.Num());
	}
}

// ============================================================================
// STACK MANAGEMENT
// ============================================================================

void UMOUnifiedInventoryMenu::StackAndOrganize(UMOInventoryComponent* Inventory)
{
	if (Inventory)
	{
		Inventory->StackAndOrganize();
		UE_LOG(LogMOFramework, Log, TEXT("[UnifiedInventoryMenu] Organized inventory"));
	}
}

void UMOUnifiedInventoryMenu::StackAndOrganizePlayer()
{
	StackAndOrganize(CachedPlayerInventory.Get());
}

void UMOUnifiedInventoryMenu::StackAndOrganizeSecondary()
{
	StackAndOrganize(CachedSecondaryInventory.Get());
}

void UMOUnifiedInventoryMenu::FillStacksLeftToRight()
{
	UMOInventoryComponent* PlayerInv = CachedPlayerInventory.Get();
	UMOInventoryComponent* SecondaryInv = CachedSecondaryInventory.Get();

	if (PlayerInv && SecondaryInv)
	{
		int32 Filled = PlayerInv->FillStacksFrom(SecondaryInv);
		UE_LOG(LogMOFramework, Log, TEXT("[UnifiedInventoryMenu] Filled %d items into player stacks from secondary"), Filled);
	}
}

void UMOUnifiedInventoryMenu::FillStacksRightToLeft()
{
	UMOInventoryComponent* PlayerInv = CachedPlayerInventory.Get();
	UMOInventoryComponent* SecondaryInv = CachedSecondaryInventory.Get();

	if (PlayerInv && SecondaryInv)
	{
		int32 Filled = SecondaryInv->FillStacksFrom(PlayerInv);
		UE_LOG(LogMOFramework, Log, TEXT("[UnifiedInventoryMenu] Filled %d items into secondary stacks from player"), Filled);
	}
}

// ============================================================================
// NEARBY ITEMS
// ============================================================================

void UMOUnifiedInventoryMenu::RefreshNearbyItems(const TArray<AMOWorldItem*>& NearbyItems)
{
	if (NearbyPanel)
	{
		NearbyPanel->RefreshNearbyItems(NearbyItems);
	}
}

// ============================================================================
// BUTTON HANDLERS
// ============================================================================

void UMOUnifiedInventoryMenu::HandleOtherInventoryTabClicked()
{
	ShowOtherInventoryTab();
}

void UMOUnifiedInventoryMenu::HandleDetailsTabClicked()
{
	ShowDetailsTab();
}

void UMOUnifiedInventoryMenu::HandleNearbyTabClicked()
{
	ShowNearbyTab();
}

void UMOUnifiedInventoryMenu::HandleEquipmentTabClicked()
{
	ShowEquipmentTab();
}

void UMOUnifiedInventoryMenu::HandleStoreAllClicked()
{
	StoreAllToSecondary();
}

void UMOUnifiedInventoryMenu::HandleTakeAllClicked()
{
	TakeAllFromSecondary();
}

void UMOUnifiedInventoryMenu::HandleStackOrganizePlayerClicked()
{
	StackAndOrganizePlayer();
}

void UMOUnifiedInventoryMenu::HandleStackOrganizeSecondaryClicked()
{
	StackAndOrganizeSecondary();
}

void UMOUnifiedInventoryMenu::HandleFillLeftClicked()
{
	FillStacksLeftToRight();
}

void UMOUnifiedInventoryMenu::HandleFillRightClicked()
{
	FillStacksRightToLeft();
}

void UMOUnifiedInventoryMenu::HandleLootNearbyToPlayerClicked()
{
	LootAllNearbyToPlayer();
}

void UMOUnifiedInventoryMenu::HandleLootNearbyToSecondaryClicked()
{
	LootAllNearbyToSecondary();
}

void UMOUnifiedInventoryMenu::HandleCloseClicked()
{
	OnRequestClose.Broadcast();
}

void UMOUnifiedInventoryMenu::HandlePlayerSlotRightClicked(int32 SlotIndex, const FGuid& ItemGuid, FVector2D ScreenPosition)
{
	if (!ItemGuid.IsValid())
	{
		return;
	}

	// Select the clicked item
	SetSelectedItem(ItemGuid, CachedPlayerInventory.Get());

	// Broadcast context menu request
	OnContextMenuRequested.Broadcast(CachedPlayerInventory.Get(), ItemGuid, SlotIndex, ScreenPosition);
}

void UMOUnifiedInventoryMenu::HandleSecondarySlotRightClicked(int32 SlotIndex, const FGuid& ItemGuid, FVector2D ScreenPosition)
{
	if (!ItemGuid.IsValid())
	{
		return;
	}

	// Select the clicked item
	SetSelectedItem(ItemGuid, CachedSecondaryInventory.Get());

	// Broadcast context menu request
	OnContextMenuRequested.Broadcast(CachedSecondaryInventory.Get(), ItemGuid, SlotIndex, ScreenPosition);
}

void UMOUnifiedInventoryMenu::HandlePlayerSlotShiftClicked(int32 SlotIndex, const FGuid& ItemGuid)
{
	if (!ItemGuid.IsValid())
	{
		return;
	}

	UMOInventoryComponent* PlayerInv = CachedPlayerInventory.Get();
	if (!PlayerInv)
	{
		return;
	}

	// Transfer to secondary inventory if available, otherwise broadcast for external handling
	if (CachedSecondaryInventory.IsValid())
	{
		UMOInventoryComponent* SecondaryInv = CachedSecondaryInventory.Get();
		bool bTransferred = PlayerInv->TransferItem(ItemGuid, SecondaryInv);

		if (bTransferred)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[UnifiedInventoryMenu] Shift+Click: Transferred item from player to secondary"));
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[UnifiedInventoryMenu] Shift+Click: Failed to transfer item (secondary full?)"));
		}
	}
	else
	{
		// No secondary inventory - broadcast for external handling (could drop to world)
		OnQuickTransferRequested.Broadcast(ItemGuid, PlayerInv);
	}
}

void UMOUnifiedInventoryMenu::HandleSecondarySlotShiftClicked(int32 SlotIndex, const FGuid& ItemGuid)
{
	if (!ItemGuid.IsValid())
	{
		return;
	}

	UMOInventoryComponent* SecondaryInv = CachedSecondaryInventory.Get();
	UMOInventoryComponent* PlayerInv = CachedPlayerInventory.Get();

	if (!SecondaryInv || !PlayerInv)
	{
		return;
	}

	// Transfer to player inventory
	bool bTransferred = SecondaryInv->TransferItem(ItemGuid, PlayerInv);

	if (bTransferred)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[UnifiedInventoryMenu] Shift+Click: Transferred item from secondary to player"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[UnifiedInventoryMenu] Shift+Click: Failed to transfer item (player inventory full?)"));
	}
}

void UMOUnifiedInventoryMenu::HandleNearbyItemsChanged()
{
	// Refresh the nearby panel when items are dropped to world or picked up
	// Use a short delay to allow the physics system to register newly spawned actors
	UE_LOG(LogMOFramework, Verbose, TEXT("[UnifiedInventoryMenu] HandleNearbyItemsChanged - scheduling delayed refresh"));

	if (UWorld* World = GetWorld())
	{
		FTimerHandle TimerHandle;
		World->GetTimerManager().SetTimer(
			TimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				RefreshNearbyPanel();
			}),
			0.1f,  // 100ms delay for physics to update
			false  // Don't loop
		);
	}
	else
	{
		// Fallback to immediate refresh if no world
		RefreshNearbyPanel();
	}
}

void UMOUnifiedInventoryMenu::HandlePlayerSlotDropReceived(int32 TargetSlotIndex, int32 SourceSlotIndex, UMOInventoryComponent* SourceInventory)
{
	// If SourceInventory is null, this was either:
	// - A pickup from a world item (nearby → inventory)
	// - A drop to world via drag cancel (inventory → world)
	// Either way, refresh the nearby panel
	if (!SourceInventory)
	{
		// Use delayed refresh to allow physics to settle for dropped items
		if (UWorld* World = GetWorld())
		{
			FTimerHandle TimerHandle;
			World->GetTimerManager().SetTimer(
				TimerHandle,
				FTimerDelegate::CreateWeakLambda(this, [this]()
				{
					RefreshNearbyPanel();
				}),
				0.15f,  // Slight delay for physics
				false
			);
		}
		else
		{
			RefreshNearbyPanel();
		}
	}
}

// ============================================================================
// UI STATE UPDATES
// ============================================================================

void UMOUnifiedInventoryMenu::UpdateTabButtonStates()
{
	const bool bHasSecondary = HasSecondaryInventory();

	// Other Inventory tab - hidden if no secondary inventory
	if (OtherInventoryTabButton)
	{
		OtherInventoryTabButton->SetVisibility(bHasSecondary ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Details tab - always enabled
	if (DetailsTabButton)
	{
		DetailsTabButton->SetIsEnabled(true);
	}

	// Nearby tab - always enabled
	if (NearbyTabButton)
	{
		NearbyTabButton->SetIsEnabled(true);
	}

	// TODO: Visual indication of selected tab (highlight/underline)
}

void UMOUnifiedInventoryMenu::UpdateActionButtonStates()
{
	const bool bHasSecondary = HasSecondaryInventory();

	// All transfer buttons always enabled - they adapt behavior based on context
	if (StoreAllButton)
	{
		StoreAllButton->SetIsEnabled(true);
	}

	if (TakeAllButton)
	{
		TakeAllButton->SetIsEnabled(true);
	}

	if (FillLeftButton)
	{
		FillLeftButton->SetIsEnabled(true);
	}

	if (FillRightButton)
	{
		FillRightButton->SetIsEnabled(true);
	}

	// Organize buttons - player always enabled, secondary only if exists
	if (StackOrganizePlayerButton)
	{
		StackOrganizePlayerButton->SetIsEnabled(true);
	}

	if (StackOrganizeSecondaryButton)
	{
		StackOrganizeSecondaryButton->SetIsEnabled(bHasSecondary);
	}

	// Loot nearby to player always enabled
	if (LootNearbyToPlayerButton)
	{
		LootNearbyToPlayerButton->SetIsEnabled(true);
	}

	// Loot nearby to secondary only if container open
	if (LootNearbyToSecondaryButton)
	{
		LootNearbyToSecondaryButton->SetIsEnabled(bHasSecondary);
	}
}
