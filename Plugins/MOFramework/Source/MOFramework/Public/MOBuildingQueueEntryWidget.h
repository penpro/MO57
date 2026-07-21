/**
 * =============================================================================
 * MOBuildingQueueEntryWidget.h - Building Queue Entry (Stage-3 compat adapter)
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Thin compatibility subclass of UMOQueueRowWidgetBase (migration Stage 3).
 * The base renders the row (including the 3-state Active/Paused/Queued fill
 * color the building domain introduced) and emits cancel intents; this class
 * preserves the building-specific BP surface: FMOBuildQueueEntryDisplayData,
 * SetupEntry, UpdateProgress, GetEntryId/GetEntryData, the legacy
 * OnCancelRequested delegate, and the OnVisualsUpdated Blueprint event.
 * WBP_BuildQueueEntry keeps this parent; bindings and colors moved to the base
 * under their exact legacy names.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-07] LEGACY DATA IS A MIRROR: LegacyData is synthesized from the base's
 *   neutral row (by the owning adapter or SetupEntry) — never a second source
 *   of truth (F17).
 *
 * =============================================================================
 * RELATED FILES: MOQueueRowWidgetBase.h, MOBuildingQueueWidget.h,
 *                MOBuildingTypes.h, MOCraftingQueueEntryWidget.h
 * LAST UPDATED: 2026-07-20 (Stage 3: reparented onto the shared queue row)
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOQueueRowWidgetBase.h"
#include "MOBuildingTypes.h"
#include "MOBuildingQueueEntryWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOBuildQueueEntryCancelRequestedSignature, const FGuid&, EntryId);

/**
 * Visual data for a build queue entry (legacy BP-visible struct, preserved).
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOBuildQueueEntryDisplayData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	FGuid EntryId;

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	FName RecipeId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	FText RecipeName;

	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Current part / total parts (e.g., "2/5") */
	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	FText CountText;

	/** Progress 0.0 - 1.0 */
	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	float Progress = 0.0f;

	/** Time remaining for this entry. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	FText TimeRemainingText;

	/** True if this is the currently active build. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	bool bIsActive = false;

	/** Current build state. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Building|UI")
	EMOBuildState State = EMOBuildState::Ghost;
};

/**
 * Building queue entry: thin compat subclass of the shared queue row.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOBuildingQueueEntryWidget : public UMOQueueRowWidgetBase
{
	GENERATED_BODY()

public:
	UMOBuildingQueueEntryWidget(const FObjectInitializer& ObjectInitializer);

	// --- Setup (legacy API) ---

	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void SetupEntry(const FMOBuildQueueEntryDisplayData& InData);

	/** Update just the progress. */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void UpdateProgress(float NewProgress, const FText& NewTimeRemaining);

	// --- Getters (legacy API) ---

	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	FGuid GetEntryId() const { return LegacyData.EntryId; }

	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	const FMOBuildQueueEntryDisplayData& GetEntryData() const { return LegacyData; }

	// --- Delegates (legacy; re-broadcast alongside the base OnCancelIntent) ---

	UPROPERTY(BlueprintAssignable, Category="MO|Building|UI")
	FMOBuildQueueEntryCancelRequestedSignature OnCancelRequested;

	/** Store the legacy display mirror (no re-render) — called by the building
	 *  adapter after the base binds a neutral row. */
	void SetLegacyEntryData(const FMOBuildQueueEntryDisplayData& InData) { LegacyData = InData; }

	// --- Base hooks ---

	virtual void NotifyVisualsUpdated() override;
	virtual void NotifyCancelIntent(const FGuid& InRowId) override;

protected:
	/** Update visuals based on current data (legacy API). */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void UpdateVisuals();

	/** Blueprint event for custom visual updates. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Building|UI")
	void OnVisualsUpdated(const FMOBuildQueueEntryDisplayData& Data);

private:
	/** Legacy BP-visible mirror of the base's neutral row (see pitfalls). */
	FMOBuildQueueEntryDisplayData LegacyData;
};
