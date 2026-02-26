/**
 * =============================================================================
 * MOCreatureTypes.h - Creature System Type Definitions
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Defines enums and structs for the creature AI system. Activity states drive
 * both behavior tree decisions and animation state machine transitions.
 *
 * ACTIVITY STATE MACHINE:
 *   Active ←→ Resting ←→ Sleeping
 *     ↓           ↓          ↓
 *   Fleeing ←── threat detected
 *     ↓
 *   Fighting (predators only)
 *     ↓
 *   Dead (terminal)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] ANIMATION SYNC: Activity state must sync to Animation Blueprint.
 *   Set blackboard key AND ABP variable when changing state.
 *
 * [2024-02] SLEEPING AWARENESS: Sleeping creatures have reduced perception.
 *   Takes longer to detect threats and wake up.
 *
 * [2024-02] DEAD IS FINAL: Once Dead, creature should not transition to other
 *   states. Check before any state change.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOCreature.h - Uses these types
 * - BTService_CreatureActivity.h - Updates activity state
 * - ABP_Creature (Blueprint) - Animation state machine
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOCreatureTypes.generated.h"

/**
 * Activity states for creature AI.
 * See file header for state machine diagram.
 */
UENUM(BlueprintType)
enum class EMOCreatureActivityState : uint8
{
	/** Normal activity - wandering, foraging, etc. */
	Active		UMETA(DisplayName = "Active"),

	/** Resting but alert - can quickly return to active. */
	Resting		UMETA(DisplayName = "Resting"),

	/** Sleeping - reduced awareness, slower to react. */
	Sleeping	UMETA(DisplayName = "Sleeping"),

	/** Fleeing from threat. */
	Fleeing		UMETA(DisplayName = "Fleeing"),

	/** Engaged in combat (predators). */
	Fighting	UMETA(DisplayName = "Fighting"),

	/** Dead - final state. */
	Dead		UMETA(DisplayName = "Dead")
};
