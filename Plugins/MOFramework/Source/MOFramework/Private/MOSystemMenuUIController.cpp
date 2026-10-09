#include "MOSystemMenuUIController.h"
#include "MOFramework.h"
#include "MOTravelUtils.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

#include "MOUIManagerComponent.h"
#include "MOInGameMenu.h"
#include "MOPossessionMenu.h"
#include "MOConfirmationDialog.h"
#include "MOSurvivorContextMenu.h"
#include "MOSurvivorTaskMenu.h"
#include "MODeathRecapWidget.h"
#include "MOCharacter.h"
#include "MOPersistenceSubsystem.h"
#include "MOPossessionSubsystem.h"
#include "MOPossessionComponent.h"
#include "MOIdentityRegistrySubsystem.h"
#include "MOIdentityComponent.h"
#include "MOIdentifiableInterface.h"
#include "MORecruitmentComponent.h"
#include "MOGameSettings.h"
#include "MOInventoryUIController.h"
#include "EngineUtils.h"
#include "MOPrimaryGameLayout.h"

UMOSystemMenuUIController::UMOSystemMenuUIController()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMOSystemMenuUIController::BeginPlay()
{
	Super::BeginPlay();
}

void UMOSystemMenuUIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clean up in-game menu widget - unbind delegates first to prevent callbacks during destruction
	if (UMOInGameMenu* MenuWidget = InGameMenuWidget.Get())
	{
		MenuWidget->OnRequestClose.RemoveDynamic(this, &UMOSystemMenuUIController::HandleInGameMenuRequestClose);
		MenuWidget->OnExitToMainMenu.RemoveDynamic(this, &UMOSystemMenuUIController::HandleInGameMenuExitToMainMenu);
		MenuWidget->OnExitGame.RemoveDynamic(this, &UMOSystemMenuUIController::HandleInGameMenuExitGame);
		MenuWidget->OnSaveRequested.RemoveDynamic(this, &UMOSystemMenuUIController::HandleSaveRequested);
		MenuWidget->OnLoadRequested.RemoveDynamic(this, &UMOSystemMenuUIController::HandleLoadRequested);
		if (MenuWidget->IsInViewport())
		{
			MenuWidget->RemoveFromParent();
		}
	}
	InGameMenuWidget.Reset();

	// Clean up possession menu widget - unbind delegates first
	if (UMOPossessionMenu* PossessionWidget = PossessionMenuWidget.Get())
	{
		PossessionWidget->OnRequestClose.RemoveDynamic(this, &UMOSystemMenuUIController::HandlePossessionMenuRequestClose);
		PossessionWidget->OnPawnSelected.RemoveDynamic(this, &UMOSystemMenuUIController::HandlePossessionMenuPawnSelected);
		PossessionWidget->OnCreateCharacter.RemoveDynamic(this, &UMOSystemMenuUIController::HandlePossessionMenuCreateCharacter);
		if (PossessionWidget->IsInViewport())
		{
			PossessionWidget->RemoveFromParent();
		}
	}
	PossessionMenuWidget.Reset();

	// Clean up confirmation dialog widget - unbind delegates first
	if (UMOConfirmationDialog* DialogWidget = ConfirmationDialogWidget.Get())
	{
		DialogWidget->OnConfirmed.RemoveDynamic(this, &UMOSystemMenuUIController::HandleConfirmationConfirmed);
		DialogWidget->OnCancelled.RemoveDynamic(this, &UMOSystemMenuUIController::HandleConfirmationCancelled);
		if (DialogWidget->IsInViewport())
		{
			DialogWidget->RemoveFromParent();
		}
	}
	ConfirmationDialogWidget.Reset();

	// Clean up survivor context menu widget - unbind delegates first
	if (UMOSurvivorContextMenu* ContextWidget = SurvivorContextMenuWidget.Get())
	{
		ContextWidget->OnCloseRequested.RemoveDynamic(this, &UMOSystemMenuUIController::HandleSurvivorContextMenuRequestClose);
		ContextWidget->OnOpenTasksRequested.RemoveDynamic(this, &UMOSystemMenuUIController::HandleSurvivorContextMenuOpenTasks);
		ContextWidget->OnInventoryRequested.RemoveDynamic(this, &UMOSystemMenuUIController::HandleSurvivorContextMenuOpenInventory);
		if (ContextWidget->IsInViewport())
		{
			ContextWidget->RemoveFromParent();
		}
	}
	SurvivorContextMenuWidget.Reset();

	// Clean up survivor task menu widget - unbind delegates first
	if (UMOSurvivorTaskMenu* TaskWidget = SurvivorTaskMenuWidget.Get())
	{
		TaskWidget->OnRequestClose.RemoveDynamic(this, &UMOSystemMenuUIController::HandleSurvivorTaskMenuRequestClose);
		if (TaskWidget->IsInViewport())
		{
			TaskWidget->RemoveFromParent();
		}
	}
	SurvivorTaskMenuWidget.Reset();

	CurrentSurvivorTarget.Reset();

	// Death recap — unbind from the subscribed pawn (if any) and drop the
	// widget cache. The widget itself is on Layer_Modal and will be cleaned
	// up by the layer stack when the level tears down.
	UnbindDeathListener();
	DeathRecapWidget.Reset();

	Super::EndPlay(EndPlayReason);
}

// =============================================================================
// In-Game Menu
// =============================================================================

void UMOSystemMenuUIController::ToggleInGameMenu()
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	// Frame-based debounce: prevent double-toggle from ECommonInputMode::All
	const uint64 CurrentFrame = GFrameCounter;
	if (CurrentFrame == LastToggleFrame)
	{
		return;
	}
	LastToggleFrame = CurrentFrame;

	UMOUIManagerComponent* UIManager = GetUIManager();

	// If any other menu is open, close it first
	if (UIManager && UIManager->IsInventoryMenuOpen())
	{
		UIManager->CloseInventoryMenu();
		return;
	}

	if (UIManager && UIManager->IsItemContextMenuOpen())
	{
		UIManager->CloseItemContextMenu();
		return;
	}

	if (UIManager && UIManager->IsPlayerStatusVisible())
	{
		UIManager->SetPlayerStatusVisible(false);
		return;
	}

	if (UIManager && UIManager->IsCraftingMenuOpen())
	{
		UIManager->CloseCraftingMenu();
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

void UMOSystemMenuUIController::OpenInGameMenu()
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
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] InGameMenuClass not set on SystemMenuUIController."));
		return;
	}

	// Close any existing menu first
	UMOInGameMenu* ExistingMenu = InGameMenuWidget.Get();
	if (IsValid(ExistingMenu) && ExistingMenu->IsActivated())
	{
		PopWidgetFromLayer(ExistingMenu);
		InGameMenuWidget.Reset();
	}

	// Push to MENU layer, not Modal. Per MOPrimaryGameLayout layer design:
	//   Menu (150)  → system menus (in-game menu, possession menu)
	//   Modal (200) → dialogs that stack ABOVE menus (confirmations, text input)
	//
	// Previously pushed to Modal — which put the in-game menu on the same
	// layer as the dialogs spawned from it (save rename/delete, exit confirm).
	// Pushing a dialog to the same stack deactivates the in-game menu under it,
	// and CommonUI's RemainingActive lookup then misses the menu when the
	// dialog closes, dumping the player into "last UI standing" game-only
	// mode. Putting the menu on Menu layer lets dialogs cleanly stack on top
	// of it and pop without dragging the menu down.
	//
	// bIsModal=true on UMOModalWidget still blocks gameplay input regardless
	// of which layer it lives on — it doesn't require Modal layer.
	ShowModalBackground();
	UCommonActivatableWidget* CreatedWidget = PushWidgetToLayer(MOUILayerTags::Layer_Menu, InGameMenuClass);
	UMOInGameMenu* MenuWidget = Cast<UMOInGameMenu>(CreatedWidget);

	if (!IsValid(MenuWidget))
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOSysUI] Failed to create in-game menu via layer stack"));
		HideModalBackground();
		return;
	}

	// Cache reference + auto-clear-on-deactivate (any close path). See base class.
	RegisterCachedMenu(MenuWidget, InGameMenuWidget);
	MenuWidget->OnRequestClose.RemoveAll(this);
	MenuWidget->OnExitToMainMenu.RemoveAll(this);
	MenuWidget->OnExitGame.RemoveAll(this);
	MenuWidget->OnSaveRequested.RemoveAll(this);
	MenuWidget->OnLoadRequested.RemoveAll(this);
	MenuWidget->OnRequestClose.AddDynamic(this, &UMOSystemMenuUIController::HandleInGameMenuRequestClose);
	MenuWidget->OnExitToMainMenu.AddDynamic(this, &UMOSystemMenuUIController::HandleInGameMenuExitToMainMenu);
	MenuWidget->OnExitGame.AddDynamic(this, &UMOSystemMenuUIController::HandleInGameMenuExitGame);
	MenuWidget->OnSaveRequested.AddDynamic(this, &UMOSystemMenuUIController::HandleSaveRequested);
	MenuWidget->OnLoadRequested.AddDynamic(this, &UMOSystemMenuUIController::HandleLoadRequested);

	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] InGameMenu opened via layer stack"));

	// Widget handles input state via NativeOnActivated/GetDesiredInputConfig
	UpdateReticleVisibility();
}

void UMOSystemMenuUIController::CloseInGameMenu()
{
	// IMPORTANT: Get reference before clearing cache
	// Reset cache FIRST to ensure IsInGameMenuOpen() returns false immediately
	// This prevents race conditions with toggle input that fires multiple times per frame
	UMOInGameMenu* MenuWidget = InGameMenuWidget.Get();
	InGameMenuWidget.Reset();

	if (IsValid(MenuWidget) && MenuWidget->IsActivated())
	{
		PopWidgetFromLayer(MenuWidget);
	}

	UpdateReticleVisibility();

	// Manage modal background visibility
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
	}
}

bool UMOSystemMenuUIController::IsInGameMenuOpen() const
{
	return IsCachedMenuOpen(InGameMenuWidget);
}

UMOInGameMenu* UMOSystemMenuUIController::GetInGameMenu() const
{
	return InGameMenuWidget.Get();
}

void UMOSystemMenuUIController::HandleInGameMenuRequestClose()
{
	CloseInGameMenu();
}

void UMOSystemMenuUIController::HandleInGameMenuExitToMainMenu()
{
	PendingConfirmationContext = TEXT("ExitToMainMenu");
	ShowConfirmationDialog(
		NSLOCTEXT("MO", "ExitToMainMenuTitle", "Exit to Main Menu"),
		NSLOCTEXT("MO", "ExitToMainMenuMessage", "Are you sure you want to exit to the main menu? Unsaved progress will be lost."),
		NSLOCTEXT("MO", "Exit", "Exit"),
		NSLOCTEXT("MO", "Cancel", "Cancel")
	);
}

void UMOSystemMenuUIController::HandleInGameMenuExitGame()
{
	PendingConfirmationContext = TEXT("ExitGame");
	ShowConfirmationDialog(
		NSLOCTEXT("MO", "ExitGameTitle", "Exit Game"),
		NSLOCTEXT("MO", "ExitGameMessage", "Are you sure you want to quit the game? Unsaved progress will be lost."),
		NSLOCTEXT("MO", "Quit", "Quit"),
		NSLOCTEXT("MO", "Cancel", "Cancel")
	);
}

void UMOSystemMenuUIController::HandleSaveRequested(const FString& SlotName)
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] *** HANDLE SAVE REQUESTED: %s ***"), *SlotName);

	// Check if slot exists for overwrite confirmation
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
	if (GameInstance)
	{
		UMOPersistenceSubsystem* Persistence = GameInstance->GetSubsystem<UMOPersistenceSubsystem>();
		if (Persistence)
		{
			const bool bSlotExists = Persistence->DoesSaveSlotExist(SlotName);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] Slot '%s' exists: %s"), *SlotName, bSlotExists ? TEXT("YES") : TEXT("NO"));

			if (bSlotExists)
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Showing overwrite confirmation"));
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
			UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Saving to new slot (no confirmation needed): %s"), *SlotName);
			bool bSaveSuccess = Persistence->SaveWorldToSlot(SlotName);
			UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Save complete (success: %s)"), bSaveSuccess ? TEXT("YES") : TEXT("NO"));

			// Refresh both panels to show the new save
			if (bSaveSuccess)
			{
				UMOInGameMenu* MenuWidget = InGameMenuWidget.Get();
				if (IsValid(MenuWidget))
				{
					MenuWidget->RefreshSavePanelList();
					MenuWidget->RefreshLoadPanelList();
					UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Save and load panels refreshed"));
				}
			}
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] Persistence subsystem is NULL"));
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] GameInstance is NULL in HandleSaveRequested"));
	}
}

void UMOSystemMenuUIController::HandleLoadRequested(const FString& SlotName)
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
// Possession Menu
// =============================================================================

void UMOSystemMenuUIController::TogglePossessionMenu()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] TogglePossessionMenu called"));

	// Frame-based debounce: prevent double-toggle from ECommonInputMode::All
	const uint64 CurrentFrame = GFrameCounter;
	if (CurrentFrame == LastToggleFrame)
	{
		return;
	}
	LastToggleFrame = CurrentFrame;

	if (!IsLocalOwningPlayerController())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] TogglePossessionMenu - Not local owning player controller, aborting"));
		return;
	}

	// If already open, just close it
	if (IsPossessionMenuOpen())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] TogglePossessionMenu - Menu is open, closing"));
		ClosePossessionMenu();
		return;
	}

	// Possession menu can also close the in-game menu (special case)
	if (IsInGameMenuOpen())
	{
		CloseInGameMenu();
	}

	// Close other switchable menus and open this one
	UMOUIManagerComponent* UIManager = GetUIManager();
	if (UIManager)
	{
		UIManager->CloseAllSwitchableMenus();
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] TogglePossessionMenu - Opening"));
	OpenPossessionMenu();
}

void UMOSystemMenuUIController::OpenPossessionMenu()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] OpenPossessionMenu called"));

	if (!IsLocalOwningPlayerController())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] OpenPossessionMenu - Not local owning player controller"));
		return;
	}

	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] OpenPossessionMenu - PlayerController invalid"));
		return;
	}

	if (!PossessionMenuClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] PossessionMenuClass not set on SystemMenuUIController."));
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] OpenPossessionMenu - All checks passed, creating menu"));

	// Close any existing menu first
	UMOPossessionMenu* ExistingMenu = PossessionMenuWidget.Get();
	if (IsValid(ExistingMenu) && ExistingMenu->IsActivated())
	{
		PopWidgetFromLayer(ExistingMenu);
		PossessionMenuWidget.Reset();
	}

	// Create new widget via CommonUI layer stack
	ShowModalBackground();
	UCommonActivatableWidget* CreatedWidget = PushWidgetToLayer(MOUILayerTags::Layer_Menu, PossessionMenuClass);
	UMOPossessionMenu* MenuWidget = Cast<UMOPossessionMenu>(CreatedWidget);

	if (!IsValid(MenuWidget))
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOSysUI] Failed to create possession menu via layer stack"));
		HideModalBackground();
		return;
	}

	// Cache reference + auto-clear-on-deactivate (any close path). See base class.
	RegisterCachedMenu(MenuWidget, PossessionMenuWidget);
	MenuWidget->OnRequestClose.RemoveAll(this);
	MenuWidget->OnPawnSelected.RemoveAll(this);
	MenuWidget->OnCreateCharacter.RemoveAll(this);
	MenuWidget->OnRequestClose.AddDynamic(this, &UMOSystemMenuUIController::HandlePossessionMenuRequestClose);
	MenuWidget->OnPawnSelected.AddDynamic(this, &UMOSystemMenuUIController::HandlePossessionMenuPawnSelected);
	MenuWidget->OnCreateCharacter.AddDynamic(this, &UMOSystemMenuUIController::HandlePossessionMenuCreateCharacter);

	// Ensure internal button bindings are set up
	MenuWidget->EnsureButtonBindings();

	// Populate with pawn data
	RefreshPossessionMenu();

	// Widget handles input state via NativeOnActivated/GetDesiredInputConfig
	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Possession menu opened"));
}

void UMOSystemMenuUIController::ClosePossessionMenu()
{
	// IMPORTANT: Get reference before clearing cache
	// Reset cache FIRST to ensure IsPossessionMenuOpen() returns false immediately
	// This prevents race conditions with toggle input that fires multiple times per frame
	UMOPossessionMenu* MenuWidget = PossessionMenuWidget.Get();
	PossessionMenuWidget.Reset();

	if (APlayerController* OwningPC = ResolveOwningPlayerController())
	{
		if (UMOPossessionComponent* Possession = OwningPC->FindComponentByClass<UMOPossessionComponent>())
		{
			Possession->OnPossessionListReady.RemoveAll(this);
		}
	}

	if (IsValid(MenuWidget) && MenuWidget->IsActivated())
	{
		PopWidgetFromLayer(MenuWidget);
	}

	UpdateReticleVisibility();

	// Manage modal background visibility
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
	}
}

bool UMOSystemMenuUIController::IsPossessionMenuOpen() const
{
	return IsCachedMenuOpen(PossessionMenuWidget);
}

void UMOSystemMenuUIController::RefreshPossessionMenu()
{
	if (!IsValid(PossessionMenuWidget.Get()))
	{
		return;
	}

	APlayerController* PlayerController = ResolveOwningPlayerController();
	UMOPossessionComponent* Possession = IsValid(PlayerController) ? PlayerController->FindComponentByClass<UMOPossessionComponent>() : nullptr;
	if (!Possession)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] RefreshPossessionMenu: no UMOPossessionComponent on the player controller"));
		return;
	}

	// The rows come from the SERVER's pawns (UMOPossessionComponent): immediate on the host, a round trip on a client.
	// A client's own local saves are not the world it is playing in.
	Possession->OnPossessionListReady.RemoveAll(this);
	Possession->OnPossessionListReady.AddDynamic(this, &UMOSystemMenuUIController::HandlePossessionListReady);
	Possession->RequestPossessionList();
}

void UMOSystemMenuUIController::HandlePossessionListReady(const TArray<FMOPossessionListEntry>& Entries)
{
	UMOPossessionMenu* MenuWidget = PossessionMenuWidget.Get();
	if (!IsValid(MenuWidget))
	{
		return;  // closed while the answer was in flight
	}

	TArray<FMOPersistedPawnRecord> Records;
	Records.Reserve(Entries.Num());
	for (const FMOPossessionListEntry& Entry : Entries)
	{
		Records.Add(Entry.ToDisplayRecord());
	}
	MenuWidget->PopulatePawnList(Records);
}

UMOPossessionMenu* UMOSystemMenuUIController::GetPossessionMenu() const
{
	return PossessionMenuWidget.Get();
}

void UMOSystemMenuUIController::HandlePossessionMenuRequestClose()
{
	ClosePossessionMenu();
}

void UMOSystemMenuUIController::HandlePossessionMenuPawnSelected(const FGuid& PawnGuid)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Pawn selected for possession: %s"), *PawnGuid.ToString());

	// Never PlayerController->Possess() here: it is authority-only and silently does nothing on a remote client. The
	// component resolves it locally on the host and through an RPC on a client; the server may refuse (taken, dead...).
	APlayerController* PlayerController = ResolveOwningPlayerController();
	UMOPossessionComponent* Possession = IsValid(PlayerController) ? PlayerController->FindComponentByClass<UMOPossessionComponent>() : nullptr;
	if (!Possession || !Possession->RequestPossessPawn(PawnGuid))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] Could not request possession of pawn %s"), *PawnGuid.ToString());
		return;
	}

	// If the server accepts, the pawn change arrives through normal replication.
	ClosePossessionMenu();
}

void UMOSystemMenuUIController::HandlePossessionMenuCreateCharacter()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Create new character requested"));

	APlayerController* PlayerController = ResolveOwningPlayerController();
	UMOPossessionComponent* Possession = IsValid(PlayerController) ? PlayerController->FindComponentByClass<UMOPossessionComponent>() : nullptr;
	if (!Possession || !Possession->RequestCreateCharacter())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] Could not request a new character (no possession component or not the local controller)"));
		return;
	}
	ClosePossessionMenu();
}

// =============================================================================
// Confirmation Dialog
// =============================================================================

void UMOSystemMenuUIController::ShowConfirmationDialog(const FText& Title, const FText& Message, const FText& ConfirmText, const FText& CancelText)
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
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] ConfirmationDialogClass not set on SystemMenuUIController."));
		return;
	}

	// Create or reuse widget
	UMOConfirmationDialog* DialogWidget = ConfirmationDialogWidget.Get();
	bool bNeedsBindDelegates = false;

	// If dialog is already in viewport and visible, just update content
	if (IsValid(DialogWidget) && DialogWidget->IsInViewport())
	{
		DialogWidget->Setup(Title, Message, ConfirmText, CancelText);
		return;
	}

	if (IsValid(DialogWidget))
	{
		// Try to reuse existing widget - push it to the layer stack
		UCommonActivatableWidget* ActualWidget = PushWidgetInstanceToLayer(MOUILayerTags::Layer_Modal, DialogWidget);

		// CommonUI may have created a new widget (can't add existing instances to stacks)
		if (ActualWidget && ActualWidget != DialogWidget)
		{
			DialogWidget = Cast<UMOConfirmationDialog>(ActualWidget);
			ConfirmationDialogWidget = DialogWidget;
			bNeedsBindDelegates = true;
		}
	}
	else
	{
		// Create new widget via layer stack
		UCommonActivatableWidget* ActualWidget = PushWidgetToLayer(MOUILayerTags::Layer_Modal, ConfirmationDialogClass);
		DialogWidget = Cast<UMOConfirmationDialog>(ActualWidget);
		bNeedsBindDelegates = true;
	}

	if (!IsValid(DialogWidget))
	{
		return;
	}

	// Cache and bind delegates if this is a new widget
	if (bNeedsBindDelegates)
	{
		ConfirmationDialogWidget = DialogWidget;
		DialogWidget->OnConfirmed.RemoveDynamic(this, &UMOSystemMenuUIController::HandleConfirmationConfirmed);
		DialogWidget->OnCancelled.RemoveDynamic(this, &UMOSystemMenuUIController::HandleConfirmationCancelled);
		DialogWidget->OnConfirmed.AddDynamic(this, &UMOSystemMenuUIController::HandleConfirmationConfirmed);
		DialogWidget->OnCancelled.AddDynamic(this, &UMOSystemMenuUIController::HandleConfirmationCancelled);
	}

	// Setup the dialog with content
	DialogWidget->Setup(Title, Message, ConfirmText, CancelText);
}

void UMOSystemMenuUIController::HandleConfirmationConfirmed()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Confirmation confirmed: %s"), *PendingConfirmationContext);

	// Deactivate confirmation dialog (CommonUI pops from modal layer, restores input)
	UMOConfirmationDialog* DialogWidget = ConfirmationDialogWidget.Get();
	if (IsValid(DialogWidget))
	{
		DialogWidget->DeactivateWidget();
	}

	if (PendingConfirmationContext == TEXT("ExitToMainMenu"))
	{
		ReturnToMainMenuImmediate();
	}
	else if (PendingConfirmationContext == TEXT("ExitGame"))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Exiting game"));
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
				UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Saved to slot: %s (success: %s)"), *SlotName, bSaveSuccess ? TEXT("YES") : TEXT("NO"));

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

		// Close all menus via UIManager
		if (UMOUIManagerComponent* UIManager = GetUIManager())
		{
			UIManager->CloseAllMenus();
		}

		// Store the pending load slot in settings (same approach as main menu loading)
		// This ensures a clean level reload with proper state reset
		UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
		if (Settings)
		{
			Settings->bPendingNewGame = false;  // Not a new game, it's a load
			Settings->bIsLoadingIntoGameplay = true;  // Keep loading screen until pawn lands
			Settings->PendingNewGameSlot = SlotName;  // Reuse field for load slot
			Settings->SaveSettings();
		}

		// Reload the gameplay level to get a clean state
		// The GameMode's BeginPlay will handle loading the save data
		UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Reloading level to load save: %s"), *SlotName);
		// Net-mode-aware: keeps the listen-server role + connected clients when
		// the host loads a save in co-op (plain OpenLevel dropped both).
		UMOTravelUtils::TravelToGameplayLevel(GetOwner(), GameplayLevelPath);
	}

	PendingConfirmationContext.Empty();
	OnConfirmationConfirmed.Broadcast();
}

void UMOSystemMenuUIController::HandleConfirmationCancelled()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Confirmation cancelled: %s"), *PendingConfirmationContext);

	// Deactivate confirmation dialog (CommonUI pops from modal layer, restores input)
	UMOConfirmationDialog* DialogWidget = ConfirmationDialogWidget.Get();
	if (IsValid(DialogWidget))
	{
		DialogWidget->DeactivateWidget();
	}

	PendingConfirmationContext.Empty();
	OnConfirmationCancelled.Broadcast();
}

// =============================================================================
// Survivor Context Menu
// =============================================================================

void UMOSystemMenuUIController::ShowSurvivorContextMenu(APawn* Survivor, FVector2D ScreenPosition)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] ShowSurvivorContextMenu called for %s at position (%f, %f)"),
		Survivor ? *Survivor->GetName() : TEXT("nullptr"), ScreenPosition.X, ScreenPosition.Y);

	if (!IsLocalOwningPlayerController())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] ShowSurvivorContextMenu - not local owning player controller"));
		return;
	}

	if (!IsValid(Survivor))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] ShowSurvivorContextMenu called with invalid survivor"));
		return;
	}

	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] ShowSurvivorContextMenu - PlayerController invalid"));
		return;
	}

	if (!SurvivorContextMenuClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] SurvivorContextMenuClass not set on SystemMenuUIController. Please set it in the Blueprint or C++ defaults."));
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Using SurvivorContextMenuClass: %s"), *SurvivorContextMenuClass->GetName());

	// Store current target
	CurrentSurvivorTarget = Survivor;

	// Create widget if needed
	UMOSurvivorContextMenu* MenuWidget = SurvivorContextMenuWidget.Get();
	if (!IsValid(MenuWidget))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Creating new survivor context menu widget"));
		MenuWidget = CreateWidget<UMOSurvivorContextMenu>(PlayerController, SurvivorContextMenuClass);
		SurvivorContextMenuWidget = MenuWidget;

		if (!IsValid(MenuWidget))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] Failed to create survivor context menu widget. Check if the Blueprint widget exists and inherits from UMOSurvivorContextMenu."));
			return;
		}
		UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Successfully created survivor context menu widget"));
	}
	else
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Reusing existing survivor context menu widget"));
	}

	// Bind delegates
	MenuWidget->OnCloseRequested.RemoveDynamic(this, &UMOSystemMenuUIController::HandleSurvivorContextMenuRequestClose);
	MenuWidget->OnOpenTasksRequested.RemoveDynamic(this, &UMOSystemMenuUIController::HandleSurvivorContextMenuOpenTasks);
	MenuWidget->OnInventoryRequested.RemoveDynamic(this, &UMOSystemMenuUIController::HandleSurvivorContextMenuOpenInventory);
	MenuWidget->OnCloseRequested.AddDynamic(this, &UMOSystemMenuUIController::HandleSurvivorContextMenuRequestClose);
	MenuWidget->OnOpenTasksRequested.AddDynamic(this, &UMOSystemMenuUIController::HandleSurvivorContextMenuOpenTasks);
	MenuWidget->OnInventoryRequested.AddDynamic(this, &UMOSystemMenuUIController::HandleSurvivorContextMenuOpenInventory);

	// Show modal background and add context menu to viewport FIRST
	// (SetPositionInViewport only works after widget is in viewport)
	// Context menus are UCommonUserWidget, not activatable
	ShowModalBackground();
	MenuWidget->AddToViewport(SurvivorContextMenuZOrder);

	// THEN initialize for this survivor (which calls SetPopupPosition)
	MenuWidget->InitializeForSurvivor(Survivor, ScreenPosition);

	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Widget added to viewport (ZOrder: %d, IsInViewport: %s)"),
		SurvivorContextMenuZOrder,
		MenuWidget->IsInViewport() ? TEXT("true") : TEXT("false"));

	// Widget handles input state via NativeOnActivated/GetDesiredInputConfig
	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Survivor context menu opened for %s at screen position (%f, %f)"),
		*Survivor->GetName(), ScreenPosition.X, ScreenPosition.Y);
}

void UMOSystemMenuUIController::CloseSurvivorContextMenu()
{
	UMOSurvivorContextMenu* MenuWidget = SurvivorContextMenuWidget.Get();
	if (IsValid(MenuWidget))
	{
		if (MenuWidget->IsInViewport())
		{
			MenuWidget->RemoveFromParent();
		}
	}

	UpdateReticleVisibility();

	// Widget handles input state restoration via NativeOnDeactivated
	// Just manage modal background visibility
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
	}
}

bool UMOSystemMenuUIController::IsSurvivorContextMenuOpen() const
{
	return IsCachedMenuOpen(SurvivorContextMenuWidget);
}

void UMOSystemMenuUIController::ShowSurvivorTaskMenu(APawn* Survivor)
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	if (!IsValid(Survivor))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] ShowSurvivorTaskMenu called with invalid survivor"));
		return;
	}

	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	if (!SurvivorTaskMenuClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] SurvivorTaskMenuClass not set on SystemMenuUIController."));
		return;
	}

	// Store current target
	CurrentSurvivorTarget = Survivor;

	// Create or reuse widget
	UMOSurvivorTaskMenu* MenuWidget = SurvivorTaskMenuWidget.Get();
	bool bNeedsBindDelegates = false;

	if (IsValid(MenuWidget))
	{
		// Try to reuse existing widget - push it to the layer stack
		ShowModalBackground();
		UCommonActivatableWidget* ActualWidget = PushWidgetInstanceToLayer(MOUILayerTags::Layer_Menu, MenuWidget);

		// CommonUI may have created a new widget (can't add existing instances to stacks)
		if (ActualWidget && ActualWidget != MenuWidget)
		{
			MenuWidget = Cast<UMOSurvivorTaskMenu>(ActualWidget);
			RegisterCachedMenu(MenuWidget, SurvivorTaskMenuWidget);
			bNeedsBindDelegates = true;
		}
	}
	else
	{
		// Create new widget via layer stack
		ShowModalBackground();
		UCommonActivatableWidget* ActualWidget = PushWidgetToLayer(MOUILayerTags::Layer_Menu, SurvivorTaskMenuClass);
		MenuWidget = Cast<UMOSurvivorTaskMenu>(ActualWidget);
		bNeedsBindDelegates = true;
	}

	if (!IsValid(MenuWidget))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] Failed to create survivor task menu widget."));
		HideModalBackground();
		return;
	}

	// Cache and bind delegates if this is a new widget
	if (bNeedsBindDelegates)
	{
		RegisterCachedMenu(MenuWidget, SurvivorTaskMenuWidget);
		MenuWidget->OnRequestClose.RemoveDynamic(this, &UMOSystemMenuUIController::HandleSurvivorTaskMenuRequestClose);
		MenuWidget->OnRequestClose.AddDynamic(this, &UMOSystemMenuUIController::HandleSurvivorTaskMenuRequestClose);
	}

	// Initialize for this survivor
	MenuWidget->InitializeForSurvivor(Survivor);

	// Widget handles input state via NativeOnActivated/GetDesiredInputConfig
	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Survivor task menu opened for %s"), *Survivor->GetName());
}

void UMOSystemMenuUIController::CloseSurvivorTaskMenu()
{
	UMOSurvivorTaskMenu* MenuWidget = SurvivorTaskMenuWidget.Get();
	if (IsValid(MenuWidget))
	{
		MenuWidget->DeactivateWidget();
	}

	UpdateReticleVisibility();

	// Widget handles input state restoration via NativeOnDeactivated
	// Just manage modal background visibility
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
	}
}

bool UMOSystemMenuUIController::IsSurvivorTaskMenuOpen() const
{
	return IsCachedMenuOpen(SurvivorTaskMenuWidget);
}

void UMOSystemMenuUIController::HandleSurvivorContextMenuRequestClose()
{
	CloseSurvivorContextMenu();
}

void UMOSystemMenuUIController::HandleSurvivorContextMenuOpenTasks(APawn* Survivor)
{
	// Close context menu first
	CloseSurvivorContextMenu();

	// Open task menu for the survivor
	ShowSurvivorTaskMenu(Survivor);
}

void UMOSystemMenuUIController::HandleSurvivorContextMenuOpenInventory(APawn* Survivor)
{
	// Close context menu first
	CloseSurvivorContextMenu();

	if (!IsValid(Survivor))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] HandleSurvivorContextMenuOpenInventory - invalid survivor"));
		return;
	}

	// Find the inventory UI controller and open inventory with this survivor as container
	if (AActor* Owner = GetOwner())
	{
		if (UMOInventoryUIController* InventoryUI = Owner->FindComponentByClass<UMOInventoryUIController>())
		{
			// Open inventory with the survivor as the "container" for item exchange
			InventoryUI->OpenInventoryWithContainer(Survivor);
			UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Opened inventory exchange with survivor %s"), *Survivor->GetName());
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] HandleSurvivorContextMenuOpenInventory - no MOInventoryUIController found"));
		}
	}
}

void UMOSystemMenuUIController::HandleSurvivorTaskMenuRequestClose()
{
	CloseSurvivorTaskMenu();
}

// =============================================================================
// DEATH RECAP
// =============================================================================

void UMOSystemMenuUIController::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	Super::OnPossessedPawnChanged(OldPawn, NewPawn);

	// Always unbind from the previous pawn first — if BindDeathListenerToPawn
	// short-circuits (e.g. NewPawn isn't a MOCharacter), we still leave the
	// listener clean rather than dangling on the old pawn.
	UnbindDeathListener();

	if (AMOCharacter* NewMOChar = Cast<AMOCharacter>(NewPawn))
	{
		BindDeathListenerToPawn(NewMOChar);
	}
}

void UMOSystemMenuUIController::BindDeathListenerToPawn(AMOCharacter* Character)
{
	if (!IsValid(Character))
	{
		return;
	}

	// AddUnique pattern via Remove-then-Add: dynamic delegates don't expose
	// "already bound" cheaply, so we always remove first.
	Character->OnPawnDied.RemoveDynamic(this, &UMOSystemMenuUIController::HandlePawnDied);
	Character->OnPawnDied.AddDynamic(this, &UMOSystemMenuUIController::HandlePawnDied);

	SubscribedDeathPawn = Character;

	UE_LOG(LogMOFramework, Log,
		TEXT("[MOSysUI] Bound OnPawnDied listener to '%s'"), *Character->GetName());
}

void UMOSystemMenuUIController::UnbindDeathListener()
{
	if (AMOCharacter* OldChar = SubscribedDeathPawn.Get())
	{
		OldChar->OnPawnDied.RemoveDynamic(this, &UMOSystemMenuUIController::HandlePawnDied);
	}
	SubscribedDeathPawn.Reset();
}

void UMOSystemMenuUIController::HandlePawnDied(AMOCharacter* DeadCharacter, EMOBodyPartType CausePart)
{
	if (!IsLocalOwningPlayerController())
	{
		// Death recap is a local-player-only UI — remote / dedicated-server
		// PCs shouldn't try to push a widget.
		return;
	}

	if (!DeathRecapWidgetClass)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOSysUI] Pawn '%s' died but DeathRecapWidgetClass is not set — recap suppressed. "
			     "Assign WBP_DeathRecap on UMOSystemMenuUIController in BP defaults."),
			DeadCharacter ? *DeadCharacter->GetName() : TEXT("NULL"));
		return;
	}

	// If a previous recap is still showing (shouldn't happen — recap holds you
	// until you click out — but defensive), pop the old one first.
	if (UMODeathRecapWidget* Existing = DeathRecapWidget.Get())
	{
		if (Existing->IsActivated())
		{
			Existing->DeactivateWidget();
		}
		DeathRecapWidget.Reset();
	}

	// Build the snapshot BEFORE pushing the widget so the data exists before
	// NativeConstruct can ask for it. UMODeathRecapWidget::SetRecapData
	// buffers the data if the widget isn't constructed yet, but the cleaner
	// flow is to push, then immediately populate.
	const FMODeathRecapData Data = UMODeathRecapWidget::BuildRecapData(DeadCharacter, CausePart);

	UCommonActivatableWidget* Created = PushWidgetToLayer(
		MOUILayerTags::Layer_Modal, DeathRecapWidgetClass);

	UMODeathRecapWidget* RecapWidget = Cast<UMODeathRecapWidget>(Created);
	if (!IsValid(RecapWidget))
	{
		UE_LOG(LogMOFramework, Error,
			TEXT("[MOSysUI] Failed to push death recap widget to Layer_Modal"));
		return;
	}

	RegisterCachedMenu(RecapWidget, DeathRecapWidget);
	RecapWidget->SetRecapData(Data);

	// Route "Return to Main Menu" to the centralized exit path. The lambda
	// captures a weak self so a teardown mid-click doesn't crash the recap.
	TWeakObjectPtr<UMOSystemMenuUIController> WeakSelf(this);
	RecapWidget->OnReturnToMenuRequested.AddLambda([WeakSelf]()
	{
		if (UMOSystemMenuUIController* Self = WeakSelf.Get())
		{
			Self->ReturnToMainMenuImmediate();
		}
	});

	// Continue (respawn) — no respawn flow yet, so this just closes the recap.
	// MOGameMode-side respawn logic is a separate task; when wired up this
	// lambda will call into it.
	RecapWidget->OnContinueRequested.AddLambda([WeakSelf]()
	{
		if (UMOSystemMenuUIController* Self = WeakSelf.Get())
		{
			if (UMODeathRecapWidget* W = Self->DeathRecapWidget.Get())
			{
				if (W->IsActivated())
				{
					W->DeactivateWidget();
				}
			}
		}
	});

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOSysUI] Death recap shown for '%s' (cause: %d)"),
		DeadCharacter ? *DeadCharacter->GetName() : TEXT("NULL"),
		static_cast<int32>(CausePart));
}

void UMOSystemMenuUIController::ReturnToMainMenuImmediate()
{
	// Close all menus via UIManager so we don't carry stale modal state into
	// the main menu level.
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		UIManager->CloseAllMenus();
	}

	// Mark that we're returning from gameplay so the intro doesn't play
	if (UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings())
	{
		Settings->MarkReturningFromGameplay();
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Exiting to main menu: %s"), *MainMenuLevelPath);
	// DELIBERATELY plain OpenLevel (not MOTravelUtils): the host exiting to the
	// main menu is a session END — dropping the server role and disconnecting
	// clients is the intent. Graceful co-op teardown belongs to the session
	// layer (charter Move 3).
	UGameplayStatics::OpenLevel(this, *MainMenuLevelPath);
}
