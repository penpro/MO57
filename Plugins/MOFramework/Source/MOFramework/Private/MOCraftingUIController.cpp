#include "MOCraftingUIController.h"
#include "MOFramework.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/UserWidget.h"

#include "MOUIManagerComponent.h"
#include "MOCraftingMenu.h"
#include "MOStationContextMenu.h"
#include "MOKeepOnHarvestContextMenu.h"
#include "MOHarvestProgressWidget.h"
#include "MOCraftingStationActor.h"
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
#include "MORecipeDatabaseSettings.h"
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

void UMOCraftingUIController::StartHarvestOperation(FName RecipeId)
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

	// Get recipe display name
	FText ActionName = NSLOCTEXT("MO", "Harvesting", "Harvesting...");
	if (const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId))
	{
		ActionName = FText::Format(NSLOCTEXT("MO", "HarvestingFormat", "{0}..."), Recipe->DisplayName);
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

	// Start the harvest
	WidgetInst->StartHarvest(
		CurrentHarvestTarget.ISMComponent.Get(),
		CurrentHarvestTarget.InstanceIndex,
		RecipeId,
		ActionName,
		GetCachedInventory(),
		GetCachedSkills()
	);

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Started harvest operation: %s"), *RecipeId.ToString());
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

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftUI] Starting smart inspection for '%s'"), *ItemId.ToString());
}

void UMOCraftingUIController::HandleKeepOnHarvestContextMenuHarvestClicked(FName RecipeId)
{
	StartHarvestOperation(RecipeId);
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
