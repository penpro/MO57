/**
 * =============================================================================
 * MOIntroWidget.h - Game Launch Intro Video Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Displays intro video on game launch (studio logo, title sequence). Handles
 * skip input and broadcasts completion events to transition to main menu.
 *
 * FEATURES:
 * - Full-screen video display via material
 * - "Press any key to skip" hint (delayed appearance)
 * - Keyboard/mouse input for skipping
 * - OnIntroCompleted/OnIntroSkipped events
 *
 * VIDEO SETUP:
 * 1. PlayerController creates MediaPlayer and MediaTexture
 * 2. Creates UMaterialInstanceDynamic with video texture
 * 3. Calls SetVideoMaterial() on this widget
 * 4. Widget displays via VideoImage
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] INPUT FOCUS: Widget must have keyboard focus to receive skip input.
 *   Set focus in NativeConstruct() after adding to viewport.
 *
 * [2024-02] SKIP DELAY: SkipHintDelay prevents accidental skip on fast key
 *   presses. Default 2 seconds before skip is allowed.
 *
 * [2024-02] MEDIA PLAYER LIFECYCLE: PlayerController owns MediaPlayer. Widget
 *   only holds material reference - don't try to control playback directly.
 *
 * =============================================================================
 * RELATED FILES: MOMainMenuPlayerController.h, MOGameInstance.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOIntroWidget.generated.h"

class UImage;
class UTextBlock;
class UMaterialInstanceDynamic;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOIntroCompletedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOIntroSkippedSignature);

UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOIntroWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOIntroWidget(const FObjectInitializer& ObjectInitializer);

	// ============================================================================
	// SETUP
	// ============================================================================

	/**
	 * Set the video material for display.
	 * Called by player controller after setting up media player.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Intro")
	void SetVideoMaterial(UMaterialInstanceDynamic* InVideoMaterial);

	/**
	 * Called when playback starts.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Intro")
	void OnPlaybackStarted();

	/**
	 * Skip the intro immediately.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Intro")
	void SkipIntro();

	/**
	 * Called when video playback completes naturally.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Intro")
	void OnVideoFinished();

	/**
	 * Check if intro is active.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Intro")
	bool IsIntroActive() const { return bIsActive; }

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Called when the intro video completes naturally. */
	UPROPERTY(BlueprintAssignable, Category="MO|Intro")
	FMOIntroCompletedSignature OnIntroCompleted;

	/** Called when the intro is skipped by user. */
	UPROPERTY(BlueprintAssignable, Category="MO|Intro")
	FMOIntroSkippedSignature OnIntroSkipped;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// ============================================================================
	// BIND WIDGETS
	// ============================================================================

	/** Image widget displaying the video. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> VideoImage;

	/** Text hint for skipping (e.g., "Press any key to skip"). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SkipHintText;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Delay before showing skip hint (seconds). */
	UPROPERTY(EditAnywhere, Category="MO|Intro")
	float SkipHintDelay = 2.0f;

private:
	/** Whether intro is currently active. */
	/**
	 * Skippable from the moment the widget is created. Setting this to false only
	 * when SkipIntro / OnVideoFinished completes — not relying on the media player's
	 * OnPlaybackStarted callback, which can fire late or not at all in packaged
	 * builds and leave the intro un-skippable.
	 */
	bool bIsActive = true;

	/** Time elapsed since intro started. */
	float ElapsedTime = 0.0f;
};
