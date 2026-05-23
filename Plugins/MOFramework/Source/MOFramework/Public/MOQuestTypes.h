/**
 * =============================================================================
 * MOQuestTypes.h - Quest Data Structures
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Data structures for the quest system. Defines objective types, quest
 * definitions (DataTable rows), runtime quest state, and save data.
 *
 * KEY TYPES:
 * - EMOObjectiveType: Event, ItemCraft, ItemPickup, SkillLevelUp, etc.
 * - FMOQuestObjective: Single objective within a quest
 * - FMOQuestDefinitionRow: DataTable row defining a quest
 * - FMOQuestState: Runtime state tracking progress
 * - FMOQuestSaveData: Persistence structure
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] OBJECTIVE TYPE MATCHING: Each EMOObjectiveType has specific
 *   TargetEventOrId semantics. See TargetEventOrId comment in FMOQuestObjective.
 *
 * [2024-02] OPTIONAL VS SEQUENTIAL: bOptional objectives don't block completion.
 *   bSequential objectives must be completed in order within the quest.
 *
 * [2024-02] CUSTOM OBJECTIVES: EMOObjectiveType::Custom requires manual
 *   ReportObjectiveProgress() calls - no automatic event matching.
 *
 * =============================================================================
 * RELATED FILES: MOQuestSubsystem.h, MOQuestDelegates.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MOQuestTypes.generated.h"

/**
 * Types of quest objectives.
 * Determines how the objective listens for completion events.
 */
UENUM(BlueprintType)
enum class EMOObjectiveType : uint8
{
	/** Generic event trigger (e.g., "CraftingMenuOpened", "InventoryOpened") */
	Event,

	/** Triggered when a specific item is crafted. TargetEventOrId = ItemId. */
	ItemCraft,

	/** Triggered when a specific item is picked up. TargetEventOrId = ItemId. */
	ItemPickup,

	/** Triggered when a specific item is dropped. TargetEventOrId = ItemId. */
	ItemDrop,

	/** Triggered when a specific skill levels up. TargetEventOrId = SkillId. */
	SkillLevelUp,

	/** Triggered when entering a location trigger volume. TargetEventOrId = LocationTag. */
	LocationReach,

	/** Blueprint-defined custom condition. Use ReportObjectiveProgress() manually. */
	Custom
};

/**
 * Single objective within a quest.
 * Objectives are completed by matching events or manual reporting.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOQuestObjective
{
	GENERATED_BODY()

	/** Unique identifier for this objective within the quest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest")
	FName ObjectiveId;

	/** Player-facing description (e.g., "Craft a stone axe"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest")
	FText Description;

	/** How this objective detects completion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest")
	EMOObjectiveType Type = EMOObjectiveType::Event;

	/**
	 * Target to match for completion.
	 * - Event: Event name (e.g., "CraftingMenuOpened")
	 * - ItemCraft/Pickup/Drop: ItemId (e.g., "Stick01")
	 * - SkillLevelUp: SkillId (e.g., "Woodcutting")
	 * - LocationReach: Location tag
	 * - Custom: Ignored (use manual reporting)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest")
	FName TargetEventOrId;

	/** Number of times the event must occur (e.g., "Craft 5 sticks"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest", meta=(ClampMin="1"))
	int32 RequiredCount = 1;

	/** If true, this objective is not required for quest completion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest")
	bool bOptional = false;

	/** If true, this objective must be completed in order (after previous objectives). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest")
	bool bSequential = false;

	// ========================================================================
	// TUTORIAL HINT FIELDS
	// ========================================================================
	// These drive the tutorial popup widget. When this objective becomes the
	// "current" objective for an active tutorial quest, the popup shows
	// HintTitle + HintBody (formatted through FMOInputHintFormatter so
	// {key:ActionName} tokens resolve to the player's live key bindings).
	//
	// If bShowAsTutorialPopup is false, the objective is silent — it advances
	// the chain but no popup appears (useful for sequencer / book-keeping
	// objectives between teachable moments).

	/** Short title shown at the top of the tutorial popup banner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest|Tutorial")
	FText HintTitle;

	/**
	 * Body text shown in the tutorial popup. Supports token substitution:
	 *   {key:ActionName}            → "[Space]"
	 *   {key:ActionName:Building}   → key in the named context
	 *   {key:ActionName:1}          → secondary slot binding
	 * Tokens are resolved at display time, so rebinds update text live.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest|Tutorial", meta=(MultiLine="true"))
	FText HintBody;

	/**
	 * If true, show the tutorial popup banner while this objective is the
	 * current target of an active tutorial quest. Set false for silent
	 * objectives that just progress the chain.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest|Tutorial")
	bool bShowAsTutorialPopup = false;

	FMOQuestObjective()
		: Type(EMOObjectiveType::Event)
		, RequiredCount(1)
		, bOptional(false)
		, bSequential(false)
		, bShowAsTutorialPopup(false)
	{
	}
};

// ============================================================================
// QUEST REWARDS
// ============================================================================
// Reward plumbing is wired but no reward actually does anything yet — the
// quest subsystem broadcasts OnApplyQuestRewards on completion so future
// systems (XP, knowledge, items) can hook in without modifying the subsystem.
// FMOQuestRewardSpec lives on the row so designers can author rewards now.

/** Kind of reward to apply on quest completion. */
UENUM(BlueprintType)
enum class EMOQuestRewardType : uint8
{
	/** No reward. */
	None,
	/** Grant XP. TargetId = SkillId, Amount = XP value. */
	XP,
	/** Grant a knowledge entry. TargetId = KnowledgeId. Amount ignored. */
	Knowledge,
	/** Grant items. TargetId = ItemId, Amount = quantity. */
	Item
};

/** A single reward granted when a quest completes. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOQuestRewardSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest|Reward")
	EMOQuestRewardType Type = EMOQuestRewardType::None;

	/** Target identifier — skill name, knowledge id, item id, etc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest|Reward")
	FName TargetId;

	/** Amount — XP value, item count, etc. Ignored for booleans like Knowledge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest|Reward")
	float Amount = 0.0f;
};

/**
 * DataTable row defining a quest.
 * Quest definitions are loaded from DT_Quests DataTable.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOQuestDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Unique identifier for this quest. Must match the row name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest")
	FName QuestId;

	/** Player-facing quest title. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest")
	FText DisplayName;

	/** Detailed quest description shown in quest log. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest")
	FText Description;

	/** If true, this is a tutorial quest (may be filtered separately in UI). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest")
	bool bIsTutorial = false;

	/** If true, quest starts automatically when prerequisites are met. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest")
	bool bAutoStart = false;

	/** Quest IDs that must be completed before this quest can start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest")
	TArray<FName> Prerequisites;

	/** Ordered list of objectives to complete. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest")
	TArray<FMOQuestObjective> Objectives;

	/** Sort order for UI display (lower = appears first). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest")
	int32 SortOrder = 0;

	/** Category for grouping in quest log (e.g., "Tutorial", "Main", "Side"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest")
	FName Category;

	/**
	 * Rewards granted when this quest completes. Empty for now on most
	 * quests; the subsystem broadcasts OnApplyQuestRewards regardless so
	 * future XP / knowledge / item systems can hook in without code changes
	 * here.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest|Reward")
	TArray<FMOQuestRewardSpec> Rewards;

	FMOQuestDefinitionRow()
		: bIsTutorial(false)
		, bAutoStart(false)
		, SortOrder(0)
	{
	}
};

/**
 * Runtime state for an active or completed quest.
 * Tracks progress through objectives.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOQuestState
{
	GENERATED_BODY()

	/** The quest this state tracks. */
	UPROPERTY(BlueprintReadOnly, Category="Quest")
	FName QuestId;

	/** Progress for each objective. Key = ObjectiveId, Value = CurrentCount. */
	UPROPERTY(BlueprintReadOnly, Category="Quest")
	TMap<FName, int32> ObjectiveProgress;

	/** Set of completed objective IDs. */
	UPROPERTY(BlueprintReadOnly, Category="Quest")
	TSet<FName> CompletedObjectives;

	/** True if all required objectives are complete. */
	UPROPERTY(BlueprintReadOnly, Category="Quest")
	bool bIsComplete = false;

	/** True if this quest should show on HUD tracker. */
	UPROPERTY(BlueprintReadWrite, Category="Quest")
	bool bIsTracked = true;

	/** Timestamp when quest was started. */
	UPROPERTY(BlueprintReadOnly, Category="Quest")
	FDateTime StartTime;

	/** Timestamp when quest was completed (if complete). */
	UPROPERTY(BlueprintReadOnly, Category="Quest")
	FDateTime CompletionTime;

	FMOQuestState()
		: bIsComplete(false)
		, bIsTracked(true)
	{
	}

	/** Get progress for a specific objective. Returns 0 if not started. */
	int32 GetObjectiveProgress(FName ObjectiveId) const
	{
		const int32* Progress = ObjectiveProgress.Find(ObjectiveId);
		return Progress ? *Progress : 0;
	}

	/** Check if a specific objective is complete. */
	bool IsObjectiveComplete(FName ObjectiveId) const
	{
		return CompletedObjectives.Contains(ObjectiveId);
	}
};

/**
 * Save data for quest progress.
 * Used by persistence system to save/load quest state.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOQuestSaveData
{
	GENERATED_BODY()

	/** All active quest states. */
	UPROPERTY()
	TArray<FMOQuestState> ActiveQuests;

	/** IDs of all completed quests. */
	UPROPERTY()
	TArray<FName> CompletedQuestIds;
};
