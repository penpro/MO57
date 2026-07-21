/**
 * =============================================================================
 * MOCraftingQueueWidget.h - Crafting Queue Display Widget (Stage-3 compat adapter)
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * The crafting ADAPTER over the shared queue renderer (migration Stage 3).
 * UMOQueueRendererBase owns the row lifecycle, tick poll, header, empty state,
 * and cancel-intent routing; this class binds UMOCraftingQueueComponent events,
 * translates queue entries into neutral display rows, and executes validated
 * cancellation (CancelCraft/CancelAllCrafts — the refund policy lives HERE, in
 * the domain, not in shared presentation).
 *
 * COMPATIBILITY: class name, InitializeQueue signature, BlueprintCallable API
 * (RefreshQueue/UpdateProgress/getters), BlueprintImplementableEvents
 * (OnQueueUpdated/OnProgressUpdated), and the typed QueueEntryWidgetClass all
 * preserved — WBP_CraftingQueue keeps this parent; the widget bindings moved to
 * the base under their exact legacy names.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] ENTRY WIDGET CLASS: QueueEntryWidgetClass must be set in Blueprint
 *   or entries won't spawn (synced into the base's RowWidgetClass).
 *
 * [2026-07] EVENTS-FOR-STRUCTURE, POLL-FOR-PROGRESS: OnQueueChanged /
 *   OnCraftCompleted rebuild rows; OnCraftProgress is deliberately unused (the
 *   base's tick poll is smoother than the component's 0.5s throttle).
 *
 * [2026-07] QUEUED-ROW ETA: non-active rows estimate base Recipe->CraftTime *
 *   Count (tool-speed bonuses not applied — GetEffectiveCraftDuration is
 *   component-private). Legacy-parity carry-over, not a Stage-3 change.
 *
 * =============================================================================
 * RELATED FILES: MOQueueRendererBase.h, MOCraftingQueueEntryWidget.h,
 *                MOCraftingQueueComponent.h, MOCraftingMenu.h
 * LAST UPDATED: 2026-07-20 (Stage 3: reparented onto the shared queue renderer)
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOQueueRendererBase.h"
#include "MOCraftingTypes.h"
#include "MOCraftingQueueWidget.generated.h"

class UMOCraftingQueueComponent;
class UMOCraftingQueueEntryWidget;
struct FMORecipeDefinitionRow;

/**
 * Crafting queue display: thin domain adapter over UMOQueueRendererBase.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOCraftingQueueWidget : public UMOQueueRendererBase
{
	GENERATED_BODY()

public:
	UMOCraftingQueueWidget(const FObjectInitializer& ObjectInitializer);

	// --- Initialization (legacy API) ---

	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void InitializeQueue(UMOCraftingQueueComponent* InQueueComponent);

	// --- Refresh (legacy API, forwards to the shared renderer) ---

	/** Rebuild the queue display from current state. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void RefreshQueue();

	/** Update just the progress display (call frequently). */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void UpdateProgress();

	// --- Getters (legacy API) ---

	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	bool IsQueueEmpty() const;

	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	int32 GetQueueLength() const;

	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	float GetCurrentProgress() const;

	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	FText GetTimeRemainingText() const;

	// --- Configuration (legacy typed class, synced into base RowWidgetClass) ---

	/** Widget class to use for queue entries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|UI")
	TSubclassOf<UMOCraftingQueueEntryWidget> QueueEntryWidgetClass;

	// --- Pure adapter translation (headless-testable; MOFramework.UI.Queue.*) ---

	/**
	 * Translate one crafting queue entry into a neutral display row. Pure: the
	 * recipe row is pre-resolved by the caller, ActiveRemainingSeconds applies
	 * to the active row only (queued rows estimate CraftTime * Count).
	 */
	static FMOQueueDisplayRow BuildCraftingDisplayRow(
		const FMOCraftingQueueEntry& Entry,
		const FMORecipeDefinitionRow* Recipe,
		bool bIsActive,
		float ActiveRemainingSeconds);

	// --- Domain hooks (adapter seam) ---

	virtual bool HasQueueSource_Implementation() const override;
	virtual void BuildDisplayRows_Implementation(TArray<FMOQueueDisplayRow>& OutRows) const override;
	virtual void GetHeaderDisplay_Implementation(FMOQueueHeaderDisplay& OutHeader) const override;
	virtual bool GetActiveRowLiveProgress_Implementation(float& OutProgress, float& OutRemainingSeconds) const override;
	virtual void ExecuteCancelRow_Implementation(const FGuid& RowId) override;
	virtual void ExecuteCancelAll_Implementation() override;

protected:
	virtual void NativeDestruct() override;
	virtual void NotifyRowsRefreshed(int32 RowCount) override;
	virtual void NotifyProgressUpdated(float Progress, const FText& TimeRemaining) override;
	virtual void OnRowWidgetBound(UMOQueueRowWidgetBase* RowWidget, const FMOQueueDisplayRow& InRow) override;

	/** Handle queue changed event. */
	UFUNCTION()
	void HandleQueueChanged();

	/** Handle craft progress event (unused — tick poll wins; see pitfalls). */
	UFUNCTION()
	void HandleCraftProgress(const FGuid& EntryId, float Progress);

	/** Handle craft completed event (fires per repeat; rebuilds rows). */
	UFUNCTION()
	void HandleCraftCompleted(const FGuid& EntryId, const FMOCraftResult& Result);

	/** Blueprint event when queue is updated. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Crafting|UI")
	void OnQueueUpdated(int32 QueueLength);

	/** Blueprint event when progress updates. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Crafting|UI")
	void OnProgressUpdated(float Progress, const FText& TimeRemaining);

private:
	UPROPERTY()
	TWeakObjectPtr<UMOCraftingQueueComponent> QueueComponent;

	/** Copy the legacy typed entry class into the base's generic RowWidgetClass. */
	void SyncRowWidgetClass();
};
