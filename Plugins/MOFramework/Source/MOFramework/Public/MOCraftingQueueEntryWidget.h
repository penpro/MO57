/**
 * =============================================================================
 * MOCraftingQueueEntryWidget.h - Crafting Queue Entry (Stage-3 compat adapter)
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Thin compatibility subclass of UMOQueueRowWidgetBase (migration Stage 3).
 * The base renders the row and emits cancel intents; this class preserves the
 * crafting-specific BP surface: FMOQueueEntryDisplayData, SetupEntry,
 * UpdateProgress, GetEntryId/GetEntryData, the legacy OnCancelRequested
 * delegate, and the OnVisualsUpdated Blueprint event. WBP_CraftQueueEntry
 * keeps this parent; all widget bindings and colors moved to the base under
 * their exact legacy names.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] DUAL CANCEL BUTTONS: handled by the base (CancelButton preferred
 *   over CancelButtonSimple; bind only one in Blueprint).
 *
 * [2026-07] LEGACY DATA IS A MIRROR: LegacyData is synthesized from the base's
 *   neutral row (by the owning adapter or SetupEntry) — it is presentation
 *   compatibility state, never a second source of truth (F17).
 *
 * =============================================================================
 * RELATED FILES: MOQueueRowWidgetBase.h, MOCraftingQueueWidget.h,
 *                MOCraftingTypes.h, MOBuildingQueueEntryWidget.h
 * LAST UPDATED: 2026-07-20 (Stage 3: reparented onto the shared queue row)
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOQueueRowWidgetBase.h"
#include "MOCraftingTypes.h"
#include "MOCraftingQueueEntryWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOQueueEntryCancelRequestedSignature, const FGuid&, EntryId);

/**
 * Visual data for a queue entry (legacy BP-visible struct, preserved verbatim).
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOQueueEntryDisplayData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	FGuid EntryId;

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	FName RecipeId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	FText RecipeName;

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Current count / total count (e.g., "2/5") */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	FText CountText;

	/** Progress 0.0 - 1.0 */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	float Progress = 0.0f;

	/** Time remaining for this entry. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	FText TimeRemainingText;

	/** True if this is the currently active craft. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	bool bIsActive = false;
};

/**
 * Crafting queue entry: thin compat subclass of the shared queue row.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOCraftingQueueEntryWidget : public UMOQueueRowWidgetBase
{
	GENERATED_BODY()

public:
	UMOCraftingQueueEntryWidget(const FObjectInitializer& ObjectInitializer);

	// --- Setup (legacy API) ---

	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void SetupEntry(const FMOQueueEntryDisplayData& InData);

	/** Update just the progress. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void UpdateProgress(float NewProgress, const FText& NewTimeRemaining);

	// --- Getters (legacy API) ---

	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	FGuid GetEntryId() const { return LegacyData.EntryId; }

	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	const FMOQueueEntryDisplayData& GetEntryData() const { return LegacyData; }

	// --- Delegates (legacy; re-broadcast alongside the base OnCancelIntent) ---

	UPROPERTY(BlueprintAssignable, Category="MO|Crafting|UI")
	FMOQueueEntryCancelRequestedSignature OnCancelRequested;

	/** Store the legacy display mirror (no re-render) — called by the crafting
	 *  adapter after the base binds a neutral row. */
	void SetLegacyEntryData(const FMOQueueEntryDisplayData& InData) { LegacyData = InData; }

	// --- Base hooks ---

	virtual void NotifyVisualsUpdated() override;
	virtual void NotifyCancelIntent(const FGuid& InRowId) override;

protected:
	/** Update visuals based on current data (legacy API). */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void UpdateVisuals();

	/** Blueprint event for custom visual updates. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Crafting|UI")
	void OnVisualsUpdated(const FMOQueueEntryDisplayData& Data);

private:
	/** Legacy BP-visible mirror of the base's neutral row (see pitfalls). */
	FMOQueueEntryDisplayData LegacyData;
};
