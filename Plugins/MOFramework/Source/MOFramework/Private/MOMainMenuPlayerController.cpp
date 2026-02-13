#include "MOMainMenuPlayerController.h"
#include "MOFramework.h"
#include "MOMainMenuWidget.h"
#include "MOIntroWidget.h"
#include "MOGameSettings.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "MediaSource.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "MediaSoundComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

AMOMainMenuPlayerController::AMOMainMenuPlayerController()
{
	// Show mouse cursor in menu
	bShowMouseCursor = true;
}

void AMOMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] BeginPlay called"));
	UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] MainMenuWidgetClass: %s"),
		MainMenuWidgetClass ? *MainMenuWidgetClass->GetName() : TEXT("NULL"));
	UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] IntroWidgetClass: %s"),
		IntroWidgetClass ? *IntroWidgetClass->GetName() : TEXT("NULL"));
	UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] IntroVideoSource: %s"),
		IntroVideoSource ? *IntroVideoSource->GetName() : TEXT("NULL"));

	// Set input mode to UI only for menu
	FInputModeUIOnly InputMode;
	SetInputMode(InputMode);

	// Always play intro - user can skip with any key
	UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] Playing intro video"));
	PlayIntroVideo();
}

void AMOMainMenuPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Input handling for intro skip is done in the intro widget itself
	// via NativeOnKeyDown and NativeOnMouseButtonDown
}

void AMOMainMenuPlayerController::ShowMainMenu()
{
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

	MainMenuWidget = CreateWidget<UMOMainMenuWidget>(this, MainMenuWidgetClass);
	if (MainMenuWidget)
	{
		MainMenuWidget->AddToViewport(100);

		// Bind menu callbacks
		MainMenuWidget->OnNewGameRequested.AddDynamic(this, &AMOMainMenuPlayerController::HandleNewGameRequested);
		MainMenuWidget->OnLoadGameRequested.AddDynamic(this, &AMOMainMenuPlayerController::HandleLoadGameRequested);
		MainMenuWidget->OnExitGameRequested.AddDynamic(this, &AMOMainMenuPlayerController::HandleExitGameRequested);

		UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Main menu displayed"));
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

	if (!IntroVideoSource)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuPlayerController] No IntroVideoSource set, skipping intro"));
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

	// Setup media player and components
	SetupMediaPlayer();

	// Create intro widget
	IntroWidget = CreateWidget<UMOIntroWidget>(this, IntroWidgetClass);
	if (IntroWidget)
	{
		IntroWidget->AddToViewport(200);  // Above main menu
		IntroWidget->OnIntroCompleted.AddDynamic(this, &AMOMainMenuPlayerController::HandleIntroComplete);
		IntroWidget->OnIntroSkipped.AddDynamic(this, &AMOMainMenuPlayerController::HandleIntroComplete);

		// Set focus on intro widget so it can receive key input
		IntroWidget->SetFocus();

		// Pass the video material to the widget
		IntroWidget->SetVideoMaterial(VideoMaterialInstance);

		// Open the media source - playback will start in HandleMediaOpened
		if (MediaPlayer)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Opening media source: %s"), *IntroVideoSource->GetName());
			MediaPlayer->OpenSource(IntroVideoSource);
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

void AMOMainMenuPlayerController::StartNewGame()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Starting new game"));

	// Set pending new game flags in settings
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (Settings)
	{
		Settings->bPendingNewGame = true;
		// Generate a slot name - this will be replaced with proper naming later
		Settings->PendingNewGameSlot = FString::Printf(TEXT("World_%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
		Settings->SaveSettings();

		UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Created pending new game slot: %s"), *Settings->PendingNewGameSlot);
	}

	// Load gameplay level
	UGameplayStatics::OpenLevel(this, *GameplayLevelPath);
}

void AMOMainMenuPlayerController::LoadGame(const FString& SlotName)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Loading game from slot: %s"), *SlotName);

	// Set pending load slot in settings
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (Settings)
	{
		Settings->bPendingNewGame = false;  // Not a new game
		Settings->PendingNewGameSlot = SlotName;  // Reuse field for load slot
		Settings->SaveSettings();
	}

	// Load gameplay level - the gameplay GameMode will handle loading the save
	UGameplayStatics::OpenLevel(this, *GameplayLevelPath);
}

void AMOMainMenuPlayerController::ExitGame()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Exiting game"));

	// Save settings before exit
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (Settings)
	{
		Settings->SaveSettings();
	}

	// Exit the game
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
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

	// Clean up media player
	CleanupMediaPlayer();

	// Clean up intro widget
	if (IntroWidget)
	{
		IntroWidget->RemoveFromParent();
		IntroWidget = nullptr;
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

	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Media player cleaned up"));
}

void AMOMainMenuPlayerController::HandleMediaOpened(FString OpenedUrl)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuPlayerController] Media opened: %s"), *OpenedUrl);

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
