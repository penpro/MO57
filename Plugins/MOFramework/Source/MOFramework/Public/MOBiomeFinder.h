#pragma once

#include "CoreMinimal.h"
#include "MOWorldGen.h"
#include "MOPCGWaterMask.h"

/**
 * Where a biome IS, from the world generator alone (no streaming, no PCG): the `biome` dev teleport and MO.WorldGen.FindBiomeSpot ask this.
 * A biome is the generator's hard answer (FMOWorldGen::ResolveBiome with the generator's slope), so a "spot" is the middle of a biome, not its soft edge.
 * Pure and deterministic; game thread or worker. Cost: ~25 us a coarse sample, ~0.1 ms a full one (SampleColumn + four slope taps).
 */
struct MOFRAMEWORK_API FMOBiomeFinder
{
	FMOBiomeFinder(const FMOWorldGenParamsRef& InParams, int32 InTerrainSeed);

	/** The biome at (X, Y) with the generator's slope there (index into Params->Biomes, INDEX_NONE if none); OutGround = generator ground. */
	int32 BiomeAt(double X, double Y, float& OutGround) const;

	/**
	 * The middle of BiomeIndex and somewhere to stand: dry land of that biome whose ring 35 m out is the same biome and dry -- and, unless bRough, within
	 * 3.5 m of this height (flat enough to land on). OutGround = generator ground at (X, Y).
	 */
	bool IsDeepInside(double X, double Y, int32 BiomeIndex, bool bRough, float& OutGround) const;

	/**
	 * The nearest IsDeepInside spot of BiomeIndex to From: square rings of CoarseStepCm outward to MaxRadiusCm (clamped to the world), a cheap biome test per
	 * coarse sample, then a 100 m grid around each hit, nearest-to-From first. OutSpot.Z = generator ground. OutSamples (optional) = coarse samples taken.
	 */
	bool FindNearest(const FVector2D& From, int32 BiomeIndex, double MaxRadiusCm, bool bRough, FVector& OutSpot, int32* OutSamples = nullptr, double CoarseStepCm = 60000.0) const;

private:
	FMOWorldGenParamsRef Params;
	int32 TerrainSeed = 0;
	FMOPCGWaterMask Water;

	/** True when BiomeIndex only applies on slopes (a cliff / scree band): the cheap slope-0 coarse test can never see it. */
	bool IsSlopeBanded(int32 BiomeIndex) const;
};
