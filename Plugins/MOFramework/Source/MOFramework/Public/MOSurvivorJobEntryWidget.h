#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOSurvivorJobTypes.h"
#include "MOSurvivorJobEntryWidget.generated.h"

class UTextBlock;
class UMOCommonButton;
class UImage;
class UProgressBar;

/**
 * Delegate fired when a job entry's cancel button is clicked.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOJobEntryCancelledSignature, FGuid, JobId);

/**
 * Widget representing a single job in the survivor's job queue.
 * Shows job name, progress, and allows cancellation.
 */
UCLASS(Abstract)
class MOFRAMEWORK_API UMOSurvivorJobEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Initialize this entry with a job entry.
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Survivor")
	void InitializeFromJobEntry(const FMOSurvivorJobEntry& JobEntry, const FMOSurvivorJobDefinitionRow& JobDef);

	/**
	 * Update the progress display.
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Survivor")
	void UpdateProgress(float Progress);

	/**
	 * Update the state display.
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Survivor")
	void UpdateState(EMOSurvivorJobState NewState);

	/**
	 * Get the job ID this entry represents.
	 */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Survivor")
	FGuid GetJobId() const { return JobId; }

	/** Fired when cancel is clicked for this job. */
	UPROPERTY(BlueprintAssignable, Category = "MO|UI|Survivor")
	FMOJobEntryCancelledSignature OnJobCancelled;

protected:
	virtual void NativeConstruct() override;

	// Widget bindings
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Survivor")
	TObjectPtr<UTextBlock> JobNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Survivor")
	TObjectPtr<UTextBlock> JobStateText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Survivor")
	TObjectPtr<UTextBlock> RepeatCountText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Survivor")
	TObjectPtr<UImage> JobIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Survivor")
	TObjectPtr<UProgressBar> ProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Survivor")
	TObjectPtr<UMOCommonButton> CancelButton;

private:
	/** The job ID this entry represents. */
	FGuid JobId;

	/** Cached job entry data. */
	FMOSurvivorJobEntry CachedJobEntry;

	/** Cached job definition. */
	FMOSurvivorJobDefinitionRow CachedJobDef;

	void HandleCancelClicked();

	/** Get display text for job state. */
	FText GetStateDisplayText(EMOSurvivorJobState State) const;
};
