/**
 * =============================================================================
 * MOLoadingOverlay.h - Loading Screen Overlay Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Full-screen loading overlay widget. Shows during level transitions and
 * fades out when gameplay is ready. Used by MOGameInstance during map loads.
 *
 * FEATURES:
 * - Full-screen black background
 * - Optional loading text
 * - Configurable fade-out animation
 * - Auto-removes from viewport when fade completes
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] TICK-BASED FADE: Fade animation uses NativeTick, not UMG animation.
 *   Requires SetVisibility(Visible) and tick enabled during fade.
 *
 * [2024-02] AUTO-REMOVE: FadeOutAndRemove() removes widget from viewport when
 *   opacity reaches 0. Don't cache pointer after calling this.
 *
 * [2024-02] Z-ORDER: Should be added at very high Z-order (9999) to appear
 *   above all other widgets during loading.
 *
 * =============================================================================
 * RELATED FILES: MOGameInstance.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOLoadingOverlay.generated.h"

class UImage;
class UTextBlock;

/**
 * Full-screen loading overlay widget.
 * Shows during level transitions and fades out when gameplay is ready.
 */
UCLASS()
class MOFRAMEWORK_API UMOLoadingOverlay : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOLoadingOverlay(const FObjectInitializer& ObjectInitializer);

	/**
	 * Show the overlay immediately at full opacity.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Loading")
	void ShowOverlay();

	/**
	 * Begin fading out the overlay. Removes from viewport when complete.
	 * @param OverrideDuration  If > 0, uses this duration instead of the
	 *                          widget's configured FadeOutDuration. Callers can
	 *                          pin the fade length without depending on the
	 *                          WBP default.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Loading")
	void FadeOutAndRemove(float OverrideDuration = -1.0f);

	/**
	 * Set the loading text displayed on the overlay.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Loading")
	void SetLoadingText(const FText& InText);

	/** Whether the overlay is currently visible (not fading out). */
	UFUNCTION(BlueprintPure, Category="MO|Loading")
	bool IsOverlayVisible() const { return bIsVisible && !bIsFadingOut; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ============================================================================
	// BIND WIDGETS
	// ============================================================================

	/** The full-screen background image (set to black). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UImage> BackgroundImage;

	/** Optional loading text. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LoadingText;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/**
	 * Duration of the fade-out animation in seconds. Used by FadeOutAndRemove().
	 * Default 1.0s feels like a deliberate hand-off from "loading" to "game",
	 * giving the world a moment to settle visually before player input matters.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Loading")
	float FadeOutDuration = 1.0f;

private:
	/** Current opacity (1.0 = fully visible, 0.0 = invisible). */
	float CurrentOpacity = 1.0f;

	/** Whether we're currently fading out. */
	bool bIsFadingOut = false;

	/** Whether the overlay is logically visible. */
	bool bIsVisible = false;

	/** Update the visual opacity of the overlay. */
	void UpdateOpacity(float NewOpacity);
};
