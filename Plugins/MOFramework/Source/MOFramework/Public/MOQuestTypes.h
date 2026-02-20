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

	FMOQuestObjective()
		: Type(EMOObjectiveType::Event)
		, RequiredCount(1)
		, bOptional(false)
		, bSequential(false)
	{
	}
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
