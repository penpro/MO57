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

	/** Default behavior tree for prey creatures. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Prey")
	TSoftObjectPtr<UBehaviorTree> DefaultPreyBehaviorTree;

	virtual void BeginPlay() override;
};
