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
#include "MOHarvestProgressWidget.h"
#include "MOCraftingStationActor.h"
#include "MOCarcassActor.h"
#include "MOCarcassDefinitionRow.h"
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

	// Clean up harvest progress widget - unbind delegates first
	if (UMOHarvestProgressWidget* ProgressWidget = HarvestProgressWidget.Get())
	{
		ProgressWidget->OnHarvestCompleted.RemoveDynamic(this, &UMOCraftingUIController::HandleHarvestCompleted);
		ProgressWidget->OnHarvestCancelled.RemoveDynamic(this, &UMOCraftingUIController::HandleHarvestCancelled);
		if (ProgressWidget->IsInViewport())
		{
			ProgressWidget->RemoveFromParent();
		}
	}
	HarvestProgressWidget.Reset();
	CurrentHarvestTarget.Reset();

	// Clean up carcass butchering
	if (bIsCarcassButchering)
	{
		CancelCarcassButchering();
	}
	CurrentCarcassTarget.Reset();
	CurrentCarcassPartId = NAME_None;

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

	// Create widget if needed
	UMOCraftingMenu* MenuWidget = CraftingMenuWidget.Get();
	if (!IsValid(MenuWidget))
	{
		MenuWidget = CreateWidget<UMOCraftingMenu>(PlayerController, CraftingMenuClass);
		CraftingMenuWidget = MenuWidget;

		if (!IsValid(MenuWidget))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftUI] Failed to create crafting menu widget."));
			return;
		}

		// Bind delegates
		MenuWidget->OnRequestClose.RemoveDynamic(this, &UMOCraftingUIController::HandleCraftingMenuRequestClose);
		MenuWidget->OnRequestClose.AddDynamic(this, &UMOCraftingUIController::HandleCraftingMenuRequestClose);
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

	// Show modal background and menu
	ShowModalBackground();
	MenuWidget->AddToViewport(CraftingMenuZOrder);

	// Set input mode
	ApplyInputModeForMenuOpen(MenuWidget);

	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Crafting menu opened"));
}

void UMOCraftingUIController::CloseCraftingMenu()
{
	APlayerController* PlayerController = ResolveOwningPlayerController();

	UMOCraftingMenu* MenuWidget = CraftingMenuWidget.Get();
	if (IsValid(MenuWidget))
	{
		// Clear station reference to reset UI state
		MenuWidget->SetActiveStationActor(nullptr);

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
			ApplyInputModeForMenuClosed();
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Crafting menu closed"));
}

bool UMOCraftingUIController::IsCraftingMenuOpen() const
{
	UMOCraftingMenu* MenuWidget = CraftingMenuWidget.Get();
	return IsValid(MenuWidget) && MenuWidget->IsInViewport();
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

	// Add to viewport
	WidgetInst->AddToViewport(StationContextMenuZOrder);

	// Position at screen location from world position
	FVector2D ScreenPosition;
	if (PC->ProjectWorldLocationToScreen(WorldPosition, ScreenPosition))
	{
		WidgetInst->SetPopupPosition(ScreenPosition);
	}

	// Set input mode
	ApplyInputModeForMenuOpen(WidgetInst);
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

	// Check if we should hide modal background
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
		APlayerController* PC = ResolveOwningPlayerController();
		if (PC)
		{
			ApplyInputModeForMenuClosed();
		}
	}

	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Hiding Station Context Menu"));
}

bool UMOCraftingUIController::IsStationContextMenuOpen() const
{
	UMOStationContextMenu* WidgetInst = StationContextMenuWidget.Get();
	return WidgetInst && WidgetInst->IsInViewport();
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

	// Add to viewport
	WidgetInst->AddToViewport(KeepOnHarvestContextMenuZOrder);

	// Position at screen location from world position
	FVector2D ScreenPosition;
	if (PC->ProjectWorldLocationToScreen(Target.HitResult.ImpactPoint, ScreenPosition))
	{
		WidgetInst->SetPopupPosition(ScreenPosition);
	}

	// Set input mode
	ApplyInputModeForMenuOpen(WidgetInst);
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

	// Check if we should hide modal background
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
		APlayerController* PC = ResolveOwningPlayerController();
		if (PC)
		{
			ApplyInputModeForMenuClosed();
		}
	}

	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Hiding KeepOnHarvest Context Menu"));
}

bool UMOCraftingUIController::IsKeepOnHarvestContextMenuOpen() const
{
	UMOKeepOnHarvestContextMenu* WidgetInst = KeepOnHarvestContextMenuWidget.Get();
	return WidgetInst && WidgetInst->IsInViewport();
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

	if (!HarvestProgressWidgetClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftUI] StartHarvestOperation: No HarvestProgressWidgetClass set"));
		return;
	}

	APlayerController* PC = ResolveOwningPlayerController();
	if (!PC)
	{
		return;
	}

	// Get action display name from resource definition (single source of truth)
	FText ActionDisplayName = NSLOCTEXT("MO", "Harvesting", "Harvesting...");
	UMOHarvestSubsystem* HarvestSubsystem = GetWorld()->GetSubsystem<UMOHarvestSubsystem>();
	if (HarvestSubsystem)
	{
		TArray<FName> TargetTags = HarvestSubsystem->CollectTargetTags(CurrentHarvestTarget.ISMComponent.Get());
		FName ResourceNodeId = HarvestSubsystem->ExtractResourceNodeId(TargetTags);
		if (!ResourceNodeId.IsNone())
		{
			if (const FMOResourceNodeDefinitionRow* ResourceDef = UMOResourceDatabaseSettings::GetResourceDefinition(ResourceNodeId))
			{
				if (const FMOResourceHarvestAction* Action = ResourceDef->FindAction(ActionId))
				{
					ActionDisplayName = FText::Format(NSLOCTEXT("MO", "HarvestingFormat", "{0}..."), Action->DisplayName);
				}
			}
		}
	}

	// Create progress widget if needed
	UMOHarvestProgressWidget* WidgetInst = HarvestProgressWidget.Get();
	if (!WidgetInst)
	{
		WidgetInst = CreateWidget<UMOHarvestProgressWidget>(PC, HarvestProgressWidgetClass);
		HarvestProgressWidget = WidgetInst;
	}

	if (!WidgetInst)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOCraftUI] Failed to create Harvest Progress widget"));
		return;
	}

	// Bind delegates
	WidgetInst->OnHarvestCompleted.RemoveDynamic(this, &UMOCraftingUIController::HandleHarvestCompleted);
	WidgetInst->OnHarvestCancelled.RemoveDynamic(this, &UMOCraftingUIController::HandleHarvestCancelled);
	WidgetInst->OnHarvestCompleted.AddDynamic(this, &UMOCraftingUIController::HandleHarvestCompleted);
	WidgetInst->OnHarvestCancelled.AddDynamic(this, &UMOCraftingUIController::HandleHarvestCancelled);

	// Add to viewport
	WidgetInst->AddToViewport(HarvestProgressZOrder);

	// Start the harvest with ActionId (MOHarvestSubsystem will look up the action from resource definition)
	WidgetInst->StartHarvest(
		CurrentHarvestTarget.ISMComponent.Get(),
		CurrentHarvestTarget.InstanceIndex,
		ActionId,
		ActionDisplayName,
		GetCachedInventory(),
		GetCachedSkills()
	);

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Started harvest operation: %s"), *ActionId.ToString());
}

void UMOCraftingUIController::CancelHarvestOperation()
{
	UMOHarvestProgressWidget* WidgetInst = HarvestProgressWidget.Get();
	if (WidgetInst && WidgetInst->IsHarvestInProgress())
	{
		WidgetInst->CancelHarvest();
	}
}

bool UMOCraftingUIController::IsHarvestInProgress() const
{
	UMOHarvestProgressWidget* WidgetInst = HarvestProgressWidget.Get();
	return WidgetInst && WidgetInst->IsHarvestInProgress();
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

	// Delegate inspection to UIManager which will route to CharacterUIController
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		UIManager->StartItemInspection(ItemId, FGuid());
	}

	// Fire TreeInspected event for quest objectives
	// This triggers when inspecting via the harvest context menu (trees, rocks, etc.)
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

void UMOCraftingUIController::HandleHarvestCompleted(bool bCompleted, const FMOCraftResult& Result)
{
	UMOHarvestProgressWidget* WidgetInst = HarvestProgressWidget.Get();
	if (WidgetInst && WidgetInst->IsInViewport())
	{
		WidgetInst->RemoveFromParent();
	}

	CurrentHarvestTarget.Reset();

	if (bCompleted && Result.bSuccess)
	{
		// Get notification component via UIManager
		if (UMOUIManagerComponent* UIManager = GetUIManager())
		{
			UMONotificationComponent* NotifComp = UIManager->GetNotificationComponent();
			if (NotifComp)
			{
				// Show notification for produced items
				for (const auto& Pair : Result.ProducedItems)
				{
					FText ItemName = UMOItemDatabaseSettings::GetItemDisplayName(Pair.Key);
					NotifComp->ShowItemPickupNotification(ItemName, Pair.Value);
				}

				// Show "Inventory Full" notification for items that couldn't be added
				if (Result.HasFailedItems())
				{
					NotifComp->ShowWarningNotification(
						NSLOCTEXT("MO", "InventoryFull", "Inventory Full! Some items could not be picked up."),
						5.0f
					);
				}
			}
		}
	}

	// Restore input mode if no other menus are open
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
		ApplyInputModeForMenuClosed();
	}
	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Harvest completed: %s, FailedItems=%d"),
		bCompleted ? TEXT("success") : TEXT("cancelled"),
		Result.FailedItems.Num());
}

void UMOCraftingUIController::HandleHarvestCancelled()
{
	UMOHarvestProgressWidget* WidgetInst = HarvestProgressWidget.Get();
	if (WidgetInst && WidgetInst->IsInViewport())
	{
		WidgetInst->RemoveFromParent();
	}

	CurrentHarvestTarget.Reset();

	// Restore input mode if no other menus are open
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
		ApplyInputModeForMenuClosed();
	}
	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Harvest cancelled"));
}

// =============================================================================
// Carcass Butchering
// =============================================================================

void UMOCraftingUIController::StartCarcassButchering(AMOCarcassActor* Carcass, FName PartId)
{
	if (!IsValid(Carcass))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftUI] StartCarcassButchering: Invalid carcass"));
		return;
	}

	if (PartId.IsNone())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftUI] StartCarcassButchering: No part specified"));
		return;
	}

	APlayerController* PC = ResolveOwningPlayerController();
	if (!PC)
	{
		return;
	}

	// Cancel any existing butchering
	if (bIsCarcassButchering)
	{
		CancelCarcassButchering();
	}

	// Get part definition for display name and time
	const FMOCarcassDefinitionRow* CarcassDef = Carcass->GetCarcassDefinitionRow();
	if (!CarcassDef)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftUI] StartCarcassButchering: No carcass definition"));
		return;
	}

	const FMOCarcassPartEntry* Part = CarcassDef->FindPart(PartId);
	if (!Part)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftUI] StartCarcassButchering: Part '%s' not found"), *PartId.ToString());
		return;
	}

	// Start harvest on carcass actor
	if (!Carcass->StartHarvestPart(PartId, PC))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftUI] StartCarcassButchering: Failed to start harvest"));
		return;
	}

	// Store state
	CurrentCarcassTarget = Carcass;
	CurrentCarcassPartId = PartId;
	bIsCarcassButchering = true;
	CarcassButcherElapsed = 0.0f;
	CarcassButcherTotal = Part->HarvestTime;

	// Bind carcass delegates
	Carcass->OnHarvestProgress.AddDynamic(this, &UMOCraftingUIController::HandleCarcassHarvestProgress);
	Carcass->OnPartHarvested.AddDynamic(this, &UMOCraftingUIController::HandleCarcassPartHarvested);
	Carcass->OnHarvestCancelled.AddDynamic(this, &UMOCraftingUIController::HandleCarcassHarvestCancelled);

	// Create/show progress widget
	if (!HarvestProgressWidgetClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftUI] StartCarcassButchering: No HarvestProgressWidgetClass set"));
		CancelCarcassButchering();
		return;
	}

	UMOHarvestProgressWidget* WidgetInst = HarvestProgressWidget.Get();
	if (!WidgetInst)
	{
		WidgetInst = CreateWidget<UMOHarvestProgressWidget>(PC, HarvestProgressWidgetClass);
		HarvestProgressWidget = WidgetInst;
	}

	if (!WidgetInst)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOCraftUI] Failed to create Harvest Progress widget for carcass"));
		CancelCarcassButchering();
		return;
	}

	// Bind widget delegates for cancel
	WidgetInst->OnHarvestCancelled.RemoveDynamic(this, &UMOCraftingUIController::HandleCarcassHarvestCancelled);
	WidgetInst->OnHarvestCancelled.AddDynamic(this, &UMOCraftingUIController::HandleCarcassHarvestCancelled);

	// Add to viewport
	WidgetInst->AddToViewport(HarvestProgressZOrder);

	// Show modal background and lock input
	ShowModalBackground();
	ApplyInputModeForMenuOpen(WidgetInst);
	UpdateReticleVisibility();

	// Start timer for UI updates (carcass actor handles actual progress)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CarcassButcherTimerHandle);
		World->GetTimerManager().SetTimer(
			CarcassButcherTimerHandle,
			this,
			&UMOCraftingUIController::TickCarcassButchering,
			0.05f,  // 20 Hz for smooth progress
			true
		);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Started carcass butchering: %s (%.1fs)"),
		*PartId.ToString(), CarcassButcherTotal);
}

void UMOCraftingUIController::CancelCarcassButchering()
{
	if (!bIsCarcassButchering)
	{
		return;
	}

	// Clear timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CarcassButcherTimerHandle);
	}

	// Cancel on carcass actor
	if (AMOCarcassActor* Carcass = CurrentCarcassTarget.Get())
	{
		// Unbind delegates
		Carcass->OnHarvestProgress.RemoveDynamic(this, &UMOCraftingUIController::HandleCarcassHarvestProgress);
		Carcass->OnPartHarvested.RemoveDynamic(this, &UMOCraftingUIController::HandleCarcassPartHarvested);
		Carcass->OnHarvestCancelled.RemoveDynamic(this, &UMOCraftingUIController::HandleCarcassHarvestCancelled);

		Carcass->CancelHarvest();
	}

	// Hide progress widget
	UMOHarvestProgressWidget* WidgetInst = HarvestProgressWidget.Get();
	if (WidgetInst && WidgetInst->IsInViewport())
	{
		WidgetInst->RemoveFromParent();
	}

	// Reset state
	bIsCarcassButchering = false;
	CurrentCarcassTarget.Reset();
	CurrentCarcassPartId = NAME_None;

	// Restore input mode
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
		ApplyInputModeForMenuClosed();
	}
	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Carcass butchering cancelled"));
}

bool UMOCraftingUIController::IsCarcassButcheringInProgress() const
{
	return bIsCarcassButchering;
}

void UMOCraftingUIController::TickCarcassButchering()
{
	if (!bIsCarcassButchering)
	{
		return;
	}

	AMOCarcassActor* Carcass = CurrentCarcassTarget.Get();
	if (!Carcass)
	{
		CancelCarcassButchering();
		return;
	}

	// Get progress from carcass actor
	float Progress = Carcass->GetHarvestProgress();
	float TimeRemaining = CarcassButcherTotal * (1.0f - Progress);

	// Update widget display
	UMOHarvestProgressWidget* WidgetInst = HarvestProgressWidget.Get();
	if (WidgetInst)
	{
		// Get part display name
		FText ActionName = FText::FromName(CurrentCarcassPartId);
		if (const FMOCarcassDefinitionRow* Def = Carcass->GetCarcassDefinitionRow())
		{
			if (const FMOCarcassPartEntry* Part = Def->FindPart(CurrentCarcassPartId))
			{
				ActionName = FText::Format(NSLOCTEXT("MO", "ButcheringFormat", "Butchering {0}..."), Part->DisplayName);
			}
		}

		WidgetInst->UpdateDisplay(Progress, TimeRemaining, ActionName);
	}
}

void UMOCraftingUIController::HandleCarcassHarvestProgress(float Progress, float TotalTime)
{
	// Progress is handled by TickCarcassButchering, this is just for logging
	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCraftUI] Carcass harvest progress: %.1f/%.1f"), Progress, TotalTime);
}

void UMOCraftingUIController::HandleCarcassPartHarvested(FName PartId, bool bSuccess)
{
	if (!bIsCarcassButchering || PartId != CurrentCarcassPartId)
	{
		return;
	}

	// Clear timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CarcassButcherTimerHandle);
	}

	// Unbind delegates from carcass
	if (AMOCarcassActor* Carcass = CurrentCarcassTarget.Get())
	{
		Carcass->OnHarvestProgress.RemoveDynamic(this, &UMOCraftingUIController::HandleCarcassHarvestProgress);
		Carcass->OnPartHarvested.RemoveDynamic(this, &UMOCraftingUIController::HandleCarcassPartHarvested);
		Carcass->OnHarvestCancelled.RemoveDynamic(this, &UMOCraftingUIController::HandleCarcassHarvestCancelled);
	}

	// Hide progress widget
	UMOHarvestProgressWidget* WidgetInst = HarvestProgressWidget.Get();
	if (WidgetInst && WidgetInst->IsInViewport())
	{
		WidgetInst->RemoveFromParent();
	}

	// Show success notification
	if (bSuccess)
	{
		if (UMOUIManagerComponent* UIManager = GetUIManager())
		{
			UMONotificationComponent* NotifComp = UIManager->GetNotificationComponent();
			if (NotifComp)
			{
				// Get part info for notification
				if (AMOCarcassActor* Carcass = CurrentCarcassTarget.Get())
				{
					if (const FMOCarcassDefinitionRow* Def = Carcass->GetCarcassDefinitionRow())
					{
						if (const FMOCarcassPartEntry* Part = Def->FindPart(PartId))
						{
							FText ItemName = UMOItemDatabaseSettings::GetItemDisplayName(Part->GetItemForStage(Carcass->GetDecayStage()));
							NotifComp->ShowItemPickupNotification(ItemName, Part->Quantity);
						}
					}
				}
			}
		}
	}

	// Reset state
	bIsCarcassButchering = false;
	CurrentCarcassTarget.Reset();
	CurrentCarcassPartId = NAME_None;

	// Restore input mode
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
		ApplyInputModeForMenuClosed();
	}
	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Carcass part harvested: %s (success: %s)"),
		*PartId.ToString(), bSuccess ? TEXT("true") : TEXT("false"));
}

void UMOCraftingUIController::HandleCarcassHarvestCancelled()
{
	// This is called when the carcass actor's harvest is cancelled
	// (e.g., player moved out of range, or widget cancel button clicked)
	CancelCarcassButchering();
}
