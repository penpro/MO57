/**
 * =============================================================================
 * MOQueueRendererBase.h - Shared queue renderer (UI migration Stage 3)
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * The one queue container behind crafting, building, and (later) survivor job
 * presentation. Owns the full row-widget lifecycle (create/bind/teardown),
 * the 0.1s progress poll, the header block, the empty state, and cancel-intent
 * routing. Domain subclasses are ADAPTERS: they bind their source component's
 * events, translate entries into FMOQueueDisplayRow values, and execute
 * validated cancellation. The shared layer performs NO gameplay mutation,
 * loads no DataTables, and searches no actors (migration-plan boundary).
 *
 * DOMAIN HOOKS (BlueprintNativeEvent, Stage-2 convention):
 *   HasQueueSource            - is a source bound? (gates all rendering)
 *   BuildDisplayRows          - translate source entries -> neutral rows
 *   GetHeaderDisplay          - header title/progress/remaining (default: row 0)
 *   GetActiveRowLiveProgress  - live tick values for row 0 (default: none)
 *   ExecuteCancelRow/All      - validated cancellation (default: no-op)
 *
 * PROGRESS IS PULL-BASED: sources do not reliably push progress (see
 * MOQueueDisplayTypes.h pitfalls). NativeTick polls every ProgressUpdateInterval.
 *
 * COMPATIBILITY:
 * UMOCraftingQueueWidget / UMOBuildingQueueWidget are thin subclasses; their
 * WBPs keep their parents. Every BindWidgetOptional name below (including the
 * crafting-flavored CurrentCraftNameText, which the building WBP deliberately
 * reused) moved UP from the legacy classes verbatim so existing bindings and
 * WBP-set defaults survive via name-based property serialization.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] ROW CLASS: RowWidgetClass must be set (compat subclasses copy their
 *   legacy typed QueueEntryWidgetClass into it) or no rows spawn.
 *
 * [2026-07] REBUILD SEMANTICS: RefreshRows destroys and recreates every row
 *   widget (legacy parity, keeps bindings trivially idempotent). If row reuse/
 *   diffing is ever added, the per-fresh-widget OnCancelIntent binding must
 *   become idempotent first (F18).
 *
 * [2026-07] NO PARALLEL STATE: subclasses must not keep their own row arrays,
 *   tick accumulators, or entry-widget lists (the F17 double-state trap the
 *   Stage-2 list consolidation had to unwind).
 *
 * =============================================================================
 * RELATED FILES: MOQueueDisplayTypes.h, MOQueueRowWidgetBase.h,
 *                MOCraftingQueueWidget.h, MOBuildingQueueWidget.h,
 *                MOScrollListBase.h (the Stage-2 shared-base precedent)
 * LAST UPDATED: 2026-07-20 (Stage 3 introduction)
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOQueueDisplayTypes.h"
#include "MOQueueRendererBase.generated.h"

class UMOQueueRowWidgetBase;
class UScrollBox;
class UVerticalBox;
class UTextBlock;
class UProgressBar;
class UMOCommonButton;

/** Observer delegates for cancel intents (the renderer ALSO routes intents to
 *  its ExecuteCancel* domain hooks — these exist so controllers/BP can watch). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOQueueCancelRowIntentSignature, const FGuid&, RowId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOQueueCancelAllIntentSignature);

/**
 * Shared queue renderer: rows + header + empty state + cancel-intent routing.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOQueueRendererBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOQueueRendererBase(const FObjectInitializer& ObjectInitializer);

	// --- Shared API ---

	/** Rebuild all rows from the domain adapter (destroy + recreate; legacy parity). */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Queue")
	void RefreshRows();

	/** Poll the domain for header + active-row progress (tick path). */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Queue")
	void UpdateProgressDisplay();

	/** Programmatic cancel-all intent — same path as the CancelAll button. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Queue")
	void RequestCancelAll();

	UFUNCTION(BlueprintPure, Category="MO|UI|Queue")
	int32 GetRowCount() const { return RowWidgets.Num(); }

	/** The row widgets in display order (row 0 = active). For automation/BP. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Queue")
	UMOQueueRowWidgetBase* GetRowWidgetAt(int32 Index) const;

	UFUNCTION(BlueprintPure, Category="MO|UI|Queue")
	bool IsShowingEmptyState() const;

	// --- Intent observers ---

	UPROPERTY(BlueprintAssignable, Category="MO|UI|Queue")
	FMOQueueCancelRowIntentSignature OnCancelRowIntent;

	UPROPERTY(BlueprintAssignable, Category="MO|UI|Queue")
	FMOQueueCancelAllIntentSignature OnCancelAllIntent;

	// --- Configuration ---

	/** Row widget class. Compat subclasses copy their legacy typed
	 *  QueueEntryWidgetClass into this before population. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Queue")
	TSubclassOf<UMOQueueRowWidgetBase> RowWidgetClass;

	/** How often the progress poll runs (seconds). Name/default preserved from
	 *  the legacy queue widgets so WBP overrides survive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Queue")
	float ProgressUpdateInterval = 0.1f;

	// --- Domain hooks (adapter seam) ---

	/** Is a queue source currently bound? Gates all rendering (an unbound
	 *  renderer leaves its visuals untouched, matching legacy early-returns). */
	UFUNCTION(BlueprintNativeEvent, Category="MO|UI|Queue")
	bool HasQueueSource() const;
	virtual bool HasQueueSource_Implementation() const { return false; }

	/** Translate the source's entries into display rows (row 0 = active). */
	UFUNCTION(BlueprintNativeEvent, Category="MO|UI|Queue")
	void BuildDisplayRows(TArray<FMOQueueDisplayRow>& OutRows) const;
	virtual void BuildDisplayRows_Implementation(TArray<FMOQueueDisplayRow>& OutRows) const {}

	/** Header block values. Default derives from the current rows (row 0). */
	UFUNCTION(BlueprintNativeEvent, Category="MO|UI|Queue")
	void GetHeaderDisplay(FMOQueueHeaderDisplay& OutHeader) const;
	virtual void GetHeaderDisplay_Implementation(FMOQueueHeaderDisplay& OutHeader) const;

	/** Fresh live progress for row 0 (poll path). Return false for "no live
	 *  row-0 update" (header still refreshes). */
	UFUNCTION(BlueprintNativeEvent, Category="MO|UI|Queue")
	bool GetActiveRowLiveProgress(float& OutProgress, float& OutRemainingSeconds) const;
	virtual bool GetActiveRowLiveProgress_Implementation(float& OutProgress, float& OutRemainingSeconds) const { return false; }

	/** Execute a validated cancel for one row. The DOMAIN owns validation,
	 *  authority handling, and refund policy — never the shared layer. */
	UFUNCTION(BlueprintNativeEvent, Category="MO|UI|Queue")
	void ExecuteCancelRow(const FGuid& RowId);
	virtual void ExecuteCancelRow_Implementation(const FGuid& RowId) {}

	/** Execute a validated cancel-all. */
	UFUNCTION(BlueprintNativeEvent, Category="MO|UI|Queue")
	void ExecuteCancelAll();
	virtual void ExecuteCancelAll_Implementation() {}

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Subclass hooks so legacy BlueprintImplementableEvents keep firing. */
	virtual void NotifyRowsRefreshed(int32 RowCount) {}
	virtual void NotifyProgressUpdated(float Progress, const FText& TimeRemaining) {}

	/** Rows as of the last RefreshRows — for domain header overrides. */
	const TArray<FMOQueueDisplayRow>& GetLastBuiltRows() const { return LastBuiltRows; }

	/** Called after a fresh row widget is bound (SetRow done) and before its
	 *  visuals notification — compat subclasses synchronize legacy display
	 *  structs here. */
	virtual void OnRowWidgetBound(UMOQueueRowWidgetBase* RowWidget, const FMOQueueDisplayRow& InRow) {}

	/** Row cancel-intent handler: broadcasts the observer delegate, then routes
	 *  to the domain hook. */
	UFUNCTION()
	void HandleRowCancelIntent(const FGuid& RowId);

	/** CancelAll button handler. */
	void HandleCancelAllClicked();

	/** Clear the header block widgets (empty queue / unbound source). */
	void ClearHeaderDisplay();

	/** Resolve the row container (scrollbox preferred, legacy parity). */
	virtual class UPanelWidget* GetRowContainer() const;

	// --- Widget bindings (legacy names shared by both queue WBP families;
	//     CurrentCraftNameText is crafting-flavored because the building WBP
	//     deliberately reused the crafting binding names) ---

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
	/** Base-owned row widgets. Subclasses must not keep a parallel list (F17). */
	UPROPERTY()
	TArray<TObjectPtr<UMOQueueRowWidgetBase>> RowWidgets;

	/** Rows as of the last RefreshRows (source for the default header). */
	UPROPERTY()
	TArray<FMOQueueDisplayRow> LastBuiltRows;

	float TimeSinceLastUpdate = 0.0f;
};
