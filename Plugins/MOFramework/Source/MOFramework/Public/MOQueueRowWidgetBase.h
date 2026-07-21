/**
 * =============================================================================
 * MOQueueRowWidgetBase.h - Shared queue row widget (UI migration Stage 3)
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * The one row widget behind every queue presentation. Renders a domain-neutral
 * FMOQueueDisplayRow (title, icon, count, progress, remaining, state color) and
 * emits a CANCEL INTENT carrying the row id. It never executes cancellation —
 * the renderer routes the intent to its domain hook (migration Stage 3 boundary).
 *
 * COMPATIBILITY:
 * UMOCraftingQueueEntryWidget and UMOBuildingQueueEntryWidget are thin
 * subclasses. Their WBPs (WBP_CraftQueueEntry / WBP_BuildQueueEntry) keep their
 * parents and bindings: every BindWidgetOptional name below (RecipeNameText,
 * RecipeIcon, CountText, ProgressBar, TimeRemainingText, CancelButton,
 * CancelButtonSimple) and the color properties moved UP from the legacy classes
 * verbatim, so name-based property serialization keeps WBP overrides intact.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] DUAL CANCEL BUTTONS: CancelButton (UMOCommonButton) is preferred
 *   over CancelButtonSimple (UButton). Bind only one in Blueprint.
 *
 * [2024-02->2026-07] ONE INTENT PER CLICK (F18): button bindings are idempotent
 *   (RemoveAll/RemoveDynamic before add in NativeConstruct) and torn down in
 *   NativeDestruct. Regression contract: repeated construct/destruct then one
 *   click yields exactly one cancel intent.
 *
 * [2026-07] SYNC ICON LOAD: icons load via LoadSynchronous with a Hidden
 *   fallback — carried over from the legacy entry widgets.
 *
 * =============================================================================
 * RELATED FILES: MOQueueDisplayTypes.h, MOQueueRendererBase.h,
 *                MOCraftingQueueEntryWidget.h, MOBuildingQueueEntryWidget.h
 * LAST UPDATED: 2026-07-20 (Stage 3 introduction)
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOQueueDisplayTypes.h"
#include "MOQueueRowWidgetBase.generated.h"

class UTextBlock;
class UProgressBar;
class UImage;
class UButton;
class UMOCommonButton;

/** Cancel intent from one queue row: "the user asked to cancel operation RowId". */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOQueueRowCancelIntentSignature, const FGuid&, RowId);

/**
 * Shared queue row: renders one FMOQueueDisplayRow and emits cancel intents.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOQueueRowWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOQueueRowWidgetBase(const FObjectInitializer& ObjectInitializer);

	/** Bind a display row to this widget and render it (no BP notify — the
	 *  caller decides when NotifyVisualsUpdated fires; see renderer flow). */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Queue")
	void SetRow(const FMOQueueDisplayRow& InRow);

	/** Update only the live progress/remaining display (tick path). */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Queue")
	void UpdateLiveProgress(float NewProgress, const FText& NewTimeRemaining);

	/** Programmatic cancel-intent emission — same path as a button click.
	 *  Public for controllers/automation (F18 one-intent contract applies). */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Queue")
	void RequestCancel();

	UFUNCTION(BlueprintPure, Category="MO|UI|Queue")
	FGuid GetRowId() const { return Row.RowId; }

	UFUNCTION(BlueprintPure, Category="MO|UI|Queue")
	const FMOQueueDisplayRow& GetRow() const { return Row; }

	/** Re-render all visuals from the stored row (no BP notify). */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Queue")
	void UpdateRowVisuals();

	/** Fire the subclass/BP visual notification for the current row. Called by
	 *  the renderer after the domain hook has synchronized any legacy data. */
	virtual void NotifyVisualsUpdated() {}

	// --- Cancel intent ---

	UPROPERTY(BlueprintAssignable, Category="MO|UI|Queue")
	FMOQueueRowCancelIntentSignature OnCancelIntent;

	// --- Configuration (names/values preserved from the legacy entry widgets;
	//     WBP-overridden defaults survive via name-based serialization) ---

	/** Progress fill color for the Active state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Queue")
	FLinearColor ActiveColor = FLinearColor(0.2f, 0.4f, 0.2f, 1.0f);

	/** Progress fill color for the Queued state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Queue")
	FLinearColor QueuedColor = FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);

	/** Progress fill color for the Paused state (building interrupts). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Queue")
	FLinearColor PausedColor = FLinearColor(0.4f, 0.3f, 0.1f, 1.0f);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Subclass hook fired alongside the base cancel intent, so legacy per-domain
	 *  delegates (OnCancelRequested) keep broadcasting. */
	virtual void NotifyCancelIntent(const FGuid& InRowId) {}

	/** Cancel button handler (either button) — emits the intent. */
	UFUNCTION()
	void HandleCancelClicked();

	// --- Widget bindings (legacy names shared by both queue WBP families) ---

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RecipeNameText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UImage> RecipeIcon;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CountText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TimeRemainingText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> CancelButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UButton> CancelButtonSimple;

private:
	/** The bound display row. Base-owned — subclasses must not keep a parallel
	 *  row store (F17 lesson from the Stage-2 list consolidation). */
	UPROPERTY()
	FMOQueueDisplayRow Row;
};
