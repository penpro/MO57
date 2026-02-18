#include "MOSystemMenuUIController.h"
#include "MOFramework.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

#include "MOUIManagerComponent.h"
#include "MOInGameMenu.h"
#include "MOPossessionMenu.h"
#include "MOConfirmationDialog.h"
#include "MOPersistenceSubsystem.h"
#include "MOPossessionSubsystem.h"
#include "MOIdentityRegistrySubsystem.h"
#include "MOIdentityComponent.h"
#include "MOIdentifiableInterface.h"
#include "MOGameSettings.h"

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

	// Create widget if needed
	UMOInGameMenu* MenuWidget = InGameMenuWidget.Get();
	if (!IsValid(MenuWidget))
	{
		MenuWidget = CreateWidget<UMOInGameMenu>(PlayerController, InGameMenuClass);
		InGameMenuWidget = MenuWidget;

		if (!IsValid(MenuWidget))
		{
			return;
		}

		// Bind delegates
		MenuWidget->OnRequestClose.RemoveDynamic(this, &UMOSystemMenuUIController::HandleInGameMenuRequestClose);
		MenuWidget->OnExitToMainMenu.RemoveDynamic(this, &UMOSystemMenuUIController::HandleInGameMenuExitToMainMenu);
		MenuWidget->OnExitGame.RemoveDynamic(this, &UMOSystemMenuUIController::HandleInGameMenuExitGame);
		MenuWidget->OnSaveRequested.RemoveDynamic(this, &UMOSystemMenuUIController::HandleSaveRequested);
		MenuWidget->OnLoadRequested.RemoveDynamic(this, &UMOSystemMenuUIController::HandleLoadRequested);
		MenuWidget->OnRequestClose.AddDynamic(this, &UMOSystemMenuUIController::HandleInGameMenuRequestClose);
		MenuWidget->OnExitToMainMenu.AddDynamic(this, &UMOSystemMenuUIController::HandleInGameMenuExitToMainMenu);
		MenuWidget->OnExitGame.AddDynamic(this, &UMOSystemMenuUIController::HandleInGameMenuExitGame);
		MenuWidget->OnSaveRequested.AddDynamic(this, &UMOSystemMenuUIController::HandleSaveRequested);
		MenuWidget->OnLoadRequested.AddDynamic(this, &UMOSystemMenuUIController::HandleLoadRequested);

		UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] InGameMenu delegates bound"));
	}

	// Show modal background and menu
	ShowModalBackground();
	MenuWidget->AddToViewport(InGameMenuZOrder);

	// Set input mode
	ApplyInputModeForMenuOpen(MenuWidget);

	UpdateReticleVisibility();
}

void UMOSystemMenuUIController::CloseInGameMenu()
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

	// Only restore input mode if no other menus are open
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
		if (IsValid(PlayerController) && PlayerController->IsLocalController())
		{
			ApplyInputModeForMenuClosed();
		}
	}
}

bool UMOSystemMenuUIController::IsInGameMenuOpen() const
{
	UMOInGameMenu* MenuWidget = InGameMenuWidget.Get();
	return IsValid(MenuWidget) && MenuWidget->IsInViewport();
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

	// Create widget if needed
	UMOPossessionMenu* MenuWidget = PossessionMenuWidget.Get();
	if (!IsValid(MenuWidget))
	{
		MenuWidget = CreateWidget<UMOPossessionMenu>(PlayerController, PossessionMenuClass);
		PossessionMenuWidget = MenuWidget;

		if (!IsValid(MenuWidget))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] Failed to create possession menu widget."));
			return;
		}
	}

	// Always rebind delegates to ensure they're connected (defensive against widget reconstruction)
	MenuWidget->OnRequestClose.RemoveDynamic(this, &UMOSystemMenuUIController::HandlePossessionMenuRequestClose);
	MenuWidget->OnPawnSelected.RemoveDynamic(this, &UMOSystemMenuUIController::HandlePossessionMenuPawnSelected);
	MenuWidget->OnCreateCharacter.RemoveDynamic(this, &UMOSystemMenuUIController::HandlePossessionMenuCreateCharacter);
	MenuWidget->OnRequestClose.AddDynamic(this, &UMOSystemMenuUIController::HandlePossessionMenuRequestClose);
	MenuWidget->OnPawnSelected.AddDynamic(this, &UMOSystemMenuUIController::HandlePossessionMenuPawnSelected);
	MenuWidget->OnCreateCharacter.AddDynamic(this, &UMOSystemMenuUIController::HandlePossessionMenuCreateCharacter);

	// Ensure internal button bindings are set up (defensive against widget reuse issues)
	MenuWidget->EnsureButtonBindings();

	// Populate with pawn data
	RefreshPossessionMenu();

	// Show modal background and menu
	ShowModalBackground();
	MenuWidget->AddToViewport(PossessionMenuZOrder);

	// Set input mode
	ApplyInputModeForMenuOpen(MenuWidget);

	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Possession menu opened"));
}

void UMOSystemMenuUIController::ClosePossessionMenu()
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

	// Only restore input mode if no other menus are open
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
		if (IsValid(PlayerController) && PlayerController->IsLocalController())
		{
			ApplyInputModeForMenuClosed();
		}
	}
}

bool UMOSystemMenuUIController::IsPossessionMenuOpen() const
{
	UMOPossessionMenu* MenuWidget = PossessionMenuWidget.Get();
	return IsValid(MenuWidget) && MenuWidget->IsInViewport();
}

void UMOSystemMenuUIController::RefreshPossessionMenu()
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
			UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Successfully possessed pawn %s"), *PawnGuid.ToString());
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
				UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Spawned and possessed pawn %s"), *PawnGuid.ToString());
				return;
			}
		}
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] Failed to find or spawn pawn %s"), *PawnGuid.ToString());
}

void UMOSystemMenuUIController::HandlePossessionMenuCreateCharacter()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Create new character requested"));

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

	// Diagnostic logging
	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Spawn check: PossessionSubsystem=%s, DefaultPawnClass=%s"),
		PossessionSubsystem ? TEXT("valid") : TEXT("NULL"),
		DefaultPawnClassForNewCharacter ? *DefaultPawnClassForNewCharacter->GetName() : TEXT("NULL"));

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
						UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Registered new pawn record: %s (%s)"),
							*NewRecord.CharacterName, *PawnGuid.ToString());
					}
				}
			}

			ClosePossessionMenu();
			UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Created and possessed new character"));
			return;
		}
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOSysUI] Failed to create new character. Check DefaultPawnClassForNewCharacter is set."));
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

	// Create widget if needed
	UMOConfirmationDialog* DialogWidget = ConfirmationDialogWidget.Get();
	if (!IsValid(DialogWidget))
	{
		DialogWidget = CreateWidget<UMOConfirmationDialog>(PlayerController, ConfirmationDialogClass);
		ConfirmationDialogWidget = DialogWidget;

		if (!IsValid(DialogWidget))
		{
			return;
		}

		// Bind delegates
		DialogWidget->OnConfirmed.RemoveDynamic(this, &UMOSystemMenuUIController::HandleConfirmationConfirmed);
		DialogWidget->OnCancelled.RemoveDynamic(this, &UMOSystemMenuUIController::HandleConfirmationCancelled);
		DialogWidget->OnConfirmed.AddDynamic(this, &UMOSystemMenuUIController::HandleConfirmationConfirmed);
		DialogWidget->OnCancelled.AddDynamic(this, &UMOSystemMenuUIController::HandleConfirmationCancelled);
	}

	// Setup the dialog with content
	DialogWidget->Setup(Title, Message, ConfirmText, CancelText);

	// Show dialog
	if (!DialogWidget->IsInViewport())
	{
		DialogWidget->AddToViewport(ConfirmationDialogZOrder);
	}
}

void UMOSystemMenuUIController::HandleConfirmationConfirmed()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Confirmation confirmed: %s"), *PendingConfirmationContext);

	// Close the confirmation dialog
	UMOConfirmationDialog* DialogWidget = ConfirmationDialogWidget.Get();
	if (IsValid(DialogWidget) && DialogWidget->IsInViewport())
	{
		DialogWidget->RemoveFromParent();
	}

	if (PendingConfirmationContext == TEXT("ExitToMainMenu"))
	{
		// Close all menus via UIManager
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
		UGameplayStatics::OpenLevel(this, *MainMenuLevelPath);
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
			Settings->PendingNewGameSlot = SlotName;  // Reuse field for load slot
			Settings->SaveSettings();
		}

		// Reload the gameplay level to get a clean state
		// The GameMode's BeginPlay will handle loading the save data
		UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Reloading level to load save: %s"), *SlotName);
		UGameplayStatics::OpenLevel(this, *GameplayLevelPath);
	}

	PendingConfirmationContext.Empty();
	OnConfirmationConfirmed.Broadcast();
}

void UMOSystemMenuUIController::HandleConfirmationCancelled()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOSysUI] Confirmation cancelled: %s"), *PendingConfirmationContext);

	// Close the confirmation dialog
	UMOConfirmationDialog* DialogWidget = ConfirmationDialogWidget.Get();
	if (IsValid(DialogWidget) && DialogWidget->IsInViewport())
	{
		DialogWidget->RemoveFromParent();
	}

	PendingConfirmationContext.Empty();
	OnConfirmationCancelled.Broadcast();
}
