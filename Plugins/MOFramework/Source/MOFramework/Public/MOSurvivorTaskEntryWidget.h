/**
 * =============================================================================
 * MOSurvivorTaskEntryWidget.h - Task List Entry Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Individual task entry widget for display in the task selection list (left
 * side of task menu). Shows task name, description, icon, and add button.
 * Clicking the entry fires OnTaskClicked delegate to add job to queue.
 *
 * DISPLAY ELEMENTS:
 * - TaskNameText: Job display name
 * - TaskDescriptionText: Brief description
 * - TaskIcon: Job category icon
 * - AddButton: Button to add job to queue
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] COMMON BUTTON: AddButton is UMOCommonButton. Bind click with
 *   OnClicked().AddUObject().
 *
 * [2024-02] CACHED DATA: CachedJobDef stores job definition. Call
 *   InitializeFromJobDefinition() to update.
 *
 * =============================================================================
 * RELATED FILES: MOSurvivorTaskMenu.h, MOSurvivorJobTypes.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOSurvivorJobTypes.h"
#include "MOSurvivorTaskEntryWidget.generated.h"

class UTextBlock;
class UMOCommonButton;
class UImage;

/**
 * Delegate fired when a task entry is clicked.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOTaskEntryClickedSignature, EMOSurvivorJobType, JobType);

/**
 * Widget representing a single task in the task selection list.
 * Shows task name, description, and allows adding to queue.
 */
UCLASS(Abstract)
class MOFRAMEWORK_API UMOSurvivorTaskEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Initialize this entry with a job definition.
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Survivor")
	void InitializeFromJobDefinition(const FMOSurvivorJobDefinitionRow& JobDef);

	/**
	 * Get the job type this entry represents.
	 */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Survivor")
	EMOSurvivorJobType GetJobType() const { return JobType; }

	/** Fired when this task entry is clicked (to add to queue). */
	UPROPERTY(BlueprintAssignable, Category = "MO|UI|Survivor")
	FMOTaskEntryClickedSignature OnTaskClicked;

protected:
	virtual void NativeConstruct() override;

	// Widget bindings
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Survivor")
	TObjectPtr<UTextBlock> TaskNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Survivor")
	TObjectPtr<UTextBlock> TaskDescriptionText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Survivor")
	TObjectPtr<UImage> TaskIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Survivor")
	TObjectPtr<UMOCommonButton> AddButton;

private:
	/** The job type this entry represents. */
	EMOSurvivorJobType JobType = EMOSurvivorJobType::None;

	/** Cached job definition data. */
	FMOSurvivorJobDefinitionRow CachedJobDef;

	void HandleAddClicked();
};
