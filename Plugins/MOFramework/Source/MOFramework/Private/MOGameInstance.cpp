#include "MOGameInstance.h"
#include "MOFramework.h"
#include "MOGameSettings.h"
#include "MOLoadingOverlay.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

// Console command: MO.Loading.Skip [0|1]
//   Toggle the black loading overlay on/off (debug). The voxel-wait + pawn
//   spawn-wait sequence still runs identically — only the visual overlay
//   is suppressed so you can see the world during the wait.
static FAutoConsoleCommandWithWorldAndArgs GMOLoadingSkipCmd(
	TEXT("MO.Loading.Skip"),
	TEXT("Toggle loading overlay visibility (debug). Usage: MO.Loading.Skip [0|1]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (!World) return;
			UMOGameInstance* GI = Cast<UMOGameInstance>(World->GetGameInstance());
			if (!GI) return;

			bool bNewValue = !GI->bSkipLoadingOverlay; // toggle by default
			if (Args.Num() > 0)
			{
				bNewValue = (Args[0] == TEXT("1") || Args[0].ToBool());
			}
			GI->bSkipLoadingOverlay = bNewValue;

			UE_LOG(LogMOFramework, Warning,
				TEXT("[MOGameInstance] bSkipLoadingOverlay = %s"),
				bNewValue ? TEXT("TRUE (overlay HIDDEN)") : TEXT("false (overlay normal)"));
		}));

void UMOGameInstance::Init()
{
	Super::Init();

	// Reset intro playback flag on fresh game launch
	// This ensures the intro plays when the game starts, but not when
	// returning to main menu from gameplay (which sets bPlayIntro = false)
	if (UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings())
	{
		Settings->bPlayIntro = true;
	}

	// Bind loading screen delegates
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UMOGameInstance::BeginLoadingScreen);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UMOGameInstance::EndLoadingScreen);
}

void UMOGameInstance::Shutdown()
{
	// Unbind loading screen delegates
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	// Clean up loading overlay
	if (LoadingOverlayWidget)
	{
		LoadingOverlayWidget->RemoveFromParent();
		LoadingOverlayWidget = nullptr;
	}

	Super::Shutdown();
}

void UMOGameInstance::ShowLoadingOverlay()
{
	// Debug short-circuit — overlay suppressed via MO.Loading.Skip console cmd.
	if (bSkipLoadingOverlay)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOGameInstance] ShowLoadingOverlay: skipped (bSkipLoadingOverlay=true)"));
		return;
	}

	if (!LoadingOverlayClass)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOGameInstance] ShowLoadingOverlay: LoadingOverlayClass not set in BP_MOGameInstance!"));
		return;
	}

	// Remove any existing overlay
	if (LoadingOverlayWidget)
	{
		LoadingOverlayWidget->RemoveFromParent();
		LoadingOverlayWidget = nullptr;
	}

	// Create new overlay
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC)
	{
		LoadingOverlayWidget = CreateWidget<UMOLoadingOverlay>(PC, LoadingOverlayClass);
		if (LoadingOverlayWidget)
		{
			// Set random loading tip if available
			if (LoadingTips.Num() > 0)
			{
				const int32 TipIndex = FMath::RandRange(0, LoadingTips.Num() - 1);
				LoadingOverlayWidget->SetLoadingText(LoadingTips[TipIndex]);
			}

			// Add to viewport at highest Z-order
			LoadingOverlayWidget->AddToViewport(9999);
			LoadingOverlayWidget->ShowOverlay();
		}
		else
		{
			UE_LOG(LogMOFramework, Error, TEXT("[MOGameInstance] CreateWidget failed to create overlay!"));
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGameInstance] ShowLoadingOverlay: No player controller available yet"));
	}
}

void UMOGameInstance::BeginLoadingScreen(const FString& MapName)
{
	// Skip loading screen for initial launch to intro
	if (!ShouldShowLoadingScreen(MapName))
	{
		bWaitingForManualDismiss = false;
		return;
	}

	// Check if transitioning to gameplay (needs manual dismissal)
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	const bool bToGameplay = Settings && Settings->bIsLoadingIntoGameplay;

	if (bToGameplay)
	{
		// Just set the flag - we'll create the overlay in EndLoadingScreen
		// after the new level loads (otherwise it gets destroyed with old level)
		bWaitingForManualDismiss = true;
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameInstance] Loading into gameplay - will show overlay after level loads"));
	}
	else
	{
		bWaitingForManualDismiss = false;
	}
}

void UMOGameInstance::EndLoadingScreen(UWorld* InLoadedWorld)
{
	if (bWaitingForManualDismiss)
	{
		// Create the overlay now in the new level's context
		// (creating it in BeginLoadingScreen would destroy it with the old level)
		ShowLoadingOverlay();
	}
	else if (LoadingOverlayWidget)
	{
		// Not waiting for manual dismiss, hide the overlay now
		LoadingOverlayWidget->FadeOutAndRemove();
		LoadingOverlayWidget = nullptr;
	}
}

bool UMOGameInstance::ShouldShowLoadingScreen(const FString& MapName) const
{
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (!Settings)
	{
		return true;
	}

	// Skip loading screen for initial launch to main menu (intro will play)
	// The main menu level contains "LoadingLevel" in its name
	if (Settings->bPlayIntro && MapName.Contains(TEXT("LoadingLevel")))
	{
		return false;  // Just stay black until intro video starts
	}

	return true;
}

void UMOGameInstance::DismissLoadingScreen()
{
	if (!bWaitingForManualDismiss)
	{
		return;
	}

	if (LoadingOverlayWidget)
	{
		// Override the WBP's FadeOutDuration with the GameInstance setting,
		// so the fade is consistent regardless of WBP defaults. 1.0s default
		// gives the world a moment to settle visually before player input.
		UE_LOG(LogMOFramework, Log,
			TEXT("[MOGameInstance] DismissLoadingScreen: starting %.1fs fade-out"),
			LoadingScreenFadeOutSeconds);

		LoadingOverlayWidget->FadeOutAndRemove(LoadingScreenFadeOutSeconds);
		// Don't null the pointer - the widget will remove itself after fade
	}

	bWaitingForManualDismiss = false;

	// Clear the gameplay transition flag
	if (UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings())
	{
		Settings->bIsLoadingIntoGameplay = false;
	}
}

bool UMOGameInstance::IsLoadingOverlayVisible() const
{
	return LoadingOverlayWidget && LoadingOverlayWidget->IsOverlayVisible();
}
