/**
 * =============================================================================
 * MOSpeciesPlacement.h - How a scattered species is placed: ONE policy for the spawner and the gallery
 * =============================================================================
 *
 * The biome spawner (MOPCGBiomeSpawnerSettings) plants species in the world; the species gallery (MOSpeciesGallery, `MO.Gallery`) lays every species
 * of a biome out on a test floor and drops the character on each. Both ask these functions, so the gallery tests exactly what the world gets:
 * the collision profile (resource type policy + the species' own Collision say), the burial depth, and what the pawn should experience.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOBiomeDefinitionRow.h"

struct FMOResourceNodeDefinitionRow;

/** What the character should experience with a placed species. */
enum class EMOSpeciesContact : uint8
{
	WalkThrough,   // grass, flowers, ferns, walk-through bushes: the character passes
	BlocksWalk,    // trees, saplings, solid bushes: the character is stopped (at the trunk / core), not lifted onto the canopy
	Standable      // rocks, boulders, logs, stumps: the character can stand ON it
};

struct MOFRAMEWORK_API FMOSpeciesPlacement
{
	/** The collision profile an instance is placed with: the resource type's policy (FMOResourceNodeDefinitionRow::GetPlacementCollisionProfile), DecorativeProfile without
	 *  a resource node, and the species' own Collision say over both (Passable / Solid). */
	static FName CollisionProfile(EMOSpeciesCollision SpeciesCollision, const FMOResourceNodeDefinitionRow* Resource, FName DecorativeProfile);

	/** Offset along the instance's own up axis (cm) the spawner buries it by: species aligned to the ground are first lifted so the mesh BOTTOM touches the surface,
	 *  then every species sinks SinkFraction of its scaled height (auto: 0.25 aligned, 0 upright) plus SinkMinCm. */
	static float BurialOffsetCm(const FMOBiomeSpeciesEntry& Species, const FBoxSphereBounds& MeshBounds, float Scale);

	/** bFaceDownhill: the instance rotation -- world up, the mesh's face (FaceYawDeg) turned toward the downhill direction of SurfaceNormal, plus JitterRad.
	 *  Flat ground has no downhill: FallbackYawRad. */
	static FQuat FaceDownhillRotation(const FMOBiomeSpeciesEntry& Species, const FVector& SurfaceNormal, float JitterRad, float FallbackYawRad);

	/** bFaceDownhill: how much lower the ground lies under the mesh's face edge than at its pivot (cm) on a slope of SlopeRad -- the extra burial that seats the
	 *  front (0 for other species and on flat ground). */
	static float FaceDropCm(const FMOBiomeSpeciesEntry& Species, const FBoxSphereBounds& MeshBounds, float Scale, float SlopeRad);

	/** True for profiles that stop the character. */
	static bool ProfileBlocks(FName Profile);

	/** What the character should experience: nothing blocking = WalkThrough; a blocking tree / bush / upright species = BlocksWalk; any other blocking species = Standable. */
	static EMOSpeciesContact ExpectedContact(const FMOBiomeSpeciesEntry& Species, const FMOResourceNodeDefinitionRow* Resource, FName Profile);

	static const TCHAR* ContactName(EMOSpeciesContact Contact);
};
