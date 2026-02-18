#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOIntroWidget.generated.h"

class UImage;
class UTextBlock;
class UMaterialInstanceDynamic;

/**
 * Widget for playing intro video on game launch.
 *
 * Features:
 * - Full-screen video playback
 * - "Press any key to skip" hint
 * - Broadcasts completion/skip events
 *
 * Note: Video playback is managed by the owning player controller.
 * This widget just displays the video texture and handles skip input.
 */
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
	bool bIsActive = false;

	/** Time elapsed since intro started. */
	float ElapsedTime = 0.0f;
};
