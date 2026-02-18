#pragma once

#include "CoreMinimal.h"
#include "MOCreatureTypes.generated.h"

/**
 * Activity states for creature AI.
 * Controls both behavior tree logic and animation states.
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
