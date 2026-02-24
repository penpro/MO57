#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MOSurvivorJobTypes.h"
#include "MOSurvivorTaskMenu.generated.h"

class UScrollBox;
class UTextBlock;
class UMOCommonButton;
class UMOSurvivorJobQueueComponent;
class UMOSurvivorTaskEntryWidget;
class UMOSurvivorJobEntryWidget;

/**
 * Delegate fired when the task menu requests to close.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOTaskMenuCloseRequestedSignature);

/**
 * Full task assignment panel for survivors.
 *
 * Layout:
 * - Left side: Available tasks organized by category
 * - Right side: Current job queue with progress
 * - Header: Survivor name and current status
 *
 * Similar to the crafting menu with its recipe list and queue display.
 */
UCLASS(Abstract)
class MOFRAMEWORK_API UMOSurvivorTaskMenu : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UMOSurvivorTaskMenu(const FObjectInitializer& ObjectInitializer);

	// ============================================================================
	// INITIALIZATION
	// ============================================================================

	/**
	 * Initialize the menu for a specific survivor.
	 * @param Survivor - The pawn to assign tasks to
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Survivor")
	void InitializeForSurvivor(APawn* Survivor);

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Fired when the menu requests to close. */
	UPROPERTY(BlueprintAssignable, Category = "MO|UI|Survivor")
	FMOTaskMenuCloseRequestedSignature OnRequestClose;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// ============================================================================
	// WIDGET BINDINGS
	// ============================================================================

	/** Survivor's display name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Survivor")
	TObjectPtr<UTextBlock> SurvivorNameText;

	/** Current command/task status. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Survivor")
	TObjectPtr<UTextBlock> CurrentJobText;

	/** Scroll box for available tasks (left side). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Survivor")
	TObjectPtr<UScrollBox> TaskListScrollBox;

	/** Scroll box for job queue (right side). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Survivor")
	TObjectPtr<UScrollBox> JobQueueScrollBox;

	/** Close button. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "MO|UI|Survivor")
	TObjectPtr<UMOCommonButton> CloseButton;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Widget class to use for task entries in the list. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|Survivor|Config")
	TSubclassOf<UMOSurvivorTaskEntryWidget> TaskEntryWidgetClass;

	/** Widget class to use for job entries in the queue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|Survivor|Config")
	TSubclassOf<UMOSurvivorJobEntryWidget> JobEntryWidgetClass;

private:
	/** The survivor pawn this menu is controlling. */
	TWeakObjectPtr<APawn> TargetSurvivor;

	/** Cached job queue component. */
	TWeakObjectPtr<UMOSurvivorJobQueueComponent> CachedJobQueue;

	// Event handlers
	void HandleCloseClicked();

	UFUNCTION()
	void OnQueueChanged();

	// UI updates
	void UpdateDisplayTexts();
	void PopulateTaskList();
	void RefreshJobQueue();

	/**
	 * Get the job queue for the target pawn.
	 */
	UMOSurvivorJobQueueComponent* GetJobQueue() const;

	/**
	 * Handle a task being clicked in the task list.
	 * Enqueues the job type to the survivor's job queue.
	 */
	UFUNCTION()
	void HandleTaskClicked(EMOSurvivorJobType JobType);

	/**
	 * Handle a job being cancelled in the queue.
	 */
	UFUNCTION()
	void HandleJobCancelled(FGuid JobId);
};
