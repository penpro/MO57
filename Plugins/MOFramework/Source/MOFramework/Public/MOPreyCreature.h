/**
 * =============================================================================
 * MOPreyCreature.h - Prey Creature AI Base Class
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Base class for prey creatures that flee from threats by default.
 * Only fights when cornered (no escape route).
 *
 * BEHAVIOR PRIORITY:
 * 1. Flee when threat detected (FleeDetectionRange)
 * 2. Only fight when cornered (IsCornered() returns true)
 * 3. Wander/graze when idle
 *
 * EXAMPLES: deer, rabbit, boar (flees first, may fight if cornered)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] FLEE SPEED: FleeSpeedMultiplier applied to run speed, not walk.
 *   Total flee speed = RunSpeed * FleeSpeedMultiplier.
 *
 * [2024-02] SAFE DISTANCE: Creature stops fleeing when SafeDistance from
 *   threat is reached, not when threat is lost from sight.
 *
 * [2024-02] BEHAVIOR TREE: DefaultPreyBehaviorTree must be set in derived BP
 *   or creature will have no AI behavior.
 *
 * =============================================================================
 * RELATED FILES: MOCreature.h, MOPredatorCreature.h, MOCreatureDefinitionRow.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOCreature.h"
#include "MOPreyCreature.generated.h"

class UBehaviorTree;

/**
 * Prey creature - flees from threats by default.
 *
 * Behavior priority:
 * 1. Flee when threat detected
 * 2. Only fight when cornered (no escape route)
 * 3. Wander/graze when idle
 *
 * Examples: deer, rabbit, boar (flees first, may fight if cornered)
 */
UCLASS()
class MOFRAMEWORK_API AMOPreyCreature : public AMOCreature
{
	GENERATED_BODY()

public:
	AMOPreyCreature();

	// ============================================================================
	// PREY BEHAVIOR
	// ============================================================================

	/** Check if this creature should flee from a threat. */
	UFUNCTION(BlueprintPure, Category="MO|Creature|Prey")
	bool ShouldFleeFromThreat() const;

	/** Check if the creature is cornered (no escape routes). */
	UFUNCTION(BlueprintPure, Category="MO|Creature|Prey")
	bool IsCornered() const;

protected:
	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Speed multiplier when fleeing (applied to run speed). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Prey")
	float FleeSpeedMultiplier = 1.3f;

	/** Distance at which prey detects threats and starts fleeing (before being attacked). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Prey")
	float FleeDetectionRange = 800.f;

	/** Distance at which prey feels safe and stops fleeing. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Prey")
	float SafeDistance = 1500.f;

	/**
	 * Health fraction below which the placeholder IsCornered() check considers this
	 * prey cornered (fight-back trigger) while in attack range. Tunable per species
	 * (audit M2 -- was a bare 0.15f literal). Replaced by the EQS escape-route query
	 * when M3 lands.
	 */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Prey", meta=(ClampMin="0.0", ClampMax="1.0"))
	float CorneredHealthPercent = 0.15f;

	/** Default behavior tree for prey creatures. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Prey")
	TSoftObjectPtr<UBehaviorTree> DefaultPreyBehaviorTree;

	virtual void BeginPlay() override;
};
