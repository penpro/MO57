/**
 * =============================================================================
 * MOPCGWaterMask.h - "Is this point in water?" for every PCG spawner (one chokepoint)
 * =============================================================================
 *
 * PURPOSE:
 * Nothing is planted, dropped or spawned in water: not trees on a lake bed, not grass in the sea shallows, not a stone tool under a river. The generator is the authority on where the water is (the sea, rivers,
 * ponds and sea-level lakes are terrain BELOW SeaLevelCm; an upland lake is an ellipse above it, FMOWorldGen::IsInUplandLakeZone), so this one struct asks it, and every PCG node that must respect water asks THIS:
 * MOPCGElevationBandsSettings (every spawner chain in the scatter graphs passes through one, so native Static Mesh Spawners, the MO item / resource / spawn-point nodes and anything a modder wires after it are
 * covered without touching the graphs) and MOPCGBiomeSpawnerSettings (its own all-in-one node).
 *
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * - The upland lakes were NOT excluded for the biome spawner and not at all for the other chains (found 2026-10-11 with MO.PCG.WaterAudit: 51,374 of 56,937 instances within 200 m of a lake were in it): the elevation bands
 *   only knew the sea plane (Z < 0), and upland lakes are ABOVE sea level.
 * - A point is a SURFACE point (voxel sampler / voxel projection), so its own Z is the ground height: the sea test needs no generator sample. If a graph ever feeds non-surface points, pass the real ground height.
 * - The mask is only active when the world has an active seed (a new game / a loaded one). Without one (the main menu, legacy noise mode) nothing is water and nothing is filtered.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOWorldGen.h"

class AActor;

struct MOFRAMEWORK_API FMOPCGWaterMask
{
	/** Inactive: nothing is water. */
	FMOPCGWaterMask() = default;

	/** The mask for these generator params and terrain seed. BankRadii: how far past an upland lake's shore (1.0) still counts as water; 1.05 is the bank's first metres. */
	FMOPCGWaterMask(const FMOWorldGenParamsRef& InParams, int32 InTerrainSeed, float InBankRadii = 1.05f)
		: Params(InParams), TerrainSeed(InTerrainSeed), BankRadii(InBankRadii), bActive(true)
	{
	}

	/** The mask of the world this actor lives in: the published generator params and the world's active terrain seed. Inactive when there is no active seed. Call on the game thread. */
	static FMOPCGWaterMask ForActor(const AActor* Actor, float InBankRadii = 1.05f);

	bool IsActive() const { return bActive; }

	/** Is a point standing on the ground at (X, Y) with ground height GroundZ in water: under the sea plane (+15 cm), or inside an upland lake (shore x BankRadii). */
	bool IsInWater(double X, double Y, double GroundZ) const;

	/**
	 * How much of "the water's edge" this DRY point is, 0..1: 1 at the waterline, falling linearly to 0 at MaxHeightCm above the local water surface (the sea plane, or the upland lake's own surface);
	 * 0 when the point is in water, too high above it, or not within MaxDistCm of any water (a coastal plain 3 km from the sea is not a shore). The sea / river / pond proximity is eight height samples at
	 * MaxDistCm; the upland lake's is its ellipse. Reeds, rushes and damp-ground ferns scale their density by this.
	 */
	float ShoreFactor(double X, double Y, double GroundZ, double MaxHeightCm, double MaxDistCm = 2500.0) const;

	/**
	 * A conservative, cheap answer to "could ANY shore be inside this XY box": an upland lake near it, or ground under the sea plane (the sea, a river, a pond) on a 10 m grid over the box grown by MarginCm.
	 * False means the whole box is dry land far from water, so a water-edge planter can skip it without sampling its points (a 64 m cell of the dense shore tier would otherwise cost thousands of height samples).
	 */
	bool MayHaveShore(double MinX, double MinY, double MaxX, double MaxY, double MarginCm) const;

	/**
	 * The biome (index into the params' biome list, INDEX_NONE if none) a point on the ground belongs to, the way the biome spawner decides it: the real DT_Biomes bands with the point's own height, the slope
	 * of its up vector and the generator's climate at its position. Lets any PCG node filter by biome (MOPCGElevationBandsSettings::AllowedBiomeIds). INDEX_NONE when the mask is inactive.
	 */
	int32 BiomeAt(double X, double Y, double GroundZ, float SlopeDeg) const;

	/** The index of a biome id in the params' list (INDEX_NONE if unknown or inactive). */
	int32 FindBiome(FName BiomeId) const;

private:
	TOptional<FMOWorldGenParamsRef> Params;
	int32 TerrainSeed = 0;
	float BankRadii = 1.05f;
	bool bActive = false;
};
