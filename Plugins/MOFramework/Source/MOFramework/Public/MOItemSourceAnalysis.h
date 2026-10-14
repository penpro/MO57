#pragma once

#include "CoreMinimal.h"

class UWorld;

/** A recipe the analysis could not reach, with the first gate that stops it. */
struct MOFRAMEWORK_API FMOBlockedRecipe
{
	FName RecipeId;
	FString Reason;
};

/** Result of FMOItemSourceAnalysis::Analyze. */
struct MOFRAMEWORK_API FMOItemSourceReport
{
	/** Every obtainable item -> the first source found for it ("resource BlackLocust/StripBark", "recipe ShredBark", ...). */
	TMap<FName, FString> SourceOf;

	/** Recipes no player can ever perform, each with the first gate that stops it. */
	TArray<FMOBlockedRecipe> BlockedRecipes;

	/** Root causes: items some recipe consumes (ingredient / build part / station fuel) that NOTHING produces. */
	TArray<FName> OrphanItems;

	/** Knowledge ids some recipe needs that no inspection grant, recipe XP or harvest XP can ever level. */
	TArray<FName> OrphanKnowledge;

	int32 RecipeCount = 0;
	int32 ReachableRecipeCount = 0;

	/** Ground pickups found in PCG graphs (item ids) -- reported so a scatter change that drops one is visible. */
	TArray<FName> PickupItems;
};

/**
 * What a player can actually obtain, from the data alone -- the "every ingredient has a producer" check (#data-sources).
 *
 * Sources: resource harvest actions (DT_Resources; tool + knowledge gated as the harvest menu gates them), harvest recipes
 * whose target tag some resource carries, carcass parts (any DataTable of FMOCarcassDefinitionRow), the world's foraging dig
 * table, and ground pickups (every project PCG graph's MO Mesh Spawner / MO Item Spawner item lists; stock weighted mesh
 * spawners in graphs that run the MO HISM Tagger, mapped to items by WorldVisual.StaticMesh as the tagger maps them; biome
 * species tagged MOItem_<Id>). Whether a level actually runs a graph is not checked.
 *
 * Recipes become reachable in a fixpoint, gated as UMOCraftingSubsystem gates them: ingredients / build parts obtainable,
 * station built (and fuelled when it needs fuel), required tools held at the minimum quality, discovery knowledge reachable at
 * the discovery level, required knowledge reachable. Knowledge (an id that is not a DT_Skills row) is reachable when an
 * obtainable item's inspection grants it (up to the grant's MaxLevel), or a reachable recipe / harvest action trains it.
 * So chains resolve: inspect Bark01 -> BarkFiber -> StripBark + ShredBark -> PlantFiber01 -> inspect -> TwistCordage -> ...
 *
 * Deliberately modelled as the runtime enforces it TODAY, not as intended: carcass parts are not tool-gated (the tool check
 * in MOCarcassActor is commented out), DT_Skills levels are assumed trainable, every DT_Resources row is assumed placed
 * somewhere in the world, and the dev starter kit is not a source.
 */
struct MOFRAMEWORK_API FMOItemSourceAnalysis
{
	/** World is optional: with one, its UMOForagingSubsystem dig table counts as a source. */
	static FMOItemSourceReport Analyze(UWorld* World);
};
