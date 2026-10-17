// The terrain under placed compositions: our ground RISES to the ground a composition was authored on (FMOWorldFeatureGround), so a hillside set
// (Electric Dreams' cliff assembly: a cliff face with a 4 m plateau behind it) stands on a landform instead of floating over voids.
//
// Applied where the terrain height is made -- the MO Terrain voxel node (UMOTerrainFunctionLibrary::Compute) -- so the rendered ground, its
// collision, every LOD and every co-op machine agree; the composition grounding pass measures against it too. The sites are the world feature
// placer's own (FMOWorldFeaturePlacer over the full catalog, so priorities resolve exactly as at spawn time), found per 1 km tile on first use and
// kept: a pure function of (generator params, terrain seed, catalog). Any thread.

#pragma once

#include "CoreMinimal.h"
#include "MOWorldGen.h"
#include "MOWorldFeatureTypes.h"

struct MOFRAMEWORK_API FMOFeatureTerrain
{
	/** The feature catalog (every row: priorities are resolved across all of them). Game thread, before the voxel runtime generates; replaces any earlier one. */
	static void SetCatalog(const TArray<TPair<FName, FMOWorldFeatureDefinitionRow>>& Rows);

	/** True if some catalog row carries authored ground (otherwise Apply is a no-op). */
	static bool HasShapingRows();

	/** The terrain height at (X, Y): BaseHeightCm (the generator's), raised to the authored ground of every composition placed over it. Any thread. */
	static float Apply(const FMOWorldGenParamsRef& Params, int32 TerrainSeed, double X, double Y, float BaseHeightCm);

	/** Where a site member's composition actor stands: what UMOWorldFeatureSubsystem spawns it at, and the frame its authored ground is in. */
	static FTransform MemberRootTransform(const FMOWorldFeatureDefinitionRow& Row, const FMOWorldFeatureMember& Member);
};
