#include "MOMainMenuPlayerController.h"
#include "MOAudioSubsystem.h"
#include "MOFramework.h"
#include "MOTravelUtils.h"
#include "MOMainMenuWidget.h"
#include "MOIntroWidget.h"
#include "MOGameSettings.h"
#include "MOGameInstance.h"
#include "MOGameUIManagerSubsystem.h"
#include "MOPrimaryGameLayout.h"
#include "MOSessionSubsystem.h"
#include "MOPersistenceSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "MediaSource.h"
#include "FileMediaSource.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "MediaSoundComponent.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "Misc/PackageName.h"
#include "Engine/Engine.h"

AMOMainMenuPlayerController::AMOMainMenuPlayerController()
{
	// NOTE: Cursor visibility is managed by CommonUI via GetDesiredInputConfig()
	// on the main menu widget. Do not set bShowMouseCursor manually.
}

void AMOMainMenuPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The intro's fallback timer lives in the GAME INSTANCE's timer manager, which outlives this controller (and its world): travelling
	// away from the menu before it fired (hosting a session, joining a friend's invite, loading a save) left a callback holding a dead
	// `this`, and it crashed the packaged game five seconds later (UMediaPlayer::IsPlaying, access violation).
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VideoFallbackTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AMOMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Arriving at the main menu means no game is running. A session still registered here outlived its world
	// (quit to menu, disconnect, travel failure): destroy it so hosting/joining works again and a Steam lobby stops
	// being advertised. This is the one place every route out of gameplay converges.
	if (UMOSessionSubsystem* Sessions = UMOSessionSubsystem::Get(this))
	{
		Sessions->ReleaseStaleSession();
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] BeginPlay called"));
	UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] MainMenuWidgetClass: %s"),
		MainMenuWidgetClass ? *MainMenuWidgetClass->GetName() : TEXT("NULL"));
	UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] IntroWidgetClass: %s"),
		IntroWidgetClass ? *IntroWidgetClass->GetName() : TEXT("NULL"));
	UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] IntroVideoSource: %s, IntroVideoFileName: %s"),
		IntroVideoSource ? *IntroVideoSource->GetName() : TEXT("NULL"),
		*IntroVideoFileName);

	// Bring up the UMOPrimaryGameLayout for the main menu world.
	{
		UMOGameUIManagerSubsystem* UISubsystem = UMOGameUIManagerSubsystem::Get(this);
		UE_LOG(LogMOFramework, Warning, TEXT("[MainMenu-DIAG] BeginPlay: UISubsystem=%s"),
			UISubsystem ? *UISubsystem->GetName() : TEXT("NULL"));
		if (UISubsystem)
		{
			UISubsystem->NotifyPlayerAdded(this);
			UMOPrimaryGameLayout* Layout = UISubsystem->GetRootLayoutForPlayer(this);
			UE_LOG(LogMOFramework, Warning, TEXT("[MainMenu-DIAG] After NotifyPlayerAdded: layout=%s (class=%s)"),
				Layout ? *Layout->GetName() : TEXT("NULL"),
				Layout ? *Layout->GetClass()->GetName() : TEXT("<n/a>"));
		}
	}

	// Packaged builds default to Game-only input mode at PC startup, which prevents
	// Slate (intro widget, main menu) from receiving keyboard/mouse events until a
	// CommonUI widget activates and applies its FUIInputConfig. The intro fires
	// before any UI widget activates, so set GameAndUI here so it can be skipped.
	// CommonUI's Menu input mode takes over once the main menu widget activates.
	FInputModeGameAndUI InitialInputMode;
	InitialInputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InitialInputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InitialInputMode);
	bShowMouseCursor = true;

	// Check if we should play the intro
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	const bool bShouldPlayIntro = Settings ? Settings->bPlayIntro : true;

	if (bShouldPlayIntro)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] Playing intro video"));
		PlayIntroVideo();
	}
	else
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Skipping intro (bPlayIntro=false)"));
		ShowMainMenu();
	}
}

void AMOMainMenuPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Input handling for intro skip is done in the intro widget itself
	// via NativeOnKeyDown and NativeOnMouseButtonDown
}

void AMOMainMenuPlayerController::ShowMainMenu()
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MainMenu-DIAG] ShowMainMenu: existing widget=%s class=%s"),
		MainMenuWidget ? *MainMenuWidget->GetName() : TEXT("NULL"),
		MainMenuWidgetClass ? *MainMenuWidgetClass->GetName() : TEXT("NULL"));

	if (MainMenuWidget)
	{
		MainMenuWidget->SetVisibility(ESlateVisibility::Visible);
		return;
	}

	if (!MainMenuWidgetClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] MainMenuWidgetClass not set!"));
		return;
	}

	// Push to the Menu layer of the primary game layout — the stack handles
	// activation, input config, focus, and registers the widget with the UI
	// manager. This puts the main menu inside the same CommonUI lifecycle
	// the in-game UI uses, so modals spawned from it (via PushModalWidget)
	// nest correctly and input handoff just works on close.
	UMOGameUIManagerSubsystem* UISubsystem = UMOGameUIManagerSubsystem::Get(this);
	UMOPrimaryGameLayout* Layout = UISubsystem ? UISubsystem->GetRootLayoutForPlayer(this) : nullptr;
	UE_LOG(LogMOFramework, Warning, TEXT("[MainMenu-DIAG] ShowMainMenu: layout=%s"),
		Layout ? *Layout->GetName() : TEXT("NULL"));
	if (Layout)
	{
		UCommonActivatableWidget* Pushed = Layout->PushWidgetToLayer(
			MOUILayerTags::Layer_Menu, MainMenuWidgetClass);
		MainMenuWidget = Cast<UMOMainMenuWidget>(Pushed);
		UE_LOG(LogMOFramework, Warning, TEXT("[MainMenu-DIAG] ShowMainMenu: pushed=%s cast=%s"),
			Pushed ? *Pushed->GetClass()->GetName() : TEXT("NULL"),
			MainMenuWidget ? *MainMenuWidget->GetName() : TEXT("NULL"));
		if (MainMenuWidget)
		{
			MainMenuWidget->OnNewGameRequested.AddDynamic(this, &AMOMainMenuPlayerController::HandleNewGameRequested);
			MainMenuWidget->OnLoadGameRequested.AddDynamic(this, &AMOMainMenuPlayerController::HandleLoadGameRequested);
			MainMenuWidget->OnExitGameRequested.AddDynamic(this, &AMOMainMenuPlayerController::HandleExitGameRequested);
			MainMenuWidget->OnHostSessionRequested.AddDynamic(this, &AMOMainMenuPlayerController::HandleHostSessionRequested);
			MainMenuWidget->OnHostSavedGameRequested.AddDynamic(this, &AMOMainMenuPlayerController::HandleHostSavedGameRequested);

			UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Main menu pushed to Menu layer"));
			return;
		}
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] PushWidgetToLayer returned non-MainMenu widget; falling back to AddToViewport"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] No primary layout (PrimaryGameLayoutClass not set in Project Settings?). Falling back to AddToViewport — modals will use viewport fallback path."));
	}

	// Fallback path: if the primary layout couldn't be created (project not
	// configured yet, etc.), keep the menu visible via AddToViewport. This
	// preserves the old behavior so the game still loads — but modals
	// spawned from this menu will go through PushModalWidget's viewport
	// fallback branch, which has slightly weaker input handoff than the
	// layer-stack path. Configure PrimaryGameLayoutClass in
	// Project Settings → Plugins → MO UI Settings to fix.
	MainMenuWidget = CreateWidget<UMOMainMenuWidget>(this, MainMenuWidgetClass);
	if (MainMenuWidget)
	{
		MainMenuWidget->AddToViewport(100);
		MainMenuWidget->ActivateWidget();

		MainMenuWidget->OnNewGameRequested.AddDynamic(this, &AMOMainMenuPlayerController::HandleNewGameRequested);
		MainMenuWidget->OnLoadGameRequested.AddDynamic(this, &AMOMainMenuPlayerController::HandleLoadGameRequested);
		MainMenuWidget->OnExitGameRequested.AddDynamic(this, &AMOMainMenuPlayerController::HandleExitGameRequested);
		MainMenuWidget->OnHostSessionRequested.AddDynamic(this, &AMOMainMenuPlayerController::HandleHostSessionRequested);
		MainMenuWidget->OnHostSavedGameRequested.AddDynamic(this, &AMOMainMenuPlayerController::HandleHostSavedGameRequested);
	}
}

void AMOMainMenuPlayerController::HideMainMenu()
{
	if (MainMenuWidget)
	{
		MainMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void AMOMainMenuPlayerController::PlayIntroVideo()
{
	if (!IntroWidgetClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] IntroWidgetClass not set, skipping intro"));
		HandleIntroComplete();
		return;
	}

	// Get or create the media source (handles both asset and runtime creation)
	UMediaSource* MediaSource = GetOrCreateIntroMediaSource();
	if (!MediaSource)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] No media source available, skipping intro"));
		HandleIntroComplete();
		return;
	}

	if (!VideoMaterial)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] No VideoMaterial set, skipping intro"));
		HandleIntroComplete();
		return;
	}

	bIntroPlaying = true;

	// NOTE: Cursor visibility managed by CommonUI via widget GetDesiredInputConfig()

	// Setup media player and components
	SetupMediaPlayer();

	UE_LOG(LogMOFramework, Warning, TEXT("[MainMenu-DIAG] PlayIntroVideo: pushing intro to Modal layer"));

	// Push intro widget onto the Modal layer so it lives in the CommonUI
	// stack. The stack auto-activates it (which claims focus via
	// NativeGetDesiredFocusTarget → self) and removes it on deactivate
	// (called from SkipIntro / OnVideoFinished). No manual SetFocus needed.
	UMOGameUIManagerSubsystem* UISubsystem = UMOGameUIManagerSubsystem::Get(this);
	UMOPrimaryGameLayout* Layout = UISubsystem ? UISubsystem->GetRootLayoutForPlayer(this) : nullptr;
	UE_LOG(LogMOFramework, Warning, TEXT("[MainMenu-DIAG] PlayIntroVideo: layout=%s IntroWidgetClass=%s"),
		Layout ? *Layout->GetName() : TEXT("NULL"),
		IntroWidgetClass ? *IntroWidgetClass->GetName() : TEXT("NULL"));

	if (Layout)
	{
		UCommonActivatableWidget* PushedIntro = Layout->PushWidgetToLayer(
			MOUILayerTags::Layer_Modal, IntroWidgetClass);
		IntroWidget = Cast<UMOIntroWidget>(PushedIntro);
		UE_LOG(LogMOFramework, Warning, TEXT("[MainMenu-DIAG] PlayIntroVideo: PushWidgetToLayer returned %s, cast result: %s"),
			PushedIntro ? *PushedIntro->GetClass()->GetName() : TEXT("NULL"),
			IntroWidget ? *IntroWidget->GetName() : TEXT("NULL (cast failed)"));
	}
	else
	{
		// Fallback: layout not available (PrimaryGameLayoutClass unset in
		// project settings). Old AddToViewport path so the game still boots.
		UE_LOG(LogMOFramework, Warning, TEXT("[MainMenu-DIAG] PlayIntroVideo: NO LAYOUT — using AddToViewport fallback"));
		IntroWidget = CreateWidget<UMOIntroWidget>(this, IntroWidgetClass);
		if (IntroWidget)
		{
			IntroWidget->AddToViewport(200);
			IntroWidget->ActivateWidget();
		}
	}

	if (IntroWidget)
	{
		IntroWidget->OnIntroCompleted.AddDynamic(this, &AMOMainMenuPlayerController::HandleIntroComplete);
		IntroWidget->OnIntroSkipped.AddDynamic(this, &AMOMainMenuPlayerController::HandleIntroComplete);

		// Pass the video material to the widget
		IntroWidget->SetVideoMaterial(VideoMaterialInstance);

		// Open the media source - playback will start in HandleMediaOpened
		if (MediaPlayer)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Opening media source: %s"), *MediaSource->GetName());

			// Set a fallback timer in case media fails to open
			GetWorldTimerManager().SetTimer(VideoFallbackTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (bIntroPlaying && MediaPlayer && !MediaPlayer->IsPlaying())
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] Media failed to start after timeout, skipping intro"));
					CleanupMediaPlayer();
					HandleIntroComplete();
				}
			}), 5.0f, false);  // 5 second timeout (weak: dropped if this controller is gone when it fires)

			MediaPlayer->OpenSource(MediaSource);
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] MediaPlayer is null"));
			HandleIntroComplete();
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] Failed to create IntroWidget"));
		CleanupMediaPlayer();
		HandleIntroComplete();
	}
}

void AMOMainMenuPlayerController::SkipIntroVideo()
{
	if (bIntroPlaying)
	{
		// Stop media playback
		if (MediaPlayer)
		{
			MediaPlayer->Close();
		}

		// Tell intro widget to skip
		if (IntroWidget)
		{
			IntroWidget->SkipIntro();
		}
	}
}

bool AMOMainMenuPlayerController::ValidateGameplayLevelExists() const
{
	FString LevelPackagePath = GameplayLevelPath;
	if (!LevelPackagePath.StartsWith(TEXT("/Game/")))
	{
		LevelPackagePath = FString::Printf(TEXT("/Game/%s"), *GameplayLevelPath);
	}

	if (FPackageName::DoesPackageExist(LevelPackagePath))
	{
		return true;
	}

	UE_LOG(LogMOFramework, Error, TEXT("[MOMainMenuPlayerController] Gameplay level not found: %s"), *LevelPackagePath);
	UE_LOG(LogMOFramework, Error, TEXT("[MOMainMenuPlayerController] Make sure the level is included in packaging settings (MapsToCook or DirectoriesToAlwaysCook)"));

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red,
			FString::Printf(TEXT("ERROR: Level '%s' not found! Check packaging settings."), *GameplayLevelPath));
	}
#endif
	return false;
}

void AMOMainMenuPlayerController::StartNewGame()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Starting new game"));

	if (!ValidateGameplayLevelExists())
	{
		return;
	}

	// Set pending new game flags in settings
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (Settings)
	{
		Settings->bPendingNewGame = true;
		Settings->bIsLoadingIntoGameplay = true;  // Keep loading screen until pawn lands
		// Generate a slot name - this will be replaced with proper naming later
		Settings->PendingNewGameSlot = FString::Printf(TEXT("World_%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
		Settings->SaveSettings();

		UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Created pending new game slot: %s"), *Settings->PendingNewGameSlot);
	}

	// Show loading overlay FIRST, then delay level load to let it render
	if (UMOGameInstance* GameInstance = Cast<UMOGameInstance>(GetGameInstance()))
	{
		GameInstance->ShowLoadingOverlay();
	}

	// Store level path and delay the actual load so loading screen is visible
	PendingLevelPath = GameplayLevelPath;
	GetWorld()->GetTimerManager().SetTimer(
		DelayedLevelLoadTimerHandle,
		this,
		&AMOMainMenuPlayerController::ExecuteDelayedLevelLoad,
		2.0f,  // 2 second delay - let player read loading screen
		false
	);
}

void AMOMainMenuPlayerController::LoadGame(const FString& SlotName)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Loading game from slot: %s"), *SlotName);

	if (!ValidateGameplayLevelExists())
	{
		return;
	}

	// Set pending load slot in settings
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (Settings)
	{
		Settings->bPendingNewGame = false;  // Not a new game
		Settings->bIsLoadingIntoGameplay = true;  // Keep loading screen until pawn lands
		Settings->PendingNewGameSlot = SlotName;  // Reuse field for load slot
		Settings->SaveSettings();
	}

	// Show loading overlay FIRST, then delay level load to let it render
	if (UMOGameInstance* GameInstance = Cast<UMOGameInstance>(GetGameInstance()))
	{
		GameInstance->ShowLoadingOverlay();
	}

	// Store level path and delay the actual load so loading screen is visible
	PendingLevelPath = GameplayLevelPath;
	GetWorld()->GetTimerManager().SetTimer(
		DelayedLevelLoadTimerHandle,
		this,
		&AMOMainMenuPlayerController::ExecuteDelayedLevelLoad,
		2.0f,  // 2 second delay - let player read loading screen
		false
	);
}

void AMOMainMenuPlayerController::HostSession(const FString& SessionDisplayName, int32 MaxPlayers)
{
	StartHosting(SessionDisplayName, MaxPlayers, FString());
}

void AMOMainMenuPlayerController::HostSavedGame(const FString& SlotName)
{
	FString DisplayName = SlotName;
	bool bFound = false;
	if (UMOPersistenceSubsystem* Persistence = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMOPersistenceSubsystem>() : nullptr)
	{
		FMOSaveMetadata Meta;
		if (Persistence->GetSaveSlotMetadata(SlotName, Meta))
		{
			bFound = true;
			if (!Meta.DisplayName.IsEmpty())
			{
				DisplayName = Meta.DisplayName.ToString();
			}
		}
	}
	if (!bFound)
	{
		// Say so on the panel that asked, instead of loading a world that is not there.
		UE_LOG(LogMOFramework, Error, TEXT("[MOMainMenuPlayerController] HostSavedGame: save slot '%s' not found"), *SlotName);
		if (MainMenuWidget)
		{
			MainMenuWidget->NotifyHostSessionResult(false, FString::Printf(TEXT("Save '%s' could not be found."), *SlotName));
		}
		return;
	}
	StartHosting(DisplayName, UMOSessionSubsystem::DefaultMaxPlayers, SlotName);
}

void AMOMainMenuPlayerController::StartHosting(const FString& SessionDisplayName, int32 MaxPlayers, const FString& SaveSlotToResume)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Hosting requested: '%s' (max %d) %s"),
		*SessionDisplayName, MaxPlayers,
		SaveSlotToResume.IsEmpty() ? TEXT("-- new world") : *FString::Printf(TEXT("-- resuming save '%s'"), *SaveSlotToResume));

	if (!ValidateGameplayLevelExists())
	{
		if (MainMenuWidget)
		{
			MainMenuWidget->NotifyHostSessionResult(false, TEXT("Gameplay level not found."));
		}
		return;
	}

	UMOSessionSubsystem* SessionSubsystem = UMOSessionSubsystem::Get(this);
	if (!SessionSubsystem)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOMainMenuPlayerController] HostSession: no session subsystem"));
		if (MainMenuWidget)
		{
			MainMenuWidget->NotifyHostSessionResult(false, TEXT("No online subsystem available."));
		}
		return;
	}

	// Same pending-world setup as StartNewGame (fresh world) or LoadGame (resume a save); the listen-server GameMode
	// then runs the exact path a single-player game would, and publishes the world seed to joiners.
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (Settings)
	{
		Settings->bIsLoadingIntoGameplay = true;
		if (SaveSlotToResume.IsEmpty())
		{
			Settings->bPendingNewGame = true;
			Settings->PendingNewGameSlot = FString::Printf(TEXT("World_%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
			// Fresh world = fresh seed. PendingWorldSeed is never cleared, so without this a hosted world reused whatever
			// seed the last New Game panel / loaded save / hosted game left behind (every hosted world had the same
			// terrain). 0 = "pick a random seed" in AMOGameMode::HandlePendingNewGame.
			Settings->PendingWorldSeed = 0;
		}
		else
		{
			// Not a new game: PendingNewGameSlot doubles as the slot to load, and the seed comes from the save itself.
			Settings->bPendingNewGame = false;
			Settings->PendingNewGameSlot = SaveSlotToResume;
		}
		Settings->SaveSettings();
	}

	// Show the loading overlay now — CreateSession's own round-trip to Steam
	// provides the visible wait; unlike StartNewGame/LoadGame there's no
	// separate artificial delay timer here, and no ExecuteDelayedLevelLoad —
	// UMOSessionSubsystem drives the actual travel itself once CreateSession
	// succeeds (TravelToGameplayLevel with bAsListenServer=true).
	if (UMOGameInstance* GameInstance = Cast<UMOGameInstance>(GetGameInstance()))
	{
		GameInstance->ShowLoadingOverlay();
	}

	SessionSubsystem->OnHostComplete.RemoveDynamic(this, &AMOMainMenuPlayerController::HandleHostSessionComplete);
	SessionSubsystem->OnHostComplete.AddDynamic(this, &AMOMainMenuPlayerController::HandleHostSessionComplete);
	SessionSubsystem->HostSession(SessionDisplayName, MaxPlayers, GameplayLevelPath);
}

void AMOMainMenuPlayerController::HandleHostSessionComplete(bool bSuccess, const FString& ErrorMessage)
{
	if (UMOSessionSubsystem* SessionSubsystem = UMOSessionSubsystem::Get(this))
	{
		SessionSubsystem->OnHostComplete.RemoveDynamic(this, &AMOMainMenuPlayerController::HandleHostSessionComplete);
	}

	if (MainMenuWidget)
	{
		MainMenuWidget->NotifyHostSessionResult(bSuccess, ErrorMessage);
	}

	if (bSuccess)
	{
		// Travel is already underway (UMOSessionSubsystem drives it once
		// CreateSession succeeds) — nothing further to do here.
		return;
	}

	UE_LOG(LogMOFramework, Error, TEXT("[MOMainMenuPlayerController] HostSession failed: %s"), *ErrorMessage);

	// Undo the pending-new-game state and drop the loading overlay so the
	// player isn't stuck staring at a black screen after a failed host attempt.
	if (UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings())
	{
		Settings->bPendingNewGame = false;
		Settings->bIsLoadingIntoGameplay = false;
		Settings->SaveSettings();
	}
	if (UMOGameInstance* GameInstance = Cast<UMOGameInstance>(GetGameInstance()))
	{
		// DismissLoadingScreen() would no-op here: it gates on
		// bWaitingForManualDismiss, which only a real level transition sets,
		// and no travel ever happened on this failure path.
		GameInstance->HideLoadingOverlayImmediate();
	}

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red,
			FString::Printf(TEXT("Failed to host session: %s"), *ErrorMessage));
	}
#endif
}

void AMOMainMenuPlayerController::ExitGame()
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] ExitGame requested — saving settings and quitting"));

	// Save settings before exit
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (Settings)
	{
		Settings->SaveSettings();
	}

	// Quit. Use the world as the context object (more reliable than the PC in a
	// menu world that has no pawn) and ignore platform restrictions so a desktop
	// standalone build actually closes. QuitGame from a menu-world PC has been
	// observed to no-op in packaged builds, so we follow with a hard platform
	// exit request as a fallback (harmless if the engine is already tearing down).
	UWorld* World = GetWorld();
	UKismetSystemLibrary::QuitGame(World ? static_cast<UObject*>(World) : static_cast<UObject*>(this),
		this, EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/true);
	FPlatformMisc::RequestExit(/*Force=*/false);

	UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] ExitGame: QuitGame + RequestExit issued"));
}

void AMOMainMenuPlayerController::ExecuteDelayedLevelLoad()
{
	if (PendingLevelPath.IsEmpty())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] ExecuteDelayedLevelLoad called with empty path"));
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Opening level: %s"), *PendingLevelPath);
	// Net-mode-aware: OpenLevel here silently dropped the listen-server role and
	// every connected client (mptest, 2026-07-03). The helper ServerTravels when
	// hosting and OpenLevels in standalone.
	UMOTravelUtils::TravelToGameplayLevel(this, PendingLevelPath);
	PendingLevelPath.Empty();
}

void AMOMainMenuPlayerController::HandleAnyKeyPressed()
{
	if (bIntroPlaying)
	{
		SkipIntroVideo();
	}
}

void AMOMainMenuPlayerController::HandleIntroComplete()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Intro complete"));

	bIntroPlaying = false;

	// NOTE: Cursor visibility managed by CommonUI via widget GetDesiredInputConfig()

	// Mark intro as played so it won't play again this session
	if (UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings())
	{
		Settings->bPlayIntro = false;
	}

	// Clear fallback timer
	GetWorldTimerManager().ClearTimer(VideoFallbackTimerHandle);

	// Clean up media player
	CleanupMediaPlayer();

	// Intro widget deactivates itself in SkipIntro/OnVideoFinished, which
	// triggers the layer stack to remove it (or RemoveFromParent in the
	// AddToViewport fallback path). Just clear our reference — no manual
	// removal needed.
	IntroWidget = nullptr;

	// NOW start main-menu music. AMOMainMenuGameMode::BeginPlay deferred the
	// call because the intro has its own audio — starting Cedar in parallel
	// with the intro video clashes. Intro is done, safe to start now.
	if (UWorld* World = GetWorld())
	{
		if (UMOAudioSubsystem* Audio = UMOAudioSubsystem::Get(World))
		{
			Audio->HandleWorldAudioContext(World);
		}
	}

	// Show main menu
	ShowMainMenu();
}

void AMOMainMenuPlayerController::HandleNewGameRequested()
{
	StartNewGame();
}

void AMOMainMenuPlayerController::HandleLoadGameRequested(const FString& SlotName)
{
	LoadGame(SlotName);
}

void AMOMainMenuPlayerController::HandleExitGameRequested()
{
	ExitGame();
}

void AMOMainMenuPlayerController::HandleHostSessionRequested(const FString& DisplayName, int32 MaxPlayers)
{
	HostSession(DisplayName, MaxPlayers);
}

void AMOMainMenuPlayerController::HandleHostSavedGameRequested(const FString& SlotName)
{
	HostSavedGame(SlotName);
}

void AMOMainMenuPlayerController::SetupMediaPlayer()
{
	// Create media player
	MediaPlayer = NewObject<UMediaPlayer>(this);
	if (!MediaPlayer)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOMainMenuPlayerController] Failed to create MediaPlayer"));
		return;
	}

	// Bind media events
	MediaPlayer->OnEndReached.AddDynamic(this, &AMOMainMenuPlayerController::HandleMediaEndReached);
	MediaPlayer->OnMediaOpened.AddDynamic(this, &AMOMainMenuPlayerController::HandleMediaOpened);
	MediaPlayer->OnMediaOpenFailed.AddDynamic(this, &AMOMainMenuPlayerController::HandleMediaOpenFailed);

	// Create media texture linked to the player
	MediaTexture = NewObject<UMediaTexture>(this);
	if (MediaTexture)
	{
		MediaTexture->SetMediaPlayer(MediaPlayer);
		MediaTexture->UpdateResource();
	}

	// Create sound component attached to this controller (which is an Actor)
	MediaSoundComponent = NewObject<UMediaSoundComponent>(this);
	if (MediaSoundComponent)
	{
		MediaSoundComponent->SetMediaPlayer(MediaPlayer);
		MediaSoundComponent->RegisterComponent();
		UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] MediaSoundComponent created and registered"));
	}

	// Create dynamic material instance from the video material
	if (VideoMaterial)
	{
		VideoMaterialInstance = UMaterialInstanceDynamic::Create(VideoMaterial, this);
		if (VideoMaterialInstance && MediaTexture)
		{
			// Set the media texture on the material
			VideoMaterialInstance->SetTextureParameterValue(FName("MediaTexture"), MediaTexture);
			UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Video material instance created with MediaTexture"));
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Media player setup complete"));
}

void AMOMainMenuPlayerController::CleanupMediaPlayer()
{
	if (MediaPlayer)
	{
		MediaPlayer->Close();
		MediaPlayer->OnEndReached.RemoveAll(this);
		MediaPlayer->OnMediaOpened.RemoveAll(this);
		MediaPlayer->OnMediaOpenFailed.RemoveAll(this);
		MediaPlayer = nullptr;
	}

	if (MediaSoundComponent)
	{
		MediaSoundComponent->Stop();
		MediaSoundComponent->UnregisterComponent();
		MediaSoundComponent = nullptr;
	}

	MediaTexture = nullptr;
	VideoMaterialInstance = nullptr;
	RuntimeMediaSource = nullptr;

	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Media player cleaned up"));
}

UMediaSource* AMOMainMenuPlayerController::GetOrCreateIntroMediaSource()
{
	// In packaged builds, ALWAYS use runtime path resolution since asset paths don't work
	// In editor, prefer the asset if set, otherwise use runtime path
#if WITH_EDITOR
	if (IntroVideoSource)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Using asset-based IntroVideoSource in editor"));
		return IntroVideoSource;
	}
#endif

	// Create a FileMediaSource at runtime with the correct absolute path
	if (IntroVideoFileName.IsEmpty())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] No IntroVideoFileName set"));
		// Fallback to asset in editor if filename is empty
#if WITH_EDITOR
		return IntroVideoSource;
#else
		return nullptr;
#endif
	}

	// Build the correct path for both editor and packaged builds
	// Videos are staged as non-UFS content at: Content/Penumbra/Movies/
	FString VideoPath;

	// Try multiple possible locations
	TArray<FString> PossiblePaths;

#if WITH_EDITOR
	// In editor, try both standard and custom locations
	PossiblePaths.Add(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Movies"), IntroVideoFileName));
	PossiblePaths.Add(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Penumbra/Movies"), IntroVideoFileName));
#else
	// In packaged build, try several locations
	// 1. Standard Content/Movies location (UE default for movies)
	PossiblePaths.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Content/Movies"), IntroVideoFileName));
	// 2. Content/Movies relative to launch dir
	PossiblePaths.Add(FPaths::Combine(FPaths::LaunchDir(), TEXT("Content/Movies"), IntroVideoFileName));
	// 3. Legacy Penumbra location
	PossiblePaths.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Content/Penumbra/Movies"), IntroVideoFileName));
	// 4. Just the Movies folder relative to the game (non-UFS staging)
	PossiblePaths.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Movies"), IntroVideoFileName));
	// 5. Movies folder relative to launch dir
	PossiblePaths.Add(FPaths::Combine(FPaths::LaunchDir(), TEXT("Movies"), IntroVideoFileName));
#endif

	// Find the first path that exists
	bool bFoundVideo = false;
	for (const FString& Path : PossiblePaths)
	{
		FString FullPath = FPaths::ConvertRelativePathToFull(Path);
		UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Checking video path: %s"), *FullPath);
		if (FPaths::FileExists(FullPath))
		{
			VideoPath = FullPath;
			bFoundVideo = true;
			UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Found video at: %s"), *VideoPath);
			break;
		}
	}

	if (!bFoundVideo)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] Video file not found in any searched location"));
		// Log all searched paths for debugging
		for (const FString& Path : PossiblePaths)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController]   Searched: %s"), *FPaths::ConvertRelativePathToFull(Path));
		}
		return nullptr;
	}

	// Create the FileMediaSource
	RuntimeMediaSource = NewObject<UFileMediaSource>(this);
	if (UFileMediaSource* FileSource = Cast<UFileMediaSource>(RuntimeMediaSource))
	{
		FileSource->SetFilePath(VideoPath);
		UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Created runtime FileMediaSource: %s"), *VideoPath);
	}

	return RuntimeMediaSource;
}

void AMOMainMenuPlayerController::HandleMediaOpened(FString OpenedUrl)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Media opened: %s"), *OpenedUrl);

	// Clear the fallback timer since media opened successfully
	GetWorldTimerManager().ClearTimer(VideoFallbackTimerHandle);

	if (MediaPlayer)
	{
		// Start playback
		MediaPlayer->Play();
		UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Media playback started"));

		// Notify intro widget that playback started
		if (IntroWidget)
		{
			IntroWidget->OnPlaybackStarted();
		}
	}
}

void AMOMainMenuPlayerController::HandleMediaOpenFailed(FString FailedUrl)
{
	UE_LOG(LogMOFramework, Error, TEXT("[MOMainMenuPlayerController] Failed to open media: %s"), *FailedUrl);
	CleanupMediaPlayer();
	HandleIntroComplete();
}

void AMOMainMenuPlayerController::HandleMediaEndReached()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Media playback ended"));

	// Notify intro widget that video finished
	if (IntroWidget)
	{
		IntroWidget->OnVideoFinished();
	}
}
