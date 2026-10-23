#include "MOCraftingUIController.h"
#include "MOFramework.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"

#include "MOUIManagerComponent.h"
#include "MOCraftingMenu.h"
#include "MOStationContextMenu.h"
#include "MOKeepOnHarvestContextMenu.h"
#include "MOHarvestActions.h"
#include "MOTimedActionComponent.h"
#include "MOCraftingStationActor.h"
#include "MOWorldActionMenu.h"
#include "MOPlayerController.h"
#include "MOCraftingCapableInterface.h"
#include "MOInventoryComponent.h"
#include "MOInventoryHolderInterface.h"
#include "MOSkillsComponent.h"
#include "MOKnowledgeComponent.h"
#include "MOCraftingQueueComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MOModalBackground.h"
#include "MONotificationComponent.h"
#include "MOHarvestSubsystem.h"
#include "MOCraftingSubsystem.h"
#include "MOQuestSubsystem.h"
#include "MORecipeDatabaseSettings.h"
#include "MOResourceDatabaseSettings.h"
#include "MOItemDatabaseSettings.h"
#include "MOPrimaryGameLayout.h"

UMOCraftingUIController::UMOCraftingUIController()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMOCraftingUIController::BeginPlay()
{
	Super::BeginPlay();
}

void UMOCraftingUIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clean up crafting menu widget - unbind delegates first
	if (UMOCraftingMenu* MenuWidget = CraftingMenuWidget.Get())
	{
		MenuWidget->OnRequestClose.RemoveDynamic(this, &UMOCraftingUIController::HandleCraftingMenuRequestClose);
		if (MenuWidget->IsInViewport())
		{
			MenuWidget->RemoveFromParent();
		}
	}
	CraftingMenuWidget.Reset();

	// Clean up station context menu widget - unbind delegates first
	if (UMOStationContextMenu* StationWidget = StationContextMenuWidget.Get())
	{
		StationWidget->OnRequestClose.RemoveDynamic(this, &UMOCraftingUIController::HandleStationContextMenuRequestClose);
		StationWidget->OnOpenClicked.RemoveDynamic(this, &UMOCraftingUIController::HandleStationContextMenuOpen);
		StationWidget->OnCraftClicked.RemoveDynamic(this, &UMOCraftingUIController::HandleStationContextMenuCraft);
		StationWidget->OnLightClicked.RemoveDynamic(this, &UMOCraftingUIController::HandleStationContextMenuLight);
		if (StationWidget->IsInViewport())
		{
			StationWidget->RemoveFromParent();
		}
	}
	StationContextMenuWidget.Reset();
	CurrentStationTarget.Reset();

	// Clean up keep-on-harvest context menu widget - unbind delegates first
	if (UMOKeepOnHarvestContextMenu* HarvestMenuWidget = KeepOnHarvestContextMenuWidget.Get())
	{
		HarvestMenuWidget->OnRequestClose.RemoveDynamic(this, &UMOCraftingUIController::HandleKeepOnHarvestContextMenuRequestClose);
		HarvestMenuWidget->OnInspectClicked.RemoveDynamic(this, &UMOCraftingUIController::HandleKeepOnHarvestContextMenuInspectClicked);
		HarvestMenuWidget->OnHarvestClicked.RemoveDynamic(this, &UMOCraftingUIController::HandleKeepOnHarvestContextMenuHarvestClicked);
		HarvestMenuWidget->OnChopDownClicked.RemoveDynamic(this, &UMOCraftingUIController::HandleKeepOnHarvestContextMenuChopDownClicked);
		if (HarvestMenuWidget->IsInViewport())
		{
			HarvestMenuWidget->RemoveFromParent();
		}
	}
	KeepOnHarvestContextMenuWidget.Reset();

	CurrentHarvestTarget.Reset();

	HideWorldActionMenu();

	Super::EndPlay(EndPlayReason);
}

// =============================================================================
// Crafting Menu
// =============================================================================

void UMOCraftingUIController::ToggleCraftingMenu()
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	// Frame-based debounce: one toggle per frame even if the key reaches this controller twice (added when menus ran in ECommonInputMode::All
	// and both the game action and the menu fired; menus use Menu mode since the May-June rework -- kept as a cheap guard)
	const uint64 CurrentFrame = GFrameCounter;
	if (CurrentFrame == LastToggleFrame)
	{
		return;
	}
	LastToggleFrame = CurrentFrame;

	// Query UIManager for in-game menu state
	UMOUIManagerComponent* UIManager = GetUIManager();
	if (UIManager && UIManager->IsInGameMenuOpen())
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
	if (UIManager)
	{
		UIManager->CloseAllSwitchableMenus();
	}
	OpenCraftingMenu();
}

void UMOCraftingUIController::OpenCraftingMenu()
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
		UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] OpenCraftingMenu - No valid pawn, showing notification"));
		ShowNoPawnNotification();
		return;
	}

	if (!CraftingMenuClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftUI] CraftingMenuClass not set on CraftingUIController."));
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
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftUI] No UMOInventoryComponent found on current pawn."));
		return;
	}

	// Close any existing menu first
	CloseCachedMenu(CraftingMenuWidget);

	// Create new widget via CommonUI layer stack
	ShowModalBackground();
	UCommonActivatableWidget* CreatedWidget = PushWidgetToLayer(MOUILayerTags::Layer_Menu, CraftingMenuClass);
	UMOCraftingMenu* MenuWidget = Cast<UMOCraftingMenu>(CreatedWidget);

	if (!IsValid(MenuWidget))
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOCraftUI] Failed to create crafting menu via layer stack"));
		HideModalBackground();
		return;
	}

	// Cache reference + auto-clear-on-deactivate (any close path). See base class.
	RegisterCachedMenu(MenuWidget, CraftingMenuWidget);
	MenuWidget->OnRequestClose.RemoveAll(this);
	MenuWidget->OnRequestClose.AddDynamic(this, &UMOCraftingUIController::HandleCraftingMenuRequestClose);

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

	// Widget handles input state via NativeOnActivated/GetDesiredInputConfig
	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Crafting menu opened"));
}

void UMOCraftingUIController::CloseCraftingMenu()
{
	// IMPORTANT: Get reference before clearing cache
	// Reset cache FIRST to ensure IsCraftingMenuOpen() returns false immediately
	// This prevents race conditions with toggle input that fires multiple times per frame
	UMOCraftingMenu* MenuWidget = CraftingMenuWidget.Get();
	CraftingMenuWidget.Reset();

	if (IsValid(MenuWidget))
	{
		// Clear station reference to reset UI state
		MenuWidget->SetActiveStationActor(nullptr);

		PopWidgetFromLayer(MenuWidget);   // not gated on IsActivated(): see CloseCachedMenu
	}

	UpdateReticleVisibility();

	// Manage modal background visibility
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Crafting menu closed"));
}

bool UMOCraftingUIController::IsCraftingMenuOpen() const
{
	return IsCachedMenuOpen(CraftingMenuWidget);
}

UMOCraftingMenu* UMOCraftingUIController::GetCraftingMenu() const
{
	return CraftingMenuWidget.Get();
}

void UMOCraftingUIController::HandleCraftingMenuRequestClose()
{
	CloseCraftingMenu();
}

// =============================================================================
// Station Context Menu
// =============================================================================

void UMOCraftingUIController::ShowStationContextMenu(AActor* StationActor, FVector WorldPosition)
{
	AMOCraftingStationActor* Station = Cast<AMOCraftingStationActor>(StationActor);
	if (!Station)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftUI] ShowStationContextMenu: Invalid station actor"));
		return;
	}

	if (!StationContextMenuClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftUI] ShowStationContextMenu: No StationContextMenuClass set"));
		return;
	}

	APlayerController* PC = ResolveOwningPlayerController();
	if (!PC)
	{
		return;
	}

	// Close any open menus first
	UMOUIManagerComponent* UIManager = GetUIManager();
	if (UIManager)
	{
		UIManager->CloseAllSwitchableMenus();
	}
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
		UE_LOG(LogMOFramework, Error, TEXT("[MOCraftUI] Failed to create Station Context Menu widget"));
		return;
	}

	// Bind delegates (remove first to avoid duplicates)
	WidgetInst->OnRequestClose.RemoveDynamic(this, &UMOCraftingUIController::HandleStationContextMenuRequestClose);
	WidgetInst->OnOpenClicked.RemoveDynamic(this, &UMOCraftingUIController::HandleStationContextMenuOpen);
	WidgetInst->OnCraftClicked.RemoveDynamic(this, &UMOCraftingUIController::HandleStationContextMenuCraft);
	WidgetInst->OnLightClicked.RemoveDynamic(this, &UMOCraftingUIController::HandleStationContextMenuLight);
	WidgetInst->OnRequestClose.AddDynamic(this, &UMOCraftingUIController::HandleStationContextMenuRequestClose);
	WidgetInst->OnOpenClicked.AddDynamic(this, &UMOCraftingUIController::HandleStationContextMenuOpen);
	WidgetInst->OnCraftClicked.AddDynamic(this, &UMOCraftingUIController::HandleStationContextMenuCraft);
	WidgetInst->OnLightClicked.AddDynamic(this, &UMOCraftingUIController::HandleStationContextMenuLight);

	CurrentStationTarget = Station;

	// Initialize widget with station
	WidgetInst->InitializeForStation(Station);

	// Show modal background
	ShowModalBackground();

	// Add context menu directly to viewport (context menus are UCommonUserWidget, not activatable)
	WidgetInst->AddToViewport(StationContextMenuZOrder);

	// Position at screen location from world position
	FVector2D ScreenPosition;
	if (PC->ProjectWorldLocationToScreen(WorldPosition, ScreenPosition))
	{
		WidgetInst->SetPopupPosition(ScreenPosition);
	}

	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Showing Station Context Menu for %s"), *Station->GetName());
}

void UMOCraftingUIController::HideStationContextMenu()
{
	UMOStationContextMenu* WidgetInst = StationContextMenuWidget.Get();
	if (!WidgetInst || !WidgetInst->IsInViewport())
	{
		return;
	}

	WidgetInst->RemoveFromParent();
	CurrentStationTarget = nullptr;

	// Widget handles input state restoration via NativeOnDeactivated
	// Just manage modal background visibility
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
	}

	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Hiding Station Context Menu"));
}

bool UMOCraftingUIController::IsStationContextMenuOpen() const
{
	return IsCachedMenuOpen(StationContextMenuWidget);
}

void UMOCraftingUIController::HandleStationContextMenuRequestClose()
{
	HideStationContextMenu();
}

void UMOCraftingUIController::HandleStationContextMenuOpen()
{
	// Open unified inventory with the station as the container
	AMOCraftingStationActor* Station = CurrentStationTarget.Get();
	if (Station)
	{
		HideStationContextMenu();
		// Delegate to UIManager for inventory operations
		if (UMOUIManagerComponent* UIManager = GetUIManager())
		{
			UIManager->OpenInventoryWithContainer(Station);
		}
	}
}

void UMOCraftingUIController::HandleStationContextMenuCraft()
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

void UMOCraftingUIController::HandleStationContextMenuLight()
{
	// Station lighting is handled by the widget itself
	// Just refresh the display in case we need to update anything
	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Station lit via context menu"));
}

// =============================================================================
// KeepOnHarvest Context Menu
// =============================================================================

void UMOCraftingUIController::ShowKeepOnHarvestContextMenu(const FMOInteractionTarget& Target)
{
	if (!Target.IsValid() || !Target.bIsInstancedMeshTarget)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftUI] ShowKeepOnHarvestContextMenu: Invalid or non-ISM target"));
		return;
	}

	if (!KeepOnHarvestContextMenuClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftUI] ShowKeepOnHarvestContextMenu: No KeepOnHarvestContextMenuClass set"));
		return;
	}

	APlayerController* PC = ResolveOwningPlayerController();
	if (!PC)
	{
		return;
	}

	// Close any open menus first
	UMOUIManagerComponent* UIManager = GetUIManager();
	if (UIManager)
	{
		UIManager->CloseAllSwitchableMenus();
	}
	HideKeepOnHarvestContextMenu();

	// Create widget if needed
	UMOKeepOnHarvestContextMenu* WidgetInst = KeepOnHarvestContextMenuWidget.Get();
	if (!WidgetInst)
	{
		WidgetInst = CreateWidget<UMOKeepOnHarvestContextMenu>(PC, KeepOnHarvestContextMenuClass);
		KeepOnHarvestContextMenuWidget = WidgetInst;
	}

	if (!WidgetInst)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOCraftUI] Failed to create KeepOnHarvest Context Menu widget"));
		return;
	}

	// Bind delegates (remove first to avoid duplicates)
	WidgetInst->OnRequestClose.RemoveDynamic(this, &UMOCraftingUIController::HandleKeepOnHarvestContextMenuRequestClose);
	WidgetInst->OnInspectClicked.RemoveDynamic(this, &UMOCraftingUIController::HandleKeepOnHarvestContextMenuInspectClicked);
	WidgetInst->OnHarvestClicked.RemoveDynamic(this, &UMOCraftingUIController::HandleKeepOnHarvestContextMenuHarvestClicked);
	WidgetInst->OnChopDownClicked.RemoveDynamic(this, &UMOCraftingUIController::HandleKeepOnHarvestContextMenuChopDownClicked);
	WidgetInst->OnRequestClose.AddDynamic(this, &UMOCraftingUIController::HandleKeepOnHarvestContextMenuRequestClose);
	WidgetInst->OnInspectClicked.AddDynamic(this, &UMOCraftingUIController::HandleKeepOnHarvestContextMenuInspectClicked);
	WidgetInst->OnHarvestClicked.AddDynamic(this, &UMOCraftingUIController::HandleKeepOnHarvestContextMenuHarvestClicked);
	WidgetInst->OnChopDownClicked.AddDynamic(this, &UMOCraftingUIController::HandleKeepOnHarvestContextMenuChopDownClicked);

	CurrentHarvestTarget = Target;

	// Initialize widget with target and pawn components
	WidgetInst->InitializeForTarget(Target, GetCachedKnowledge(), GetCachedSkills(), GetCachedInventory());

	// Show modal background
	ShowModalBackground();

	// Add context menu directly to viewport (context menus are UCommonUserWidget, not activatable)
	WidgetInst->AddToViewport(KeepOnHarvestContextMenuZOrder);

	// Position at screen location from world position
	FVector2D ScreenPosition;
	if (PC->ProjectWorldLocationToScreen(Target.HitResult.ImpactPoint, ScreenPosition))
	{
		WidgetInst->SetPopupPosition(ScreenPosition);
	}

	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Showing KeepOnHarvest Context Menu"));
}

void UMOCraftingUIController::HideKeepOnHarvestContextMenu()
{
	UMOKeepOnHarvestContextMenu* WidgetInst = KeepOnHarvestContextMenuWidget.Get();
	if (!WidgetInst || !WidgetInst->IsInViewport())
	{
		return;
	}

	WidgetInst->RemoveFromParent();
	CurrentHarvestTarget.Reset();

	// Widget handles input state restoration via NativeOnDeactivated
	// Just manage modal background visibility
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
	}

	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Hiding KeepOnHarvest Context Menu"));
}

bool UMOCraftingUIController::IsKeepOnHarvestContextMenuOpen() const
{
	return IsCachedMenuOpen(KeepOnHarvestContextMenuWidget);
}

// =============================================================================
// Harvest Operations
// =============================================================================

void UMOCraftingUIController::StartHarvestOperation(FName ActionId)
{
	if (!CurrentHarvestTarget.IsValid())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftUI] StartHarvestOperation: No valid harvest target"));
		return;
	}
	AMOPlayerController* PC = Cast<AMOPlayerController>(ResolveOwningPlayerController());
	if (!PC)
	{
		return;
	}
	// The server owns the harvest, from the host and a joiner alike (FMOHarvestActions): it resolves its own copy of the instance, times the action on the
	// pawn (the timed-action bar shows it here) and sends the outcome back (ShowHarvestOutcome). The instance travels as mesh + place: a PCG component has
	// no network identity. (Until Oct 18 2026 this started a progress widget whose UI timer harvested on THIS machine: on a joiner, "Inventory Full!".)
	const FMOInstanceRef Instance = FMOInstanceRef::Make(CurrentHarvestTarget.ISMComponent.Get(), CurrentHarvestTarget.InstanceIndex);
	if (!Instance.IsSet())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftUI] StartHarvestOperation: the target instance is gone"));
		return;
	}
	PC->ServerBeginHarvest(Instance, ActionId);
	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Asked the server to harvest: %s on %s"), *ActionId.ToString(), *Instance.ToString());
}

void UMOCraftingUIController::CancelHarvestOperation()
{
	const APlayerController* PC = ResolveOwningPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UMOTimedActionComponent* Timed = Pawn ? Pawn->FindComponentByClass<UMOTimedActionComponent>() : nullptr;
	if (Timed && Timed->IsActive() && Timed->GetState().ActionId == FMOHarvestActions::TimedActionId)
	{
		Timed->ServerCancel();
	}
}

bool UMOCraftingUIController::IsHarvestInProgress() const
{
	const APlayerController* PC = ResolveOwningPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UMOTimedActionComponent* Timed = Pawn ? Pawn->FindComponentByClass<UMOTimedActionComponent>() : nullptr;
	return Timed && Timed->IsActive() && Timed->GetState().ActionId == FMOHarvestActions::TimedActionId;
}

void UMOCraftingUIController::ShowHarvestOutcome(const FMOHarvestOutcome& Outcome)
{
	UMOUIManagerComponent* UIManager = GetUIManager();
	UMONotificationComponent* NotifComp = UIManager ? UIManager->GetNotificationComponent() : nullptr;
	if (NotifComp)
	{
		// Show notification for produced items
		for (int32 i = 0; i < Outcome.ProducedIds.Num() && i < Outcome.ProducedCounts.Num(); ++i)
		{
			NotifComp->ShowItemPickupNotification(UMOItemDatabaseSettings::GetItemDisplayName(Outcome.ProducedIds[i]), Outcome.ProducedCounts[i]);
		}

		// Distinguish "node is depleted" from "your inventory is full". These are
		// different problems and need different messages — players were dropping
		// items trying to "fix" a full inventory when the actual issue was that
		// the tree/rock had nothing left to give.
		const bool bAnyProduced = Outcome.ProducedIds.Num() > 0;
		if (Outcome.bDropped && !Outcome.bFailed)
		{
			// No room in the pack: what it gave lies at the harvester's feet (BR23-15).
			NotifComp->ShowWarningNotification(NSLOCTEXT("MO", "HarvestDropped", "Your pack is full: the rest is on the ground at your feet."), 5.0f);
		}
		else if (Outcome.bFailed)
		{
			// The inventory add contract refused some output because capacity was exhausted.
			NotifComp->ShowWarningNotification(NSLOCTEXT("MO", "InventoryFull", "Inventory Full! Some items could not be picked up."), 5.0f);
		}
		else if (Outcome.bDepleted && !bAnyProduced)
		{
			// Every yield was skipped due to depletion AND nothing was produced — the node itself is exhausted.
			NotifComp->ShowWarningNotification(NSLOCTEXT("MO", "NodeDepleted", "Nothing left here. Try another resource or wait for it to regrow."), 4.0f);
		}
		else if (Outcome.bOutOfSeason && !bAnyProduced)
		{
			// Everything this action yields is out of season — say so, don't pretend the node is broken or the inventory full.
			NotifComp->ShowWarningNotification(NSLOCTEXT("MO", "OutOfSeason", "Out of season. Nothing to gather here this time of year."), 4.0f);
		}
		// If some yields produced and others depleted, the produced notifications
		// already gave positive feedback — don't pile on a warning.
	}
	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Harvest '%s' finished: produced %d kind(s), failed=%d depleted=%d outOfSeason=%d dropped=%d"), *Outcome.ActionId.ToString(),
		Outcome.ProducedIds.Num(), Outcome.bFailed ? 1 : 0, Outcome.bDepleted ? 1 : 0, Outcome.bOutOfSeason ? 1 : 0, Outcome.bDropped ? 1 : 0);
}

void UMOCraftingUIController::HandleKeepOnHarvestContextMenuRequestClose()
{
	HideKeepOnHarvestContextMenu();
}

void UMOCraftingUIController::HandleKeepOnHarvestContextMenuInspectClicked()
{
	// Get the smart inspect item ID from the menu
	UMOKeepOnHarvestContextMenu* WidgetInst = KeepOnHarvestContextMenuWidget.Get();
	if (!WidgetInst)
	{
		return;
	}

	FName ItemId = WidgetInst->GetSmartInspectItemId();
	if (ItemId.IsNone())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftUI] No item to inspect"));
		return;
	}

	// Delegate inspection to UIManager which will route to CharacterUIController. The thing studied is in the world (the stick on the ground, the
	// tree the bark comes from): the server checks it is within reach by its mesh + place.
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		UIManager->StartItemInspectionAt(ItemId, FGuid(), FMOInstanceRef::Make(CurrentHarvestTarget.ISMComponent.Get(), CurrentHarvestTarget.InstanceIndex));
	}

	// "Study the Forest" (Tutorial_InspectTree): only a TREE counts. This menu also serves bushes, rocks and ground items (a stick picked off the
	// ground is not a tree).
	if (!WidgetInst->IsTreeTarget())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Starting smart inspection for '%s'"), *ItemId.ToString());
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UMOQuestSubsystem* QuestSub = GI->GetSubsystem<UMOQuestSubsystem>())
			{
				QuestSub->FireGameEvent(FName(TEXT("TreeInspected")));
				UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Fired TreeInspected event for quest tracking"));
			}
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Starting smart inspection for '%s'"), *ItemId.ToString());
}

void UMOCraftingUIController::HandleKeepOnHarvestContextMenuHarvestClicked(FName ActionId)
{
	StartHarvestOperation(ActionId);
}

void UMOCraftingUIController::HandleKeepOnHarvestContextMenuChopDownClicked()
{
	// This is also a harvest operation (with bDestroysTarget = true)
	// The recipe ID should come from the menu
	UMOKeepOnHarvestContextMenu* WidgetInst = KeepOnHarvestContextMenuWidget.Get();
	if (!WidgetInst)
	{
		return;
	}

	// Get the chop down recipe from harvest subsystem
	UMOHarvestSubsystem* HarvestSubsystem = GetWorld()->GetSubsystem<UMOHarvestSubsystem>();
	if (!HarvestSubsystem)
	{
		return;
	}

	TArray<FName> TargetTags = HarvestSubsystem->CollectTargetTags(CurrentHarvestTarget.ISMComponent.Get());
	FName ChopDownRecipeId = HarvestSubsystem->GetDestroyRecipeForTags(
		TargetTags,
		GetCachedKnowledge(),
		GetCachedSkills(),
		GetCachedInventory()
	);

	if (!ChopDownRecipeId.IsNone())
	{
		StartHarvestOperation(ChopDownRecipeId);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftUI] No chop down recipe available"));
	}
}

// =============================================================================
// World Actions (IMOWorldActionProvider)
// =============================================================================

void UMOCraftingUIController::ShowWorldActionMenu(AActor* Target, FVector WorldPosition)
{
	APlayerController* PC = ResolveOwningPlayerController();
	UClass* MenuClass = WorldActionMenuClass.LoadSynchronous();
	if (!PC || !IsValid(Target) || !MenuClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftUI] ShowWorldActionMenu: no controller, no target or no menu class (%s)"), *WorldActionMenuClass.ToString());
		return;
	}
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		UIManager->CloseAllSwitchableMenus();
	}
	HideStationContextMenu();
	HideWorldActionMenu();

	UMOWorldActionMenu* Menu = CreateWidget<UMOWorldActionMenu>(PC, MenuClass);
	if (!Menu)
	{
		return;
	}
	if (!Menu->InitializeForTarget(Target))
	{
		if (AMOPlayerController* MOPC = Cast<AMOPlayerController>(PC))
		{
			MOPC->ClientShowNotification(NSLOCTEXT("MOWorldAction", "Nothing", "There is nothing left to do here."), 2.5f, EMONotificationType::Info);
		}
		return;
	}
	WorldActionMenuWidget = Menu;
	Menu->OnCloseRequested.AddDynamic(this, &UMOCraftingUIController::HandleWorldActionMenuClosed);
	ShowModalBackground();
	Menu->AddToViewport(WorldActionMenuZOrder);   // a context menu is a UCommonUserWidget, not a stack entry
	FVector2D ScreenPosition;
	if (PC->ProjectWorldLocationToScreen(WorldPosition, ScreenPosition))
	{
		Menu->SetPopupPosition(ScreenPosition);
	}
	UpdateReticleVisibility();
	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Showing world action menu for %s"), *Target->GetName());
}

void UMOCraftingUIController::HideWorldActionMenu()
{
	UMOWorldActionMenu* Menu = WorldActionMenuWidget.Get();
	WorldActionMenuWidget.Reset();
	if (!Menu)
	{
		return;
	}
	Menu->OnCloseRequested.RemoveAll(this);
	if (Menu->IsInViewport())
	{
		Menu->RemoveFromParent();
	}
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
	}
	UpdateReticleVisibility();
}

bool UMOCraftingUIController::IsWorldActionMenuOpen() const
{
	return IsCachedMenuOpen(WorldActionMenuWidget);
}

void UMOCraftingUIController::HandleWorldActionMenuClosed()
{
	HideWorldActionMenu();
}
