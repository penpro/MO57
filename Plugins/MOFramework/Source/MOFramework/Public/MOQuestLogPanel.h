/**
 * =============================================================================
 * MOQuestLogPanel.h - Full Quest Log Panel Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Full quest log panel showing all active and completed quests. Two-column
 * layout: quest list on left, details/objectives on right. Opens from
 * in-game menu or keybind via MOQuestUIController.
 *
 * LAYOUT:
 * +------------------+------------------------+
 * |   Quest List     |    Quest Details       |
 * |   (ScrollBox)    |    - Title             |
 * |                  |    - Description       |
 * |   [Entry]        |    - Objectives        |
 * |   [Entry]        |    [Track] [Abandon]   |
 * +------------------+------------------------+
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] ENTRY CLASS: QuestEntryClass must be set before RefreshQuestList().
 *   Either set in Blueprint defaults or call SetQuestEntryClass() first.
 *
 * [2024-02] SUBSYSTEM BINDING: Binds to quest subsystem events in NativeConstruct.
 *   Must unbind in NativeDestruct to prevent stale delegate references.
 *
 * [2024-02] TRACK BUTTON TEXT: Updates dynamically ("Track"/"Untrack") based
 *   on quest's bIsTracked state.
 *
 * =============================================================================
 * RELATED FILES: MOQuestUIController.h, MOQuestLogEntry.h, MOQuestSubsystem.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MOQuestTypes.h"
#include "MOQuestLogEntry.h"
#include "MOQuestLogPanel.generated.h"

class UScrollBox;
class UVerticalBox;
class UTextBlock;
class UMOCommonButton;
class UMOQuestSubsystem;

/** Delegate for requesting panel close. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnQuestLogCloseRequested);

/**
 * Full quest log panel showing all active and completed quests.
 * Opens from in-game menu or keybind.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOQuestLogPanel : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UMOQuestLogPanel(const FObjectInitializer& ObjectInitializer);

	/** Refresh the quest list. */
	UFUNCTION(BlueprintCallable, Category="MO|Quest|UI")
	void RefreshQuestList();

	/** Select a quest to show details. */
	UFUNCTION(BlueprintCallable, Category="MO|Quest|UI")
	void SelectQuest(FName QuestId);

	/** Toggle tracking for the selected quest. */
	UFUNCTION(BlueprintCallable, Category="MO|Quest|UI")
	void ToggleTrackSelectedQuest();

	/** Abandon the selected quest. */
	UFUNCTION(BlueprintCallable, Category="MO|Quest|UI")
	void AbandonSelectedQuest();

	/** Set the widget class to use for quest entries. */
	UFUNCTION(BlueprintCallable, Category="MO|Quest|UI")
	void SetQuestEntryClass(TSubclassOf<UMOQuestLogEntry> InClass);

	/** Delegate for close requests. */
	UPROPERTY(BlueprintAssignable, Category="MO|Quest|UI")
	FMOOnQuestLogCloseRequested OnCloseRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
	/** Handle quest entry selection. */
	void HandleQuestEntrySelected(FName QuestId);

	/** Handle close button click. */
	UFUNCTION()
	void HandleCloseClicked();

	/** Handle track button click. */
	UFUNCTION()
	void HandleTrackClicked();

	/** Handle abandon button click. */
	UFUNCTION()
	void HandleAbandonClicked();

	/** Bind to quest subsystem events. */
	void BindToQuestSubsystem();

	/** Unbind from quest subsystem events. */
	void UnbindFromQuestSubsystem();

	/** Handle quest state changes to refresh list. */
	UFUNCTION()
	void HandleQuestStateChanged(FName QuestId);

	/** Refresh details panel for selected quest. */
	void RefreshDetailsPanel();

	/** Create display data for a quest. */
	FMOQuestLogDisplayData CreateQuestDisplayData(const FMOQuestState& State, const FMOQuestDefinitionRow& Definition) const;

	/** Populate objectives list for selected quest. */
	void PopulateObjectives(const FMOQuestState& State, const FMOQuestDefinitionRow& Definition);

protected:
	// =========================================================================
	// QUEST LIST (LEFT SIDE)
	// =========================================================================

	/** Scroll box containing quest entries. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UScrollBox> QuestListScrollBox;

	/** Widget class for quest list entries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Quest|UI")
	TSubclassOf<UMOQuestLogEntry> QuestEntryClass;

	// =========================================================================
	// DETAILS PANEL (RIGHT SIDE)
	// =========================================================================

	/** Selected quest title. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UTextBlock> SelectedQuestTitle;

	/** Selected quest description. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UTextBlock> SelectedQuestDescription;

	/** Container for objective entries. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UVerticalBox> ObjectivesContainer;

	/** Track/untrack button. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UMOCommonButton> TrackButton;

	/** Track button label. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TrackButtonText;

	/** Abandon quest button. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> AbandonButton;

	/** Close panel button. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> CloseButton;

	// =========================================================================
	// STATE
	// =========================================================================

	/** Currently selected quest ID. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Quest|UI")
	FName SelectedQuestId;

private:
	/** Created quest entry widgets. */
	UPROPERTY()
	TArray<TObjectPtr<UMOQuestLogEntry>> QuestEntries;

	/** Cached reference to quest subsystem. */
	TWeakObjectPtr<UMOQuestSubsystem> QuestSubsystem;
};
