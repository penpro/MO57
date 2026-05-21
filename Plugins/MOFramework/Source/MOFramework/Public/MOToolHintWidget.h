/**
 * =============================================================================
 * MOToolHintWidget.h - Tool/Action Hint Popup Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Small popup widget showing current tool or action hint. Fades in/out when
 * tool changes. Used for contextual UI feedback like "Dig", "Raise", etc.
 *
 * FEATURES:
 * - ShowHint/ShowHintWithIcon: Display text with optional icon
 * - Auto-fade after configurable duration
 * - Blueprint-overridable fade animations (PlayFadeIn/PlayFadeOut)
 * - Events for hint shown/hidden
 *
 * BLUEPRINT SETUP:
 * - Bind TextBlock named "HintText" (BindWidgetOptional)
 * - Override PlayFadeIn_Implementation/PlayFadeOut_Implementation for animations
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] TIMER HANDLE: HideTimerHandle must be cleared in NativeDestruct
 *   to prevent callbacks on destroyed widget.
 *
 * [2024-02] FADE ANIMATION: Default PlayFadeIn/PlayFadeOut are empty.
 *   Must implement in Blueprint for visual feedback.
 *
 * [2024-02] VISIBILITY STATE: bIsVisible tracks logical visibility, not
 *   widget visibility. Check both if needed.
 *
 * =============================================================================
 * RELATED FILES: MOTerraformingComponent.h (uses for mode hints)
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

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
	 * @param Duration Auto-hide behavior:
	 *                   > 0  : hide after this many seconds
	 *                   == 0 : hide after DefaultDuration (the legacy default)
	 *                   < 0  : PERSISTENT — never auto-hide. Caller is
	 *                          responsible for calling HideHint() later.
	 *                          Use for indicators of an ongoing state (e.g.
	 *                          current terraform tool while in terraform mode).
	 *                          Stored as the widget's "background" hint;
	 *                          transient hints (duration >= 0) display over it
	 *                          and the persistent text resurfaces when the
	 *                          transient one ends.
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

	/**
	 * Immediately hide the hint, clearing BOTH persistent and transient state.
	 * Use this when the persistent-state owner (e.g. terraform mode) actually
	 * ends — focus-hint code that just needs to dismiss a transient overlay
	 * should call EndTransientHint() instead.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|ToolHint")
	void HideHint();

	/**
	 * End any in-flight transient (auto-hide) hint. If a persistent hint was
	 * previously set via ShowHint(text, < 0), it re-appears. If not, the
	 * widget fully hides.
	 *
	 * This is the focus-hint code path: when the player aims at empty space,
	 * we want to drop the "Black Alder"-style transient overlay, but NOT
	 * wipe a persistent indicator (like the active terraform tool).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|ToolHint")
	void EndTransientHint();

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

	/**
	 * Background text to display while no transient is active. Set by
	 * ShowHint(text, < 0). Cleared by HideHint(). Survives transient
	 * overlays via the EndTransientHint / OnHideTimerExpired paths.
	 */
	FText PersistentText;
	bool bHasPersistent = false;

	void ScheduleHide(float Duration);
	void OnHideTimerExpired();
};
