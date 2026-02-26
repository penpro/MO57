/**
 * =============================================================================
 * MOQuestTrackerEntry.h - HUD Quest Tracker Entry Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Widget representing a single tracked quest entry on the HUD. Shows quest
 * name, current objective description, and progress. Used by MOQuestHUDWidget.
 *
 * DISPLAY DATA:
 * FMOQuestTrackerDisplayData contains QuestName, CurrentObjectiveText,
 * CurrentProgress, RequiredProgress, ProgressPercent, and bIsComplete.
 *
 * ANIMATIONS:
 * - PlayUpdateAnimation(): Progress changed (BlueprintNativeEvent)
 * - PlayCompletionAnimation(): Quest completed (BlueprintNativeEvent)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] PROGRESS PERCENT: ProgressPercent is 0.0-1.0 for progress bars,
 *   calculated from CurrentProgress / RequiredProgress.
 *
 * [2024-02] BLUEPRINT ANIMATIONS: Override PlayUpdateAnimation_Implementation
 *   and PlayCompletionAnimation_Implementation in Blueprint for custom effects.
 *
 * =============================================================================
 * RELATED FILES: MOQuestHUDWidget.h, MOQuestTypes.h, MOQuestSubsystem.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOQuestTypes.h"
#include "MOQuestTrackerEntry.generated.h"

class UTextBlock;
class UProgressBar;

/**
 * Display data for a single tracked quest in the HUD.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOQuestTrackerDisplayData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|Quest|UI")
	FName QuestId;

	UPROPERTY(BlueprintReadOnly, Category="MO|Quest|UI")
	FText QuestName;

	UPROPERTY(BlueprintReadOnly, Category="MO|Quest|UI")
	FText CurrentObjectiveText;

	UPROPERTY(BlueprintReadOnly, Category="MO|Quest|UI")
	int32 CurrentProgress = 0;

	UPROPERTY(BlueprintReadOnly, Category="MO|Quest|UI")
	int32 RequiredProgress = 1;

	/** Progress as 0.0 - 1.0 for progress bars. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Quest|UI")
	float ProgressPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="MO|Quest|UI")
	bool bIsComplete = false;
};

/**
 * Widget representing a single tracked quest entry on the HUD.
 * Shows quest name, current objective, and progress.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOQuestTrackerEntry : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOQuestTrackerEntry(const FObjectInitializer& ObjectInitializer);

	/** Set up the entry with quest tracking data. */
	UFUNCTION(BlueprintCallable, Category="MO|Quest|UI")
	void SetupEntry(const FMOQuestTrackerDisplayData& InData);

	/** Update progress without rebuilding everything. */
	UFUNCTION(BlueprintCallable, Category="MO|Quest|UI")
	void UpdateProgress(int32 NewProgress, int32 RequiredProgress);

	/** Play update animation when progress changes. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="MO|Quest|UI")
	void PlayUpdateAnimation();

	/** Play completion animation when quest completes. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="MO|Quest|UI")
	void PlayCompletionAnimation();

	/** Get the quest ID this entry is tracking. */
	UFUNCTION(BlueprintPure, Category="MO|Quest|UI")
	FName GetQuestId() const { return CachedData.QuestId; }

protected:
	virtual void NativeConstruct() override;

	/** Update display from cached data. */
	UFUNCTION(BlueprintCallable, Category="MO|Quest|UI")
	void RefreshDisplay();

protected:
	/** Quest name text. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UTextBlock> QuestNameText;

	/** Current objective description. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UTextBlock> ObjectiveText;

	/** Progress text (e.g., "2/5"). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ProgressText;

	/** Visual progress bar. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar;

	/** Cached display data. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Quest|UI")
	FMOQuestTrackerDisplayData CachedData;
};
