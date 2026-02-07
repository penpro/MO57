#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOToolHintWidget.generated.h"

class UTextBlock;

/**
 * Small popup widget showing current tool or action hint.
 * Fades in/out when tool changes. Reusable for any tool hints.
 *
 * Usage:
 * - Call ShowHint("Dig") to display a tool hint
 * - Automatically fades out after duration
 * - Call ShowHint again to update and reset timer
 *
 * Setup in Blueprint:
 * - Bind a TextBlock named "HintText"
 * - Add fade animations if desired
 */
UCLASS()
class MOFRAMEWORK_API UMOToolHintWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================================
	// API
	// ============================================================================

	/**
	 * Show a tool hint message.
	 * @param HintText The text to display (e.g., "Dig", "Raise", "Place Wall")
	 * @param Duration How long to show before fading (0 = use default)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|ToolHint")
	void ShowHint(const FText& HintText, float Duration = 0.0f);

	/**
	 * Show a tool hint with icon.
	 * @param HintText The text to display
	 * @param Icon Optional icon texture
	 * @param Duration How long to show before fading (0 = use default)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|ToolHint")
	void ShowHintWithIcon(const FText& HintText, UTexture2D* Icon, float Duration = 0.0f);

	/** Immediately hide the hint. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|ToolHint")
	void HideHint();

	/** Check if hint is currently visible. */
	UFUNCTION(BlueprintPure, Category="MO|UI|ToolHint")
	bool IsHintVisible() const { return bIsVisible; }

	// ============================================================================
	// EVENTS
	// ============================================================================

	/** Called when a new hint is shown. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOToolHintShownSignature, const FText&, HintText);
	UPROPERTY(BlueprintAssignable, Category="MO|UI|ToolHint")
	FMOToolHintShownSignature OnHintShown;

	/** Called when hint is hidden. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOToolHintHiddenSignature);
	UPROPERTY(BlueprintAssignable, Category="MO|UI|ToolHint")
	FMOToolHintHiddenSignature OnHintHidden;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Called to perform fade in animation. Override in Blueprint for custom animation. */
	UFUNCTION(BlueprintNativeEvent, Category="MO|UI|ToolHint")
	void PlayFadeIn();

	/** Called to perform fade out animation. Override in Blueprint for custom animation. */
	UFUNCTION(BlueprintNativeEvent, Category="MO|UI|ToolHint")
	void PlayFadeOut();

	// ============================================================================
	// BINDINGS
	// ============================================================================

	/** Text block showing the hint. Bind in Blueprint. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> HintText;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Default duration to show hints (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|ToolHint")
	float DefaultDuration = 2.0f;

	/** Fade in duration (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|ToolHint")
	float FadeInDuration = 0.15f;

	/** Fade out duration (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|ToolHint")
	float FadeOutDuration = 0.3f;

private:
	bool bIsVisible = false;
	FTimerHandle HideTimerHandle;

	void ScheduleHide(float Duration);
	void OnHideTimerExpired();
};
