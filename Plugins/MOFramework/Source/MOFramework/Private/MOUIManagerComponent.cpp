#include "MOUIManagerComponent.h"
#include "MOFramework.h"

#include "EngineUtils.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"

#include "MOInventoryComponent.h"
#include "MOInventoryMenu.h"
#include "MOReticleWidget.h"
#include "MOInGameMenu.h"
#include "MOItemContextMenu.h"
#include "MOConfirmationDialog.h"
#include "MOPersistenceSubsystem.h"
#include "MOSavePanel.h"
#include "MOLoadPanel.h"
#include "MOSurvivalStatsComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOWorldItem.h"
#include "MOStatusPanel.h"
#include "MOModalBackground.h"
#include "MOVitalsComponent.h"
#include "MOMetabolismComponent.h"
#include "MOMentalStateComponent.h"
#include "MONotificationWidget.h"
#include "MOPossessionMenu.h"
#include "MOPawnEntryWidget.h"
#include "MOPossessionSubsystem.h"
#include "MOIdentityRegistrySubsystem.h"
#include "MOIdentityComponent.h"
#include "MOCraftingMenu.h"
#include "MOSkillsPanel.h"
#include "MOSkillsComponent.h"
#include "MOKnowledgeComponent.h"
#include "MOCraftingQueueComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MOInventoryHolderInterface.h"
#include "MOIdentifiableInterface.h"
#include "MOInspectionProgressWidget.h"
#include "MORecipeDatabaseSettings.h"
#include "MONotificationComponent.h"
#include "MOBuildingMenu.h"
#include "MOBuildWidget.h"
#include "MOGhostContextMenu.h"
#include "MOStationContextMenu.h"
#include "MOBuildableActor.h"
#include "MOCraftingStationActor.h"
#include "MOCraftingCapableInterface.h"
#include "MOBuildingComponent.h"
#include "MOBuildProgressComponent.h"
#include "MOPlayerController.h"
#include "MOUnifiedInventoryMenu.h"
#include "MOItemComponent.h"
#include "MOModeIndicatorWidget.h"
#include "MOToolHintWidget.h"

UMOUIManagerComponent::UMOUIManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	CurrentGameplayMode = EMOGameplayMode::Explore;
}

void UMOUIManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalOwningPlayerController())
	{
		if (bCreateReticleOnBeginPlay)
		{
			CreateReticle();
		}

		if (bCreateStatusPanelOnBeginPlay)
		{
			CreateStatusPanel();
		}

		if (bCreateModeIndicatorOnBeginPlay)
		{
			CreateModeIndicator();
		}

		if (bCreateToolHintOnBeginPlay)
		{
			CreateToolHint();
		}
	}
}

void UMOUIManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Ensure we restore input mode on teardown if this component dies while menu is open.
	CloseAllMenus();

	// Clean up reticle widget
	if (UMOReticleWidget* Reticle = ReticleWidget.Get())
	{
		Reticle->RemoveFromParent();
	}
	ReticleWidget.Reset();

	// Clean up status panel widget
	if (UMOStatusPanel* Status = StatusPanelWidget.Get())
	{
		Status->RemoveFromParent();
	}
	StatusPanelWidget.Reset();

	// Clean up modal background
	if (UMOModalBackground* Background = ModalBackgroundWidget.Get())
	{
		Background->RemoveFromParent();
	}
	ModalBackgroundWidget.Reset();

	// Clean up no-pawn notification
	HideNoPawnNotification();

	Super::EndPlay(EndPlayReason);
}

APlayerController* UMOUIManagerComponent::ResolveOwningPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

bool UMOUIManagerComponent::IsLocalOwningPlayerController() const
{
	const APlayerController* PlayerController = ResolveOwningPlayerController();
	return IsValid(PlayerController) && PlayerController->IsLocalController();
}

bool UMOUIManagerComponent::HasValidPawn() const
{
	const APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return false;
	}
	return IsValid(PlayerController->GetPawn());
}

bool UMOUIManagerComponent::IsInventoryMenuOpen() const
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

UMOInventoryComponent* UMOUIManagerComponent::ResolveCurrentPawnInventoryComponent() const
{
	// Use cached inventory component (populated in CachePawnComponents on possession)
	return GetCachedInventory();
}

void UMOUIManagerComponent::ToggleInventoryMenu()
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	// Don't allow opening while in-game menu is open
	if (IsInGameMenuOpen())
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
	CloseAllSwitchableMenus();
	OpenInventoryMenu();
}

void UMOUIManagerComponent::OpenInventoryMenu()
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
		UE_LOG(LogMOFramework, Log, TEXT("[MOUI] OpenInventoryMenu - No valid pawn, showing notification"));
		ShowNoPawnNotification();
		return;
	}

	if (!InventoryMenuClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] InventoryMenuClass not set on UI manager component."));
		return;
	}

	UMOInventoryComponent* InventoryComponent = ResolveCurrentPawnInventoryComponent();
	if (!IsValid(InventoryComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] No UMOInventoryComponent found on current pawn."));
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
		MenuWidget->OnRequestClose.AddDynamic(this, &UMOUIManagerComponent::HandleInventoryMenuRequestClose);

		// Bind right-click for context menu
		MenuWidget->OnSlotRightClicked.AddDynamic(this, &UMOUIManagerComponent::HandleInventoryMenuSlotRightClicked);
	}

	// Always re-initialize on open in case pawn changed.
	MenuWidget->InitializeMenu(InventoryComponent);

	if (!MenuWidget->IsInViewport())
	{
		ShowModalBackground();
		MenuWidget->AddToViewport(InventoryMenuZOrder);
	}

	UpdateReticleVisibility();
	ApplyInputModeForMenuOpen(PlayerController, MenuWidget);
}

void UMOUIManagerComponent::CloseInventoryMenu()
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

	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
		if (IsValid(PlayerController) && PlayerController->IsLocalController())
		{
			ApplyInputModeForMenuClosed(PlayerController);
		}
	}
}

// =============================================================================
// Unified Inventory Menu (Container Support)
// =============================================================================

void UMOUIManagerComponent::OpenInventoryWithContainer(AActor* ContainerActor)
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
		UE_LOG(LogMOFramework, Log, TEXT("[MOUI] OpenInventoryWithContainer - No valid pawn, showing notification"));
		ShowNoPawnNotification();
		return;
	}

	if (!UnifiedInventoryMenuClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] UnifiedInventoryMenuClass not set, falling back to regular inventory menu."));
		// Fall back to regular inventory menu
		SetActiveContainer(ContainerActor);
		OpenInventoryMenu();
		return;
	}

	UMOInventoryComponent* InventoryComponent = ResolveCurrentPawnInventoryComponent();
	if (!IsValid(InventoryComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] No UMOInventoryComponent found on current pawn."));
		return;
	}

	// Close other switchable menus
	CloseAllSwitchableMenus();

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
			UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Failed to create unified inventory menu widget."));
			return;
		}

		// Bind delegates
		MenuWidget->OnRequestClose.RemoveAll(this);
		MenuWidget->OnRequestClose.AddDynamic(this, &UMOUIManagerComponent::HandleUnifiedInventoryMenuRequestClose);

		MenuWidget->OnContextMenuRequested.RemoveAll(this);
		MenuWidget->OnContextMenuRequested.AddDynamic(this, &UMOUIManagerComponent::HandleUnifiedInventoryMenuContextMenuRequested);
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
	ApplyInputModeForMenuOpen(PlayerController, MenuWidget);

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Unified inventory menu opened with container: %s"),
		IsValid(ContainerActor) ? *ContainerActor->GetName() : TEXT("None"));
}

void UMOUIManagerComponent::SetActiveContainer(AActor* ContainerActor)
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

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Active container set to: %s"),
		IsValid(ContainerActor) ? *ContainerActor->GetName() : TEXT("None"));
}

void UMOUIManagerComponent::ClearActiveContainer()
{
	CurrentContainerActor.Reset();

	// If unified menu is open, clear its container
	UMOUnifiedInventoryMenu* MenuWidget = UnifiedInventoryWidget.Get();
	if (IsValid(MenuWidget) && MenuWidget->IsInViewport())
	{
		MenuWidget->ClearSecondaryInventorySource();
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Active container cleared"));
}

AActor* UMOUIManagerComponent::GetActiveContainer() const
{
	return CurrentContainerActor.Get();
}

bool UMOUIManagerComponent::HasActiveContainer() const
{
	return CurrentContainerActor.IsValid();
}

void UMOUIManagerComponent::HandleUnifiedInventoryMenuRequestClose()
{
	UMOUnifiedInventoryMenu* MenuWidget = UnifiedInventoryWidget.Get();
	if (IsValid(MenuWidget) && MenuWidget->IsInViewport())
	{
		MenuWidget->RemoveFromParent();
	}

	UpdateReticleVisibility();

	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
		APlayerController* PlayerController = ResolveOwningPlayerController();
		if (IsValid(PlayerController) && PlayerController->IsLocalController())
		{
			ApplyInputModeForMenuClosed(PlayerController);
		}
	}
}

void UMOUIManagerComponent::HandleUnifiedInventoryMenuContextMenuRequested(UMOInventoryComponent* InventoryComponent, const FGuid& ItemGuid, int32 SlotIndex, FVector2D ScreenPosition)
{
	// Show the context menu for the item
	ShowItemContextMenu(InventoryComponent, ItemGuid, SlotIndex, ScreenPosition);
}

// =============================================================================
// Nearby World Items
// =============================================================================

TArray<AMOWorldItem*> UMOUIManagerComponent::QueryNearbyWorldItems() const
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

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] QueryNearbyWorldItems starting from origin %s, radius=%.0f"),
		*Origin.ToString(), NearbyItemsQueryRadius);

	int32 TotalItemsInWorld = 0;
	int32 ItemsInRange = 0;
	int32 ItemsPickupable = 0;

	// Iterate through all world items and check distance
	// This is more reliable than overlap queries which can miss items with QueryOnly collision
	for (TActorIterator<AMOWorldItem> It(World); It; ++It)
	{
		AMOWorldItem* WorldItem = *It;
		TotalItemsInWorld++;

		if (!IsValid(WorldItem))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] QueryNearbyWorldItems: Found invalid WorldItem"));
			continue;
		}

		// Check distance
		const FVector ItemLocation = WorldItem->GetActorLocation();
		const float DistanceSquared = FVector::DistSquared(Origin, ItemLocation);
		const float Distance = FMath::Sqrt(DistanceSquared);

		if (DistanceSquared > RadiusSquared)
		{
			UE_LOG(LogMOFramework, Verbose, TEXT("[MOUI] QueryNearbyWorldItems: %s at %s is too far (%.0fcm > %.0fcm)"),
				*WorldItem->GetName(), *ItemLocation.ToString(), Distance, NearbyItemsQueryRadius);
			continue;
		}

		ItemsInRange++;

		// Check if pickupable with detailed logging
		const bool bIsHidden = WorldItem->IsHidden();
		const bool bHasCollision = WorldItem->GetActorEnableCollision();
		UMOItemComponent* ItemComp = WorldItem->GetItemComponent();
		const bool bHasItemComp = IsValid(ItemComp);
		const FName ItemDefId = bHasItemComp ? ItemComp->ItemDefinitionId : NAME_None;
		const bool bHasItemDef = !ItemDefId.IsNone();

		UE_LOG(LogMOFramework, Log, TEXT("[MOUI] QueryNearbyWorldItems: %s at %.0fcm - Hidden=%s, Collision=%s, HasItemComp=%s, ItemDefId=%s"),
			*WorldItem->GetName(), Distance,
			bIsHidden ? TEXT("YES") : TEXT("no"),
			bHasCollision ? TEXT("yes") : TEXT("NO"),
			bHasItemComp ? TEXT("yes") : TEXT("NO"),
			bHasItemDef ? *ItemDefId.ToString() : TEXT("NONE"));

		if (WorldItem->IsPickupable())
		{
			ItemsPickupable++;
			Result.AddUnique(WorldItem);
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] QueryNearbyWorldItems: TotalInWorld=%d, InRange=%d, Pickupable=%d"),
		TotalItemsInWorld, ItemsInRange, ItemsPickupable);

	return Result;
}

int32 UMOUIManagerComponent::LootAllNearbyItems()
{
	UMOInventoryComponent* InventoryComponent = ResolveCurrentPawnInventoryComponent();
	if (!IsValid(InventoryComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] LootAllNearbyItems - No inventory component on pawn"));
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

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] LootAllNearbyItems - Looted %d/%d items"),
		LootedCount, NearbyItems.Num());

	return LootedCount;
}

void UMOUIManagerComponent::HandleQuickTransfer(const FGuid& ItemGuid, UMOInventoryComponent* SourceInventory)
{
	if (!ItemGuid.IsValid() || !IsValid(SourceInventory))
	{
		return;
	}

	UMOInventoryComponent* PlayerInventory = ResolveCurrentPawnInventoryComponent();
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
		UE_LOG(LogMOFramework, Log, TEXT("[MOUI] QuickTransfer - No valid target inventory"));
		return;
	}

	// Perform transfer
	bool bSuccess = SourceInventory->TransferItem(ItemGuid, TargetInventory);

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] QuickTransfer %s: %s"),
		bSuccess ? TEXT("succeeded") : TEXT("failed"),
		*ItemGuid.ToString(EGuidFormats::DigitsWithHyphens));
}

void UMOUIManagerComponent::HandleLootAllNearby()
{
	int32 LootedCount = LootAllNearbyItems();

	if (LootedCount > 0)
	{
		// Show notification
		FText Message = FText::Format(
			NSLOCTEXT("MO", "LootedItemsFormat", "Looted {0} item(s)"),
			FText::AsNumber(LootedCount)
		);
		ShowNotification(Message, 2.0f);
	}
}

void UMOUIManagerComponent::ApplyInputModeForMenuOpen(APlayerController* PlayerController, UUserWidget* MenuWidget) const
{
	if (!IsValid(PlayerController) || !IsValid(MenuWidget))
	{
		return;
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

	PlayerController->SetInputMode(InputMode);
	PlayerController->bShowMouseCursor = bShowMouseCursorWhileMenuOpen;

	if (bLockMovementWhileMenuOpen)
	{
		PlayerController->SetIgnoreMoveInput(true);
	}

	if (bLockLookWhileMenuOpen)
	{
		PlayerController->SetIgnoreLookInput(true);
	}
}

void UMOUIManagerComponent::ApplyInputModeForMenuClosed(APlayerController* PlayerController) const
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	FInputModeGameOnly InputMode;
	PlayerController->SetInputMode(InputMode);
	PlayerController->bShowMouseCursor = false;

	PlayerController->SetIgnoreMoveInput(false);
	PlayerController->SetIgnoreLookInput(false);
}

void UMOUIManagerComponent::HandleInventoryMenuRequestClose()
{
	CloseInventoryMenu();
}

void UMOUIManagerComponent::HandleInventoryMenuSlotRightClicked(int32 SlotIndex, const FGuid& ItemGuid, FVector2D ScreenPosition)
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

void UMOUIManagerComponent::CreateReticle()
{
	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	// Use the configured class or default to UMOReticleWidget
	TSubclassOf<UMOReticleWidget> ClassToUse = ReticleWidgetClass;
	if (!ClassToUse)
	{
		ClassToUse = UMOReticleWidget::StaticClass();
	}

	UMOReticleWidget* NewReticle = CreateWidget<UMOReticleWidget>(PlayerController, ClassToUse);
	if (!IsValid(NewReticle))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Failed to create reticle widget."));
		return;
	}

	ReticleWidget = NewReticle;
	NewReticle->AddToViewport(ReticleZOrder);

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Reticle widget created and added to viewport."));
}

void UMOUIManagerComponent::CreateStatusPanel()
{
	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	if (!StatusPanelClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] StatusPanelClass not set on UI manager component."));
		return;
	}

	UMOStatusPanel* NewStatus = CreateWidget<UMOStatusPanel>(PlayerController, StatusPanelClass);
	if (!IsValid(NewStatus))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Failed to create status panel widget."));
		return;
	}

	StatusPanelWidget = NewStatus;
	NewStatus->AddToViewport(StatusPanelZOrder);

	// Start hidden - user must toggle to show
	NewStatus->SetVisibility(ESlateVisibility::Collapsed);

	// Bind close request
	NewStatus->OnRequestClose.AddDynamic(this, &UMOUIManagerComponent::HandleStatusPanelRequestClose);

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Status panel widget created (hidden by default)."));
}

void UMOUIManagerComponent::TogglePlayerStatus()
{
	// Don't allow opening while in-game menu is open
	if (IsInGameMenuOpen() && !IsPlayerStatusVisible())
	{
		return;
	}

	// If already visible, just close it
	if (IsPlayerStatusVisible())
	{
		SetPlayerStatusVisible(false);
		return;
	}

	// Close other switchable menus and open this one
	CloseAllSwitchableMenus();
	SetPlayerStatusVisible(true);
}

void UMOUIManagerComponent::HandleStatusPanelRequestClose()
{
	SetPlayerStatusVisible(false);
}

UMOStatusPanel* UMOUIManagerComponent::GetStatusPanel() const
{
	return StatusPanelWidget.Get();
}

void UMOUIManagerComponent::SetPlayerStatusVisible(bool bVisible)
{
	UMOStatusPanel* Status = StatusPanelWidget.Get();
	if (!IsValid(Status))
	{
		return;
	}

	APlayerController* PlayerController = ResolveOwningPlayerController();

	if (bVisible)
	{
		// Check for valid pawn first
		if (!HasValidPawn())
		{
			ShowNoPawnNotification();
			return;
		}

		bStatusPanelVisible = true;

		// Bind to current pawn's medical components before showing
		RebindStatusPanelToCurrentPawn();

		ShowModalBackground();
		Status->SetVisibility(ESlateVisibility::Visible);

		// Set input mode for menu interaction
		if (IsValid(PlayerController))
		{
			ApplyInputModeForMenuOpen(PlayerController, Status);
		}

		UpdateReticleVisibility();
	}
	else
	{
		bStatusPanelVisible = false;

		Status->SetVisibility(ESlateVisibility::Collapsed);

		UpdateReticleVisibility();

		// Restore game input mode if no other menus open
		if (!IsAnyMenuOpen())
		{
			HideModalBackground();
			if (IsValid(PlayerController))
			{
				ApplyInputModeForMenuClosed(PlayerController);
			}
		}
	}
}

bool UMOUIManagerComponent::IsPlayerStatusVisible() const
{
	return bStatusPanelVisible;
}

void UMOUIManagerComponent::SetReticleVisible(bool bVisible)
{
	UMOReticleWidget* Reticle = ReticleWidget.Get();
	if (!IsValid(Reticle))
	{
		return;
	}

	if (bVisible)
	{
		Reticle->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		Reticle->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UMOUIManagerComponent::IsReticleVisible() const
{
	const UMOReticleWidget* Reticle = ReticleWidget.Get();
	if (!IsValid(Reticle))
	{
		return false;
	}

	return Reticle->GetVisibility() != ESlateVisibility::Collapsed && Reticle->GetVisibility() != ESlateVisibility::Hidden;
}

UMOReticleWidget* UMOUIManagerComponent::GetReticleWidget() const
{
	return ReticleWidget.Get();
}

// =============================================================================
// Possession Menu
// =============================================================================

void UMOUIManagerComponent::TogglePossessionMenu()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] TogglePossessionMenu called"));

	if (!IsLocalOwningPlayerController())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] TogglePossessionMenu - Not local owning player controller, aborting"));
		return;
	}

	// If already open, just close it
	if (IsPossessionMenuOpen())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOUI] TogglePossessionMenu - Menu is open, closing"));
		ClosePossessionMenu();
		return;
	}

	// Possession menu can also close the in-game menu (special case)
	if (IsInGameMenuOpen())
	{
		CloseInGameMenu();
	}

	// Close other switchable menus and open this one
	CloseAllSwitchableMenus();

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] TogglePossessionMenu - Opening"));
	OpenPossessionMenu();
}

void UMOUIManagerComponent::OpenPossessionMenu()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] OpenPossessionMenu called"));

	if (!IsLocalOwningPlayerController())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] OpenPossessionMenu - Not local owning player controller"));
		return;
	}

	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] OpenPossessionMenu - PlayerController invalid"));
		return;
	}

	if (!PossessionMenuClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] PossessionMenuClass not set on UI manager component."));
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] OpenPossessionMenu - All checks passed, creating menu"));

	UMOPossessionMenu* MenuWidget = PossessionMenuWidget.Get();
	if (!IsValid(MenuWidget))
	{
		MenuWidget = CreateWidget<UMOPossessionMenu>(PlayerController, PossessionMenuClass);
		PossessionMenuWidget = MenuWidget;

		if (!IsValid(MenuWidget))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Failed to create possession menu widget."));
			return;
		}

		MenuWidget->OnRequestClose.AddDynamic(this, &UMOUIManagerComponent::HandlePossessionMenuRequestClose);
		MenuWidget->OnPawnSelected.AddDynamic(this, &UMOUIManagerComponent::HandlePossessionMenuPawnSelected);
		MenuWidget->OnCreateCharacter.AddDynamic(this, &UMOUIManagerComponent::HandlePossessionMenuCreateCharacter);
	}

	// Populate with pawn data
	RefreshPossessionMenu();

	if (!MenuWidget->IsInViewport())
	{
		ShowModalBackground();
		MenuWidget->AddToViewport(PossessionMenuZOrder);
	}

	UpdateReticleVisibility();
	ApplyInputModeForMenuOpen(PlayerController, MenuWidget);

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Possession menu opened"));
}

void UMOUIManagerComponent::ClosePossessionMenu()
{
	APlayerController* PlayerController = ResolveOwningPlayerController();

	UMOPossessionMenu* MenuWidget = PossessionMenuWidget.Get();
	if (IsValid(MenuWidget))
	{
		if (MenuWidget->IsInViewport())
		{
			MenuWidget->RemoveFromParent();
		}
	}

	UpdateReticleVisibility();

	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
		if (IsValid(PlayerController) && PlayerController->IsLocalController())
		{
			ApplyInputModeForMenuClosed(PlayerController);
		}
	}
}

bool UMOUIManagerComponent::IsPossessionMenuOpen() const
{
	const UMOPossessionMenu* MenuWidget = PossessionMenuWidget.Get();
	return IsValid(MenuWidget) && MenuWidget->IsInViewport();
}

void UMOUIManagerComponent::RefreshPossessionMenu()
{
	UMOPossessionMenu* MenuWidget = PossessionMenuWidget.Get();
	if (!IsValid(MenuWidget))
	{
		return;
	}

	// Get pawn records from persistence subsystem
	TArray<FMOPersistedPawnRecord> PawnRecords;

	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
	if (GameInstance)
	{
		UMOPersistenceSubsystem* Persistence = GameInstance->GetSubsystem<UMOPersistenceSubsystem>();
		if (Persistence)
		{
			PawnRecords = Persistence->GetAllPawnRecords();
		}
	}

	MenuWidget->PopulatePawnList(PawnRecords);
}

void UMOUIManagerComponent::HandlePossessionMenuRequestClose()
{
	ClosePossessionMenu();
}

void UMOUIManagerComponent::HandlePossessionMenuPawnSelected(const FGuid& PawnGuid)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Pawn selected for possession: %s"), *PawnGuid.ToString());

	// Find and possess the pawn
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	// Try to find the pawn in the world via identity registry
	UMOIdentityRegistrySubsystem* IdentityRegistry = World->GetSubsystem<UMOIdentityRegistrySubsystem>();
	if (IdentityRegistry)
	{
		AActor* FoundActor = IdentityRegistry->ResolveActorOrNull(PawnGuid);
		if (APawn* FoundPawn = Cast<APawn>(FoundActor))
		{
			PlayerController->Possess(FoundPawn);
			ClosePossessionMenu();
			UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Successfully possessed pawn %s"), *PawnGuid.ToString());
			return;
		}
	}

	// If pawn isn't in world, need to spawn it from save data
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
	if (GameInstance)
	{
		UMOPersistenceSubsystem* Persistence = GameInstance->GetSubsystem<UMOPersistenceSubsystem>();
		if (Persistence)
		{
			APawn* SpawnedPawn = Persistence->SpawnPawnFromRecord(PawnGuid);
			if (SpawnedPawn)
			{
				PlayerController->Possess(SpawnedPawn);
				ClosePossessionMenu();
				UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Spawned and possessed pawn %s"), *PawnGuid.ToString());
				return;
			}
		}
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Failed to find or spawn pawn %s"), *PawnGuid.ToString());
}

void UMOUIManagerComponent::HandlePossessionMenuCreateCharacter()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Create new character requested"));

	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Spawn a new pawn using the possession subsystem
	UMOPossessionSubsystem* PossessionSubsystem = World->GetSubsystem<UMOPossessionSubsystem>();
	if (PossessionSubsystem && DefaultPawnClassForNewCharacter)
	{
		APawn* NewPawn = PossessionSubsystem->ServerSpawnAndPossessPawn(
			PlayerController,
			DefaultPawnClassForNewCharacter,
			300.0f,
			FVector::ZeroVector,
			true
		);

		if (NewPawn)
		{
			// Register the new pawn with the persistence subsystem
			UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
			if (GameInstance)
			{
				UMOPersistenceSubsystem* Persistence = GameInstance->GetSubsystem<UMOPersistenceSubsystem>();
				if (Persistence)
				{
					// Get the pawn's identity GUID via interface
					FGuid PawnGuid;
					if (NewPawn->Implements<UMOIdentifiableInterface>())
					{
						if (UMOIdentityComponent* IdentityComp = IMOIdentifiableInterface::Execute_GetIdentityComponent(NewPawn))
						{
							PawnGuid = IdentityComp->GetOrCreateGuid();
						}
					}

					if (PawnGuid.IsValid())
					{
						// Create a new pawn record
						FMOPersistedPawnRecord NewRecord;
						NewRecord.PawnGuid = PawnGuid;
						NewRecord.Transform = NewPawn->GetActorTransform();
						NewRecord.PawnClassPath = FSoftClassPath(NewPawn->GetClass());
						NewRecord.CharacterName = FString::Printf(TEXT("Character %d"), FMath::RandRange(1, 9999));
						NewRecord.Gender = TEXT("Unknown");
						NewRecord.AgeInDays = FMath::RandRange(18 * 365, 40 * 365); // 18-40 years old
						NewRecord.bIsDeceased = false;
						NewRecord.HealthPercent = 1.0f;
						NewRecord.StatusText = TEXT("Healthy");
						NewRecord.LastPlayedTime = FDateTime::Now();

						Persistence->RegisterPawnRecord(NewRecord);
						UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Registered new pawn record: %s (%s)"),
							*NewRecord.CharacterName, *PawnGuid.ToString());
					}
				}
			}

			ClosePossessionMenu();
			UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Created and possessed new character"));
			return;
		}
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Failed to create new character. Check DefaultPawnClassForNewCharacter is set."));
}

// =============================================================================
// Crafting Menu
// =============================================================================

void UMOUIManagerComponent::ToggleCraftingMenu()
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	// Don't allow opening crafting while in-game menu is open
	if (IsInGameMenuOpen())
	{
		return;
	}

	// If crafting is already open, just close it
	if (IsCraftingMenuOpen())
	{
		CloseCraftingMenu();
		return;
	}

	// Close all switchable menus and open crafting
	CloseAllSwitchableMenus();
	OpenCraftingMenu();
}

void UMOUIManagerComponent::OpenCraftingMenu()
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
		UE_LOG(LogMOFramework, Log, TEXT("[MOUI] OpenCraftingMenu - No valid pawn, showing notification"));
		ShowNoPawnNotification();
		return;
	}

	if (!CraftingMenuClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] CraftingMenuClass not set on UI manager component."));
		return;
	}

	APawn* CurrentPawn = PlayerController->GetPawn();
	if (!IsValid(CurrentPawn))
	{
		return;
	}

	// Get required components from the pawn via interface
	UMOInventoryComponent* Inventory = nullptr;
	if (CurrentPawn->Implements<UMOInventoryHolderInterface>())
	{
		Inventory = IMOInventoryHolderInterface::Execute_GetInventory(CurrentPawn);
	}
	if (!IsValid(Inventory))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] No UMOInventoryComponent found on current pawn."));
		return;
	}

	UMOCraftingMenu* MenuWidget = CraftingMenuWidget.Get();
	if (!IsValid(MenuWidget))
	{
		MenuWidget = CreateWidget<UMOCraftingMenu>(PlayerController, CraftingMenuClass);
		CraftingMenuWidget = MenuWidget;

		if (!IsValid(MenuWidget))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Failed to create crafting menu widget."));
			return;
		}

		MenuWidget->OnRequestClose.AddDynamic(this, &UMOUIManagerComponent::HandleCraftingMenuRequestClose);
	}

	// Get optional components from cache (avoids repeated FindComponentByClass)
	UMOSkillsComponent* Skills = GetCachedSkills();
	UMOKnowledgeComponent* Knowledge = GetCachedKnowledge();
	UMOCraftingQueueComponent* CraftingQueue = GetCachedCraftingQueue();
	UMORecipeDiscoveryComponent* Discovery = GetCachedRecipeDiscovery();

	// Initialize with components
	MenuWidget->InitializeMenu(Inventory, Skills, Knowledge, CraftingQueue, Discovery);

	// Check for active crafting station and pass it to the menu
	if (CurrentPawn->Implements<UMOCraftingCapableInterface>())
	{
		AActor* StationActor = IMOCraftingCapableInterface::Execute_GetActiveCraftingStation(CurrentPawn);
		if (AMOCraftingStationActor* Station = Cast<AMOCraftingStationActor>(StationActor))
		{
			MenuWidget->SetActiveStationActor(Station);
		}
	}

	if (!MenuWidget->IsInViewport())
	{
		ShowModalBackground();
		MenuWidget->AddToViewport(CraftingMenuZOrder);
	}

	UpdateReticleVisibility();
	ApplyInputModeForMenuOpen(PlayerController, MenuWidget);

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Crafting menu opened"));
}

void UMOUIManagerComponent::CloseCraftingMenu()
{
	APlayerController* PlayerController = ResolveOwningPlayerController();

	UMOCraftingMenu* MenuWidget = CraftingMenuWidget.Get();
	if (IsValid(MenuWidget))
	{
		if (MenuWidget->IsInViewport())
		{
			MenuWidget->RemoveFromParent();
		}
	}

	UpdateReticleVisibility();

	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
		if (IsValid(PlayerController) && PlayerController->IsLocalController())
		{
			ApplyInputModeForMenuClosed(PlayerController);
		}
	}
}

bool UMOUIManagerComponent::IsCraftingMenuOpen() const
{
	const UMOCraftingMenu* MenuWidget = CraftingMenuWidget.Get();
	return IsValid(MenuWidget) && MenuWidget->IsInViewport();
}

UMOCraftingMenu* UMOUIManagerComponent::GetCraftingMenu() const
{
	return CraftingMenuWidget.Get();
}

void UMOUIManagerComponent::HandleCraftingMenuRequestClose()
{
	CloseCraftingMenu();
}

// =============================================================================
// Skills Panel
// =============================================================================

void UMOUIManagerComponent::ToggleSkillsPanel()
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	// Don't allow opening while in-game menu is open
	if (IsInGameMenuOpen())
	{
		return;
	}

	// If skills panel is already open, just close it
	if (IsSkillsPanelOpen())
	{
		CloseSkillsPanel();
		return;
	}

	// Close all switchable menus and open skills panel
	CloseAllSwitchableMenus();
	OpenSkillsPanel();
}

void UMOUIManagerComponent::OpenSkillsPanel()
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
		UE_LOG(LogMOFramework, Log, TEXT("[MOUI] OpenSkillsPanel - No valid pawn, showing notification"));
		ShowNoPawnNotification();
		return;
	}

	if (!SkillsPanelClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] SkillsPanelClass not set on UI manager component."));
		return;
	}

	// Get current pawn
	APawn* CurrentPawn = PlayerController->GetPawn();
	if (!CurrentPawn)
	{
		return;
	}

	// Create widget if needed
	UMOSkillsPanel* PanelWidget = SkillsPanelWidget.Get();
	if (!PanelWidget)
	{
		PanelWidget = CreateWidget<UMOSkillsPanel>(PlayerController, SkillsPanelClass);
		if (!PanelWidget)
		{
			UE_LOG(LogMOFramework, Error, TEXT("[MOUI] Failed to create Skills Panel widget."));
			return;
		}

		SkillsPanelWidget = PanelWidget;
		PanelWidget->OnRequestClose.AddDynamic(this, &UMOUIManagerComponent::HandleSkillsPanelRequestClose);
	}

	// Get skills and knowledge components from cache
	UMOSkillsComponent* Skills = GetCachedSkills();
	UMOKnowledgeComponent* Knowledge = GetCachedKnowledge();

	// Initialize with both components
	PanelWidget->InitializePanelWithKnowledge(Skills, Knowledge);

	if (!PanelWidget->IsInViewport())
	{
		ShowModalBackground();
		PanelWidget->AddToViewport(SkillsPanelZOrder);
	}

	ApplyInputModeForMenuOpen(PlayerController, PanelWidget);
	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Skills Panel opened"));
}

void UMOUIManagerComponent::CloseSkillsPanel()
{
	APlayerController* PlayerController = ResolveOwningPlayerController();

	UMOSkillsPanel* PanelWidget = SkillsPanelWidget.Get();
	if (IsValid(PanelWidget))
	{
		if (PanelWidget->IsInViewport())
		{
			PanelWidget->RemoveFromParent();
		}
	}

	UpdateReticleVisibility();

	// Only restore input mode if no other menus are open
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
		if (IsValid(PlayerController) && PlayerController->IsLocalController())
		{
			ApplyInputModeForMenuClosed(PlayerController);
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Skills Panel closed"));
}

bool UMOUIManagerComponent::IsSkillsPanelOpen() const
{
	UMOSkillsPanel* PanelWidget = SkillsPanelWidget.Get();
	return PanelWidget && PanelWidget->IsInViewport();
}

UMOSkillsPanel* UMOUIManagerComponent::GetSkillsPanel() const
{
	return SkillsPanelWidget.Get();
}

void UMOUIManagerComponent::HandleSkillsPanelRequestClose()
{
	CloseSkillsPanel();
}

// =============================================================================
// In-Game Menu
// =============================================================================

void UMOUIManagerComponent::ToggleInGameMenu()
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	// If any other menu is open, close it first
	if (IsInventoryMenuOpen())
	{
		CloseInventoryMenu();
		return;
	}

	if (IsItemContextMenuOpen())
	{
		CloseItemContextMenu();
		return;
	}

	if (IsPlayerStatusVisible())
	{
		SetPlayerStatusVisible(false);
		return;
	}

	if (IsCraftingMenuOpen())
	{
		CloseCraftingMenu();
		return;
	}

	// Toggle in-game menu
	if (IsInGameMenuOpen())
	{
		CloseInGameMenu();
	}
	else
	{
		OpenInGameMenu();
	}
}

void UMOUIManagerComponent::OpenInGameMenu()
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

	if (!InGameMenuClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] InGameMenuClass not set on UI manager component."));
		return;
	}

	UMOInGameMenu* MenuWidget = InGameMenuWidget.Get();
	if (!IsValid(MenuWidget))
	{
		MenuWidget = CreateWidget<UMOInGameMenu>(PlayerController, InGameMenuClass);
		InGameMenuWidget = MenuWidget;

		if (!IsValid(MenuWidget))
		{
			return;
		}

		MenuWidget->OnRequestClose.AddDynamic(this, &UMOUIManagerComponent::HandleInGameMenuRequestClose);
		MenuWidget->OnExitToMainMenu.AddDynamic(this, &UMOUIManagerComponent::HandleInGameMenuExitToMainMenu);
		MenuWidget->OnExitGame.AddDynamic(this, &UMOUIManagerComponent::HandleInGameMenuExitGame);
		MenuWidget->OnSaveRequested.AddDynamic(this, &UMOUIManagerComponent::HandleSaveRequested);
		MenuWidget->OnLoadRequested.AddDynamic(this, &UMOUIManagerComponent::HandleLoadRequested);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] InGameMenu delegates bound (OnSaveRequested, OnLoadRequested)"));
	}

	if (!MenuWidget->IsInViewport())
	{
		ShowModalBackground();
		MenuWidget->AddToViewport(InGameMenuZOrder);
	}

	UpdateReticleVisibility();
	ApplyInputModeForMenuOpen(PlayerController, MenuWidget);
}

void UMOUIManagerComponent::CloseInGameMenu()
{
	APlayerController* PlayerController = ResolveOwningPlayerController();

	UMOInGameMenu* MenuWidget = InGameMenuWidget.Get();
	if (IsValid(MenuWidget))
	{
		if (MenuWidget->IsInViewport())
		{
			MenuWidget->RemoveFromParent();
		}
	}

	UpdateReticleVisibility();

	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
		if (IsValid(PlayerController) && PlayerController->IsLocalController())
		{
			ApplyInputModeForMenuClosed(PlayerController);
		}
	}
}

bool UMOUIManagerComponent::IsInGameMenuOpen() const
{
	const UMOInGameMenu* MenuWidget = InGameMenuWidget.Get();
	return IsValid(MenuWidget) && MenuWidget->IsInViewport();
}

void UMOUIManagerComponent::HandleInGameMenuRequestClose()
{
	CloseInGameMenu();
}

void UMOUIManagerComponent::HandleInGameMenuExitToMainMenu()
{
	PendingConfirmationContext = TEXT("ExitToMainMenu");
	ShowConfirmationDialog(
		NSLOCTEXT("MO", "ExitToMainMenuTitle", "Exit to Main Menu"),
		NSLOCTEXT("MO", "ExitToMainMenuMessage", "Are you sure you want to exit to the main menu? Unsaved progress will be lost."),
		NSLOCTEXT("MO", "Exit", "Exit"),
		NSLOCTEXT("MO", "Cancel", "Cancel")
	);
}

void UMOUIManagerComponent::HandleInGameMenuExitGame()
{
	PendingConfirmationContext = TEXT("ExitGame");
	ShowConfirmationDialog(
		NSLOCTEXT("MO", "ExitGameTitle", "Exit Game"),
		NSLOCTEXT("MO", "ExitGameMessage", "Are you sure you want to quit the game? Unsaved progress will be lost."),
		NSLOCTEXT("MO", "Quit", "Quit"),
		NSLOCTEXT("MO", "Cancel", "Cancel")
	);
}

void UMOUIManagerComponent::HandleSaveRequested(const FString& SlotName)
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] *** HANDLE SAVE REQUESTED: %s ***"), *SlotName);

	// Check if slot exists for overwrite confirmation
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
	if (GameInstance)
	{
		UMOPersistenceSubsystem* Persistence = GameInstance->GetSubsystem<UMOPersistenceSubsystem>();
		if (Persistence)
		{
			const bool bSlotExists = Persistence->DoesSaveSlotExist(SlotName);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Slot '%s' exists: %s"), *SlotName, bSlotExists ? TEXT("YES") : TEXT("NO"));

			if (bSlotExists)
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Showing overwrite confirmation"));
				PendingConfirmationContext = FString::Printf(TEXT("Save:%s"), *SlotName);
				ShowConfirmationDialog(
					NSLOCTEXT("MO", "OverwriteSaveTitle", "Overwrite Save"),
					FText::Format(NSLOCTEXT("MO", "OverwriteSaveMessage", "Are you sure you want to overwrite '{0}'?"), FText::FromString(SlotName)),
					NSLOCTEXT("MO", "Overwrite", "Overwrite"),
					NSLOCTEXT("MO", "Cancel", "Cancel")
				);
				return;
			}

			// New save - proceed directly without confirmation
			UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Saving to new slot (no confirmation needed): %s"), *SlotName);
			bool bSaveSuccess = Persistence->SaveWorldToSlot(SlotName);
			UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Save complete (success: %s)"), bSaveSuccess ? TEXT("YES") : TEXT("NO"));

			// Refresh both panels to show the new save
			if (bSaveSuccess)
			{
				UMOInGameMenu* MenuWidget = InGameMenuWidget.Get();
				if (IsValid(MenuWidget))
				{
					MenuWidget->RefreshSavePanelList();
					MenuWidget->RefreshLoadPanelList();
					UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Save and load panels refreshed"));
				}
			}
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Persistence subsystem is NULL"));
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] GameInstance is NULL in HandleSaveRequested"));
	}
}

void UMOUIManagerComponent::HandleLoadRequested(const FString& SlotName)
{
	PendingConfirmationContext = FString::Printf(TEXT("Load:%s"), *SlotName);
	ShowConfirmationDialog(
		NSLOCTEXT("MO", "LoadGameTitle", "Load Game"),
		NSLOCTEXT("MO", "LoadGameMessage", "Are you sure you want to load this save? Unsaved progress will be lost."),
		NSLOCTEXT("MO", "Load", "Load"),
		NSLOCTEXT("MO", "Cancel", "Cancel")
	);
}

// =============================================================================
// Item Context Menu
// =============================================================================

void UMOUIManagerComponent::ShowItemContextMenu(UMOInventoryComponent* InventoryComponent, const FGuid& ItemGuid, int32 SlotIndex, FVector2D ScreenPosition)
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
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] ItemContextMenuClass not set on UI manager component."));
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

	MenuWidget->OnMenuClosed.AddDynamic(this, &UMOUIManagerComponent::HandleContextMenuClosed);
	MenuWidget->OnActionSelected.AddDynamic(this, &UMOUIManagerComponent::HandleContextMenuAction);

	MenuWidget->InitializeForItem(InventoryComponent, ItemGuid, SlotIndex);

	// Add to viewport first, then position
	MenuWidget->AddToViewport(ItemContextMenuZOrder);

	// Position at mouse cursor using viewport slot positioning
	MenuWidget->SetMenuPosition(ScreenPosition);
}

void UMOUIManagerComponent::CloseItemContextMenu()
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
}

bool UMOUIManagerComponent::IsItemContextMenuOpen() const
{
	const UMOItemContextMenu* MenuWidget = ItemContextMenuWidget.Get();
	return IsValid(MenuWidget) && MenuWidget->IsInViewport();
}

void UMOUIManagerComponent::HandleContextMenuClosed()
{
	ItemContextMenuWidget.Reset();
}

void UMOUIManagerComponent::HandleContextMenuAction(FName ActionId, const FGuid& ItemGuid)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Context menu action: %s for item %s"),
		*ActionId.ToString(), *ItemGuid.ToString(EGuidFormats::DigitsWithHyphens));

	UMOInventoryComponent* InventoryComponent = ResolveCurrentPawnInventoryComponent();
	if (!IsValid(InventoryComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] No inventory component for context menu action"));
		return;
	}

	if (ActionId == FName("Use"))
	{
		// Consume item - apply nutrition to survival stats (use cached component)
		UMOSurvivalStatsComponent* SurvivalStats = GetCachedSurvivalStats();
		if (IsValid(SurvivalStats))
		{
			if (SurvivalStats->ConsumeItem(InventoryComponent, ItemGuid))
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Item consumed successfully"));
			}
			else
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Failed to consume item"));
			}
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] No SurvivalStatsComponent cached for pawn"));
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
		// Drop entire stack into world (DropItemByGuid drops the whole stack)
		DropItemToWorldByGuid(InventoryComponent, ItemGuid);
	}
	else if (ActionId == FName("Inspect"))
	{
		// Close context menu first to ensure IsAnyMenuOpen() returns correct state
		CloseItemContextMenu();
		// Get item definition ID from inventory
		FMOInventoryEntry Entry;
		if (InventoryComponent->TryGetEntryByGuid(ItemGuid, Entry))
		{
			StartItemInspection(Entry.ItemDefinitionId, ItemGuid);
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Inspect action - item not found in inventory"));
		}
	}
	else if (ActionId == FName("SplitStack"))
	{
		// Close context menu first
		CloseItemContextMenu();

		// Get the item entry
		FMOInventoryEntry Entry;
		if (!InventoryComponent->TryGetEntryByGuid(ItemGuid, Entry))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] SplitStack - item not found"));
			return;
		}

		// Can't split single items
		if (Entry.Quantity <= 1)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOUI] SplitStack - nothing to split (quantity=%d)"), Entry.Quantity);
			return;
		}

		// Split in half (floor division)
		const int32 SplitAmount = Entry.Quantity / 2;
		const FName ItemDefId = Entry.ItemDefinitionId;

		// Remove split amount from original stack
		if (!InventoryComponent->RemoveItemByGuid(ItemGuid, SplitAmount))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] SplitStack - failed to remove from original stack"));
			return;
		}

		// Generate new GUID for split portion
		const FGuid NewGuid = FGuid::NewGuid();

		// Check if there's room in inventory (an empty slot)
		if (InventoryComponent->HasEmptySlot())
		{
			// Add split portion to inventory
			InventoryComponent->AddItemByGuid(NewGuid, ItemDefId, SplitAmount);
			UE_LOG(LogMOFramework, Log, TEXT("[MOUI] SplitStack - split %d items into new stack"), SplitAmount);
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
				UE_LOG(LogMOFramework, Log, TEXT("[MOUI] SplitStack - dropped %d items to world (no room in inventory)"), SplitAmount);
			}
			else
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] SplitStack - no room and no pawn to drop items"));
			}
		}
	}
	else if (ActionId == FName("Craft"))
	{
		// Close context menu first to ensure IsAnyMenuOpen() returns correct state
		CloseItemContextMenu();
		// Close inventory and open crafting menu
		CloseInventoryMenu();
		OpenCraftingMenu();
		UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Craft action - opened crafting menu"));
	}
	else if (ActionId == FName("Details"))
	{
		// Close context menu first
		CloseItemContextMenu();
		// Switch to details tab in unified inventory menu
		UMOUnifiedInventoryMenu* MenuWidget = UnifiedInventoryWidget.Get();
		if (IsValid(MenuWidget))
		{
			MenuWidget->SetSelectedItem(ItemGuid, InventoryComponent);
			MenuWidget->ShowDetailsTab();
			UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Details action - switched to details panel for item %s"), *ItemGuid.ToString(EGuidFormats::DigitsWithHyphens));
		}
	}
	else if (ActionId == FName("Transfer"))
	{
		// Close context menu first
		CloseItemContextMenu();
		// Quick transfer - handled by HandleQuickTransfer
		HandleQuickTransfer(ItemGuid, InventoryComponent);
		UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Transfer action - quick transferred item %s"), *ItemGuid.ToString(EGuidFormats::DigitsWithHyphens));
	}
}

// =============================================================================
// Confirmation Dialog
// =============================================================================

void UMOUIManagerComponent::ShowConfirmationDialog(const FText& Title, const FText& Message, const FText& ConfirmText, const FText& CancelText)
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

	if (!ConfirmationDialogClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] ConfirmationDialogClass not set on UI manager component."));
		return;
	}

	UMOConfirmationDialog* DialogWidget = ConfirmationDialogWidget.Get();
	if (!IsValid(DialogWidget))
	{
		DialogWidget = CreateWidget<UMOConfirmationDialog>(PlayerController, ConfirmationDialogClass);
		ConfirmationDialogWidget = DialogWidget;

		if (!IsValid(DialogWidget))
		{
			return;
		}

		DialogWidget->OnConfirmed.AddDynamic(this, &UMOUIManagerComponent::HandleConfirmationConfirmed);
		DialogWidget->OnCancelled.AddDynamic(this, &UMOUIManagerComponent::HandleConfirmationCancelled);
	}

	DialogWidget->Setup(Title, Message, ConfirmText, CancelText);

	if (!DialogWidget->IsInViewport())
	{
		DialogWidget->AddToViewport(ConfirmationDialogZOrder);
	}
}

void UMOUIManagerComponent::HandleConfirmationConfirmed()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Confirmation confirmed: %s"), *PendingConfirmationContext);

	// Close the confirmation dialog
	UMOConfirmationDialog* DialogWidget = ConfirmationDialogWidget.Get();
	if (IsValid(DialogWidget) && DialogWidget->IsInViewport())
	{
		DialogWidget->RemoveFromParent();
	}

	if (PendingConfirmationContext == TEXT("ExitToMainMenu"))
	{
		CloseAllMenus();
		UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Exiting to main menu: %s"), *MainMenuLevelPath);
		UGameplayStatics::OpenLevel(this, *MainMenuLevelPath);
	}
	else if (PendingConfirmationContext == TEXT("ExitGame"))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Exiting game"));
		UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
	}
	else if (PendingConfirmationContext.StartsWith(TEXT("Save:")))
	{
		FString SlotName = PendingConfirmationContext.RightChop(5);
		UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
		if (GameInstance)
		{
			UMOPersistenceSubsystem* Persistence = GameInstance->GetSubsystem<UMOPersistenceSubsystem>();
			if (Persistence)
			{
				bool bSaveSuccess = Persistence->SaveWorldToSlot(SlotName);
				UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Saved to slot: %s (success: %s)"), *SlotName, bSaveSuccess ? TEXT("YES") : TEXT("NO"));

				// Refresh panels to show updated save
				if (bSaveSuccess)
				{
					UMOInGameMenu* MenuWidget = InGameMenuWidget.Get();
					if (IsValid(MenuWidget))
					{
						MenuWidget->RefreshSavePanelList();
						MenuWidget->RefreshLoadPanelList();
					}
				}
			}
		}
	}
	else if (PendingConfirmationContext.StartsWith(TEXT("Load:")))
	{
		FString SlotName = PendingConfirmationContext.RightChop(5);
		UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
		if (GameInstance)
		{
			UMOPersistenceSubsystem* Persistence = GameInstance->GetSubsystem<UMOPersistenceSubsystem>();
			if (Persistence)
			{
				CloseAllMenus();
				Persistence->LoadWorldFromSlot(SlotName);
				UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Loaded from slot: %s"), *SlotName);
			}
		}
	}

	PendingConfirmationContext.Empty();
	OnConfirmationConfirmed.Broadcast();
}

void UMOUIManagerComponent::HandleConfirmationCancelled()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Confirmation cancelled: %s"), *PendingConfirmationContext);

	// Close the confirmation dialog
	UMOConfirmationDialog* DialogWidget = ConfirmationDialogWidget.Get();
	if (IsValid(DialogWidget) && DialogWidget->IsInViewport())
	{
		DialogWidget->RemoveFromParent();
	}

	PendingConfirmationContext.Empty();
	OnConfirmationCancelled.Broadcast();
}

// =============================================================================
// Menu Stack Helpers
// =============================================================================

bool UMOUIManagerComponent::IsAnyMenuOpen() const
{
	// Check unified inventory menu
	const UMOUnifiedInventoryMenu* UnifiedMenu = UnifiedInventoryWidget.Get();
	bool bUnifiedMenuOpen = IsValid(UnifiedMenu) && UnifiedMenu->IsInViewport();

	return IsInventoryMenuOpen() || bUnifiedMenuOpen || IsInGameMenuOpen() || IsItemContextMenuOpen() || IsPlayerStatusVisible() || IsPossessionMenuOpen() || IsCraftingMenuOpen() || IsSkillsPanelOpen() || IsBuildingMenuOpen() || IsBuildWidgetOpen() || IsStationContextMenuOpen() || IsInspectionInProgress();
}

void UMOUIManagerComponent::CloseAllMenus()
{
	CloseItemContextMenu();

	// Close status panel
	bStatusPanelVisible = false;
	UMOStatusPanel* Status = StatusPanelWidget.Get();
	if (IsValid(Status))
	{
		Status->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Close inventory (but don't recurse into CloseInventoryMenu's modal handling)
	UMOInventoryMenu* InvMenu = InventoryMenuWidget.Get();
	if (IsValid(InvMenu) && InvMenu->IsInViewport())
	{
		InvMenu->RemoveFromParent();
	}

	// Close unified inventory menu
	UMOUnifiedInventoryMenu* UnifiedMenu = UnifiedInventoryWidget.Get();
	if (IsValid(UnifiedMenu) && UnifiedMenu->IsInViewport())
	{
		UnifiedMenu->RemoveFromParent();
	}

	// Close in-game menu
	UMOInGameMenu* GameMenu = InGameMenuWidget.Get();
	if (IsValid(GameMenu) && GameMenu->IsInViewport())
	{
		GameMenu->RemoveFromParent();
	}

	// Close crafting menu
	UMOCraftingMenu* CraftMenu = CraftingMenuWidget.Get();
	if (IsValid(CraftMenu) && CraftMenu->IsInViewport())
	{
		CraftMenu->RemoveFromParent();
	}

	// Close skills panel
	UMOSkillsPanel* SkillsPanel = SkillsPanelWidget.Get();
	if (IsValid(SkillsPanel) && SkillsPanel->IsInViewport())
	{
		SkillsPanel->RemoveFromParent();
	}

	// Close building menu
	UMOBuildingMenu* BuildMenu = BuildingMenuWidget.Get();
	if (IsValid(BuildMenu) && BuildMenu->IsInViewport())
	{
		BuildMenu->RemoveFromParent();
	}

	// Close ghost context menu
	UMOGhostContextMenu* GhostMenuInst = GhostContextMenuWidget.Get();
	if (IsValid(GhostMenuInst) && GhostMenuInst->IsInViewport())
	{
		GhostMenuInst->RemoveFromParent();
	}
	CurrentBuildTarget.Reset();

	// Close station context menu
	UMOStationContextMenu* StationMenuInst = StationContextMenuWidget.Get();
	if (IsValid(StationMenuInst) && StationMenuInst->IsInViewport())
	{
		StationMenuInst->RemoveFromParent();
	}
	CurrentStationTarget.Reset();

	// Cancel any active inspection
	CancelItemInspection();

	// Close confirmation dialog
	UMOConfirmationDialog* DialogWidget = ConfirmationDialogWidget.Get();
	if (IsValid(DialogWidget) && DialogWidget->IsInViewport())
	{
		DialogWidget->RemoveFromParent();
	}

	// Hide modal background
	HideModalBackground();

	// Restore input mode
	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (IsValid(PlayerController) && PlayerController->IsLocalController())
	{
		ApplyInputModeForMenuClosed(PlayerController);
	}

	UpdateReticleVisibility();
}

void UMOUIManagerComponent::CloseAllSwitchableMenus()
{
	// Close all menus that participate in menu switching.
	// These are the main gameplay menus that toggle between each other.
	// NOT included: In-game menu (pause), confirmation dialogs, context menus, inspection.

	// Close inventory
	UMOInventoryMenu* InvMenu = InventoryMenuWidget.Get();
	if (IsValid(InvMenu) && InvMenu->IsInViewport())
	{
		InvMenu->RemoveFromParent();
	}

	// Close unified inventory menu
	UMOUnifiedInventoryMenu* UnifiedMenu = UnifiedInventoryWidget.Get();
	if (IsValid(UnifiedMenu) && UnifiedMenu->IsInViewport())
	{
		UnifiedMenu->RemoveFromParent();
	}

	// Close crafting menu
	UMOCraftingMenu* CraftMenu = CraftingMenuWidget.Get();
	if (IsValid(CraftMenu) && CraftMenu->IsInViewport())
	{
		CraftMenu->RemoveFromParent();
	}

	// Close skills panel
	UMOSkillsPanel* SkillsPanel = SkillsPanelWidget.Get();
	if (IsValid(SkillsPanel) && SkillsPanel->IsInViewport())
	{
		SkillsPanel->RemoveFromParent();
	}

	// Close status panel
	if (bStatusPanelVisible)
	{
		bStatusPanelVisible = false;
		UMOStatusPanel* Status = StatusPanelWidget.Get();
		if (IsValid(Status))
		{
			Status->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Close possession menu
	UMOPossessionMenu* PossMenu = PossessionMenuWidget.Get();
	if (IsValid(PossMenu) && PossMenu->IsInViewport())
	{
		PossMenu->RemoveFromParent();
	}

	// Close building menu
	UMOBuildingMenu* BuildMenu = BuildingMenuWidget.Get();
	if (IsValid(BuildMenu) && BuildMenu->IsInViewport())
	{
		BuildMenu->RemoveFromParent();
	}

	// Hide modal background if no menus remain open
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();

		APlayerController* PlayerController = ResolveOwningPlayerController();
		if (IsValid(PlayerController) && PlayerController->IsLocalController())
		{
			ApplyInputModeForMenuClosed(PlayerController);
		}
	}
}

void UMOUIManagerComponent::UpdateReticleVisibility()
{
	const bool bMenuOpen = IsAnyMenuOpen();

	if (bHideReticleWhenMenuOpen)
	{
		SetReticleVisible(!bMenuOpen);
	}

	// Note: bHideStatusPanelWhenMenuOpen is handled differently - the status panel
	// IS a menu, so it shouldn't hide itself. This flag would be for hiding a
	// persistent HUD-style status display, which we don't currently have.
}

void UMOUIManagerComponent::ShowModalBackground()
{
	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	UMOModalBackground* Background = ModalBackgroundWidget.Get();
	if (!IsValid(Background))
	{
		Background = CreateWidget<UMOModalBackground>(PlayerController, UMOModalBackground::StaticClass());
		if (!IsValid(Background))
		{
			return;
		}

		ModalBackgroundWidget = Background;
		Background->OnBackgroundClicked.AddDynamic(this, &UMOUIManagerComponent::HandleModalBackgroundClicked);
	}

	if (!Background->IsInViewport())
	{
		Background->AddToViewport(ModalBackgroundZOrder);
	}
}

void UMOUIManagerComponent::HideModalBackground()
{
	UMOModalBackground* Background = ModalBackgroundWidget.Get();
	if (IsValid(Background) && Background->IsInViewport())
	{
		Background->RemoveFromParent();
	}
}

void UMOUIManagerComponent::HandleModalBackgroundClicked()
{
	// Close all open menus when clicking outside
	CloseAllMenus();
}

void UMOUIManagerComponent::DropItemToWorldByGuid(UMOInventoryComponent* InventoryComponent, const FGuid& ItemGuid)
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
			UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] DropItem: No ground found at drop location, using player height"));
		}
	}

	const FRotator DropRotation(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f);

	// Drop the item by GUID
	AActor* DroppedActor = InventoryComponent->DropItemByGuid(ItemGuid, DropLocation, DropRotation);
	if (IsValid(DroppedActor))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Dropped item at %s"), *DropLocation.ToString());

		// Enable physics for dropped item
		if (AMOWorldItem* WorldItem = Cast<AMOWorldItem>(DroppedActor))
		{
			WorldItem->EnableDropPhysics();
		}
	}
}

void UMOUIManagerComponent::GetCurrentPawnMedicalComponents(UMOVitalsComponent*& OutVitals, UMOMetabolismComponent*& OutMetabolism, UMOMentalStateComponent*& OutMental) const
{
	// Use cached components instead of FindComponentByClass
	OutVitals = GetCachedVitals();
	OutMetabolism = GetCachedMetabolism();
	OutMental = GetCachedMentalState();
}

void UMOUIManagerComponent::RebindStatusPanelToCurrentPawn()
{
	UMOStatusPanel* Status = StatusPanelWidget.Get();
	if (!IsValid(Status))
	{
		return;
	}

	UMOVitalsComponent* Vitals = nullptr;
	UMOMetabolismComponent* Metabolism = nullptr;
	UMOMentalStateComponent* Mental = nullptr;

	GetCurrentPawnMedicalComponents(Vitals, Metabolism, Mental);

	// Bind to medical components (null-safe - will unbind if any are null)
	Status->BindToMedicalComponents(Vitals, Metabolism, Mental);

	// Refresh the Info tab with current pawn data
	Status->RefreshCharacterInfo();

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Status panel rebound to current pawn (Vitals: %s, Metabolism: %s, Mental: %s)"),
		IsValid(Vitals) ? TEXT("Yes") : TEXT("No"),
		IsValid(Metabolism) ? TEXT("Yes") : TEXT("No"),
		IsValid(Mental) ? TEXT("Yes") : TEXT("No"));
}

// =============================================================================
// Pawn Component Caching
// =============================================================================

void UMOUIManagerComponent::CachePawnComponents(APawn* NewPawn)
{
	// Clear any existing cached references
	ClearCachedPawnComponents();

	if (!IsValid(NewPawn))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOUI] CachePawnComponents - No pawn, caches cleared"));
		return;
	}

	CachedPawn = NewPawn;

	// Cache all pawn components we frequently need
	// Use interface if available for inventory, otherwise fall back to FindComponentByClass
	if (NewPawn->Implements<UMOInventoryHolderInterface>())
	{
		CachedInventoryComponent = IMOInventoryHolderInterface::Execute_GetInventory(NewPawn);
	}
	else
	{
		CachedInventoryComponent = NewPawn->FindComponentByClass<UMOInventoryComponent>();
	}

	CachedSkillsComponent = NewPawn->FindComponentByClass<UMOSkillsComponent>();
	CachedKnowledgeComponent = NewPawn->FindComponentByClass<UMOKnowledgeComponent>();
	CachedCraftingQueueComponent = NewPawn->FindComponentByClass<UMOCraftingQueueComponent>();
	CachedRecipeDiscoveryComponent = NewPawn->FindComponentByClass<UMORecipeDiscoveryComponent>();
	CachedVitalsComponent = NewPawn->FindComponentByClass<UMOVitalsComponent>();
	CachedMetabolismComponent = NewPawn->FindComponentByClass<UMOMetabolismComponent>();
	CachedMentalStateComponent = NewPawn->FindComponentByClass<UMOMentalStateComponent>();
	CachedSurvivalStatsComponent = NewPawn->FindComponentByClass<UMOSurvivalStatsComponent>();

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] CachePawnComponents - Cached %d components for pawn %s"),
		(CachedInventoryComponent.IsValid() ? 1 : 0) +
		(CachedSkillsComponent.IsValid() ? 1 : 0) +
		(CachedKnowledgeComponent.IsValid() ? 1 : 0) +
		(CachedCraftingQueueComponent.IsValid() ? 1 : 0) +
		(CachedRecipeDiscoveryComponent.IsValid() ? 1 : 0) +
		(CachedVitalsComponent.IsValid() ? 1 : 0) +
		(CachedMetabolismComponent.IsValid() ? 1 : 0) +
		(CachedMentalStateComponent.IsValid() ? 1 : 0) +
		(CachedSurvivalStatsComponent.IsValid() ? 1 : 0),
		*NewPawn->GetName());

	// Auto-rebind status panel if it exists
	RebindStatusPanelToCurrentPawn();
}

void UMOUIManagerComponent::ClearCachedPawnComponents()
{
	CachedPawn.Reset();
	CachedInventoryComponent.Reset();
	CachedSkillsComponent.Reset();
	CachedKnowledgeComponent.Reset();
	CachedCraftingQueueComponent.Reset();
	CachedRecipeDiscoveryComponent.Reset();
	CachedVitalsComponent.Reset();
	CachedMetabolismComponent.Reset();
	CachedMentalStateComponent.Reset();
	CachedSurvivalStatsComponent.Reset();
}

UMOInventoryComponent* UMOUIManagerComponent::GetCachedInventory() const
{
	return CachedInventoryComponent.Get();
}

UMOSkillsComponent* UMOUIManagerComponent::GetCachedSkills() const
{
	return CachedSkillsComponent.Get();
}

UMOKnowledgeComponent* UMOUIManagerComponent::GetCachedKnowledge() const
{
	return CachedKnowledgeComponent.Get();
}

UMOCraftingQueueComponent* UMOUIManagerComponent::GetCachedCraftingQueue() const
{
	return CachedCraftingQueueComponent.Get();
}

UMORecipeDiscoveryComponent* UMOUIManagerComponent::GetCachedRecipeDiscovery() const
{
	return CachedRecipeDiscoveryComponent.Get();
}

UMOVitalsComponent* UMOUIManagerComponent::GetCachedVitals() const
{
	return CachedVitalsComponent.Get();
}

UMOMetabolismComponent* UMOUIManagerComponent::GetCachedMetabolism() const
{
	return CachedMetabolismComponent.Get();
}

UMOMentalStateComponent* UMOUIManagerComponent::GetCachedMentalState() const
{
	return CachedMentalStateComponent.Get();
}

UMOSurvivalStatsComponent* UMOUIManagerComponent::GetCachedSurvivalStats() const
{
	return CachedSurvivalStatsComponent.Get();
}

// =============================================================================
// No Pawn Notification
// =============================================================================

void UMOUIManagerComponent::ShowNoPawnNotification()
{
	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] ShowNoPawnNotification - No valid player controller"));
		return;
	}

	// Hide any existing notification first
	HideNoPawnNotification();

	// Use configured class or default to UMONotificationWidget
	TSubclassOf<UMONotificationWidget> WidgetClass = NoPawnNotificationClass;
	if (!WidgetClass)
	{
		WidgetClass = UMONotificationWidget::StaticClass();
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Creating notification widget of class: %s"), *WidgetClass->GetName());

	// Create notification widget
	UMONotificationWidget* NotificationWidget = CreateWidget<UMONotificationWidget>(PlayerController, WidgetClass);
	if (!IsValid(NotificationWidget))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Failed to create notification widget"));
		return;
	}

	NoPawnNotificationWidget = NotificationWidget;
	NotificationWidget->SetMessage(NoPawnMessage);
	NotificationWidget->AddToViewport(NoPawnNotificationZOrder);

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Notification widget added to viewport at Z-order %d"), NoPawnNotificationZOrder);

	// Set timer to auto-hide
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			NoPawnNotificationTimerHandle,
			this,
			&UMOUIManagerComponent::HideNoPawnNotification,
			NoPawnNotificationDuration,
			false
		);
	}

	// Broadcast delegate so possession menu can hook in
	OnNoPawnForMenu.Broadcast();

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Showing no-pawn notification: %s"), *NoPawnMessage.ToString());
}

void UMOUIManagerComponent::HideNoPawnNotification()
{
	// Clear timer if active
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NoPawnNotificationTimerHandle);
	}

	// Remove widget
	UUserWidget* Widget = NoPawnNotificationWidget.Get();
	if (IsValid(Widget))
	{
		Widget->RemoveFromParent();
	}
	NoPawnNotificationWidget.Reset();
}

// =============================================================================
// Item Inspection
// =============================================================================

void UMOUIManagerComponent::StartItemInspection(FName ItemDefinitionId, const FGuid& ItemGuid)
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

	// Cancel any existing inspection
	if (IsInspectionInProgress())
	{
		CancelItemInspection();
	}

	// Close inventory menu while inspecting
	CloseInventoryMenu();

	// Get item display name from database
	FText ItemDisplayName = UMOItemDatabaseSettings::GetItemDisplayName(ItemDefinitionId);
	if (ItemDisplayName.IsEmpty())
	{
		ItemDisplayName = FText::FromName(ItemDefinitionId);
	}

	// Get knowledge and skills components from pawn
	APawn* CurrentPawn = PlayerController->GetPawn();
	if (!IsValid(CurrentPawn))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] StartItemInspection - No pawn to inspect with"));
		ShowNoPawnNotification();
		return;
	}

	UMOKnowledgeComponent* KnowledgeComp = GetCachedKnowledge();
	UMOSkillsComponent* SkillsComp = GetCachedSkills();

	if (!IsValid(KnowledgeComp))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] StartItemInspection - Pawn has no KnowledgeComponent"));
		return;
	}

	// Create inspection widget if needed
	if (!InspectionProgressWidgetClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] InspectionProgressWidgetClass not set on UI manager component"));
		return;
	}

	UMOInspectionProgressWidget* InspectionWidget = InspectionProgressWidget.Get();
	if (!IsValid(InspectionWidget))
	{
		InspectionWidget = CreateWidget<UMOInspectionProgressWidget>(PlayerController, InspectionProgressWidgetClass);
		if (!IsValid(InspectionWidget))
		{
			UE_LOG(LogMOFramework, Error, TEXT("[MOUI] Failed to create inspection widget"));
			return;
		}

		InspectionProgressWidget = InspectionWidget;

		// Bind delegates (remove first to avoid duplicates)
		InspectionWidget->OnInspectionCompleted.RemoveDynamic(this, &UMOUIManagerComponent::HandleInspectionCompleted);
		InspectionWidget->OnInspectionCancelled.RemoveDynamic(this, &UMOUIManagerComponent::HandleInspectionCancelled);
		InspectionWidget->OnInspectionCompleted.AddDynamic(this, &UMOUIManagerComponent::HandleInspectionCompleted);
		InspectionWidget->OnInspectionCancelled.AddDynamic(this, &UMOUIManagerComponent::HandleInspectionCancelled);
	}

	// Store the item being inspected
	InspectingItemGuid = ItemGuid;

	// Show the widget
	InspectionWidget->AddToViewport(InspectionProgressZOrder);

	// Set up input mode for inspection (game and UI to allow ESC to cancel)
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(InspectionWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
	PlayerController->SetShowMouseCursor(true);

	// Start the inspection
	InspectionWidget->StartInspection(ItemDefinitionId, ItemDisplayName, KnowledgeComp, SkillsComp, InspectionDuration);

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Started inspection of item '%s' (GUID: %s)"),
		*ItemDefinitionId.ToString(), *ItemGuid.ToString(EGuidFormats::DigitsWithHyphens));
}

void UMOUIManagerComponent::CancelItemInspection()
{
	UMOInspectionProgressWidget* InspectionWidget = InspectionProgressWidget.Get();
	if (IsValid(InspectionWidget) && InspectionWidget->IsInspectionInProgress())
	{
		InspectionWidget->CancelInspection();
	}
}

bool UMOUIManagerComponent::IsInspectionInProgress() const
{
	const UMOInspectionProgressWidget* InspectionWidget = InspectionProgressWidget.Get();
	return IsValid(InspectionWidget) && InspectionWidget->IsInspectionInProgress();
}

void UMOUIManagerComponent::HandleInspectionCompleted(bool bCompleted, const FMOInspectionResult& Result)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Inspection completed: Success=%s, XPGrants=%d"),
		bCompleted ? TEXT("true") : TEXT("false"),
		Result.XPGrants.Num());

	// Remove widget from viewport
	UMOInspectionProgressWidget* InspectionWidget = InspectionProgressWidget.Get();
	if (IsValid(InspectionWidget))
	{
		InspectionWidget->RemoveFromParent();
	}

	// Clear inspecting item
	InspectingItemGuid.Invalidate();

	// Only restore input mode if no other menus are open
	// (e.g., inventory might still be open)
	if (!IsAnyMenuOpen())
	{
		APlayerController* PlayerController = ResolveOwningPlayerController();
		if (IsValid(PlayerController))
		{
			ApplyInputModeForMenuClosed(PlayerController);
		}
	}

	// Show notifications for inspection results
	if (bCompleted && Result.bSuccess)
	{
		UMONotificationComponent* NotificationComp = ResolveNotificationComponent();

		// Cycle through showing popup for each affected entry (both skills and knowledge)
		// Each entry gets its own popup that displays sequentially
		for (const FMOInspectionXPGrant& Grant : Result.XPGrants)
		{
			if (Grant.XPAmount > 0.0f && NotificationComp)
			{
				// Both skills and knowledge use the same popup system
				// The bIsKnowledge flag can be used by the widget to style differently if desired
				NotificationComp->ShowSkillPopup(Grant.Id, 3.0f);

				UE_LOG(LogMOFramework, Log, TEXT("[MOUI]   Showing popup for %s '%s': +%.0f XP, Level %d -> %d"),
					Grant.bIsKnowledge ? TEXT("knowledge") : TEXT("skill"),
					*Grant.Id.ToString(),
					Grant.XPAmount,
					Grant.LevelBefore,
					Grant.LevelAfter);
			}
		}

		// Show feedback message about learning potential (e.g., "more to learn" or "nothing more to learn")
		if (!Result.FeedbackMessage.IsEmpty() && NotificationComp)
		{
			NotificationComp->ShowNotification(Result.FeedbackMessage, 4.0f);
		}

		// Note: Recipe unlock notifications are handled by MOCharacter via
		// OnKnowledgeLearned -> HandleKnowledgeLearned which triggers recipe discovery.
		// Recipe discovery then fires OnRecipeDiscovered -> HandleRecipeDiscovered
		// which shows the recipe unlock notification.
	}
}

void UMOUIManagerComponent::HandleInspectionCancelled()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Inspection cancelled"));

	// Remove widget from viewport
	UMOInspectionProgressWidget* InspectionWidget = InspectionProgressWidget.Get();
	if (IsValid(InspectionWidget))
	{
		InspectionWidget->RemoveFromParent();
	}

	// Clear inspecting item
	InspectingItemGuid.Invalidate();

	// Only restore input mode if no other menus are open
	if (!IsAnyMenuOpen())
	{
		APlayerController* PlayerController = ResolveOwningPlayerController();
		if (IsValid(PlayerController))
		{
			ApplyInputModeForMenuClosed(PlayerController);
		}
	}
}

// =============================================================================
// Notifications (Delegated to UMONotificationComponent)
// =============================================================================

UMONotificationComponent* UMOUIManagerComponent::ResolveNotificationComponent() const
{
	// Check cache first
	UMONotificationComponent* Cached = CachedNotificationComponent.Get();
	if (IsValid(Cached))
	{
		return Cached;
	}

	// Find on owner
	AActor* Owner = GetOwner();
	if (IsValid(Owner))
	{
		UMONotificationComponent* Found = Owner->FindComponentByClass<UMONotificationComponent>();
		if (IsValid(Found))
		{
			// Cache it (const_cast needed for caching in const method)
			const_cast<UMOUIManagerComponent*>(this)->CachedNotificationComponent = Found;
			return Found;
		}
	}

	return nullptr;
}

UMONotificationComponent* UMOUIManagerComponent::GetNotificationComponent() const
{
	return ResolveNotificationComponent();
}

void UMOUIManagerComponent::ShowNotification(const FText& Message, float Duration)
{
	UMONotificationComponent* NotificationComp = ResolveNotificationComponent();
	if (IsValid(NotificationComp))
	{
		NotificationComp->ShowNotification(Message, Duration);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] ShowNotification called but no UMONotificationComponent found on owner. Add UMONotificationComponent to your PlayerController."));
	}
}

void UMOUIManagerComponent::ShowSkillIncreaseNotification(FName SkillId, float XPAmount)
{
	UMONotificationComponent* NotificationComp = ResolveNotificationComponent();
	if (IsValid(NotificationComp))
	{
		NotificationComp->ShowSkillIncreaseNotification(SkillId, XPAmount);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] ShowSkillIncreaseNotification called but no UMONotificationComponent found on owner."));
	}
}

void UMOUIManagerComponent::ShowRecipeUnlockedNotification(FName RecipeId)
{
	UMONotificationComponent* NotificationComp = ResolveNotificationComponent();
	if (IsValid(NotificationComp))
	{
		NotificationComp->ShowRecipeUnlockedNotification(RecipeId);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] ShowRecipeUnlockedNotification called but no UMONotificationComponent found on owner."));
	}
}

// =============================================================================
// Building Menu
// =============================================================================

void UMOUIManagerComponent::ToggleBuildingMenu()
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	// Don't allow opening while in-game menu is open
	if (IsInGameMenuOpen())
	{
		return;
	}

	// If building menu is already open, just close it
	if (IsBuildingMenuOpen())
	{
		CloseBuildingMenu();
		return;
	}

	// Close all switchable menus and open building menu
	CloseAllSwitchableMenus();
	OpenBuildingMenu();
}

void UMOUIManagerComponent::OpenBuildingMenu()
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
		UE_LOG(LogMOFramework, Log, TEXT("[MOUI] OpenBuildingMenu - No valid pawn, showing notification"));
		ShowNoPawnNotification();
		return;
	}

	if (!BuildingMenuClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] BuildingMenuClass not set on UI manager component."));
		return;
	}

	// Create widget if needed
	UMOBuildingMenu* MenuWidget = BuildingMenuWidget.Get();
	if (!IsValid(MenuWidget))
	{
		MenuWidget = CreateWidget<UMOBuildingMenu>(PlayerController, BuildingMenuClass);
		if (!IsValid(MenuWidget))
		{
			UE_LOG(LogMOFramework, Error, TEXT("[MOUI] Failed to create Building Menu widget"));
			return;
		}

		BuildingMenuWidget = MenuWidget;

		// Bind delegates
		MenuWidget->OnRequestClose.RemoveDynamic(this, &UMOUIManagerComponent::HandleBuildingMenuRequestClose);
		MenuWidget->OnBuildingSelected.RemoveDynamic(this, &UMOUIManagerComponent::HandleBuildingSelected);
		MenuWidget->OnRequestClose.AddDynamic(this, &UMOUIManagerComponent::HandleBuildingMenuRequestClose);
		MenuWidget->OnBuildingSelected.AddDynamic(this, &UMOUIManagerComponent::HandleBuildingSelected);
	}

	// Initialize menu with cached pawn component data
	UMOKnowledgeComponent* Knowledge = GetCachedKnowledge();
	UMORecipeDiscoveryComponent* Discovery = GetCachedRecipeDiscovery();
	UMOInventoryComponent* Inventory = GetCachedInventory();
	UMOSkillsComponent* Skills = GetCachedSkills();
	MenuWidget->InitializeMenu(Knowledge, Discovery, Inventory, Skills);

	// Show modal background and menu
	ShowModalBackground();
	MenuWidget->AddToViewport(BuildingMenuZOrder);

	// Set input mode
	ApplyInputModeForMenuOpen(PlayerController, MenuWidget);

	// Set focus for keyboard navigation
	MenuWidget->SetFocus();

	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Building Menu opened"));
}

void UMOUIManagerComponent::CloseBuildingMenu()
{
	APlayerController* PlayerController = ResolveOwningPlayerController();

	UMOBuildingMenu* MenuWidget = BuildingMenuWidget.Get();
	if (IsValid(MenuWidget))
	{
		if (MenuWidget->IsInViewport())
		{
			MenuWidget->RemoveFromParent();
		}
	}

	UpdateReticleVisibility();

	// Only restore input mode if no other menus are open
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
		if (IsValid(PlayerController) && PlayerController->IsLocalController())
		{
			ApplyInputModeForMenuClosed(PlayerController);
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Building Menu closed"));
}

bool UMOUIManagerComponent::IsBuildingMenuOpen() const
{
	UMOBuildingMenu* MenuWidget = BuildingMenuWidget.Get();
	return IsValid(MenuWidget) && MenuWidget->IsInViewport();
}

UMOBuildingMenu* UMOUIManagerComponent::GetBuildingMenu() const
{
	return BuildingMenuWidget.Get();
}

void UMOUIManagerComponent::HandleBuildingMenuRequestClose()
{
	CloseBuildingMenu();
}

void UMOUIManagerComponent::HandleBuildingSelected(FName RecipeId)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Building selected: %s"), *RecipeId.ToString());

	// Close the building menu
	CloseBuildingMenu();

	// Enter placement mode via building component
	AMOPlayerController* PC = Cast<AMOPlayerController>(ResolveOwningPlayerController());
	if (IsValid(PC))
	{
		UMOBuildingComponent* BuildingComp = PC->GetBuildingComponent();
		if (IsValid(BuildingComp))
		{
			BuildingComp->EnterPlacementMode(RecipeId);
		}
	}
}

// =============================================================================
// Ghost Context Menu (Ghost Interaction)
// =============================================================================

void UMOUIManagerComponent::ShowBuildWidget(AMOBuildableActor* Target)
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	if (!IsValid(Target))
	{
		return;
	}

	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	if (!GhostContextMenuClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] GhostContextMenuClass not set on UI manager component."));
		return;
	}

	// Get the player's inventory for material sourcing via interface
	UMOInventoryComponent* BuilderInventory = nullptr;
	APawn* Pawn = PlayerController->GetPawn();
	if (IsValid(Pawn) && Pawn->Implements<UMOInventoryHolderInterface>())
	{
		BuilderInventory = IMOInventoryHolderInterface::Execute_GetInventory(Pawn);
	}

	// Create widget if needed
	UMOGhostContextMenu* WidgetInst = GhostContextMenuWidget.Get();
	if (!IsValid(WidgetInst))
	{
		WidgetInst = CreateWidget<UMOGhostContextMenu>(PlayerController, GhostContextMenuClass);
		if (!IsValid(WidgetInst))
		{
			UE_LOG(LogMOFramework, Error, TEXT("[MOUI] Failed to create Ghost Context Menu"));
			return;
		}

		GhostContextMenuWidget = WidgetInst;

		// Bind delegates
		WidgetInst->OnRequestClose.RemoveDynamic(this, &UMOUIManagerComponent::HandleGhostContextMenuRequestClose);
		WidgetInst->OnBuildStarted.RemoveDynamic(this, &UMOUIManagerComponent::HandleGhostContextMenuBuildStarted);
		WidgetInst->OnCancelled.RemoveDynamic(this, &UMOUIManagerComponent::HandleGhostContextMenuCancelled);
		WidgetInst->OnRequestClose.AddDynamic(this, &UMOUIManagerComponent::HandleGhostContextMenuRequestClose);
		WidgetInst->OnBuildStarted.AddDynamic(this, &UMOUIManagerComponent::HandleGhostContextMenuBuildStarted);
		WidgetInst->OnCancelled.AddDynamic(this, &UMOUIManagerComponent::HandleGhostContextMenuCancelled);
	}

	CurrentBuildTarget = Target;

	// Initialize widget with target building and player inventory
	WidgetInst->InitializeForGhost(Target, BuilderInventory);

	// Show clickable modal background first (clicking it will close the menu)
	ShowModalBackground();

	// Bind modal click to close ghost menu (if not already bound)
	if (UMOModalBackground* Modal = ModalBackgroundWidget.Get())
	{
		Modal->OnBackgroundClicked.RemoveDynamic(this, &UMOUIManagerComponent::HandleGhostContextMenuRequestClose);
		Modal->OnBackgroundClicked.AddDynamic(this, &UMOUIManagerComponent::HandleGhostContextMenuRequestClose);
	}

	// Add menu to viewport above background
	WidgetInst->AddToViewport(GhostContextMenuZOrder);

	// Get viewport size
	int32 ViewportX, ViewportY;
	PlayerController->GetViewportSize(ViewportX, ViewportY);

	// Try to position near the target building by projecting world position to screen
	FVector BoundsOrigin;
	FVector BoundsExtent;
	Target->GetActorBounds(true, BoundsOrigin, BoundsExtent);

	// Use center of object for positioning
	FVector WorldPosition = BoundsOrigin;

	FVector2D ScreenPosition;
	bool bProjected = PlayerController->ProjectWorldLocationToScreen(WorldPosition, ScreenPosition, false);

	UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Projection: World(%.0f, %.0f, %.0f) -> Screen(%.0f, %.0f), success=%d, viewport=(%d, %d)"),
		WorldPosition.X, WorldPosition.Y, WorldPosition.Z,
		ScreenPosition.X, ScreenPosition.Y, bProjected ? 1 : 0,
		ViewportX, ViewportY);

	// Check if the projected position is valid (on screen)
	bool bValidPosition = bProjected &&
		ScreenPosition.X >= 0 && ScreenPosition.X <= ViewportX &&
		ScreenPosition.Y >= 0 && ScreenPosition.Y <= ViewportY;

	if (bValidPosition)
	{
		// Offset to the right so menu doesn't obscure object
		ScreenPosition.X = FMath::Min(ScreenPosition.X + 50.0f, (float)ViewportX - 320.0f);
		ScreenPosition.Y = FMath::Clamp(ScreenPosition.Y - 100.0f, 10.0f, (float)ViewportY - 420.0f);
	}
	else
	{
		// Fallback: center of screen
		ScreenPosition.X = (ViewportX - 300.0f) * 0.5f;
		ScreenPosition.Y = (ViewportY - 400.0f) * 0.5f;
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Using center fallback position"));
	}

	WidgetInst->SetPopupPosition(ScreenPosition);
	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Final menu position: (%.0f, %.0f)"), ScreenPosition.X, ScreenPosition.Y);

	// Set input mode - use Game and UI so keyboard still works
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(WidgetInst->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	PlayerController->SetInputMode(InputMode);
	PlayerController->SetShowMouseCursor(true);

	WidgetInst->SetKeyboardFocus();

	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Ghost Context Menu opened for: %s"), *Target->GetName());
}

void UMOUIManagerComponent::HideBuildWidget()
{
	APlayerController* PlayerController = ResolveOwningPlayerController();

	// Unbind modal background click
	if (UMOModalBackground* Modal = ModalBackgroundWidget.Get())
	{
		Modal->OnBackgroundClicked.RemoveDynamic(this, &UMOUIManagerComponent::HandleGhostContextMenuRequestClose);
	}

	UMOGhostContextMenu* WidgetInst = GhostContextMenuWidget.Get();
	if (IsValid(WidgetInst))
	{
		if (WidgetInst->IsInViewport())
		{
			WidgetInst->RemoveFromParent();
		}
	}

	CurrentBuildTarget.Reset();

	UpdateReticleVisibility();

	// Hide modal and restore input mode
	HideModalBackground();
	if (IsValid(PlayerController) && PlayerController->IsLocalController())
	{
		ApplyInputModeForMenuClosed(PlayerController);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Ghost Context Menu closed"));
}

bool UMOUIManagerComponent::IsBuildWidgetOpen() const
{
	UMOGhostContextMenu* WidgetInst = GhostContextMenuWidget.Get();
	return IsValid(WidgetInst) && WidgetInst->IsInViewport();
}

void UMOUIManagerComponent::HandleGhostContextMenuRequestClose()
{
	HideBuildWidget();
}

void UMOUIManagerComponent::HandleGhostContextMenuBuildStarted()
{
	// Don't close the menu when build starts - let user watch progress
	// The menu will auto-close when build completes or user moves mouse away
	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Build started from Ghost Context Menu"));
}

void UMOUIManagerComponent::HandleGhostContextMenuCancelled()
{
	// The context menu handles cancellation and material dropping internally
	// Just close the widget
	HideBuildWidget();

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Build cancelled from Ghost Context Menu"));
}

// ============================================================================
// STATION CONTEXT MENU
// ============================================================================

void UMOUIManagerComponent::ShowStationContextMenu(AActor* StationActor, FVector WorldPosition)
{
	AMOCraftingStationActor* Station = Cast<AMOCraftingStationActor>(StationActor);
	if (!Station)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] ShowStationContextMenu: Invalid station actor"));
		return;
	}

	if (!StationContextMenuClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] ShowStationContextMenu: No StationContextMenuClass set"));
		return;
	}

	APlayerController* PC = ResolveOwningPlayerController();
	if (!PC)
	{
		return;
	}

	// Close any open menus first
	CloseAllSwitchableMenus();
	HideStationContextMenu();

	// Create widget if needed
	UMOStationContextMenu* WidgetInst = StationContextMenuWidget.Get();
	if (!WidgetInst)
	{
		WidgetInst = CreateWidget<UMOStationContextMenu>(PC, StationContextMenuClass);
		StationContextMenuWidget = WidgetInst;
	}

	if (!WidgetInst)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOUI] Failed to create Station Context Menu widget"));
		return;
	}

	// Bind delegates (remove first to avoid duplicates)
	WidgetInst->OnRequestClose.RemoveDynamic(this, &UMOUIManagerComponent::HandleStationContextMenuRequestClose);
	WidgetInst->OnOpenClicked.RemoveDynamic(this, &UMOUIManagerComponent::HandleStationContextMenuOpen);
	WidgetInst->OnCraftClicked.RemoveDynamic(this, &UMOUIManagerComponent::HandleStationContextMenuCraft);
	WidgetInst->OnLightClicked.RemoveDynamic(this, &UMOUIManagerComponent::HandleStationContextMenuLight);
	WidgetInst->OnRequestClose.AddDynamic(this, &UMOUIManagerComponent::HandleStationContextMenuRequestClose);
	WidgetInst->OnOpenClicked.AddDynamic(this, &UMOUIManagerComponent::HandleStationContextMenuOpen);
	WidgetInst->OnCraftClicked.AddDynamic(this, &UMOUIManagerComponent::HandleStationContextMenuCraft);
	WidgetInst->OnLightClicked.AddDynamic(this, &UMOUIManagerComponent::HandleStationContextMenuLight);

	CurrentStationTarget = Station;

	// Initialize widget with station
	WidgetInst->InitializeForStation(Station);

	// Show modal background
	ShowModalBackground();

	// Bind modal click to close station menu
	if (UMOModalBackground* Modal = ModalBackgroundWidget.Get())
	{
		Modal->OnBackgroundClicked.RemoveDynamic(this, &UMOUIManagerComponent::HandleStationContextMenuRequestClose);
		Modal->OnBackgroundClicked.AddDynamic(this, &UMOUIManagerComponent::HandleStationContextMenuRequestClose);
	}

	// Add to viewport
	WidgetInst->AddToViewport(StationContextMenuZOrder);

	// Position at screen location from world position
	FVector2D ScreenPosition;
	if (PC->ProjectWorldLocationToScreen(WorldPosition, ScreenPosition))
	{
		WidgetInst->SetPopupPosition(ScreenPosition);
	}

	// Set input mode
	ApplyInputModeForMenuOpen(PC, WidgetInst);
	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Showing Station Context Menu for %s"), *Station->GetName());
}

void UMOUIManagerComponent::HideStationContextMenu()
{
	UMOStationContextMenu* WidgetInst = StationContextMenuWidget.Get();
	if (!WidgetInst || !WidgetInst->IsInViewport())
	{
		return;
	}

	WidgetInst->RemoveFromParent();
	CurrentStationTarget = nullptr;

	// Check if we should hide modal background
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
		APlayerController* PC = ResolveOwningPlayerController();
		if (PC)
		{
			ApplyInputModeForMenuClosed(PC);
		}
	}

	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Hiding Station Context Menu"));
}

bool UMOUIManagerComponent::IsStationContextMenuOpen() const
{
	UMOStationContextMenu* WidgetInst = StationContextMenuWidget.Get();
	return WidgetInst && WidgetInst->IsInViewport();
}

void UMOUIManagerComponent::HandleStationContextMenuRequestClose()
{
	HideStationContextMenu();
}

void UMOUIManagerComponent::HandleStationContextMenuOpen()
{
	// Open unified inventory with the station as the container
	AMOCraftingStationActor* Station = CurrentStationTarget.Get();
	if (Station)
	{
		HideStationContextMenu();
		OpenInventoryWithContainer(Station);
	}
}

void UMOUIManagerComponent::HandleStationContextMenuCraft()
{
	// Open crafting menu (station is already set as active via interface)
	AMOCraftingStationActor* Station = CurrentStationTarget.Get();
	if (Station)
	{
		// Set active crafting station on pawn
		APlayerController* PC = ResolveOwningPlayerController();
		if (PC)
		{
			APawn* Pawn = PC->GetPawn();
			if (Pawn && Pawn->Implements<UMOCraftingCapableInterface>())
			{
				IMOCraftingCapableInterface::Execute_SetActiveCraftingStation(Pawn, Station);
			}
		}

		HideStationContextMenu();
		OpenCraftingMenu();
	}
}

void UMOUIManagerComponent::HandleStationContextMenuLight()
{
	// Station lighting is handled by the widget itself
	// Just refresh the display in case we need to update anything
	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Station lit via context menu"));
}

// =============================================================================
// Mode Indicator
// =============================================================================

void UMOUIManagerComponent::CreateModeIndicator()
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

	// Use configured class or default
	TSubclassOf<UMOModeIndicatorWidget> WidgetClass = ModeIndicatorClass;
	if (!WidgetClass)
	{
		WidgetClass = UMOModeIndicatorWidget::StaticClass();
	}

	UMOModeIndicatorWidget* WidgetInst = CreateWidget<UMOModeIndicatorWidget>(PlayerController, WidgetClass);
	if (!IsValid(WidgetInst))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Failed to create Mode Indicator widget"));
		return;
	}

	ModeIndicatorWidget = WidgetInst;
	WidgetInst->AddToViewport(ModeIndicatorZOrder);

	// Set initial mode
	WidgetInst->SetMode(CurrentGameplayMode);

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Mode Indicator created"));
}

void UMOUIManagerComponent::SetGameplayMode(EMOGameplayMode NewMode)
{
	CurrentGameplayMode = NewMode;

	UMOModeIndicatorWidget* WidgetInst = ModeIndicatorWidget.Get();
	if (IsValid(WidgetInst))
	{
		WidgetInst->SetMode(NewMode);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Gameplay mode set to: %s"),
		*UMOModeIndicatorWidget::GetModeDisplayName(NewMode).ToString());
}

EMOGameplayMode UMOUIManagerComponent::GetGameplayMode() const
{
	return CurrentGameplayMode;
}

UMOModeIndicatorWidget* UMOUIManagerComponent::GetModeIndicator() const
{
	return ModeIndicatorWidget.Get();
}

// =============================================================================
// Tool Hint
// =============================================================================

void UMOUIManagerComponent::CreateToolHint()
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

	// Use configured class or default
	TSubclassOf<UMOToolHintWidget> WidgetClass = ToolHintClass;
	if (!WidgetClass)
	{
		WidgetClass = UMOToolHintWidget::StaticClass();
	}

	UMOToolHintWidget* WidgetInst = CreateWidget<UMOToolHintWidget>(PlayerController, WidgetClass);
	if (!IsValid(WidgetInst))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Failed to create Tool Hint widget"));
		return;
	}

	ToolHintWidget = WidgetInst;
	WidgetInst->AddToViewport(ToolHintZOrder);

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Tool Hint created"));
}

void UMOUIManagerComponent::ShowToolHint(const FText& HintText, float Duration)
{
	UMOToolHintWidget* WidgetInst = ToolHintWidget.Get();
	if (IsValid(WidgetInst))
	{
		WidgetInst->ShowHint(HintText, Duration);
	}
}

void UMOUIManagerComponent::HideToolHint()
{
	UMOToolHintWidget* WidgetInst = ToolHintWidget.Get();
	if (IsValid(WidgetInst))
	{
		WidgetInst->HideHint();
	}
}

UMOToolHintWidget* UMOUIManagerComponent::GetToolHintWidget() const
{
	return ToolHintWidget.Get();
}
