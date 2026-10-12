/**
 * =============================================================================
 * MOSplashWidget.h - The startup splash cards (studio mark, "Made with Unreal Engine", ...)
 * =============================================================================
 *
 * PURPOSE:
 * Plays the cards configured in Project Settings > MO Splash Screens one after another (fade in, hold, fade out) before the intro video. They are widgets over the
 * LIVE menu world, not a blocking movie: the game thread keeps ticking, so the voxel terrain and the PCG vegetation behind them keep building (see
 * UMOWorldReadinessSubsystem). Every second a card is on screen is a second less the player waits behind the loading overlay.
 *
 * INPUT: any key or mouse click skips the whole sequence (same contract as UMOIntroWidget).
 *
 * WBP: /MOFramework/UI/MainMenu_UI/WBP_SplashSequence (Content/Python/ui_specs/kit_splash.py). Contract: LogoImage; optional CaptionText, CardRoot.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOActivatableWidget.h"
#include "MOSplashSettings.h"
#include "MOSplashWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOSplashFinishedSignature);

UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOSplashWidget : public UMOActivatableWidget
{
	GENERATED_BODY()

public:
	UMOSplashWidget(const FObjectInitializer& ObjectInitializer);

	/** Start the sequence. Cards whose logo cannot be loaded and that have no caption are skipped; with nothing to show OnSplashFinished fires at once. */
	UFUNCTION(BlueprintCallable, Category = "MO|Splash")
	void BeginSequence(const TArray<FMOSplashCard>& InCards, float InFadeSeconds);

	/** End the sequence now (a key press, or the dev verb). */
	UFUNCTION(BlueprintCallable, Category = "MO|Splash")
	void SkipSequence();

	UFUNCTION(BlueprintPure, Category = "MO|Splash")
	bool IsSequenceRunning() const { return bRunning; }

	/** Fired once when the last card has faded out or the sequence was skipped. The widget deactivates itself first. */
	UPROPERTY(BlueprintAssignable, Category = "MO|Splash")
	FMOSplashFinishedSignature OnSplashFinished;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** The card's mark. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> LogoImage;

	/** The card's caption (the studio name under the mark). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CaptionText;

	/** Optional: the widget whose render opacity carries the fade (mark + caption). Defaults to the whole widget's logo and caption. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CardRoot;

private:
	enum class EPhase : uint8 { Gap, Prepare, FadeIn, Hold, FadeOut };

	TArray<FMOSplashCard> Cards;
	int32 CardIndex = -1;
	EPhase Phase = EPhase::Gap;
	double PhaseElapsed = 0.0;
	float FadeSeconds = 0.7f;
	bool bRunning = false;

	/** Black between cards, so one mark never overlaps the next. */
	static constexpr float GapSeconds = 0.25f;

	void ShowCard(int32 Index);

	/** True when the mark can be drawn right now: loaded, its render resource created, no editor async compile pending (the placeholder shown meanwhile is a grey checker). */
	bool IsLogoReady(const UTexture2D* Texture) const;
	void SetCardOpacity(float Alpha);
	void Finish();
};
