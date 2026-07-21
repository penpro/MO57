/**
 * =============================================================================
 * MOBuildingQueueWidget.h - Building Construction Progress (Stage-3 compat adapter)
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * The building ADAPTER over the shared queue renderer (migration Stage 3).
 * Translates ONE UMOBuildProgressComponent (a single construction: at most one
 * row, Constructing or Paused) into the neutral display-row contract, and
 * executes validated cancellation. WBP_BuildingQueue keeps this parent; the
 * widget bindings moved to the base under their exact (crafting-flavored,
 * deliberately shared) legacy names.
 *
 * LIVENESS NOTE (Stage-3 audit): this widget has NO live consumer — only the
 * deprecated UMOBuildWidget ever calls InitializeQueue; the live building
 * progress surface is UMOGhostContextMenu. It is kept behavior-correct behind
 * the compat contract; wiring the shared renderer into the live building UI is
 * an explicit separate decision (see Docs/agent/ui/SESSION_STATE.md).
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-07] STABLE ROW ID: the legacy widget minted FGuid::NewGuid() per
 *   refresh, so a cancel intent's id identified nothing. The adapter now keys
 *   the row on the buildable's identity GUID (fallback: one GUID minted per
 *   bound component), and ExecuteCancelRow verifies the id before cancelling.
 *
 * [2026-07] CANCEL SEMANTICS FORK (design call, NOT unified here): this path
 *   cancels via CancelConstruction(true) = full refund as world drops, ghost
 *   survives. The live ghost-menu path uses a skill-based partial refund to
 *   inventory + ghost destruction. Do not silently reconcile.
 *
 * =============================================================================
 * RELATED FILES: MOQueueRendererBase.h, MOBuildingQueueEntryWidget.h,
 *                MOBuildProgressComponent.h, MOGhostContextMenu.h
 * LAST UPDATED: 2026-07-20 (Stage 3: reparented onto the shared queue renderer)
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOQueueRendererBase.h"
#include "MOBuildingTypes.h"
#include "MOBuildingQueueWidget.generated.h"

class UMOBuildProgressComponent;
class UMOBuildingQueueEntryWidget;

/**
 * Building construction display: thin domain adapter over UMOQueueRendererBase.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOBuildingQueueWidget : public UMOQueueRendererBase
{
	GENERATED_BODY()

public:
	UMOBuildingQueueWidget(const FObjectInitializer& ObjectInitializer);

	// --- Initialization (legacy API) ---

	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void InitializeQueue(UMOBuildProgressComponent* InProgressComponent);

	// --- Refresh (legacy API, forwards to the shared renderer) ---

	/** Rebuild the queue display from current state. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void RefreshQueue();

	/** Update just the progress display (call frequently). */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void UpdateProgress();

	// --- Getters (legacy API) ---

	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	bool IsQueueEmpty() const;

	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	int32 GetQueueLength() const;

	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	float GetCurrentProgress() const;

	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	FText GetTimeRemainingText() const;

	// --- Configuration (legacy typed class, synced into base RowWidgetClass) ---

	/** Widget class to use for queue entries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|UI")
	TSubclassOf<UMOBuildingQueueEntryWidget> QueueEntryWidgetClass;

	// --- Pure adapter translation (headless-testable; MOFramework.UI.Queue.*) ---

	/** Neutral row state for a build state (Constructing->Active, Paused->Paused;
	 *  Ghost/Complete produce no row at all). */
	static EMOQueueRowState RowStateForBuildState(EMOBuildState BuildState);

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

	/** Handle construction state changed event. */
	UFUNCTION()
	void HandleConstructionStateChanged(EMOBuildState NewState);

	/** Handle construction progress event (unused — tick poll wins). */
	UFUNCTION()
	void HandleConstructionProgress(float Progress);

	/** Handle construction completed event. */
	UFUNCTION()
	void HandleConstructionCompleted();

	/** Blueprint event when queue is updated. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Building|UI")
	void OnQueueUpdated(int32 QueueLength);

	/** Blueprint event when progress updates. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Building|UI")
	void OnProgressUpdated(float Progress, const FText& TimeRemaining);

private:
	UPROPERTY()
	TWeakObjectPtr<UMOBuildProgressComponent> ProgressComponent;

	/** Stable row id for the bound construction (identity GUID or minted once
	 *  per bound component — never per refresh; see pitfalls). */
	FGuid StableRowId;

	/** Does the component currently present a row (Constructing or Paused)? */
	bool HasActiveConstruction() const;

	/** Copy the legacy typed entry class into the base's generic RowWidgetClass. */
	void SyncRowWidgetClass();
};
