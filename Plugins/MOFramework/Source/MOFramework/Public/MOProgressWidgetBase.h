/**
 * =============================================================================
 * MOProgressWidgetBase.h - Generic Progress Widget Base
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Abstract base class for progress indicator widgets. Shows progress bar,
 * label, time remaining, and optional cancel button. Use for harvest
 * operations, crafting progress, inspection, loading, etc.
 *
 * SUBCLASSES:
 * - UMOHarvestProgressWidget - Harvest/gathering operations
 * - UMOInspectionProgressWidget - Item inspection operations
 *
 * WIDGET BINDINGS (in Blueprint):
 * - ProgressBar (UProgressBar, required) - Visual progress
 * - ActionNameText (UTextBlock, required) - Action/item name
 * - TimeRemainingText (UTextBlock, required) - Time display
 * - InstructionText (UTextBlock, optional) - Instruction hint
 * - CancelButton (UCommonButtonBase, optional) - Cancel action
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-03] WALL-CLOCK TIME: Uses FPlatformTime::Seconds() for accurate
 *   progress even if frame rate drops or game is minimized.
 *
 * [2026-03] DELEGATE CLEANUP: OnCompleted/OnCancelled bindings persist.
 *   Clear bindings after use if reusing widget instance.
 *
 * =============================================================================
 * RELATED FILES: MOHarvestProgressWidget.h, MOInspectionProgressWidget.h
 * LAST UPDATED: 2026-03-29
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MOProgressWidgetBase.generated.h"

class UProgressBar;
class UTextBlock;
class UCommonButtonBase;

/**
 * Delegate fired when progress operation completes.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOProgressCompletedDelegate, bool, bSuccess);

/**
 * Delegate fired when progress operation is cancelled.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOProgressCancelledDelegate);

/**
 * Base class for progress indicator widgets.
 * Uses wall-clock time for accurate progress tracking.
 * See file header for usage and pitfalls.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOProgressWidgetBase : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UMOProgressWidgetBase(const FObjectInitializer& ObjectInitializer);

	// ============================================================================
	// PROGRESS CONTROL
	// ============================================================================

	/**
	 * Start a timed progress operation.
	 * @param InActionName Display name for the action
	 * @param Duration Total duration in seconds
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Progress")
	virtual void StartProgress(const FText& InActionName, float Duration);

	/**
	 * Cancel the progress operation.
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Progress")
	virtual void CancelProgress();

	/**
	 * Manually set progress value (for non-timed operations).
	 * @param Progress Progress value (0.0 to 1.0)
	 * @param TimeRemaining Optional time remaining display
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Progress")
	virtual void SetProgress(float Progress, float TimeRemaining = -1.0f);

	/**
	 * Complete the progress immediately.
	 * @param bSuccess Whether the operation succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Progress")
	virtual void CompleteProgress(bool bSuccess = true);

	// ============================================================================
	// STATE QUERIES
	// ============================================================================

	/** Check if progress is currently active. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Progress")
	bool IsProgressActive() const { return bIsProgressActive; }

	/** Get current progress (0.0 to 1.0). */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Progress")
	float GetCurrentProgress() const { return CurrentProgress; }

	/** Get elapsed time in seconds. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Progress")
	float GetElapsedTime() const;

	/** Get remaining time in seconds. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Progress")
	float GetTimeRemaining() const;

	/** Get total duration in seconds. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Progress")
	float GetTotalDuration() const { return TotalDuration; }

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Broadcast when progress completes. */
	UPROPERTY(BlueprintAssignable, Category = "MO|UI|Progress")
	FMOProgressCompletedDelegate OnCompleted;

	/** Broadcast when progress is cancelled. */
	UPROPERTY(BlueprintAssignable, Category = "MO|UI|Progress")
	FMOProgressCancelledDelegate OnCancelled;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/**
	 * Called to update display visuals.
	 * Override in subclasses for custom display.
	 * @param Progress Current progress (0.0 to 1.0)
	 * @param TimeRemaining Seconds remaining
	 * @param InActionName Display name of action
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "MO|UI|Progress")
	void UpdateDisplay(float Progress, float TimeRemaining, const FText& InActionName);

	/**
	 * Called when progress completes successfully.
	 * Override in subclasses to add domain-specific completion logic.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "MO|UI|Progress")
	void OnProgressSuccess();

	/** Handle cancel button click. */
	UFUNCTION()
	void HandleCancelClicked();

	// ============================================================================
	// WIDGET BINDINGS
	// ============================================================================

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "MO|UI|Progress")
	TObjectPtr<UProgressBar> ProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "MO|UI|Progress")
	TObjectPtr<UTextBlock> ActionNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "MO|UI|Progress")
	TObjectPtr<UTextBlock> TimeRemainingText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Progress")
	TObjectPtr<UTextBlock> InstructionText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Progress")
	TObjectPtr<UCommonButtonBase> CancelButton;

	// ============================================================================
	// PROTECTED STATE (accessible to subclasses)
	// ============================================================================

	/** Display name of the current action. */
	UPROPERTY(BlueprintReadOnly, Category = "MO|UI|Progress")
	FText ActionName;

	/** Total duration in seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "MO|UI|Progress")
	float TotalDuration = 0.0f;

	/** Current progress (0.0 to 1.0). */
	UPROPERTY(BlueprintReadOnly, Category = "MO|UI|Progress")
	float CurrentProgress = 0.0f;

	/** Whether progress is currently active. */
	UPROPERTY(BlueprintReadOnly, Category = "MO|UI|Progress")
	bool bIsProgressActive = false;

	/** Wall-clock time when progress started (for real-time tracking). */
	double ProgressStartTime = 0.0;

	/** Whether this is a timed progress (vs manual SetProgress calls). */
	bool bIsTimedProgress = false;
};
