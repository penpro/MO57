#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOFPSCounterWidget.generated.h"

class UTextBlock;

/**
 * Simple FPS counter widget that displays current framerate.
 * Visibility is controlled by UMOGameSettings::bShowFPSCounter.
 *
 * Widget Setup:
 * 1. Create WBP_FPSCounter based on this class
 * 2. Add a TextBlock named "FPSText"
 * 3. Mark as "Is Variable"
 * 4. Add to your HUD or Game layer
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
