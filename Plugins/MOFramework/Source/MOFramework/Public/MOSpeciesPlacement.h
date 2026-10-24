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

	/** bLieOnSlope: the slab laid on the slope -- its face (FaceYawDeg in the mesh's XY) along SurfaceNormal, its up along the slope's uphill line, turned
	 *  JitterRad about the normal. Flat ground has no uphill: FallbackYawRad picks the slab's direction. */
	static FQuat LieOnSlopeRotation(const FMOBiomeSpeciesEntry& Species, const FVector& SurfaceNormal, float JitterRad, float FallbackYawRad);

	/** bLieOnSlope: the offset ALONG THE SURFACE NORMAL (cm, negative = into the ground) that leaves LieShowFraction of the slab's thickness above it. */
	static float LieOffsetCm(const FMOBiomeSpeciesEntry& Species, const FBoxSphereBounds& MeshBounds, float Scale);

	/** The ground's departure from its best plane under a footprint: Heights are 8 samples evenly round the footprint's edge (index K at angle
	 *  K x 45 deg), CentreHeight the ground at its centre. Pure (MaxGroundReliefFraction compares it to the mesh's thickness). */
	static float GroundReliefCm(const float (&Heights)[8], float CentreHeight);

	/** An upright plant on a slope (BaseRadiusCm > 0): how much deeper it goes so the downhill edge of its base meets the ground -- the base's radius at its
	 *  width (WidthCm: scale x WidthScale) times the slope's tangent (capped at 50 deg). 0 for rocks, on flat ground, or with no base radius. */
	static float SlopeDropCm(const FMOBiomeSpeciesEntry& Species, float Width, float SlopeRad);

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
