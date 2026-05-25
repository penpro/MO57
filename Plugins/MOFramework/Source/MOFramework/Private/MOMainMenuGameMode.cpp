#include "MOMainMenuGameMode.h"
#include "MOAudioSubsystem.h"
#include "MOFramework.h"
#include "MOGameSettings.h"
#include "MOMainMenuPlayerController.h"

AMOMainMenuGameMode::AMOMainMenuGameMode()
{
	// No pawn in menu - player just sees UI
	DefaultPawnClass = nullptr;

	// Use main menu player controller
	PlayerControllerClass = AMOMainMenuPlayerController::StaticClass();
}

void AMOMainMenuGameMode::BeginPlay()
{
	// Skip parent's BeginPlay which registers PCG tag mappings + tries to
	// HandlePendingNewGame. Call AGameModeBase::BeginPlay() directly.
	AGameModeBase::BeginPlay();

	UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuGameMode] Main menu level started"));
	UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuGameMode] PlayerControllerClass: %s"),
		PlayerControllerClass ? *PlayerControllerClass->GetName() : TEXT("NULL"));
	UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuGameMode] DefaultPawnClass: %s"),
		DefaultPawnClass ? *DefaultPawnClass->GetName() : TEXT("NULL (correct for menu)"));

	// Trigger main-menu music — but only if the intro video is NOT going to
	// play. The intro has its own audio; starting Cedar over it would clash.
	// When intro is queued, MOMainMenuPlayerController::HandleIntroComplete
	// calls HandleWorldAudioContext after the video finishes. When intro is
	// already done for this session (returning from gameplay), bPlayIntro
	// is false and we can start Cedar immediately.
	const UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	const bool bIntroWillPlay = Settings && Settings->bPlayIntro;
	if (bIntroWillPlay)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOMainMenuGameMode] Intro queued — deferring menu music until HandleIntroComplete"));
	}
	else if (UWorld* World = GetWorld())
	{
		if (UMOAudioSubsystem* Audio = UMOAudioSubsystem::Get(World))
		{
			Audio->HandleWorldAudioContext(World);
		}
	}
}

APawn* AMOMainMenuGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	// No pawn in menu mode - return nullptr without triggering engine warnings
	return nullptr;
}

APawn* AMOMainMenuGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	// No pawn in menu mode - return nullptr without triggering engine warnings
	return nullptr;
}
