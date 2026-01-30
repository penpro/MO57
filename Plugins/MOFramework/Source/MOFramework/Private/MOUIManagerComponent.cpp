#include "MOUIManagerComponent.h"
#include "MOFramework.h"

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

UMOUIManagerComponent::UMOUIManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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
	}
}

void UMOUIManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Ensure we restore input mode on teardown if this component dies while menu is open.
	CloseInventoryMenu();

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
	const UMOInventoryMenu* MenuWidget = InventoryMenuWidget.Get();
	return IsValid(MenuWidget) && MenuWidget->IsInViewport();
}

UMOInventoryComponent* UMOUIManagerComponent::ResolveCurrentPawnInventoryComponent() const
{
	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return nullptr;
	}

	APawn* CurrentPawn = PlayerController->GetPawn();
	if (!IsValid(CurrentPawn))
	{
		return nullptr;
	}

	return CurrentPawn->FindComponentByClass<UMOInventoryComponent>();
}

void UMOUIManagerComponent::ToggleInventoryMenu()
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	// Don't allow opening inventory while in-game menu is open
	if (IsInGameMenuOpen())
	{
		return;
	}

	if (IsInventoryMenuOpen())
	{
		CloseInventoryMenu();
		return;
	}

	// Close player status if visible (Tab closes any open UI)
	if (IsPlayerStatusVisible())
	{
		SetPlayerStatusVisible(false);
		return;
	}

	OpenInventoryMenu();
}

void UMOUIManagerComponent::OpenInventoryMenu()
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
	// Don't allow opening player status while in-game menu is open
	if (IsInGameMenuOpen() && !IsPlayerStatusVisible())
	{
		return;
	}

	SetPlayerStatusVisible(!IsPlayerStatusVisible());
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
		// Consume item - apply nutrition to survival stats
		APlayerController* PC = ResolveOwningPlayerController();
		if (IsValid(PC) && IsValid(PC->GetPawn()))
		{
			UMOSurvivalStatsComponent* SurvivalStats = PC->GetPawn()->FindComponentByClass<UMOSurvivalStatsComponent>();
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
				UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] No SurvivalStatsComponent found on pawn"));
			}
		}
	}
	else if (ActionId == FName("Drop1"))
	{
		// Drop single item into world
		DropItemToWorldByGuid(InventoryComponent, ItemGuid);
	}
	else if (ActionId == FName("DropAll"))
	{
		// Drop entire stack into world (DropItemByGuid drops the whole stack)
		DropItemToWorldByGuid(InventoryComponent, ItemGuid);
	}
	else if (ActionId == FName("Inspect"))
	{
		// TODO: Implement inspection - show detailed item info, grant knowledge XP
		UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Inspect action - not yet implemented"));
	}
	else if (ActionId == FName("SplitStack"))
	{
		// TODO: Implement stack splitting UI
		UE_LOG(LogMOFramework, Log, TEXT("[MOUI] SplitStack action - not yet implemented"));
	}
	else if (ActionId == FName("Craft"))
	{
		// TODO: Implement crafting UI filtered to this item
		UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Craft action - not yet implemented"));
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
	return IsInventoryMenuOpen() || IsInGameMenuOpen() || IsItemContextMenuOpen() || IsPlayerStatusVisible();
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

	// Close in-game menu
	UMOInGameMenu* GameMenu = InGameMenuWidget.Get();
	if (IsValid(GameMenu) && GameMenu->IsInViewport())
	{
		GameMenu->RemoveFromParent();
	}

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

	// Trace down to find ground
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		FHitResult HitResult;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(PlayerPawn);

		const FVector TraceStart = DropLocation + FVector(0.0f, 0.0f, 200.0f);
		const FVector TraceEnd = DropLocation - FVector(0.0f, 0.0f, 500.0f);

		if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
		{
			DropLocation = HitResult.Location + FVector(0.0f, 0.0f, 100.0f);
		}
		else
		{
			DropLocation = DropLocation + FVector(0.0f, 0.0f, 100.0f);
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
	OutVitals = nullptr;
	OutMetabolism = nullptr;
	OutMental = nullptr;

	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	APawn* CurrentPawn = PlayerController->GetPawn();
	if (!IsValid(CurrentPawn))
	{
		return;
	}

	OutVitals = CurrentPawn->FindComponentByClass<UMOVitalsComponent>();
	OutMetabolism = CurrentPawn->FindComponentByClass<UMOMetabolismComponent>();
	OutMental = CurrentPawn->FindComponentByClass<UMOMentalStateComponent>();
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

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Status panel rebound to current pawn (Vitals: %s, Metabolism: %s, Mental: %s)"),
		IsValid(Vitals) ? TEXT("Yes") : TEXT("No"),
		IsValid(Metabolism) ? TEXT("Yes") : TEXT("No"),
		IsValid(Mental) ? TEXT("Yes") : TEXT("No"));
}

// =============================================================================
// No Pawn Notification
// =============================================================================

void UMOUIManagerComponent::ShowNoPawnNotification()
{
	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	// Hide any existing notification first
	HideNoPawnNotification();

	// Create notification widget
	UMONotificationWidget* NotificationWidget = CreateWidget<UMONotificationWidget>(PlayerController, UMONotificationWidget::StaticClass());
	if (!IsValid(NotificationWidget))
	{
		return;
	}

	NoPawnNotificationWidget = NotificationWidget;
	NotificationWidget->SetMessage(NoPawnMessage);
	NotificationWidget->AddToViewport(NoPawnNotificationZOrder);

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

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Showing no-pawn notification"));
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
