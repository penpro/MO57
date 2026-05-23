/**
 * =============================================================================
 * MOQuestDelegates.h - Quest System Delegate Declarations
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Centralized delegate declarations for the quest system. Includes quest
 * lifecycle events, objective updates, and game event delegates.
 *
 * DELEGATE CATEGORIES:
 * - Quest Lifecycle: OnQuestStarted, OnQuestCompleted, OnQuestAbandoned
 * - Objectives: OnObjectiveUpdated, OnObjectiveCompleted
 * - Game Events: OnGameEvent, OnItemEvent, OnSkillEvent, OnLocationEvent
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] UHT DUMMY: FMOQuestDelegatesUHTDummy exists because file-scope
 *   DECLARE_DYNAMIC_MULTICAST_DELEGATE requires at least one USTRUCT/UCLASS.
 *
 * [2024-02] BROADCAST VS CALL: Quest subsystem broadcasts delegates. Components
 *   call HandleXxx() methods directly on the subsystem (not via delegate).
 *
 * =============================================================================
 * RELATED FILES: MOQuestSubsystem.h, MOQuestTypes.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOQuestDelegates.generated.h"

/**
 * Dummy struct to ensure UHT processes this header.
 * Required because DECLARE_DYNAMIC_MULTICAST_DELEGATE at file scope
 * needs at least one USTRUCT/UCLASS/UENUM for UHT processing.
 */
USTRUCT()
struct FMOQuestDelegatesUHTDummy
{
	GENERATED_BODY()
};

// ============================================================================
// QUEST LIFECYCLE DELEGATES
// ============================================================================

/** Broadcast when a quest is started. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnQuestStarted, FName, QuestId);

/** Broadcast when a quest is completed (all required objectives done). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnQuestCompleted, FName, QuestId);

/** Broadcast when a quest is abandoned by the player. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnQuestAbandoned, FName, QuestId);

// ============================================================================
// OBJECTIVE DELEGATES
// ============================================================================

/** Broadcast when an objective's progress changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOOnObjectiveUpdated, FName, QuestId, FName, ObjectiveId, int32, NewProgress);

/** Broadcast when an objective is fully completed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnObjectiveCompleted, FName, QuestId, FName, ObjectiveId);

// ============================================================================
// GAME EVENT DELEGATES (for quest system to listen to)
// ============================================================================

/**
 * Generic game event delegate.
 * Fire this for custom events that quests can listen to.
 * Example: FireGameEvent("CraftingMenuOpened")
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnGameEvent, FName, EventName);

/**
 * Item-related event delegate.
 * Used for crafting, pickup, drop events.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnItemEvent, FName, ItemId, int32, Quantity);

/**
 * Skill-related event delegate.
 * Used for skill level changes.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnSkillEvent, FName, SkillId, int32, NewLevel);

/**
 * Location-related event delegate.
 * Used for entering/exiting locations.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnLocationEvent, FName, LocationTag);

// ============================================================================
// TUTORIAL DELEGATES
// ============================================================================

/**
 * Broadcast when the active tutorial hint changes — either a new tutorial
 * objective became current, the existing one progressed, completed, or
 * tutorial popups were toggled. Listeners query
 * UMOQuestSubsystem::GetActiveTutorialHint() to read the new state.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnTutorialHintChanged);

// ============================================================================
// REWARD DELEGATES
// ============================================================================

/**
 * Broadcast after a quest completes so subsystems can apply its rewards.
 * The subsystem doesn't apply rewards directly — that keeps it decoupled
 * from the XP/knowledge/inventory components.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnApplyQuestRewards, FName, QuestId);
