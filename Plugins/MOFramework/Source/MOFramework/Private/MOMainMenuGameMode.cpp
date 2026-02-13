#include "MOMainMenuGameMode.h"
#include "MOFramework.h"
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
	// Skip parent's BeginPlay which registers PCG tag mappings
	// We don't need harvesting functionality in the main menu
	// Call AGameModeBase::BeginPlay() directly
	AGameModeBase::BeginPlay();

	UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuGameMode] Main menu level started"));
	UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuGameMode] PlayerControllerClass: %s"),
		PlayerControllerClass ? *PlayerControllerClass->GetName() : TEXT("NULL"));
	UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuGameMode] DefaultPawnClass: %s"),
		DefaultPawnClass ? *DefaultPawnClass->GetName() : TEXT("NULL (correct for menu)"));
}
