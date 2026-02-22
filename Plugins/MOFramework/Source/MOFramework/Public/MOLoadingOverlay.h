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
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Loading")
	void FadeOutAndRemove();

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

	/** Duration of the fade-out animation in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Loading")
	float FadeOutDuration = 0.5f;

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
