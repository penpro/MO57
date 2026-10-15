// Drinking straight from natural water: the action every drinker uses (the player through ServerDrinkWater, a survivor pawn's job directly). Server side.
// Docs/Water_And_Illness.md.

#pragma once

#include "CoreMinimal.h"
#include "MOWorldGen.h"

class APawn;

/** Why a drink did not start (the player is told). */
enum class EMODrinkRefusal : uint8
{
	None,
	NoPawn,
	NoWater,
	TooFar,
	Busy
};

struct MOFRAMEWORK_API FMOWaterActions
{
	/**
	 * Start drinking a draught from the water at Point (kneel, scoop with the hands): a timed action on the pawn (UMOTimedActionComponent). On completion the draught goes
	 * through UMOSurvivalStatsComponent::IngestWater (fresh water quenches; sea water costs water and loads sodium) and the source's untreated illness chance is rolled
	 * (UMOAnatomyComponent::ExposeToCondition). The kind of water is the SERVER's own reading of the world at Point. Returns None if it started.
	 */
	static EMODrinkRefusal BeginDrink(APawn* Drinker, const FVector& Point);

	/** The kind of water a drinker could reach at Point (None if out of reach or not water). */
	static EMOWaterBodyKind ReachableWaterAt(const APawn* Drinker, const FVector& Point);
};
