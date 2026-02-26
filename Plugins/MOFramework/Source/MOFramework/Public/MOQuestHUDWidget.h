/**
 * =============================================================================
 * MOQuestHUDWidget.h - Quest Tracker HUD Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * HUD widget that displays tracked quest objectives. Shows a compact list
 * of active objectives for quick reference during gameplay. Automatically
 * updates when quest progress changes.
 *
 * FEATURES:
 * - Displays up to MaxTrackedQuestsOnHUD quests (from MOQuestSettings)
 * - Auto-updates on objective progress/completion
 * - Creates/removes tracker entries as quests are tracked/untracked
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] ENTRY CLASS: TrackerEntryClass must be set before widget works.
 *   Set in Blueprint defaults or call SetTrackerEntryClass() first.
 *
 * [2024-02] SUBSYSTEM BINDING: Binds to all quest events in NativeConstruct.
 *   Must unbind in NativeDestruct to prevent stale delegate references.
 *
 * [2024-02] MAX ENTRIES: Respects MaxTrackedQuestsOnHUD from MOQuestSettings.
 *   Entries beyond this limit are not displayed.
 *
 * =============================================================================
 * RELATED FILES: MOQuestUIController.h, MOQuestTrackerEntry.h, MOQuestSubsystem.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOQuestTypes.h"
#include "MOQuestTrackerEntry.h"
#include "MOQuestHUDWidget.generated.h"

class UVerticalBox;
class UMOQuestSubsystem;

/**
 * HUD widget that displays tracked quest objectives.
 * Shows a compact list of active objectives for quick reference.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOQuestHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOQuestHUDWidget(const FObjectInitializer& ObjectInitializer);

	/** Refresh the tracked quests display. */
	UFUNCTION(BlueprintCallable, Category="MO|Quest|UI")
	void RefreshTrackedQuests();

	/** Set the widget class to use for tracker entries. */
	UFUNCTION(BlueprintCallable, Category="MO|Quest|UI")
	void SetTrackerEntryClass(TSubclassOf<UMOQuestTrackerEntry> InClass);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	/** Handle quest started event. */
	UFUNCTION()
	void HandleQuestStarted(FName QuestId);

	/** Handle objective progress update. */
	UFUNCTION()
	void HandleObjectiveUpdated(FName QuestId, FName ObjectiveId, int32 NewProgress);

	/** Handle objective completed. */
	UFUNCTION()
	void HandleObjectiveCompleted(FName QuestId, FName ObjectiveId);

	/** Handle quest completed. */
	UFUNCTION()
	void HandleQuestCompleted(FName QuestId);

	/** Handle quest abandoned. */
	UFUNCTION()
	void HandleQuestAbandoned(FName QuestId);

	/** Create display data for a quest state. */
	FMOQuestTrackerDisplayData CreateDisplayData(const FMOQuestState& State) const;

	/** Find existing entry widget for a quest. */
	UMOQuestTrackerEntry* FindEntryForQuest(FName QuestId) const;

	/** Bind to quest subsystem events. */
	void BindToQuestSubsystem();

	/** Unbind from quest subsystem events. */
	void UnbindFromQuestSubsystem();

protected:
	/** Container for tracker entries. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UVerticalBox> TrackedQuestsContainer;

	/** Widget class to spawn for each tracked quest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Quest|UI")
	TSubclassOf<UMOQuestTrackerEntry> TrackerEntryClass;

private:
	/** Currently displayed tracker entries. */
	UPROPERTY()
	TArray<TObjectPtr<UMOQuestTrackerEntry>> TrackerEntries;

	/** Cached reference to quest subsystem. */
	TWeakObjectPtr<UMOQuestSubsystem> QuestSubsystem;
};
