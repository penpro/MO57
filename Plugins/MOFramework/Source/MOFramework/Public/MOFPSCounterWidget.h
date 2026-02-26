/**
 * =============================================================================
 * MOFPSCounterWidget.h - Debug FPS Counter Display
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Debug widget displaying current FPS and frame time. Color-coded for
 * performance status. Visibility controlled by game settings.
 *
 * DISPLAY:
 * - FPS text: "60 FPS" with color (green/yellow/red based on threshold)
 * - Frame time: Optional "16.6 ms" display
 *
 * BLUEPRINT SETUP:
 * 1. Create WBP_FPSCounter based on this class
 * 2. Add TextBlock named "FPSText" (required)
 * 3. Add TextBlock named "FrameTimeText" (optional)
 * 4. Add to HUD layer
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] SETTINGS BINDING: Visibility tied to UMOGameSettings::bShowFPSCounter.
 *   Call RefreshVisibility() if settings change at runtime.
 *
 * [2024-02] UPDATE INTERVAL: Lower values = more responsive but more string
 *   allocations. 0.25s is good balance for display.
 *
 * [2024-02] SMOOTHING: SmoothedFPS provides visual stability. Raw FPS may
 *   fluctuate frame-to-frame.
 *
 * =============================================================================
 * RELATED FILES: MOGameSettings.h, MOHUDWidget.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOFPSCounterWidget.generated.h"

class UTextBlock;

/**
 * Simple FPS counter widget that displays current framerate.
 * See file header for setup and pitfalls.
 */
UCLASS()
class MOFRAMEWORK_API UMOFPSCounterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOFPSCounterWidget(const FObjectInitializer& ObjectInitializer);

	/** Force update the FPS display. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|FPS")
	void UpdateFPSDisplay();

	/** Check settings and update visibility. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|FPS")
	void RefreshVisibility();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** Text block to display FPS value. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> FPSText;

	/** Optional text block for frame time in ms. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> FrameTimeText;

	/** How often to update the display (seconds). Lower = more responsive but more string allocations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|FPS", meta=(AllowPrivateAccess="true", ClampMin="0.05", ClampMax="1.0"))
	float UpdateInterval = 0.25f;

	/** Current smoothed FPS value for display. */
	float SmoothedFPS = 60.0f;

	/** Frame counter for FPS calculation. */
	int32 FrameCount = 0;

	/** Last time we calculated FPS (FPlatformTime::Seconds). */
	double LastFPSCalculationTime = 0.0;

	/** Color for good FPS (60+). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|FPS", meta=(AllowPrivateAccess="true"))
	FSlateColor GoodFPSColor = FSlateColor(FLinearColor(0.2f, 1.0f, 0.2f, 1.0f));

	/** Color for okay FPS (30-60). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|FPS", meta=(AllowPrivateAccess="true"))
	FSlateColor OkayFPSColor = FSlateColor(FLinearColor(1.0f, 1.0f, 0.2f, 1.0f));

	/** Color for bad FPS (<30). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|FPS", meta=(AllowPrivateAccess="true"))
	FSlateColor BadFPSColor = FSlateColor(FLinearColor(1.0f, 0.2f, 0.2f, 1.0f));
};
