/**
 * =============================================================================
 * MOQuestSubsystem.h - Quest Management Subsystem
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * GameInstance subsystem for managing quests. Loads quest definitions from
 * DataTable, tracks active/completed quest states, subscribes to game events
 * (crafting, skills, items), checks objective completion, broadcasts quest
 * state changes, and handles save/load.
 *
 * QUEST FLOW:
 * 1. Quest definitions loaded from DT_Quests on Initialize()
 * 2. StartQuest() activates a quest, creating its state
 * 3. Game events fire -> HandleXxx() methods check objectives
 * 4. Objective completion triggers OnObjectiveUpdated/Completed
 * 5. All required objectives done -> OnQuestCompleted
 *
 * SETTINGS:
 * UMOQuestSettings provides project settings for quest system configuration.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] WORLD SUBSYSTEM BINDING: Binds to crafting subsystem in
 *   HandleWorldInitialized(). World subsystems not available at Initialize().
 *
 * [2024-02] EVENT HANDLERS: Components call HandleItemPickedUp/HandleSkillLevelUp
 *   directly. Crafting is bound via delegate in HandleWorldInitialized.
 *
 * [2024-02] SEQUENTIAL OBJECTIVES: bSequential objectives must be completed
 *   in order. GetCurrentSequentialObjective() finds the first incomplete one.
 *
 * =============================================================================
 * RELATED FILES: MOQuestTypes.h, MOQuestDelegates.h, MOCraftingSubsystem.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/DeveloperSettings.h"
#include "MOQuestTypes.h"
#include "MOQuestDelegates.h"
#include "MOSaveDomainInterface.h"
#include "MOQuestSubsystem.generated.h"

class UDataTable;
class UMOCraftingSubsystem;
class UMOSkillsComponent;
class UMOInventoryComponent;
struct FMOCraftResult;

/**
 * Settings for the quest system.
 * Configured in Project Settings > Game > Quest System.
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="Quest System"))
class MOFRAMEWORK_API UMOQuestSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** DataTable containing quest definitions. */
	UPROPERTY(Config, EditAnywhere, Category="Quest System", meta=(AllowedClasses="/Script/Engine.DataTable"))
	FSoftObjectPath QuestDefinitionTable;

	/** Maximum number of quests to show on HUD tracker. */
	UPROPERTY(Config, EditAnywhere, Category="Quest System", meta=(ClampMin="1", ClampMax="10"))
	int32 MaxTrackedQuestsOnHUD = 3;

	/** Whether to auto-start tutorial quests on new game. */
	UPROPERTY(Config, EditAnywhere, Category="Quest System")
	bool bAutoStartTutorials = true;

	static UMOQuestSettings* Get()
	{
		return GetMutableDefault<UMOQuestSettings>();
	}
};

/**
 * GameInstance subsystem for managing quests.
 *
 * Responsibilities:
 * - Load quest definitions from DataTable
 * - Track active/completed quest states
 * - Subscribe to game events (crafting, skills, items)
 * - Check objective completion
 * - Broadcast quest state changes
 * - Save/load quest progress
 *
 * Quest flow:
 * 1. Quest definitions loaded from DT_Quests on Initialize()
 * 2. StartQuest() activates a quest, creating its state
 * 3. Game events fire → HandleXxx() methods check objectives
 * 4. Objective completion triggers OnObjectiveUpdated/Completed
 * 5. All required objectives done → OnQuestCompleted
 */
UCLASS()
class MOFRAMEWORK_API UMOQuestSubsystem : public UGameInstanceSubsystem, public IMOSaveDomain
{
	GENERATED_BODY()

public:
	//~ Begin IMOSaveDomain (Quest save data lives here, not in the persistence subsystem)
	virtual FName GetSaveDomainName() const override { return TEXT("Quest"); }
	virtual int32 GetSaveDomainApplyPriority() const override { return 20; }
	virtual void CaptureSaveDomain(UMOWorldSaveGame& Save) override;
	virtual void ApplySaveDomain(const UMOWorldSaveGame& Save) override;
	//~ End IMOSaveDomain

	// =========================================================================
	// LIFECYCLE
	// =========================================================================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	// =========================================================================
	// QUEST MANAGEMENT
	// =========================================================================

	/**
	 * Start a quest. Creates active state and begins tracking objectives.
	 * @param QuestId The quest to start
	 * @return True if started successfully
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Quest")
	bool StartQuest(FName QuestId);

	/**
	 * Abandon a quest. Removes from active list.
	 * @param QuestId The quest to abandon
	 * @return True if abandoned successfully
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Quest")
	bool AbandonQuest(FName QuestId);

	/**
	 * Set whether a quest is tracked on HUD.
	 * @param QuestId The quest to track/untrack
	 * @param bTracked Whether to show on HUD
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Quest")
	void SetQuestTracked(FName QuestId, bool bTracked);

	/**
	 * Manually complete an objective (for Custom type objectives).
	 * @param QuestId The quest containing the objective
	 * @param ObjectiveId The objective to progress
	 * @param Count Progress amount (default 1)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Quest")
	void ReportObjectiveProgress(FName QuestId, FName ObjectiveId, int32 Count = 1);

	/**
	 * Fire a generic game event that objectives can listen to.
	 * @param EventName The event name (e.g., "CraftingMenuOpened")
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Quest")
	void FireGameEvent(FName EventName);

	/**
	 * Fast O(1) check: is any active quest listening for this Event-type
	 * objective right now? Callers that fire events at high frequency
	 * (per-tick movement / look) should check this first so they can skip
	 * the FireGameEvent call entirely when no quest cares. Once tutorial
	 * quests complete, this returns false and the per-tick broadcasts cost
	 * nothing.
	 *
	 * Maintained by the subsystem from quest start / objective progress /
	 * quest complete / abandon / load. Callers don't need to refresh it.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Quest")
	bool IsAnyQuestListeningForEvent(FName EventName) const;

	// =========================================================================
	// QUERIES
	// =========================================================================

	/**
	 * Get all active (non-completed) quests.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Quest")
	TArray<FMOQuestState> GetActiveQuests() const;

	/**
	 * Get all quests currently tracked on HUD.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Quest")
	TArray<FMOQuestState> GetTrackedQuests() const;

	/**
	 * Get state for a specific quest.
	 * @param QuestId The quest to query
	 * @param OutState Filled with state if found
	 * @return True if quest is active
	 */
	UFUNCTION(BlueprintPure, Category="MO|Quest")
	bool GetQuestState(FName QuestId, FMOQuestState& OutState) const;

	/**
	 * Check if a quest is complete.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Quest")
	bool IsQuestComplete(FName QuestId) const;

	/**
	 * Check if a quest is active (started but not complete).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Quest")
	bool IsQuestActive(FName QuestId) const;

	/**
	 * Get the definition for a quest.
	 * @param QuestId The quest to query
	 * @param OutDefinition Filled with definition if found
	 * @return True if quest definition exists
	 */
	UFUNCTION(BlueprintPure, Category="MO|Quest")
	bool GetQuestDefinition(FName QuestId, FMOQuestDefinitionRow& OutDefinition) const;

	/**
	 * Get the definition for a quest (C++ only, returns pointer).
	 * @param QuestId The quest to query
	 * @return Pointer to definition, or nullptr if not found
	 */
	const FMOQuestDefinitionRow* GetQuestDefinitionPtr(FName QuestId) const;

	/**
	 * Get all available quests that can be started (prerequisites met, not already complete).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Quest")
	TArray<FName> GetAvailableQuests() const;

	/**
	 * Get all completed quest IDs.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Quest")
	TArray<FName> GetCompletedQuestIds() const;

	// =========================================================================
	// PERSISTENCE
	// =========================================================================

	/**
	 * Build save data from current quest state.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Quest")
	void BuildSaveData(FMOQuestSaveData& OutSaveData) const;

	/**
	 * Apply save data to restore quest state.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Quest")
	void ApplySaveData(const FMOQuestSaveData& SaveData);

	/**
	 * Reset all quest progress (for new game).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Quest")
	void ResetAllQuests();

	// =========================================================================
	// DELEGATES
	// =========================================================================

	/** Broadcast when a quest is started. */
	UPROPERTY(BlueprintAssignable, Category="MO|Quest|Events")
	FMOOnQuestStarted OnQuestStarted;

	/** Broadcast when a quest is completed. */
	UPROPERTY(BlueprintAssignable, Category="MO|Quest|Events")
	FMOOnQuestCompleted OnQuestCompleted;

	/** Broadcast when a quest is abandoned. */
	UPROPERTY(BlueprintAssignable, Category="MO|Quest|Events")
	FMOOnQuestAbandoned OnQuestAbandoned;

	/** Broadcast when an objective's progress changes. */
	UPROPERTY(BlueprintAssignable, Category="MO|Quest|Events")
	FMOOnObjectiveUpdated OnObjectiveUpdated;

	/** Broadcast when an objective is completed. */
	UPROPERTY(BlueprintAssignable, Category="MO|Quest|Events")
	FMOOnObjectiveCompleted OnObjectiveCompleted;

	/**
	 * Broadcast when the active tutorial hint changes (new objective, progress,
	 * completion, or popup-enable toggle). Listeners query
	 * GetActiveTutorialHint() to read the current state.
	 */
	UPROPERTY(BlueprintAssignable, Category="MO|Quest|Events")
	FMOOnTutorialHintChanged OnTutorialHintChanged;

	/**
	 * Broadcast after a quest completes so external systems can apply rewards
	 * (XP, knowledge, items). The subsystem only fires this — it never grants
	 * rewards itself. Keeps the subsystem free of component dependencies.
	 */
	UPROPERTY(BlueprintAssignable, Category="MO|Quest|Events")
	FMOOnApplyQuestRewards OnApplyQuestRewards;

	/**
	 * Broadcast once per subsystem lifetime, immediately after quest definitions
	 * finish loading. UI controllers that spawn quest-bound widgets subscribe
	 * here instead of relying on BeginPlay timing — guarantees the subsystem
	 * is queryable before the widget is created.
	 *
	 * Listeners that subscribe AFTER this fires can call IsReady() to detect
	 * the race and act immediately. See UMOQuestUIController for the pattern.
	 */
	UPROPERTY(BlueprintAssignable, Category="MO|Quest|Events")
	FMOOnQuestSystemReady OnQuestSystemReady;

	/**
	 * Returns true once LoadQuestDefinitions has completed and OnQuestSystemReady
	 * has been broadcast. Late subscribers should check this and act
	 * immediately if true, otherwise bind and wait for the broadcast.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Quest")
	bool IsReady() const { return bIsReady; }

	// =========================================================================
	// TUTORIAL HINT API
	// =========================================================================

	/**
	 * Get the active tutorial hint, if any. Picks the first tutorial quest
	 * (bIsTutorial=true) with an active objective that has
	 * bShowAsTutorialPopup=true.
	 *
	 * @return True if a tutorial hint should be displayed.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Quest|Tutorial")
	bool GetActiveTutorialHint(FName& OutQuestId, FName& OutObjectiveId,
		FText& OutHintTitle, FText& OutHintBody) const;

	/**
	 * Skip the current tutorial objective — marks it complete without firing
	 * its normal completion event. Bound to F1 by the player controller.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Quest|Tutorial")
	void SkipCurrentTutorialObjective();

	/**
	 * Globally disable tutorial popups for this session. Quests still
	 * progress, just no banner shows. Bound to F3 by the player controller.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Quest|Tutorial")
	void SetTutorialPopupsEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="MO|Quest|Tutorial")
	bool AreTutorialPopupsEnabled() const { return bTutorialPopupsEnabled; }

	// =========================================================================
	// EVENT HANDLERS (Public for component callbacks)
	// =========================================================================

	/** Called when an item is picked up. Components should call this directly. */
	void HandleItemPickedUp(FName ItemId, int32 Quantity);

	/** Called when a skill levels up. Components should call this directly. */
	void HandleSkillLevelUp(FName SkillId, int32 OldLevel, int32 NewLevel);

	/**
	 * Called when a building finishes construction. Treated as an ItemCraft
	 * event keyed by the building's RecipeId (the recipe ROW name, e.g.
	 * "BuildCampfire") — quests with Type=ItemCraft and TargetEventOrId set to
	 * that recipe id will progress. Components / buildable actors call this
	 * directly; the subsystem doesn't watch the building system itself.
	 */
	void HandleBuildingCompleted(FName RecipeId);

private:
	// =========================================================================
	// INTERNAL EVENT HANDLERS
	// =========================================================================

	/** Called when the world initializes - binds to world subsystems. */
	void HandleWorldInitialized(UWorld* World, const UWorld::InitializationValues IVS);

	/** Called when a craft completes. */
	UFUNCTION()
	void HandleCraftCompleted(FName RecipeId, const FMOCraftResult& Result);

	// =========================================================================
	// INTERNAL
	// =========================================================================

	/** Load quest definitions from DataTable. */
	void LoadQuestDefinitions();

	/** Check prerequisites for a quest. */
	bool ArePrerequisitesMet(const FMOQuestDefinitionRow& Quest) const;

	/** Process event matching against active quest objectives. */
	void ProcessEventForObjectives(EMOObjectiveType Type, FName TargetId, int32 Count = 1);

	/** Update objective progress. */
	void UpdateObjectiveProgress(FMOQuestState& State, const FMOQuestObjective& Objective, int32 Count);

	/** Check if all required objectives are complete. */
	void CheckQuestCompletion(FMOQuestState& State);

	/** Auto-start quests with bAutoStart when prerequisites are met. */
	void CheckAutoStartQuests();

	/** Get the current active objective for sequential quests. */
	const FMOQuestObjective* GetCurrentSequentialObjective(const FMOQuestState& State, const FMOQuestDefinitionRow& Quest) const;

	/**
	 * Rebuild ActiveEventListeners from scratch by walking all active
	 * quests' incomplete Event-type objectives. Call after any change that
	 * affects which events have listeners (quest start/complete/abandon,
	 * objective progress, load/reset).
	 */
	void RebuildActiveEventListeners();

private:
	/** Cached quest definitions by QuestId. */
	UPROPERTY()
	TMap<FName, FMOQuestDefinitionRow> QuestDefinitions;

	/** Currently active quest states. */
	UPROPERTY()
	TMap<FName, FMOQuestState> ActiveQuests;

	/** Set of completed quest IDs. */
	UPROPERTY()
	TSet<FName> CompletedQuests;

	/** Delegate handle for world initialization. */
	FDelegateHandle WorldInitializedHandle;

	/** Weak reference to bound crafting subsystem. */
	TWeakObjectPtr<UMOCraftingSubsystem> BoundCraftingSubsystem;

	/**
	 * Whether tutorial popups are currently allowed. Toggleable at runtime
	 * via SetTutorialPopupsEnabled (e.g., F3 binding). Default true.
	 * Underlying quest progress is unaffected when disabled — only the
	 * popup widget's visibility.
	 */
	UPROPERTY(Transient)
	bool bTutorialPopupsEnabled = true;

	/**
	 * Set true at the end of LoadQuestDefinitions, immediately before
	 * OnQuestSystemReady fires. IsReady() reads this. Stays true for the
	 * subsystem's lifetime (the GameInstance subsystem doesn't reset).
	 */
	UPROPERTY(Transient)
	bool bIsReady = false;

	/**
	 * Set of Event names that at least one active quest is currently
	 * listening for. High-frequency event broadcasters (per-tick move /
	 * look) check this before calling FireGameEvent so they no-op when no
	 * quest is interested. Rebuilt on every quest state change.
	 */
	TSet<FName> ActiveEventListeners;
};
