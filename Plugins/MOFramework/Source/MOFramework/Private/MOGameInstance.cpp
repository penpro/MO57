#include "MOGameInstance.h"
#include "MOFramework.h"
#include "MOGameSettings.h"
#include "MOLoadingOverlay.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"

void UMOGameInstance::Init()
{
	Super::Init();

	// Reset intro playback flag on fresh game launch
	// This ensures the intro plays when the game starts, but not when
	// returning to main menu from gameplay (which sets bPlayIntro = false)
	if (UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings())
	{
		Settings->bPlayIntro = true;
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameInstance] Init - Reset bPlayIntro to true for fresh game launch"));
	}

	// Bind loading screen delegates
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UMOGameInstance::BeginLoadingScreen);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UMOGameInstance::EndLoadingScreen);

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameInstance] Initialized (LoadingOverlayClass=%s)"),
		LoadingOverlayClass ? *LoadingOverlayClass->GetName() : TEXT("NOT SET"));
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
	UE_LOG(LogMOFramework, Warning, TEXT("[MOGameInstance] ShowLoadingOverlay called (LoadingOverlayClass=%s)"),
		LoadingOverlayClass ? *LoadingOverlayClass->GetName() : TEXT("NULL"));

	if (!LoadingOverlayClass)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOGameInstance] ShowLoadingOverlay: LoadingOverlayClass not set in BP_MOGameInstance!"));
		return;
	}

	// Remove any existing overlay
	if (LoadingOverlayWidget)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameInstance] Removing existing overlay widget"));
		LoadingOverlayWidget->RemoveFromParent();
		LoadingOverlayWidget = nullptr;
	}

	// Create new overlay
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	UE_LOG(LogMOFramework, Warning, TEXT("[MOGameInstance] GetPlayerController returned: %s"),
		PC ? *PC->GetName() : TEXT("NULL"));

	if (PC)
	{
		LoadingOverlayWidget = CreateWidget<UMOLoadingOverlay>(PC, LoadingOverlayClass);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGameInstance] CreateWidget returned: %s"),
			LoadingOverlayWidget ? *LoadingOverlayWidget->GetName() : TEXT("NULL"));

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

			UE_LOG(LogMOFramework, Warning, TEXT("[MOGameInstance] Loading overlay SHOWN successfully"));
		}
		else
		{
			UE_LOG(LogMOFramework, Error, TEXT("[MOGameInstance] CreateWidget failed to create overlay!"));
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOGameInstance] ShowLoadingOverlay: No player controller available"));
	}
}

void UMOGameInstance::BeginLoadingScreen(const FString& MapName)
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOGameInstance] BeginLoadingScreen for map: %s"), *MapName);

	// Skip loading screen for initial launch to intro
	if (!ShouldShowLoadingScreen(MapName))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameInstance] Skipping loading screen for: %s"), *MapName);
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
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGameInstance] Will show loading overlay after level loads (manual-dismiss mode)"));
	}
	else
	{
		bWaitingForManualDismiss = false;
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameInstance] Non-gameplay transition, no loading overlay"));
	}
}

void UMOGameInstance::EndLoadingScreen(UWorld* InLoadedWorld)
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOGameInstance] EndLoadingScreen for world: %s (WaitingForManualDismiss=%s)"),
		InLoadedWorld ? *InLoadedWorld->GetName() : TEXT("None"),
		bWaitingForManualDismiss ? TEXT("true") : TEXT("false"));

	if (bWaitingForManualDismiss)
	{
		// Create the overlay now in the new level's context
		// (creating it in BeginLoadingScreen would destroy it with the old level)
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGameInstance] EndLoadingScreen: Calling ShowLoadingOverlay for manual-dismiss mode"));
		ShowLoadingOverlay();
	}
	else if (LoadingOverlayWidget)
	{
		// Not waiting for manual dismiss, hide the overlay now
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameInstance] EndLoadingScreen: Fading out overlay (not manual-dismiss)"));
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
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameInstance] ShouldShowLoadingScreen: SKIP (intro + LoadingLevel)"));
		return false;  // Just stay black until intro video starts
	}

	return true;
}

void UMOGameInstance::DismissLoadingScreen()
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOGameInstance] DismissLoadingScreen called (WaitingForManualDismiss=%s, HasWidget=%s)"),
		bWaitingForManualDismiss ? TEXT("true") : TEXT("false"),
		LoadingOverlayWidget ? TEXT("true") : TEXT("false"));

	if (!bWaitingForManualDismiss)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameInstance] Not waiting for manual dismiss, skipping"));
		return;
	}

	if (LoadingOverlayWidget)
	{
		LoadingOverlayWidget->FadeOutAndRemove();
		// Don't null the pointer - the widget will remove itself after fade
	}

	bWaitingForManualDismiss = false;

	// Clear the gameplay transition flag
	if (UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings())
	{
		Settings->bIsLoadingIntoGameplay = false;
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOGameInstance] Loading screen dismissed (fading out)"));
}

bool UMOGameInstance::IsLoadingOverlayVisible() const
{
	return LoadingOverlayWidget && LoadingOverlayWidget->IsOverlayVisible();
}
