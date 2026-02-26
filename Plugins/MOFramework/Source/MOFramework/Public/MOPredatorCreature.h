/**
 * =============================================================================
 * MOPredatorCreature.h - Predator Creature AI Base Class
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Base class for predator creatures that hunt and attack. Flees when
 * severely wounded. Supports pack behavior.
 *
 * BEHAVIOR PRIORITY:
 * 1. Flee when health < FleeHealthThreshold
 * 2. Attack when target is in range
 * 3. Chase target when detected
 * 4. Hunt (investigate last known position) when target is lost
 * 5. Patrol/wander when idle
 *
 * PACK BEHAVIOR:
 * - AlertPack() notifies nearby pack members within PackAlertRange
 * - IsInPack()/GetPackSize() check for allies within PackDetectionRange
 *
 * EXAMPLES: wolf, bear, big cat
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] PACK DETECTION: Only same-class predators count as pack members.
 *   FindPackMembers() uses Cast<AMOPredatorCreature>().
 *
 * [2024-02] AGGRO MEMORY: AggroMemoryDuration keeps predator aggressive after
 *   losing sight. Resets when target is re-acquired or timer expires.
 *
 * [2024-02] FLEE VS ATTACK: ShouldFlee() takes priority over ShouldAttack().
 *   Check order in behavior tree matters.
 *
 * =============================================================================
 * RELATED FILES: MOCreature.h, MOPreyCreature.h, MOCreatureDefinitionRow.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOCreature.h"
#include "MOPredatorCreature.generated.h"

class UBehaviorTree;

/**
 * Predator creature - hunts and attacks, flees when severely wounded.
 *
 * Behavior priority:
 * 1. Flee when health is critically low
 * 2. Attack when target is in range
 * 3. Chase target when detected
 * 4. Hunt (investigate last known position) when target is lost
 * 5. Patrol/wander when idle
 *
 * Examples: wolf, bear, big cat
 */
UCLASS()
class MOFRAMEWORK_API AMOPredatorCreature : public AMOCreature
{
	GENERATED_BODY()

public:
	AMOPredatorCreature();

	// ============================================================================
	// PACK BEHAVIOR
	// ============================================================================

	/** Alert nearby pack members of a threat. */
	UFUNCTION(BlueprintCallable, Category="MO|Creature|Predator")
	void AlertPack(AActor* Threat);

	/** Check if this predator is part of a pack (has nearby allies). */
	UFUNCTION(BlueprintPure, Category="MO|Creature|Predator")
	bool IsInPack() const;

	/** Get count of nearby pack members. */
	UFUNCTION(BlueprintPure, Category="MO|Creature|Predator")
	int32 GetPackSize() const;

	// ============================================================================
	// COMBAT BEHAVIOR
	// ============================================================================

	/** Check if this predator should attack (has target, not fleeing). */
	UFUNCTION(BlueprintPure, Category="MO|Creature|Predator")
	bool ShouldAttack() const;

	/** Check if this predator should flee (health below threshold). */
	UFUNCTION(BlueprintPure, Category="MO|Creature|Predator")
	bool ShouldFlee() const;

protected:
	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Range at which predator becomes aggressive. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Predator")
	float AggroRange = 1000.f;

	/** Health percentage at which predator switches to flee behavior. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Predator")
	float FleeHealthThreshold = 0.25f;

	/** Range to alert other pack members when threat detected. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Predator")
	float PackAlertRange = 1500.f;

	/** Range to consider other predators as pack members. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Predator")
	float PackDetectionRange = 2000.f;

	/** Time to stay aggressive after losing sight of target (seconds). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Predator")
	float AggroMemoryDuration = 10.f;

	/** Default behavior tree for predator creatures. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Predator")
	TSoftObjectPtr<UBehaviorTree> DefaultPredatorBehaviorTree;

	virtual void BeginPlay() override;

private:
	/** Find nearby pack members. */
	TArray<AMOPredatorCreature*> FindPackMembers() const;
};
