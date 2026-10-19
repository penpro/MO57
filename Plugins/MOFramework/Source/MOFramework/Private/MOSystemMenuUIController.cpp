#include "MOSystemMenuUIController.h"
#include "MOFramework.h"
#include "MOTravelUtils.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "MOGameMode.h"

#include "MOUIManagerComponent.h"
#include "MOInGameMenu.h"
#include "MONotificationComponent.h"
#include "MOPossessionMenu.h"
#include "MOConfirmationDialog.h"
#include "MOSurvivorContextMenu.h"
#include "MOSurvivorTaskMenu.h"
#include "MODeathRecapWidget.h"
#include "MOCharacter.h"
#include "MOColonyManagerSubsystem.h"
#include "MOPlayerController.h"
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
#include "MOCharacterCreatorPanel.h"

UMOSystemMenuUIController::UMOSystemMenuUIController()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMOSystemMenuUIController::BeginPlay()
{
	Super::BeginPlay();

	// Closing the game window is a way out of the world too: save first (no save-scumming).
	// PIE is skipped: closing a PIE window is ending a dev session, not a player leaving.
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	UWorld* World = GetWorld();
	if (PC && PC->IsLocalController() && World && World->WorldType == EWorldType::Game)
	{
		if (UGameViewportClient* ViewportClient = World->GetGameViewport())
		{
			ViewportClient->OnCloseRequested().RemoveAll(this);
			ViewportClient->OnCloseRequested().AddUObject(this, &UMOSystemMenuUIController::HandleViewportCloseRequested);
			BoundViewportClient = ViewportClient;
		}
	}

	// A load that leaves this player without a pawn (they left the world right after their last pawn died) gets the same
	// next-life step as the live death path. Host / standalone only: that is where loads happen.
	if (AMOGameMode* GameMode = World ? World->GetAuthGameMode<AMOGameMode>() : nullptr)
	{
		GameMode->OnPlayerNeedsNextLife.RemoveAll(this);
		GameMode->OnPlayerNeedsNextLife.AddUObject(this, &UMOSystemMenuUIController::HandlePlayerNeedsNextLife);
	}
}

void UMOSystemMenuUIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameViewportClient* ViewportClient = BoundViewportClient.Get())
	{
		ViewportClient->OnCloseRequested().RemoveAll(this);
	}
	BoundViewportClient.Reset();
	if (AMOGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMOGameMode>() : nullptr)
	{
		GameMode->OnPlayerNeedsNextLife.RemoveAll(this);
	}

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

	// Frame-based debounce: one toggle per frame even if the key reaches this controller twice (added when menus ran in ECommonInputMode::All
	// and both the game action and the menu fired; menus use Menu mode since the May-June rework -- kept as a cheap guard)
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
	CloseCachedMenu(InGameMenuWidget);

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
	CloseCachedMenu(InGameMenuWidget);

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

// No confirmation on either exit: leaving always saves (SaveThisWorld), so there is no
// "unsaved progress" to warn about and no way to leave without saving (Wes 2026-10-17: no save-scumming).
void UMOSystemMenuUIController::HandleInGameMenuExitToMainMenu()
{
	ReturnToMainMenuImmediate();
}

void UMOSystemMenuUIController::HandleInGameMenuExitGame()
{
	QuitGameImmediate();
}

void UMOSystemMenuUIController::HandleSaveRequested(const FString& SlotName)
{
	// One save per world (Wes, DECISIONS F24, 2026-10-17): Save always writes THIS world's own slot, with no confirm. A slot name a
	// panel passes is ignored, so no UI path can make a second save of a world (no Save As, no snapshots to reload: no save-scumming).
	if (!SlotName.IsEmpty())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Save: ignoring the requested slot '%s' -- a world has one save"), *SlotName);
	}

	const EMOWorldSaveResult Result = SaveThisWorld(TEXT("save"));

	if (const AMOPlayerController* PC = Cast<AMOPlayerController>(GetOwner()))
	{
		if (UMONotificationComponent* Notes = PC->GetNotificationComponent())
		{
			if (Result == EMOWorldSaveResult::Saved)
			{
				Notes->ShowInfoNotification(NSLOCTEXT("MO", "WorldSaved", "World saved"), 3.0f);
			}
			else if (Result == EMOWorldSaveResult::NotHost)
			{
				Notes->ShowInfoNotification(NSLOCTEXT("MO", "HostSaves", "The host saves this world"), 4.0f);
			}
			else
			{
				Notes->ShowWarningNotification(NSLOCTEXT("MO", "WorldSaveFailed", "Could not save the world"), 6.0f);
			}
		}
	}

	if (Result == EMOWorldSaveResult::Saved)
	{
		if (UMOInGameMenu* MenuWidget = InGameMenuWidget.Get())
		{
			MenuWidget->RefreshLoadPanelList();
		}
	}
}

void UMOSystemMenuUIController::HandleLoadRequested(const FString& SlotName)
{
	// One save per world: the save of the world you are standing in is not a snapshot to go back to (the in-game Load list hides it too).
	if (UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this))
	{
		const UMOPersistenceSubsystem* Persistence = GameInstance->GetSubsystem<UMOPersistenceSubsystem>();
		if (Persistence && !SlotName.IsEmpty() && SlotName == Persistence->GetCurrentSlotName())
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] Load refused: '%s' is the world you are in (one save per world)"), *SlotName);
			return;
		}
	}

	// Loading another save leaves this world: save it first like every other way out (no confirm, no save-scumming).
	if (SaveThisWorld(TEXT("load another save")) == EMOWorldSaveResult::Failed)
	{
		PendingConfirmationContext = FString::Printf(TEXT("LeaveUnsaved:Load:%s"), *SlotName);
		ShowConfirmationDialog(
			NSLOCTEXT("MO", "LeaveUnsavedTitle", "Could Not Save"),
			NSLOCTEXT("MO", "LeaveUnsavedMessage", "The world could not be saved. Leaving now loses everything since the last save. Leave anyway?"),
			NSLOCTEXT("MO", "LeaveAnyway", "Leave Anyway"),
			NSLOCTEXT("MO", "Stay", "Stay"));
		return;
	}
	LoadSlotNow(SlotName);
}

void UMOSystemMenuUIController::LoadSlotNow(const FString& SlotName)
{
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

// =============================================================================
// Possession Menu
// =============================================================================

void UMOSystemMenuUIController::TogglePossessionMenu()
{
	// DEBUG (MOSysUI, disabled Oct 18): re-enable to trace setup steps
	// UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] TogglePossessionMenu called"));

	// Frame-based debounce: one toggle per frame even if the key reaches this controller twice (added when menus ran in ECommonInputMode::All
	// and both the game action and the menu fired; menus use Menu mode since the May-June rework -- kept as a cheap guard)
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
	// DEBUG (MOSysUI, disabled Oct 18): re-enable to trace setup steps
	// UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] OpenPossessionMenu called"));

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

	// DEBUG (MOSysUI, disabled Oct 18): re-enable to trace setup steps
	// UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] OpenPossessionMenu - All checks passed, creating menu"));

	// Close any existing menu first
	CloseCachedMenu(PossessionMenuWidget);

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

	PopWidgetFromLayer(MenuWidget);   // not gated on IsActivated(): see CloseCachedMenu

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

	MenuWidget->PopulatePossessionList(Entries);
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

	// Only reached when the save on the way out FAILED and the player chose to leave anyway.
	if (PendingConfirmationContext == TEXT("LeaveUnsaved:Menu"))
	{
		LeaveToMainMenuLevel();
	}
	else if (PendingConfirmationContext == TEXT("LeaveUnsaved:Quit"))
	{
		QuitToDesktop();
	}
	else if (PendingConfirmationContext.StartsWith(TEXT("LeaveUnsaved:Load:")))
	{
		LoadSlotNow(PendingConfirmationContext.RightChop(18));
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
	// DEBUG (MOSysUI, disabled Oct 18): re-enable to trace setup steps
	// UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] ShowSurvivorContextMenu called for %s at position (%f, %f)"),
	// 	Survivor ? *Survivor->GetName() : TEXT("nullptr"), ScreenPosition.X, ScreenPosition.Y);

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

	// DEBUG (MOSysUI, disabled Oct 18): re-enable to trace setup steps
	// UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Using SurvivorContextMenuClass: %s"), *SurvivorContextMenuClass->GetName());

	// Store current target
	CurrentSurvivorTarget = Survivor;

	// Create widget if needed
	UMOSurvivorContextMenu* MenuWidget = SurvivorContextMenuWidget.Get();
	if (!IsValid(MenuWidget))
	{
		// DEBUG (MOSysUI, disabled Oct 18): re-enable to trace setup steps
		// UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Creating new survivor context menu widget"));
		MenuWidget = CreateWidget<UMOSurvivorContextMenu>(PlayerController, SurvivorContextMenuClass);
		SurvivorContextMenuWidget = MenuWidget;

		if (!IsValid(MenuWidget))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] Failed to create survivor context menu widget. Check if the Blueprint widget exists and inherits from UMOSurvivorContextMenu."));
			return;
		}
		// DEBUG (MOSysUI, disabled Oct 18): re-enable to trace setup steps
		// UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Successfully created survivor context menu widget"));
	}
	else
	{
		// DEBUG (MOSysUI, disabled Oct 18): re-enable to trace setup steps
		// UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Reusing existing survivor context menu widget"));
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

	// DEBUG (MOSysUI, disabled Oct 18): re-enable to trace setup steps
	// UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Widget added to viewport (ZOrder: %d, IsInViewport: %s)"),
	// 	SurvivorContextMenuZOrder,
	// 	MenuWidget->IsInViewport() ? TEXT("true") : TEXT("false"));

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

void UMOSystemMenuUIController::ShowCharacterCreator()
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}
	TSubclassOf<UMOCharacterCreatorPanel> CreatorClass = CharacterCreatorWidgetClass;
	if (!CreatorClass)
	{
		// /MOFramework is always cooked (DefaultGame.ini DirectoriesToAlwaysCook).
		CreatorClass = LoadClass<UMOCharacterCreatorPanel>(nullptr, TEXT("/MOFramework/UI/MainMenu_UI/WBP_CharacterCreatorPanel.WBP_CharacterCreatorPanel_C"));
	}
	if (!CreatorClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] No character creator widget (CharacterCreatorWidgetClass unset and the kit WBP missing): the new game starts without one"));
		return;
	}
	CloseCachedMenu(CharacterCreatorWidget);
	UMOCharacterCreatorPanel* Creator = Cast<UMOCharacterCreatorPanel>(PushWidgetToLayer(MOUILayerTags::Layer_Menu, CreatorClass));
	if (!IsValid(Creator))
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOSysUI] Failed to push the character creator to Layer_Menu"));
		return;
	}
	RegisterCachedMenu(Creator, CharacterCreatorWidget);
	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Character creator opened over the loading world"));
}

TSubclassOf<UMODeathRecapWidget> UMOSystemMenuUIController::ResolveDeathRecapClass() const
{
	if (DeathRecapWidgetClass)
	{
		return DeathRecapWidgetClass;
	}
	// Default policy: the kit's recap, so a project that never assigned the class still shows one. /MOFramework is always cooked (DefaultGame.ini DirectoriesToAlwaysCook).
	return LoadClass<UMODeathRecapWidget>(nullptr, TEXT("/MOFramework/UI/WBP_DeathRecap.WBP_DeathRecap_C"));
}

void UMOSystemMenuUIController::HandlePawnDied(AMOCharacter* DeadCharacter, EMOBodyPartType CausePart)
{
	if (!IsLocalOwningPlayerController())
	{
		// Death recap is a local-player-only UI — remote / dedicated-server
		// PCs shouldn't try to push a widget.
		return;
	}

	const TSubclassOf<UMODeathRecapWidget> RecapClass = ResolveDeathRecapClass();
	if (!RecapClass)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOSysUI] Pawn '%s' died but there is no death recap widget (DeathRecapWidgetClass unset and /MOFramework/UI/WBP_DeathRecap missing) — recap suppressed."),
			DeadCharacter ? *DeadCharacter->GetName() : TEXT("NULL"));
		return;
	}

	// If a previous recap is still showing (shouldn't happen — recap holds you
	// until you click out — but defensive), pop the old one first.
	CloseCachedMenu(DeathRecapWidget);

	// Build the snapshot BEFORE pushing the widget so the data exists before
	// NativeConstruct can ask for it. UMODeathRecapWidget::SetRecapData
	// buffers the data if the widget isn't constructed yet, but the cleaner
	// flow is to push, then immediately populate.
	const FMODeathRecapData Data = UMODeathRecapWidget::BuildRecapData(DeadCharacter, CausePart);

	UCommonActivatableWidget* Created = PushWidgetToLayer(
		MOUILayerTags::Layer_Modal, RecapClass);

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

	// Continue: close the recap, then take the next life (another colony pawn, or a respawn ~5 miles away when this was the last one).
	const TWeakObjectPtr<AMOCharacter> WeakDead(DeadCharacter);
	RecapWidget->OnContinueRequested.AddLambda([WeakSelf, WeakDead]()
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
			Self->ContinueAfterDeath(WeakDead.Get());
		}
	});

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOSysUI] Death recap shown for '%s' (cause: %d)"),
		DeadCharacter ? *DeadCharacter->GetName() : TEXT("NULL"),
		static_cast<int32>(CausePart));
}

void UMOSystemMenuUIController::ContinueAfterDeath(const AMOCharacter* DeadCharacter)
{
	AMOPlayerController* PC = Cast<AMOPlayerController>(GetOwner());
	UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return;
	}

	bool bHasSuccessor = false;
	if (const UMOColonyManagerSubsystem* Colony = World->GetSubsystem<UMOColonyManagerSubsystem>())
	{
		for (APawn* Candidate : Colony->GetColonyRoster())
		{
			const AMOCharacter* Character = Cast<AMOCharacter>(Candidate);
			if (Character && Character != DeadCharacter && !Character->IsDead())
			{
				bHasSuccessor = true;
				break;
			}
		}
	}

	if (bHasSuccessor)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] Death recap: the colony has another living pawn -- opening the possession menu"));
		if (UMOUIManagerComponent* UIManager = GetUIManager())
		{
			UIManager->OpenPossessionMenu();
		}
		return;
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] Death recap: that was the last pawn -- asking for a new survivor"));
	PC->ServerRequestRespawn();
}

EMOWorldSaveResult UMOSystemMenuUIController::SaveThisWorld(const TCHAR* Reason)
{
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOSysUI] Saving this world (%s): client -- the host owns the world save, nothing written"), Reason);
		return EMOWorldSaveResult::NotHost;
	}

	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UMOPersistenceSubsystem* Persistence = GameInstance ? GameInstance->GetSubsystem<UMOPersistenceSubsystem>() : nullptr;
	if (!Persistence)
	{
		UE_LOG(LogMOFramework, Error,
			TEXT("[MOSysUI] Saving this world (%s): no persistence subsystem -- NOT saved"), Reason);
		return EMOWorldSaveResult::Failed;
	}

	const bool bSaved = Persistence->SaveToCurrentSlot();
	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOSysUI] Saving this world (%s): slot '%s' ok=%d"),
		Reason, *Persistence->GetCurrentSlotName(), bSaved ? 1 : 0);
	return bSaved ? EMOWorldSaveResult::Saved : EMOWorldSaveResult::Failed;
}

void UMOSystemMenuUIController::HandlePlayerNeedsNextLife(APlayerController* PC)
{
	if (PC != GetOwner())
	{
		return;
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] Loaded without a pawn -- next life (same step as the death recap's Continue)"));
	ContinueAfterDeath(nullptr);
}

void UMOSystemMenuUIController::HandleViewportCloseRequested(FViewport* Viewport)
{
	// The window is closing either way: there is no dialog to offer, so a failed save is only logged.
	SaveThisWorld(TEXT("window closed"));
}

void UMOSystemMenuUIController::QuitGameImmediate()
{
	if (SaveThisWorld(TEXT("exit game")) == EMOWorldSaveResult::Failed)
	{
		PendingConfirmationContext = TEXT("LeaveUnsaved:Quit");
		ShowConfirmationDialog(
			NSLOCTEXT("MO", "LeaveUnsavedTitle", "Could Not Save"),
			NSLOCTEXT("MO", "LeaveUnsavedMessage", "The world could not be saved. Leaving now loses everything since the last save. Leave anyway?"),
			NSLOCTEXT("MO", "LeaveAnyway", "Leave Anyway"),
			NSLOCTEXT("MO", "Stay", "Stay"));
		return;
	}
	QuitToDesktop();
}

void UMOSystemMenuUIController::QuitToDesktop()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Exiting game"));
	UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
}

void UMOSystemMenuUIController::ReturnToMainMenuImmediate()
{
	if (SaveThisWorld(TEXT("exit to main menu")) == EMOWorldSaveResult::Failed)
	{
		PendingConfirmationContext = TEXT("LeaveUnsaved:Menu");
		ShowConfirmationDialog(
			NSLOCTEXT("MO", "LeaveUnsavedTitle", "Could Not Save"),
			NSLOCTEXT("MO", "LeaveUnsavedMessage", "The world could not be saved. Leaving now loses everything since the last save. Leave anyway?"),
			NSLOCTEXT("MO", "LeaveAnyway", "Leave Anyway"),
			NSLOCTEXT("MO", "Stay", "Stay"));
		return;
	}
	LeaveToMainMenuLevel();
}

void UMOSystemMenuUIController::LeaveToMainMenuLevel()
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
