#include "MOFPSCounterWidget.h"
#include "MOGameSettings.h"
#include "MOFramework.h"
#include "Components/TextBlock.h"
#include "HAL/PlatformTime.h"

UMOFPSCounterWidget::UMOFPSCounterWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOFPSCounterWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Initialize timing
	LastFPSCalculationTime = FPlatformTime::Seconds();
	FrameCount = 0;

	RefreshVisibility();
	UpdateFPSDisplay();

	UE_LOG(LogMOFramework, Log, TEXT("[MOFPSCounter] NativeConstruct - Visibility: %d"), static_cast<int32>(GetVisibility()));
}

void UMOFPSCounterWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Only update if visible (HitTestInvisible also means visible, just not interactable)
	if (GetVisibility() == ESlateVisibility::Collapsed || GetVisibility() == ESlateVisibility::Hidden)
	{
		return;
	}

	// Count every frame
	FrameCount++;

	// Calculate FPS at intervals using high-precision platform time
	const double CurrentTime = FPlatformTime::Seconds();
	const double ElapsedTime = CurrentTime - LastFPSCalculationTime;

	if (ElapsedTime >= UpdateInterval)
	{
		// Calculate actual FPS from frame count over elapsed time
		const float CurrentFPS = static_cast<float>(FrameCount / ElapsedTime);

		// Exponential smoothing for stable display (0.2 = responsive, 0.05 = very smooth)
		const float SmoothingFactor = 0.3f;
		SmoothedFPS = FMath::Lerp(SmoothedFPS, CurrentFPS, SmoothingFactor);

		UE_LOG(LogMOFramework, Verbose, TEXT("[MOFPSCounter] Frames: %d, Elapsed: %.3f, Current: %.1f, Smoothed: %.1f"),
			FrameCount, ElapsedTime, CurrentFPS, SmoothedFPS);

		// Reset for next interval
		FrameCount = 0;
		LastFPSCalculationTime = CurrentTime;

		UpdateFPSDisplay();
	}
}

void UMOFPSCounterWidget::RefreshVisibility()
{
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (Settings)
	{
		const bool bShouldShow = Settings->bShowFPSCounter;
		SetVisibility(bShouldShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UMOFPSCounterWidget::UpdateFPSDisplay()
{
	if (!FPSText)
	{
		return;
	}

	const int32 DisplayFPS = FMath::RoundToInt(SmoothedFPS);
	FPSText->SetText(FText::FromString(FString::Printf(TEXT("%d FPS"), DisplayFPS)));

	// Color based on performance
	if (DisplayFPS >= 60)
	{
		FPSText->SetColorAndOpacity(GoodFPSColor);
	}
	else if (DisplayFPS >= 30)
	{
		FPSText->SetColorAndOpacity(OkayFPSColor);
	}
	else
	{
		FPSText->SetColorAndOpacity(BadFPSColor);
	}

	// Optional frame time display
	if (FrameTimeText)
	{
		UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
		if (Settings && Settings->bShowFrameTime)
		{
			// Calculate frame time from FPS (ms = 1000 / FPS)
			const float FrameTimeMs = SmoothedFPS > 0.0f ? (1000.0f / SmoothedFPS) : 0.0f;
			FrameTimeText->SetText(FText::FromString(FString::Printf(TEXT("%.1f ms"), FrameTimeMs)));
			FrameTimeText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			FrameTimeText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

