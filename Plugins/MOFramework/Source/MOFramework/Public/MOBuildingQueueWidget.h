/**
 * =============================================================================
 * MOBuildingQueueWidget.h - Building Construction Progress Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Widget that displays current building construction progress and queue.
 * Mirrors UMOCraftingQueueWidget for crafting. Shows active build, progress
 * bar, time remaining, and queued entries with cancel options.
 *
 * BINDINGS:
 * - Binds to UMOBuildProgressComponent for state updates
 * - Uses UMOBuildingQueueEntryWidget for individual queue entries
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] TICK UPDATE: NativeTick updates progress at ProgressUpdateInterval
 *   (default 0.1s). RefreshQueue() rebuilds entire list - use sparingly.
 *
 * [2024-02] ENTRY WIDGET CLASS: QueueEntryWidgetClass must be set in Blueprint
 *   or entries won't spawn. Defaults to null.
 *
 * [2024-02] WEAK POINTER: ProgressComponent is TWeakObjectPtr. Check validity
 *   before use in all handlers.
 *
 * =============================================================================
 * RELATED FILES: MOBuildingQueueEntryWidget.h, MOBuildProgressComponent.h,
 *                MOBuildingUIController.h, MOCraftingQueueWidget.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOBuildingTypes.h"
#include "MOBuildingQueueWidget.generated.h"

class UMOBuildProgressComponent;
class UMOBuildingQueueEntryWidget;
class UScrollBox;
class UVerticalBox;
class UTextBlock;
class UProgressBar;
class UMOCommonButton;

/**
 * Widget that displays current building construction progress.
 * Mirrors UMOCraftingQueueWidget for crafting.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOBuildingQueueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOBuildingQueueWidget(const FObjectInitializer& ObjectInitializer);

	// --- Initialization ---

	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void InitializeQueue(UMOBuildProgressComponent* InProgressComponent);

	// --- Refresh ---

	/** Rebuild the queue display from current state. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void RefreshQueue();

	/** Update just the progress display (call frequently). */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void UpdateProgress();

	// --- Getters ---

	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	bool IsQueueEmpty() const;

	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	int32 GetQueueLength() const;

	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	float GetCurrentProgress() const;

	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	FText GetTimeRemainingText() const;

	// --- Configuration ---

	/** Widget class to use for queue entries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|UI")
	TSubclassOf<UMOBuildingQueueEntryWidget> QueueEntryWidgetClass;

	/** How often to update progress display (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|UI")
	float ProgressUpdateInterval = 0.1f;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Handle construction state changed event. */
	UFUNCTION()
	void HandleConstructionStateChanged(EMOBuildState NewState);

	/** Handle construction progress event. */
	UFUNCTION()
	void HandleConstructionProgress(float Progress);

	/** Handle construction completed event. */
	UFUNCTION()
	void HandleConstructionCompleted();

	/** Handle entry cancel request. */
	UFUNCTION()
	void HandleEntryCancelRequested(const FGuid& EntryId);

	/** Handle cancel all button. */
	UFUNCTION()
	void HandleCancelAllClicked();

	/** Blueprint event when queue is updated. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Building|UI")
	void OnQueueUpdated(int32 QueueLength);

	/** Blueprint event when progress updates. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Building|UI")
	void OnProgressUpdated(float Progress, const FText& TimeRemaining);

	// --- Widget Bindings (match MOCraftingQueueWidget) ---

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UScrollBox> QueueScrollBox;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> QueueContainer;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CurrentCraftNameText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> CurrentProgressBar;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ProgressText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TimeRemainingText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TotalTimeRemainingText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> CancelAllButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyQueueText;

private:
	UPROPERTY()
	TWeakObjectPtr<UMOBuildProgressComponent> ProgressComponent;

	UPROPERTY()
	TArray<TObjectPtr<UMOBuildingQueueEntryWidget>> EntryWidgets;

	float TimeSinceLastUpdate = 0.0f;
};
