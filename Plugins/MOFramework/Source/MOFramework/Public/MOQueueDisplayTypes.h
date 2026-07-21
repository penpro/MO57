/**
 * =============================================================================
 * MOQueueDisplayTypes.h - Domain-neutral queue display rows (UI migration Stage 3)
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * The pure display-row model shared by every queue presentation (crafting queue,
 * building construction, survivor jobs later). A domain adapter translates its
 * source component's entries into FMOQueueDisplayRow values; the shared renderer
 * (UMOQueueRendererBase + UMOQueueRowWidgetBase) renders them and emits cancel
 * INTENTS back. Nothing here touches components, DataTables, or gameplay —
 * this file must stay headless-testable (CoreMinimal only), mirroring
 * FMOListSelectionModel from migration Stage 2.
 *
 * DESIGN NOTES:
 * - RowId is an FGuid because the survivor job source keys by FGuid; crafting's
 *   EntryId is already an FGuid; building's adapter synthesizes a stable one
 *   (the buildable's identity GUID) — do NOT narrow this to FName/int32.
 * - Rows carry BOTH raw numbers (CountCurrent/CountTotal, RemainingSeconds) and
 *   display texts (CountText, TimeRemainingText). FinalizeRowTexts() is the one
 *   formatting chokepoint filling texts from raw; legacy call paths that hand a
 *   widget preformatted text simply carry it through with raw left at defaults.
 * - Completion presents as REMOVAL in every current source (crafting/survivor
 *   delete entries; building leaves Constructing/Paused). There are deliberately
 *   no terminal states in EMOQueueRowState — rows vanish instead.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-07] PULL-BASED PROGRESS: sources do NOT reliably push progress events
 *   (crafting throttles to 0.5s; survivor UpdateJobProgress broadcasts nothing
 *   authority-side). Renderers poll on their own tick; adapters expose fresh
 *   values on demand. Never design a consumer that waits for progress pushes.
 *
 * =============================================================================
 * RELATED FILES: MOQueueRendererBase.h, MOQueueRowWidgetBase.h,
 *                MOListSelectionModel.h (the Stage-2 pure-model precedent)
 * LAST UPDATED: 2026-07-20 (Stage 3 introduction)
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOQueueDisplayTypes.generated.h"

class UTexture2D;

/**
 * Presentation state of a queue row. No terminal states: every current source
 * removes finished entries, so completion presents as row removal.
 */
UENUM(BlueprintType)
enum class EMOQueueRowState : uint8
{
	/** Waiting behind the active operation. */
	Queued,
	/** The operation currently running. */
	Active,
	/** Running operation suspended (e.g. building paused by a movement interrupt). */
	Paused,
};

/**
 * One domain-neutral queue row: everything the shared renderer needs to draw an
 * operation, and the RowId it echoes back in a cancel intent.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOQueueDisplayRow
{
	GENERATED_BODY()

	/** Stable operation id, echoed in cancel intents. Domain-owned:
	 *  crafting = queue EntryId, building = buildable identity GUID,
	 *  survivor = JobId. */
	UPROPERTY(BlueprintReadOnly, Category="MO|UI|Queue")
	FGuid RowId;

	/** Domain source id (recipe id, job type name) for BP consumers/diagnostics. */
	UPROPERTY(BlueprintReadOnly, Category="MO|UI|Queue")
	FName SourceId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="MO|UI|Queue")
	FText Title;

	UPROPERTY(BlueprintReadOnly, Category="MO|UI|Queue")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Raw iteration numbers (crafting: repeat CompletedCount+1 / Count;
	 *  building: part index+1 / part total). 0/0 = not applicable. */
	UPROPERTY(BlueprintReadOnly, Category="MO|UI|Queue")
	int32 CountCurrent = 0;

	UPROPERTY(BlueprintReadOnly, Category="MO|UI|Queue")
	int32 CountTotal = 0;

	/** Preformatted count display ("2/5"). Filled by FinalizeRowTexts from the
	 *  raw numbers, or carried through from a legacy preformatted path. */
	UPROPERTY(BlueprintReadOnly, Category="MO|UI|Queue")
	FText CountText;

	/** Progress 0-1 of this row's operation. */
	UPROPERTY(BlueprintReadOnly, Category="MO|UI|Queue")
	float Progress = 0.0f;

	/** Raw remaining seconds (<0 = unknown/not applicable). */
	UPROPERTY(BlueprintReadOnly, Category="MO|UI|Queue")
	float RemainingSeconds = -1.0f;

	/** Preformatted remaining-time display. Filled by FinalizeRowTexts. */
	UPROPERTY(BlueprintReadOnly, Category="MO|UI|Queue")
	FText TimeRemainingText;

	UPROPERTY(BlueprintReadOnly, Category="MO|UI|Queue")
	EMOQueueRowState State = EMOQueueRowState::Queued;

	/** Whether the renderer should offer a cancel affordance for this row. */
	UPROPERTY(BlueprintReadOnly, Category="MO|UI|Queue")
	bool bCancellable = true;
};

/**
 * The renderer's header block (name + overall progress + remaining). Supplied by
 * the domain: crafting shows OVERALL queue progress/total remaining; building
 * mirrors its single row.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOQueueHeaderDisplay
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|UI|Queue")
	FText ActiveTitle;

	/** 0-1 progress for the header bar. */
	UPROPERTY(BlueprintReadOnly, Category="MO|UI|Queue")
	float Progress = 0.0f;

	/** Raw remaining seconds for the header (<0 = unknown). */
	UPROPERTY(BlueprintReadOnly, Category="MO|UI|Queue")
	float RemainingSeconds = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category="MO|UI|Queue")
	bool bHasRows = false;
};

/**
 * Pure formatting helpers — the single chokepoint that turns raw row numbers into
 * display texts. Headless-testable (MOFramework.UI.Queue.* automation).
 */
namespace MOQueueDisplay
{
	/** "{Current}/{Total}" (matches both legacy queue widgets' count format). */
	MOFRAMEWORK_API FText FormatCount(int32 Current, int32 Total);

	/** "{Percent}%" from a 0-1 progress (matches the legacy header text). */
	MOFRAMEWORK_API FText FormatPercent(float Progress01);

	/** Fill CountText / TimeRemainingText from the raw fields when the raw fields
	 *  are meaningful and the texts are still empty. Duration formatting delegates
	 *  to UMOUIUtils::FormatDurationAsText (the project standard). */
	MOFRAMEWORK_API void FinalizeRowTexts(FMOQueueDisplayRow& Row);
}
