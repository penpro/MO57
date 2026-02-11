#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOCraftingTypes.h"
#include "MOCraftingQueueWidget.generated.h"

class UMOCraftingQueueComponent;
class UMOCraftingQueueEntryWidget;
class UScrollBox;
class UVerticalBox;
class UTextBlock;
class UProgressBar;
class UMOCommonButton;

/**
 * Widget that displays the crafting queue and current craft progress.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOCraftingQueueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOCraftingQueueWidget(const FObjectInitializer& ObjectInitializer);

	// --- Initialization ---

	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void InitializeQueue(UMOCraftingQueueComponent* InQueueComponent);

	// --- Refresh ---

	/** Rebuild the queue display from current state. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void RefreshQueue();

	/** Update just the progress display (call frequently). */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void UpdateProgress();

	// --- Getters ---

	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	bool IsQueueEmpty() const;

	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	int32 GetQueueLength() const;

	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	float GetCurrentProgress() const;

	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	FText GetTimeRemainingText() const;

	// --- Configuration ---

	/** Widget class to use for queue entries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|UI")
	TSubclassOf<UMOCraftingQueueEntryWidget> QueueEntryWidgetClass;

	/** How often to update progress display (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|UI")
	float ProgressUpdateInterval = 0.1f;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Handle queue changed event. */
	UFUNCTION()
	void HandleQueueChanged();

	/** Handle craft progress event. */
	UFUNCTION()
	void HandleCraftProgress(const FGuid& EntryId, float Progress);

	/** Handle craft completed event. */
	UFUNCTION()
	void HandleCraftCompleted(const FGuid& EntryId, const FMOCraftResult& Result);

	/** Handle entry cancel request. */
	UFUNCTION()
	void HandleEntryCancelRequested(const FGuid& EntryId);

	/** Handle cancel all button. */
	UFUNCTION()
	void HandleCancelAllClicked();

	/** Blueprint event when queue is updated. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Crafting|UI")
	void OnQueueUpdated(int32 QueueLength);

	/** Blueprint event when progress updates. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Crafting|UI")
	void OnProgressUpdated(float Progress, const FText& TimeRemaining);

	// --- Widget Bindings ---

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
	TWeakObjectPtr<UMOCraftingQueueComponent> QueueComponent;

	UPROPERTY()
	TArray<TObjectPtr<UMOCraftingQueueEntryWidget>> EntryWidgets;

	float TimeSinceLastUpdate = 0.0f;
};
