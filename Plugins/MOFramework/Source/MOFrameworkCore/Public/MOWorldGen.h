/**
 * =============================================================================
 * MOWorldGen.h - Pure terrain / climate / biome generator (no Voxel, no UObject state)
 * =============================================================================
 *
 * PURPOSE:
 * One deterministic function library that answers, for any world position and seed:
 * what is the climate here, how high is the ground, which biome is it. The Voxel
 * graph node (UMOTerrainFunctionLibrary), the PCG biome spawner, tools, tests and the
 * PNG map dump all call THESE functions, so terrain, materials and vegetation can
 * never drift apart (the old situation: the spawner ran its own noise over the
 * terrain's height/slope).
 *
 * Design + rationale: Docs/Terrain_Generation_Exploration.md.
 *
 * =============================================================================
 * RULES (load-bearing -- a violation silently desyncs co-op worlds)
 * =============================================================================
 *
 * 1. PURE + DETERMINISTIC. Co-op clients generate their OWN terrain from the
 *    replicated seed, so two machines must produce bit-identical samples.
 *    Only IEEE-exact operations are used in the hot path (+ - * / sqrt floor abs
 *    min/max), integer hashing, and LITERAL constant tables. NO sin/cos/pow/exp/log
 *    (CRT versions are CPU-dispatched on Windows), NO runtime SIMD dispatch (do not
 *    port this to ISPC without pinning one target -- Voxel's own ISPC dispatches
 *    avx512/avx2/avx/sse4 at runtime).
 * 2. THREAD-SAFE. Voxel evaluates on worker threads. Everything here reads an
 *    immutable FMOWorldGenParams snapshot; nothing touches UObjects or DataTables.
 * 4. FLOATING-POINT SEMANTICS ARE PART OF THE CONTRACT. MOFrameworkCore.Build.cs pins FPSemantics = Precise for this module in every target; without it the packaged game
 *    compiled this file with /fp:fast (the editor already used /fp:precise) and produced DIFFERENT bits on the same CPU (found by MO.WorldGen.Checksum, 2026-10-10).
 *    After ANY change to the build rules or toolchain, run MO.WorldGen.Checksum in the editor AND in a packaged build: both must print match=1.
 * 3. CHANGING THE MATH CHANGES EVERY SAVED WORLD's base terrain. Bump
 *    FMOWorldGenParams::GeneratorVersion and update the golden checksum test
 *    (MOFramework.WorldGen.GoldenChecksum) deliberately, in the same commit.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-10] BIOME vs SLOPE: SampleColumn resolves the biome with slope = 0 (the
 *   grid-independent node cannot afford neighbour samples). Slope-banded biomes
 *   (RockyHighland) are therefore only resolved where the caller knows the slope --
 *   the PCG spawner, which has the real surface point. Terrain materials by slope
 *   belong in the voxel material graph (it has the true vertex normal).
 * [2026-10-10] v4 CHANGED THE LAND EVERYWHERE: VerticalScale = 2 doubles every height delta, coastal ranges rise from the shore, upland lakes (tier 2) sit above sea level with their own
 *   water surface (UMOUplandLakeSubsystem), sinkholes keep out of lakes. Set VerticalScale = 1 and CoastalRangeAmplitudeM = 0 for the v1-v3 terrain bit for bit (the NoWater probe pins it).
 * [2026-10-10] v5 CHANGED THE LAND AGAIN: mountains are 3.75x taller (MountainAmplitudeM 1200) and the biome decides how much relief its climate gets (ReliefScale: prairie ~0.25), the four regional
 *   fields are half size, and the continent is pulled to ocean near the world border (EdgeFalloff*), so land never meets the boundary. To reproduce the v1-v3 base terrain bit for bit (the NoWater
 *   probe) set VerticalScale 1, CoastalRangeAmplitudeM 0, MountainAmplitudeM 320 / MountainWavelengthCm 800000, the four wavelengths to 900000 / 600000 / 700000 / 1500000 and EdgeFalloffEnd <= Start.
 * [2026-10] LOD: pass the sample spacing (cm) so octaves finer than 2x spacing are
 *   skipped; without it coarse LODs alias and cost the same as LOD 0.
 *
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "MOWorldGen.generated.h"

/** What open water at a point is (FMOWorldGen::ClassifyWaterBody): its kind decides what drinking it does (salt, how likely to make you ill). */
UENUM(BlueprintType)
enum class EMOWaterBodyKind : uint8
{
	None,
	/** The ocean: land whose own height is below sea level. Salt. */
	Sea,
	/** A river channel carved below the sea plane. Fresh, flowing. */
	River,
	/** A small sea-level basin (tier 0). Fresh, still, small. */
	Pond,
	/** A sea-level lake basin (tier 1). Fresh, still. */
	Lake,
	/** A lake above sea level with its own surface (tier 2). Fresh, still. */
	UplandLake
};

/**
 * Designer-facing tuning. Lives in UMOWorldGenSettings (Project Settings) and is copied into the
 * immutable snapshot. All lengths are cm (UU) unless the name says otherwise.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORKCORE_API FMOWorldGenTuning
{
	GENERATED_BODY()

	FMOWorldGenTuning();

	// --- World frame ---------------------------------------------------------------------------

	/**
	 * The generated world covers +-this around the origin (cm). Outside it the height stamp does nothing. 6,000,000 = 60 km each way (a 120 km square); it was 30 km until 2026-10-10 ("make the voxel
	 * world about 4 times bigger, 2x the edge length"). The island edge falloff is relative to this, so the whole landmass grows with it; the PCG volumes of the levels must cover it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World", meta=(ClampMin="100000"))
	float WorldHalfSizeCm = 6000000.0f;

	/** World Z of sea level (cm). The continental spline is relative to this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World")
	float SeaLevelCm = 0.0f;

	/** Must match AVoxelWorld::VoxelSize (cm). Sample spacing at LOD n is VoxelSizeCm * 2^n, which decides which octaves are skipped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World", meta=(ClampMin="1"))
	float VoxelSizeCm = 50.0f;

	// --- Large-scale fields (wavelength of the lowest octave, cm) --------------------------------

	/** Continentalness: ocean/lowland <-> inland highland. The biggest structure in the world. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fields", meta=(ClampMin="10000"))
	float ContinentWavelengthCm = 3000000.0f;

	/**
	 * The four REGIONAL fields below set how big a biome / a stretch of mountains is. v5 halved all of them (requested 2026-10-10: "biomes are very realistic but for play testing
	 * we need to explore more of them"): a biome patch is now about 3.5 km across instead of 7 km. The continent (landmass) wavelength above is unchanged.
	 */

	/** Erosion: flat plains <-> rugged. Decides WHERE mountains may rise. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fields", meta=(ClampMin="10000"))
	float ErosionWavelengthCm = 450000.0f;

	/** Weirdness (folded into peaks-and-valleys): modulates mountain height inside mountainous regions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fields", meta=(ClampMin="10000"))
	float WeirdnessWavelengthCm = 300000.0f;

	/** Moisture and temperature decide the biome: halved AGAIN on 2026-10-10 ("make biomes about half the size again so I can test different biomes"), 3.5 km -> ~1.75 km per patch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fields", meta=(ClampMin="10000"))
	float MoistureWavelengthCm = 175000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fields", meta=(ClampMin="10000"))
	float TemperatureWavelengthCm = 375000.0f;

	/** Contrast applied to continentalness/erosion/weirdness (gradient noise rarely leaves +-0.5; the splines want the full -1..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fields", meta=(ClampMin="1"))
	float FieldContrast = 1.9f;

	/** Domain warp: positions are displaced by low-frequency noise before every field is sampled (organic, non-grid shapes). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fields", meta=(ClampMin="0"))
	float WarpStrengthCm = 70000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fields", meta=(ClampMin="10000"))
	float WarpWavelengthCm = 500000.0f;

	// --- Height shaping --------------------------------------------------------------------------

	/**
	 * Peak relief of mountain ranges at full ruggedness (metres, before VerticalScale and the biome's ReliefScale). v5: 1000 (was 320): "if there's mountains they need to be many
	 * times higher". With VerticalScale 2 a range stands up to ~2 km over its foothills while a prairie (ReliefScale ~0.25) stays gently rolling.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(ClampMin="0"))
	float MountainAmplitudeM = 1000.0f;

	/** Lowest-octave wavelength of the ranges (cm). Wider than v4's 8 km so the taller ranges keep walkable slopes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(ClampMin="1000"))
	float MountainWavelengthCm = 1000000.0f;

	/**
	 * Amplitude ratio between successive octaves of the ranges. At 0.5 every octave contributes the SAME slope (amplitude halves as frequency doubles), so six octaves of a tall range
	 * add up to near-vertical cliffs; 0.38 lets the finer octaves fade (total slope -40%) and keeps the big shapes. 0.5 is the v1-v4 value.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(ClampMin="0.2", ClampMax="0.7"))
	float MountainGain = 0.38f;

	// --- v6 mountain ranges (2026-10-11: "the mountains are super fucked looking ... larger masses with 1000 small steep features, not 1000 really tall skinny mountains") -----------
	//
	// WHY v5 LOOKED LIKE THAT (found with a numpy replica of this generator that reproduced the real test numbers): every WAVELENGTH here is in cm, and the fields that decided WHERE mountains
	// stand (erosion 4.5 km, weirdness 3 km) and the biome's mountain / hill scale (looked up from the LOCAL moisture, 1.75 km wavelength, blended over 0.12 climate units = ~150 m) were
	// all km-scale, while the ridge noise inside has a 10 km wavelength. So the mountain mask went 0 -> 1 over ~600 m and the biome scale 0.03 -> 1 over ~150 m: every km-sized patch
	// was a full-height tower with vertical walls (p99 slope 78 degrees, 3.4% of the land steeper than 60). At a coarse LOD (128 m step) 6% of adjacent samples were steeper than 45 degrees
	// (max 84): a wall the mesher cannot represent, which is the shards and holes in the distant mountains. The ridged noise itself, at full strength, is just rolling dunes.
	// v6 puts the placement at RANGE scale (tens of km, wide soft ramps), looks the biome relief up from a REGIONAL climate, and builds the mountain itself from layers:
	// a broad massif (the mass: it survives every LOD), ridged peaks with ROUNDED crests on top of it, and erosion gullies cut down the steep flanks (the fine, steep, small-scale detail:
	// culled at the LODs that cannot resolve it, so it can never collapse into points or empty cells). false = the v1-v5 mountain math, bit for bit (the water-off probe anchor).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height")
	bool bRangeMountains = true;

	/** Where the ranges stand: a smoothstep (Low..High) of a 2-octave regional field of this wavelength (cm) times the gain, times the inland factor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains", ClampMin="500000"))
	float MountainRegionWavelengthCm = 3000000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains"))
	float MountainRegionLow = -0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains"))
	float MountainRegionHigh = 0.33f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains", ClampMin="0.1"))
	float MountainRegionGain = 1.6f;

	/** The biome's hill / mountain scale is looked up from a climate of this wavelength (cm), not the local one: a biome patch is ~2-3 km, a range is tens of km. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains", ClampMin="500000"))
	float ReliefRegionWavelengthCm = 3000000.0f;

	/** The massif (metres before VerticalScale, wavelength cm): a smooth broad uplift, the part of a mountain that must never disappear at a coarse LOD. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains", ClampMin="0"))
	float MassifAmplitudeM = 515.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains", ClampMin="100000"))
	float MassifWavelengthCm = 1800000.0f;

	/** Ridged peaks standing on the massif (they need its support: little rises from the plain). Crest rounding 0 = a knife edge, 0.22 = a rounded crest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains", ClampMin="0"))
	float PeakAmplitudeM = 580.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains", ClampMin="50000"))
	float PeakWavelengthCm = 420000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains", ClampMin="1", ClampMax="8"))
	int32 PeakOctaves = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains", ClampMin="0.2", ClampMax="0.8"))
	float PeakGain = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains", ClampMin="0", ClampMax="0.6"))
	float PeakSoftness = 0.22f;

	/**
	 * Erosion gullies: ridged valley lines (wavelength cm, depth metres before VerticalScale) cut where the massif + peaks are steep (slope Low..High, rise over run). 0 = none. They fade out over
	 * the LODs (full when the LOD resolves them 6x over, gone at 1x) instead of vanishing at once: a hard cut made the surface jump by the gully depth between two neighbouring LODs, which a
	 * seam between chunks of those LODs shows as a crack. Measured with a numpy replica of the generator: 70 m gullies cut hard = 95 m jump at the 512 m LOD; 25 m, faded = ~15 m.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains", ClampMin="0"))
	float GullyAmplitudeM = 15.0f;   // v8: 25 -> 15 (playtest 2026-10-14 #28 "smooth out the mountain detail features")

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains", ClampMin="10000"))
	float GullyWavelengthCm = 60000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains", ClampMin="1", ClampMax="5"))
	int32 GullyOctaves = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains", ClampMin="0"))
	float GullySlopeLow = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains", ClampMin="0.05"))
	float GullySlopeHigh = 0.55f;

	/**
	 * Cliff bands (v8; "crags" before): the STEEP flanks (slope Low..High) are terraced into steps of CragAmplitudeM metres (before VerticalScale) whose risers always face DOWNHILL,
	 * in patches of this wavelength (Edge = how much of the steep ground gets them) with tilted band levels. Faded out over the LODs like the gullies. 0 = none.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains", ClampMin="0"))
	float CragAmplitudeM = 9.0f;   // v8 riser height (x VerticalScale in the world: ~18 m)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains", ClampMin="5000"))
	float CragWavelengthCm = 15000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains", ClampMin="0"))
	float CragSlopeLow = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains", ClampMin="0.05"))
	float CragSlopeHigh = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="bRangeMountains", ClampMin="0.1", ClampMax="0.9"))
	float CragEdge = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(ClampMin="0"))
	float HillAmplitudeM = 26.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(ClampMin="1000"))
	float HillWavelengthCm = 150000.0f;

	/**
	 * Coastal ranges: ridged hills and mountains that rise from the shore and fade out inland (where the continental mountains take over). They exist so that a view ACROSS
	 * the water ends in a real skyline -- without them every coast is a lowland beach (measured: only ~1 world in 5 had any ground 1 degree above the horizon across open
	 * water). Peak height in metres at full presence (before VerticalScale); 0 = off, which together with VerticalScale = 1 reproduces the v1-v3 terrain bit for bit.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(ClampMin="0"))
	float CoastalRangeAmplitudeM = 200.0f;

	/** Lowest-octave wavelength of the ranges (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="CoastalRangeAmplitudeM > 0", ClampMin="10000"))
	float CoastalRangeWavelengthCm = 500000.0f;

	/** Size of the stretches of coast that have ranges / have none (cm): a slow noise field decides where they stand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(EditCondition="CoastalRangeAmplitudeM > 0", ClampMin="100000"))
	float CoastalRangePresenceWavelengthCm = 1200000.0f;

	/** Metre-scale roughness so the ground is not billiard-smooth. Skipped at coarse LODs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(ClampMin="0"))
	float DetailAmplitudeM = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(ClampMin="100"))
	float DetailWavelengthCm = 3500.0f;

	/**
	 * Multiplies the whole height sum (continents + mountains + coastal ranges + hills + detail), above AND below sea level, so every height delta in the world scales together.
	 * 1 = the realistic proportions of generator v1-v3 (very flat for a game); 2 = double the relief (requested 2026-10-10: "the terrain is very very flat, which is fairly
	 * realistic but not very fun for a game"). Absolute thresholds elsewhere (river headwaters, lake rims, cave depths) stay in real metres.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(ClampMin="0.1"))
	float VerticalScale = 2.0f;

	/**
	 * Per-biome relief (FMOWorldGenBiome::ReliefScale, DT_Biomes column `reliefScale`) multiplies the hills, mountains and coastal ranges, never the continent spline or the metre-scale
	 * detail. The biome that applies is chosen from the CLIMATE alone (moisture + sea-level temperature, the same bands the biome has), as a soft first-match-wins over the biomes
	 * that have no height or slope band: the height-banded ones (shore, alpine, rocky) are CONSEQUENCES of the terrain and cannot also shape it. This is how far outside a
	 * biome's climate band its relief still applies (climate units, 0..1) -- the ramp that keeps a prairie from ending at a cliff.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(ClampMin="0"))
	float ReliefClimateBlend = 0.12f;

	// --- Ground-surface transitions (looks only: NOT in the world hash) --------------------------
	// Every biome split of the ground material goes through FMOWorldGen::SoftThreshold (Wes, 2026-10-12: "every split should have a blend band AND a noise term that breaks
	// the edge up"). Before, the surface switched at the exact band edge: a hard line at every height / climate threshold, stair-stepped by the voxel grid at a distance.

	/**
	 * Blend band of a biome HEIGHT edge: this fraction of the edge's height above sea level, between SurfaceHeightBlendMinCm and SurfaceHeightBlendCm. One fixed width
	 * cannot serve both ends: 40 m suits the alpine edges (AlpineRock above 950 m) but spread the shore biomes (which end 4 m above the sea) over kilometres of a flat
	 * coastal plain. Shore edge -> 1.2 m band (~60 m across a 2 % beach), AlpineHeath (320 m) -> 26 m, AlpineRock -> 40 m.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surfaces", meta=(ClampMin="0", ClampMax="1"))
	float SurfaceHeightBlendFraction = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surfaces", meta=(ClampMin="0"))
	float SurfaceHeightBlendMinCm = 120.0f;

	/** The widest height band (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surfaces", meta=(ClampMin="0"))
	float SurfaceHeightBlendCm = 4000.0f;

	/**
	 * Width of the blend band on every MOISTURE / TEMPERATURE edge (those fields run 0..1). 2026-10-14 (Wes: "the biomes should have a lot more blending"): 0.03 -> 0.12.
	 * Moisture crosses 0..1 in under a kilometre (1.75 km patches), so 0.03 was a 25-50 m seam; 0.12 is a ~100-200 m ecotone and still leaves the narrowest climate
	 * band (ElectricForest, moisture 0.24 wide) a pure core.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surfaces", meta=(ClampMin="0", ClampMax="0.3"))
	float SurfaceClimateBlend = 0.12f;

	/**
	 * How far the edge wanders, as a fraction of its band width on each side (deterministic 3-octave gradient noise): the band never follows a contour line.
	 * 0.6 -> 1.5 with the wider band: the edges interlock in fingers and leave islands of one biome inside its neighbour.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surfaces", meta=(ClampMin="0", ClampMax="2"))
	float SurfaceBreakup = 1.5f;

	/** Wavelength (cm) of the edge-breakup noise's first octave (two finer ones follow at 1/2.6 and 1/6.8 of it). 90 m -> 300 m with the wider band. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surfaces", meta=(ClampMin="100"))
	float SurfaceBreakupWavelengthCm = 30000.0f;

	/**
	 * A biome's hill scale below 1 also flattens the CONTINENT relief: the land above this height (metres, before VerticalScale) is pulled toward it by (1 - scale), so a prairie is a low
	 * plain, not a high plateau with a few small hills on it (the continent spline alone is ~130 m of change per 1.5 km, which no hill scale touches).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(ClampMin="0"))
	float LowlandCapM = 20.0f;

	/**
	 * The hill noise fades out over open sea: it is multiplied by a smoothstep of the continentalness between Low and High (the sea level crosses at -0.55). Without it the noise rode on
	 * the near-flat sea floor and made the shoreline meander far out into the water (reported 2026-10-10: "the shoreline meanders WAAAY out into the water"). High <= Low switches it off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height")
	float HillSeaFadeLow = -0.62f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height")
	float HillSeaFadeHigh = -0.42f;

	/**
	 * Island shaping: the continent field is pulled toward open ocean near the edge of the world so land always ends in water before the border (squarish distance, 0 = centre,
	 * 1 = the border). Land fades out between Start and End; End <= Start switches it off. v5 (requested 2026-10-10: "we never run into a world boundary on land").
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World", meta=(ClampMin="0"))
	float EdgeFalloffStart = 0.62f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World", meta=(ClampMin="0"))
	float EdgeFalloffEnd = 0.93f;

	/**
	 * Continentalness (-1..1) -> base elevation in metres above sea level (before VerticalScale). Sorted by X. Catmull-Rom interpolated. v5.1: the sea floor is deep (-80 m, x2 = -160 m)
	 * and drops away steeply from the shore (0 m at -0.55, -12 m at -0.58, -28 m at -0.62, -50 m at -0.70), so a coast has a defined edge and the ocean a real depth; v1-v5 had -6 m at the bottom.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height")
	TArray<FVector2D> ContinentSplineM;

	/** Erosion (-1..1) -> ruggedness multiplier (0 = flat, 1 = full mountains). Sorted by X. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height")
	TArray<FVector2D> ErosionReliefSpline;

	// --- Climate ---------------------------------------------------------------------------------

	/** Normalised temperature (0..1) lost per km of elevation. 0.16 ~ the real 6.5 C/km lapse rate over a 40 C span. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Climate", meta=(ClampMin="0"))
	float TemperatureLapsePerKm = 0.16f;

	/** Extra moisture near coasts / low continentalness (0..1 added at the coast). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Climate", meta=(ClampMin="0"))
	float CoastalMoistureBoost = 0.12f;

	/** North-south temperature gradient across the world (0 = none). +ve = warmer toward +Y. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Climate")
	float LatitudeTemperatureGradient = 0.0f;

	// --- Caves (carved out of the height terrain by a Subtractive volume stamp: see UMOTerrainFunctionLibrary::MOTerrainCaves) ---------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves")
	bool bEnableCaves = true;

	/** Solid rock kept above galleries and caverns (cm). A gallery breaches the ground only where it runs into a steep slope (an entrance, see
	 *  CaveEntranceSlopeMinDeg); sinkholes (off by default since v9) are the only vertical openings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="0"))
	float CaveCoverCm = 900.0f;

	/** Over this depth (cm) below the cover, tunnels taper in instead of starting abruptly. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="100"))
	float CaveFadeCm = 1600.0f;

	/** Below this depth under the ground (cm) there are no caves at all. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="2000"))
	float CaveMaxDepthCm = 14000.0f;

	/**
	 * No caves under ground higher than this (cm): they stay below where the mountain ranges rise. A cave (and its sinkhole mouth) under a summit leaves thin rock the far voxel LODs cannot represent, so the
	 * peak tears when seen from a distance (Wes, 2026-10-13). Tunnels / caverns / sinkholes taper out over CaveHighFadeCm below this ceiling (the field stays continuous). 0 = no ceiling.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="0"))
	float CaveMaxGroundCm = 75000.0f;

	/** Band (cm) below CaveMaxGroundCm over which caves taper out instead of ending at a contour. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="100"))
	float CaveHighFadeCm = 10000.0f;

	/** Ground lower than this above sea level (cm) is never undermined (coasts, basins, lake floors keep their roof); caves fade in over the next 6 m of height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves"))
	float CaveMinGroundAboveSeaCm = 300.0f;

	/**
	 * GALLERIES (v9; Wes, Oct 16 2026: "caves should be more or less horizontal so we don't just fall into them"): near-horizontal passages in stacked
	 * levels. In plan a level's passages follow where a 2D noise field is zero (meanders, loops, forks); their height follows a gentle long-wavelength
	 * field around the level's height, so a passage climbs or falls a few degrees at most. This is the meander's lattice cell (cm).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="2000"))
	float TunnelWavelengthCm = 22000.0f;

	/** Mean gallery radius (cm). v9 halved it (3.2 m wide, ~2.8 m high with the default flattening): a passage, not a hall. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="0"))
	float TunnelRadiusCm = 160.0f;

	/** Radius wobble along a gallery, 0..1 (0.25 = +-25%: never lower than a pawn can walk upright with the default flattening). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="0", ClampMax="0.8"))
	float TunnelRadiusVariation = 0.25f;

	/** Cross-section flattening: a gallery is TunnelRadiusCm wide and TunnelRadiusCm / this high (both halves). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="1", ClampMax="4"))
	float TunnelVerticalSquash = 1.15f;

	/** Vertical distance between gallery levels (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="1000"))
	float GalleryLevelSpacingCm = 3000.0f;

	/** How far a level's height wanders up and down (cm, peak). Keep under half the spacing: levels never cross. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="0"))
	float GalleryLevelAmplitudeCm = 1100.0f;

	/** Over what distance a level's height wanders (cm): with the amplitude, the steepest a gallery gets (MOFramework.WorldGen.GalleriesStayLevel). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="5000"))
	float GalleryLevelWavelengthCm = 40000.0f;

	/** Galleries exist where a mask field exceeds this (-1..1; lower = more of the meander network is open, its ends taper shut). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="-1", ClampMax="1"))
	float GalleryMaskThreshold = -0.2f;

	/** Galleries open to the surface only where the ground is at least this steep and the passage runs into the slope (a hillside entrance)... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="0", ClampMax="89"))
	float CaveEntranceSlopeMinDeg = 16.0f;

	/** ... fully from this slope on. Flatter ground keeps CaveCoverCm of rock over every passage: no hole to fall into. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="0", ClampMax="89"))
	float CaveEntranceSlopeFullDeg = 32.0f;

	/** Caverns: big chambers where a smooth 3D field exceeds CavernThreshold. Lattice cell size (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="4000"))
	float CavernWavelengthCm = 30000.0f;

	/** Field value above which a cavern exists (the field rarely exceeds 0.6). Higher = rarer, smaller. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="0.2", ClampMax="0.75"))
	float CavernThreshold = 0.45f;

	/** Caverns stay at least this deep under the ground (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="0"))
	float CavernMinDepthCm = 3500.0f;

	/** Sinkholes: shafts from the surface down to a chamber. One candidate per square cell of this edge length (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="10000"))
	float SinkholeCellCm = 30000.0f;

	/** Chance (0..1) that a cell holds a sinkhole. 0 since v9: a vertical shaft in flat ground is a hole to fall into (Wes, Oct 16 2026). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="0", ClampMax="1"))
	float SinkholeChance = 0.0f;

	/** Mean shaft radius (cm); each sinkhole varies +-30% and flares to ~1.8x at the rim. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="100"))
	float SinkholeRadiusCm = 550.0f;

	/** Mean shaft depth (cm); the chamber at the bottom is ~2.4x the shaft radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Caves", meta=(EditCondition="bEnableCaves", ClampMin="500"))
	float SinkholeDepthCm = 3800.0f;

	// --- Water: rivers and lakes are carved into low ground and sit AT SEA LEVEL; the water plane of the world (WaterBodyCustom at SeaLevelCm) fills them ----------
	// Everything below SeaLevelCm is water, so a channel or basin is just terrain dipping under sea level: no per-lake water actor, no flow simulation.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water")
	bool bEnableWater = true;

	/** Rivers follow the zero contour of a smooth noise field; this is the field lattice cell size (cm): bigger = longer, lazier rivers spaced further apart. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(EditCondition="bEnableWater", ClampMin="20000"))
	float RiverWavelengthCm = 260000.0f;

	/** River width at the water line (cm); varies +-30% along the river. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(EditCondition="bEnableWater", ClampMin="200"))
	float RiverWidthCm = 2400.0f;

	/** Depth of the river bed below sea level at the centre line (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(EditCondition="bEnableWater", ClampMin="30"))
	float RiverDepthCm = 130.0f;

	/** Bank rise per cm of horizontal distance beyond the water (0.16 ~ 9 degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(EditCondition="bEnableWater", ClampMin="0.02", ClampMax="1.0"))
	float RiverBankSlope = 0.16f;

	/** Rivers only cut where the original ground is at most this high above sea level (cm); over the next 6 m they fade out (the headwaters are not carved). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(EditCondition="bEnableWater", ClampMin="100"))
	float RiverMaxGroundAboveSeaCm = 1200.0f;

	/** Ponds: small basins. One candidate per square cell of this edge length (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(EditCondition="bEnableWater", ClampMin="30000"))
	float PondCellCm = 60000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(EditCondition="bEnableWater", ClampMin="0", ClampMax="1"))
	float PondChance = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(EditCondition="bEnableWater", ClampMin="500"))
	float PondRadiusMinCm = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(EditCondition="bEnableWater", ClampMin="500"))
	float PondRadiusMaxCm = 4800.0f;

	/** Lakes: bigger basins on a sparser grid. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(EditCondition="bEnableWater", ClampMin="100000"))
	float LakeCellCm = 200000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(EditCondition="bEnableWater", ClampMin="0", ClampMax="1"))
	float LakeChance = 0.40f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(EditCondition="bEnableWater", ClampMin="2000"))
	float LakeRadiusMinCm = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(EditCondition="bEnableWater", ClampMin="2000"))
	float LakeRadiusMaxCm = 11500.0f;

	/** A basin is only dug where the whole rim is at least this far above sea level (cm) -- below that it would just be coast -- ... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(EditCondition="bEnableWater"))
	float BasinRimMinAboveSeaCm = 60.0f;

	/** ... and at most this high (a lake on a hillside would need a cliff-high dam). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(EditCondition="bEnableWater"))
	float BasinRimMaxAboveSeaCm = 1100.0f;

	/** Basin depth below sea level at its centre = this + BasinDepthPerRadius x radius (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(EditCondition="bEnableWater", ClampMin="30"))
	float BasinDepthBaseCm = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(EditCondition="bEnableWater", ClampMin="0"))
	float BasinDepthPerRadius = 0.03f;

	/** Bank rise per cm beyond the shore of a basin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(EditCondition="bEnableWater", ClampMin="0.02", ClampMax="1.0"))
	float BasinBankSlope = 0.22f;

	// --- Upland lakes: lakes ABOVE sea level. Unlike the sea-level basins above they have a water level of their own (the spill height of the low spot they sit in),
	// so each needs a water surface at that height: UMOUplandLakeSubsystem spawns a small water body per lake near the players. ---------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upland Lakes", meta=(EditCondition="bEnableWater"))
	bool bEnableUplandLakes = true;

	/** One lake site per square cell of this edge length (cm). 180000 cm (1.8 km) measured one lake per ~5 km^2 of the whole world (a third of it is sea, and the fit test thins out the hills). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upland Lakes", meta=(EditCondition="bEnableUplandLakes", ClampMin="60000"))
	float UplandLakeCellCm = 130000.0f;

	/** Chance that a cell tries for a lake at all (the fit test below decides the rest). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upland Lakes", meta=(EditCondition="bEnableUplandLakes", ClampMin="0", ClampMax="1"))
	float UplandLakeChance = 0.95f;

	/** Long-axis radius range (cm). Reach is 2.5 radii, which must fit the cell's jitter margin (cell x 0.25): UplandLakeRadiusMaxCm <= cell x 0.1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upland Lakes", meta=(EditCondition="bEnableUplandLakes", ClampMin="2000"))
	float UplandLakeRadiusMinCm = 7000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upland Lakes", meta=(EditCondition="bEnableUplandLakes", ClampMin="2000"))
	float UplandLakeRadiusMaxCm = 13000.0f;

	/** Low-spot candidates tried per cell (each starts at a jittered point and rolls downhill); the flattest fit wins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upland Lakes", meta=(EditCondition="bEnableUplandLakes", ClampMin="1", ClampMax="8"))
	int32 UplandLakeCandidates = 6;

	/** The whole footprint must be at least this high above sea level (cm). Keeps lakes out of the river / sea-level-basin zone (rivers fade out at RiverMaxGroundAboveSeaCm + 600). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upland Lakes", meta=(EditCondition="bEnableUplandLakes"))
	float UplandLakeMinGroundAboveSeaCm = 1900.0f;

	/** Highest minus lowest ground across the footprint (cm): a lake needs a gentle low spot, not a hillside. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upland Lakes", meta=(EditCondition="bEnableUplandLakes", ClampMin="100"))
	float UplandLakeMaxReliefCm = 2400.0f;

	/** The water surface sits this far below the LOWEST ground around the lake (the spill point), so the shore is always a dry bank (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upland Lakes", meta=(EditCondition="bEnableUplandLakes", ClampMin="40"))
	float UplandLakeFreeboardCm = 100.0f;

	/** Depth below the surface at the centre = this + UplandLakeDepthPerRadius x radius (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upland Lakes", meta=(EditCondition="bEnableUplandLakes", ClampMin="30"))
	float UplandLakeDepthBaseCm = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upland Lakes", meta=(EditCondition="bEnableUplandLakes", ClampMin="0"))
	float UplandLakeDepthPerRadius = 0.02f;
};

/** One biome as the generator needs it (copied from DT_Biomes; no species palettes here). */
struct MOFRAMEWORKCORE_API FMOWorldGenBiome
{
	FName Id;
	float HeightMin = -100000.0f;
	float HeightMax = 100000.0f;
	float SlopeMinDeg = 0.0f;
	float SlopeMaxDeg = 90.0f;
	float MoistureMin = 0.0f;
	float MoistureMax = 1.0f;
	float TemperatureMin = 0.0f;
	float TemperatureMax = 1.0f;
	int32 Priority = 0;
	float EdgeBlendWidth = 5000.0f;

	/** Multiplier on the HILLS where this biome's climate applies (see FMOWorldGenTuning::ReliefClimateBlend). 1 = unchanged, 0.3 = gently rolling prairie, > 1 = hilly. */
	float ReliefScale = 1.0f;

	/** Multiplier on the MOUNTAIN RANGES and coastal ranges where this biome's climate applies: ~0 keeps a prairie free of mountains, 1 = unchanged. */
	float MountainScale = 1.0f;

	/** True for a biome whose band is CLIMATE only (no height band, no minimum slope): only these shape the relief. */
	bool IsClimateBiome() const { return HeightMin <= -50000.0f && HeightMax >= 50000.0f && SlopeMinDeg <= 0.0f; }

	/** Asset path of the UVoxelSurfaceTypeInterface painted under this biome (resolved by the Voxel adapter, not here). */
	FSoftObjectPath GroundSurfaceType;

	/** Footstep sound set (bank ids "SFX.Footsteps.<Set>1/2"). Not part of the world hash. */
	FName FootstepSet = FName(TEXT("GrassTread"));

	/** THE band test (the DT row's own Contains delegates to the same rule). */
	bool Contains(float Height, float SlopeDeg, float Moisture, float Temperature) const
	{
		return Height >= HeightMin && Height <= HeightMax
			&& SlopeDeg >= SlopeMinDeg && SlopeDeg <= SlopeMaxDeg
			&& Moisture >= MoistureMin && Moisture <= MoistureMax
			&& Temperature >= TemperatureMin && Temperature <= TemperatureMax;
	}
};

/** Immutable snapshot every consumer reads. Build with Finalize(), publish with FMOWorldGenParamsProvider. */
struct MOFRAMEWORKCORE_API FMOWorldGenParams
{
	/** Bump when the generator math changes (see RULES in the header). Stored with world saves. */
	static constexpr uint32 CurrentGeneratorVersion = 9;   // v9: caves are near-horizontal galleries (half the radius) that open only into steep slopes, no sinkholes; v8: cliff bands are terraces whose drops face downhill (the v6 crag blobs had uphill-facing rims), gullies 25 -> 15 m; v7: no caves under ground above CaveMaxGroundCm (the summits tore); v6: mountain ranges (regional mask + regional biome relief + massif / peaks / gullies; v5: per-biome relief, taller ranges, half-size regional fields, island edge; v4: upland lakes + coastal ranges; v3: rivers + sea-level lake basins; v2: caves; v1: the base height terrain)

	FMOWorldGenTuning Tuning;

	/** Biomes, highest Priority first after Finalize(). Index = the biome id the node/PCG pass around. */
	TArray<FMOWorldGenBiome> Biomes;

	/** Asset path of the UVoxelSurfaceTypeInterface painted on an upland lake's bed (resolved by the Voxel adapter; empty = the biome's ground continues under the water). NOT part of the hash: it is looks, not shape. */
	FSoftObjectPath UplandLakeBedSurfaceType;

	/** Set by Finalize(): the largest hill / mountain scale of any climate biome (>= 1) and whether any biome changes the relief at all (false = the v1-v4 math, bit for bit). */
	float MaxHillScale = 1.0f;
	float MaxMountainScale = 1.0f;
	bool bBiomeRelief = false;

	/** Conservative bounds of SampleHeightCm; the generator clamps to them (the node feeds them to OutputHeight.HeightRange). */
	float MinHeightCm = 0.0f;
	float MaxHeightCm = 0.0f;

	/** Conservative Z extent any cave can occupy (the cave stamp's bounds). Set by Finalize(). */
	float CaveMinZCm = 0.0f;
	float CaveMaxZCm = 0.0f;

	uint32 GeneratorVersion = CurrentGeneratorVersion;

	/**
	 * DEV: a biome test world -- every column is this biome (ground surface, scatter, the biome under the player); the terrain SHAPE is the seed's as always.
	 * Set from the mo.WorldGen.ForceBiome cvar (MO.BiomeTest <BiomeId>); NAME_None = off. Finalize() resolves the index. Part of the hash only when set,
	 * so an ordinary world's identity (and the golden checksum) does not change. Wes, 2026-10-14: "create some biome test levels that we can audit things
	 * inside of independently and fine tune".
	 */
	FName ForcedBiomeId;
	int32 ForcedBiomeIndex = INDEX_NONE;

	/** ComputeHash() as of Finalize(). The hot path (one call per voxel batch) uses this instead of re-hashing. */
	uint32 CachedHash = 0;
	uint32 GetHash() const { return CachedHash; }

	/** Sort biomes by priority, validate the splines, compute the height bounds. Call once after filling Tuning/Biomes. */
	void Finalize();

	/** FNV hash of everything that affects the output. For handshakes (host vs client) and the golden test. */
	uint32 ComputeHash() const;

	int32 FindBiomeIndex(FName BiomeId) const;
	FName GetBiomeId(int32 BiomeIndex) const { return Biomes.IsValidIndex(BiomeIndex) ? Biomes[BiomeIndex].Id : NAME_None; }
};

using FMOWorldGenParamsRef = TSharedRef<const FMOWorldGenParams, ESPMode::ThreadSafe>;

/** Process-wide published snapshot. Publish on the game thread; read from any thread. Never returns null. */
struct MOFRAMEWORKCORE_API FMOWorldGenParamsProvider
{
	static void Publish(const FMOWorldGenParamsRef& NewParams);
	static FMOWorldGenParamsRef Get();

	/** True once something published a snapshot (otherwise Get() hands out the built-in default, which has no biomes). */
	static bool HasPublished();

	/** The built-in default snapshot (default tuning, no biomes). */
	static FMOWorldGenParamsRef MakeDefault();
};

/** What the biome climate does to the land: how hilly and how mountainous. Both 1 = untouched. */
struct FMOBiomeRelief
{
	float Hill = 1.0f;
	float Mountain = 1.0f;
};

/** Large-scale climate at a position, before elevation is applied. */
struct FMOClimateSample
{
	float Continentalness = 0.0f;   // -1 ocean/lowland .. +1 deep inland
	float Erosion = 0.0f;           // -1 rugged .. +1 flat
	float PeaksValleys = 0.0f;      // -1 valley .. +1 peak
	float Moisture = 0.5f;          // 0..1
	float TemperatureSeaLevel = 0.5f; // 0..1 at sea level (apply lapse with TemperatureAtHeight)
	float EdgeLand = 1.0f;          // 1 inland .. 0 at the world border (the island edge falloff): hills, mountains and coastal ranges fade with it so no islet survives at the border
	float MoistureRegion = 0.5f;    // v6: the same climate at RANGE scale (ReliefRegionWavelengthCm). The biome's hill / mountain scale is looked up from these, never from the local patch. Equal to the local values when bRangeMountains is off.
	float TemperatureRegion = 0.5f;
};

struct FMOTerrainSample
{
	float HeightCm = 0.0f;
	float Moisture = 0.5f;
	float Temperature = 0.5f;       // at the sampled height
	int32 BiomeIndex = INDEX_NONE;  // slope assumed 0 (see PITFALLS)
	float Continentalness = 0.0f;
	float Erosion = 0.0f;
};

/**
 * Fingerprints of the generator over a FIXED set of parameters, seed and positions (default tuning + the three stock biome bands, seed 424242). The same function
 * backs the automation tests (MOFramework.WorldGen.GoldenChecksum / CaveGoldenChecksum, which pin the expected values) and the MO.WorldGen.Checksum console command,
 * so a build -- editor, packaged client, another machine -- can be compared against the pinned values with one line of output.
 */
struct FMOWorldGenProbe
{
	uint32 Height = 0;   // 4096 SampleColumn results (height, moisture, temperature, biome)
	uint32 Cave = 0;     // 4608 cave-field samples + the 16 x 16 sinkhole-cell scan
	uint32 ParamsHash = 0;
	uint32 GeneratorVersion = 0;
};

/** One sinkhole: a shaft from the ground down to a chamber. A grid cell holds at most one. */
struct FMOSinkhole
{
	FVector2D Center = FVector2D::ZeroVector;   // world XY (cm)
	float RadiusCm = 0.0f;                      // mean shaft radius (the rim flares to ~1.8x)
	float DepthCm = 0.0f;                       // shaft depth below the ground at Center
	float GroundCm = 0.0f;                      // ground height at Center
};

/** Where FMOWorldGen::FindCoastalSpawn would put a new game: standing on the shore, facing open water. */
struct FMOCoastalSpawn
{
	FVector2D Location = FVector2D::ZeroVector;   // world XY (cm)
	float GroundCm = 0.0f;                        // generator ground height there
	float YawDegrees = 0.0f;                      // facing the water (UE yaw: 0 = +X, 90 = +Y)
	float OpenWaterCm = 0.0f;                     // unbroken water in front of the spawn, along the view direction
	float FarReliefCm = 0.0f;                     // height above sea level of the ground that stands highest in the view across the water
	float FarDistanceCm = 0.0f;                   // how far away that ground is
	float DistanceFromOriginCm = 0.0f;
	float LandmassKm2 = 0.0f;                     // size of the connected landmass under the spawn (a lower bound: the measurement stops at FMOCoastalSearch::MinLandmassKm2)
	float Score = 0.0f;
};

/** How far FMOWorldGen::FindCoastalSpawn looks. The defaults are what the game uses; tests and tools vary them. */
struct FMOCoastalSearch
{
	float GridStepCm = 40000.0f;         // coarse grid cell (400 m)
	int32 GridHalfCells = 60;            // the grid covers +-this many cells around the origin (+-24 km)
	int32 MaxViewCells = 30;             // rays reach this many cells (12 km)
	int32 MinOpenWaterCells = 5;         // a view needs this many cells of unbroken water (2 km)
	int32 FarWindowCells = 16;           // the far shore is judged over this many cells behind the water (6.4 km)
	float FarElevationCap = 0.04f;       // the far ground's elevation angle (height / distance) that scores fully: 0.04 ~ 2.3 degrees, e.g. 200 m at 5 km
	float MinLandmassKm2 = 600.0f;       // the spawn must stand on a landmass at least this big: never an island (there are no boats; an island start would be a prison)
	float BorderMarginCm = 600000.0f;    // stay this far from the world border (6 km): the view must not end at the edge of the world
	float MinClimateGroundWeight = 0.75f; // the spawn's ground must be at least this much CLIMATE biome (SampleSurfaceBiomeWeights): not shore sand / bare rock. 0 = off; no biomes = off
};

struct MOFRAMEWORKCORE_API FMOWorldGen
{
	/** The terrain seed for an int world seed when no graph is involved (graphs get FVoxelExposedSeed::GetSeed(), see UMOWorldSeedSubsystem::GetTerrainSeed). */
	static int32 HashSeed(int32 Seed, uint32 Salt);

	/** Run the fixed probe (see FMOWorldGenProbe). Pure and deterministic: the same binary must print the same numbers on every CPU. */
	static FMOWorldGenProbe RunProbe(bool bWater = true);

	/**
	 * The values RunProbe() must produce for this generator version. THE single place they are pinned: the automation tests and MO.WorldGen.Checksum compare against
	 * these. An intentional change to the generator math bumps FMOWorldGenParams::CurrentGeneratorVersion and updates them in the same commit.
	 */
	static constexpr uint32 PinnedProbeHeight = 0x3343B4EBu;   // v8 (cliff bands face downhill, gullies 15 m); v6 (mountain ranges: regional mask + regional biome relief + massif/peaks/gullies; v5: biome relief, taller ranges, half-size regional fields, island edge; v4: upland lakes, coastal ranges, VerticalScale 2; v3: rivers + sea-level lake basins)
	/** The probe with water, coastal ranges and VerticalScale all switched OFF (RunProbe(false)) must reproduce the v1 base terrain exactly: this is the value v1 shipped. It pins the BASE height math. */
	static constexpr uint32 PinnedProbeHeightNoWater = 0x9C4AE2B3u;
	static constexpr uint32 PinnedProbeCave = 0xEA773801u;     // v9 (near-horizontal galleries opening only into steep slopes, no sinkholes); v8 (the v8 ground); v7 (no caves under ground above CaveMaxGroundCm: the summits tore; v6: caves on the v6 ground; v5: caves on the v5 ground; v4: the lake-carved, doubled-relief ground, sinkholes keep out of lakes)

	/**
	 * The cave distance saturates here (cm): "no cave within this far". Every gate in the cave field (cover, depth, low ground, sinkhole reach) is built so the
	 * field is exactly this value where the gate closes, i.e. the field is continuous, never a cliff (a cliff reads as a wall to range analysis and LOD blending).
	 * Larger than any chunk's half diagonal at the LODs that carry caves, so a chunk reading +CaveFarCm everywhere provably holds no cave surface.
	 */
	static constexpr float CaveFarCm = 3000.0f;

	/** Coarsest sample spacing (cm) at which caverns, the widest cave feature, are evaluated. Tunnels and sinkholes stop at finer spacings (see CaveMaxLOD). */
	static constexpr float CavernMaxStepCm = 800.0f;

	/**
	 * The coarsest voxel LOD (sample spacing VoxelSizeCm * 2^LOD, the MO Terrain Caves node's StepCm) at which any cave feature is still evaluated: above it the cave
	 * field is +CaveFarCm everywhere. The cave stamp is limited to LODs 0..CaveMaxLOD (UMOWorldSeedSubsystem): a volume stamp whose bounds touch a chunk sends that whole
	 * chunk through the Voxel plugin's volume meshing pass, and on the coarse far chunks of steep mountains that pass left chunk-sized holes in a checker pattern
	 * (playtest 2026-10-14 #10, the "blue/white checkerboard" = sky through the holes). Past this LOD the stamp carved nothing anyway, so the range changes no shape.
	 */
	static int32 CaveMaxLOD(const FMOWorldGenParams& Params);

	/**
	 * Cave field: approximate signed distance (cm) to the nearest cave AIR, negative INSIDE a tunnel / cavern / sinkhole. Feed a Subtractive
	 * volume stamp. Tunnels and caverns keep CaveCoverCm of rock above them (they taper in over CaveFadeCm); only sinkholes reach the ground.
	 * Not an exact distance (noise-based; gradient ~1 on average), which is all marching cubes needs. StepCm = sample spacing: features
	 * smaller than the spacing are skipped (far LODs see solid ground, never aliased caves). Costs one SampleHeightCm unless you already
	 * know the ground: use SampleCaveDistanceAtGround for many Z in one column.
	 */
	static float SampleCaveDistanceCm(const FMOWorldGenParams& Params, double X, double Y, double Z, int32 Seed, float StepCm = 50.0f);
	static float SampleCaveDistanceAtGround(const FMOWorldGenParams& Params, double X, double Y, double Z, float GroundCm, int32 Seed, float StepCm = 50.0f);

	/** The sinkhole of grid cell (CellX, CellY) (cell edge = SinkholeCellCm), or false if the cell has none / caves are off / the ground there is too low. */
	static bool GetSinkholeInCell(const FMOWorldGenParams& Params, int64 CellX, int64 CellY, int32 Seed, FMOSinkhole& Out);

	/** The sinkhole of the cell containing (X, Y), if any. */
	static bool FindSinkholeAt(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, FMOSinkhole& Out);

	/**
	 * Horizontal distance (cm) from (X,Y) to the rim zone of the sinkhole in its cell (negative = inside the zone where the shaft or its flared rim can
	 * reach), NoSinkholeDistanceCm if the cell has none. Vegetation / buildings keep out of the zone; the jitter margin guarantees a sinkhole never
	 * reaches across its own cell border, so one cell is enough.
	 */
	static float DistanceToSinkholeRimZone(const FMOWorldGenParams& Params, double X, double Y, int32 Seed);

	/** DistanceToSinkholeRimZone where there is no sinkhole: farther than any footprint. It used to be CaveFarCm (30 m) -- a world feature with a footprint
	 *  over 30 m was rejected in every cell WITHOUT a sinkhole (found Oct 16 2026 when v9 turned sinkholes off and every cliff assembly vanished). */
	static constexpr float NoSinkholeDistanceCm = 1.0e9f;

	/** Climate fields at world X/Y (cm). Cheap: no height. */
	static FMOClimateSample SampleClimate(const FMOWorldGenParams& Params, double X, double Y, int32 Seed);

	/** Ground height (cm, world Z) AFTER rivers and lakes are carved. StepCm = sample spacing; octaves finer than 2x StepCm are skipped. */
	static float SampleHeightCm(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float StepCm = 100.0f, FMOClimateSample* OutClimate = nullptr);

	/** The land before any water is carved (continents, hills, mountains). Rivers and lakes are a pure function of this. */
	static float SampleBaseHeightCm(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float StepCm = 100.0f, FMOClimateSample* OutClimate = nullptr);

	/**
	 * Cuts rivers and lake basins into BaseHeightCm (never raises ground). Returns BaseHeightCm unchanged away from low ground and water. StepCm = sample spacing: a basin tier
	 * the spacing does not resolve is skipped (IsWaterTierResolved), as the octaves finer than 2x StepCm are.
	 */
	static float ApplyWater(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float BaseHeightCm, float StepCm);

	/**
	 * Does a sample spacing of StepCm resolve the basins of this tier (0 = pond, 1 = lake, 2 = upland lake): true while the tier's largest radius is wider than the spacing, i.e.
	 * a basin spans at least two samples -- the octave rule (wavelength below 2 x StepCm is skipped) applied to a basin's diameter. A coarser LOD skips the tier: a basin narrower
	 * than one voxel can only alias into a pit, and finding it costs hundreds of full-detail base samples per cell. Without this gate a 32 km LOD-11 chunk touched ~600 upland-lake
	 * cells (5 s of meshing per chunk; the 10 s second voxel state of every new game that the near vegetation waited for, Oct 13 2026). MO.WorldGen.DebugNoLodCull 1 resolves
	 * every tier at every spacing.
	 */
	static bool IsWaterTierResolved(const FMOWorldGenParams& Params, int32 Tier, float StepCm);

	/** One lake / pond basin (a grid cell holds at most one per tier: 0 = pond, 1 = lake, 2 = upland lake). Tiers 0 and 1 sit at sea level, tier 2 at its own SurfaceCm; the basin floor is below it. */
	struct FLake
	{
		FVector2D Center = FVector2D::ZeroVector;   // world XY (cm)
		float RadiusCm = 0.0f;                      // long-axis radius at the shore
		float Aspect = 1.0f;                        // short axis / long axis
		float CosAngle = 1.0f, SinAngle = 0.0f;     // long-axis direction
		float DepthCm = 0.0f;                       // depth below the water surface at the centre
		float SurfaceCm = 0.0f;                     // world Z of the water surface (sea level for tiers 0 and 1)
		int64 CellX = 0, CellY = 0;                 // the grid cell that owns this lake (its identity)

		/** The margin the water plane has beyond the ellipse's own bounding box. */
		static constexpr double PlaneMargin = 1.02;

		/**
		 * Half-size (x, y; cm) of the WORLD-ALIGNED rectangle an upland lake's water plane covers: the ellipse's own axis-aligned bounding box, margin included. The plane cannot be rotated to the ellipse
		 * (Oceanology's water mesh is a quad tree over world-aligned bounds), so this is the rectangle the terrain must hide outside the ellipse: ComputeUplandLake checks it, UMOUplandLakeSubsystem sizes the
		 * water to it, and MOFramework.WorldGen.UplandLakePlane measures it. The ellipse's points are (R cos t, R A sin t) in its own frame; rotated by the lake's angle, its extreme x and y are
		 * R sqrt(c^2 + A^2 s^2) and R sqrt(s^2 + A^2 c^2).
		 */
		FVector2D PlaneHalfSizeCm() const
		{
			const double C = CosAngle, S = SinAngle, A = Aspect, R = RadiusCm;
			return FVector2D(PlaneMargin * R * FMath::Sqrt(C * C + A * A * S * S), PlaneMargin * R * FMath::Sqrt(S * S + A * A * C * C));
		}
	};
	static bool GetLakeInCell(const FMOWorldGenParams& Params, int32 Tier, int64 CellX, int64 CellY, int32 Seed, FLake& Out);

	/** Tier 2 only: the upland lakes whose centre lies within RadiusCm of (X, Y), appended to Out. */
	static void GetUplandLakesNear(const FMOWorldGenParams& Params, double X, double Y, double RadiusCm, int32 Seed, TArray<FLake>& Out);

	/**
	 * Is (X, Y) inside the zone of an upland lake: ZoneRadii x the lake's own ellipse (1 = the shore, 1.5 = shore + bank). Vegetation keeps out of the water, sinkholes keep
	 * out of the bowl. Cheap and cache-backed; false when upland lakes are off.
	 */
	static bool IsInUplandLakeZone(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float ZoneRadii = 1.0f);

	/**
	 * The OPEN-WATER surface height at (X, Y): an upland lake's own surface where (X, Y) is inside its shore ellipse, otherwise the ocean plane (SeaLevelCm: the sea, rivers, ponds and sea-level lake
	 * basins are all terrain below it, and the ocean mesh fills every point under the plane, so a dug pit below sea level shows water too). False when water is off. This is what a swimmer, a boat or a
	 * float should treat as "the water here"; the visible water meshes are exactly these surfaces.
	 */
	static bool GetWaterSurfaceCm(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float& OutSurfaceCm);

	/**
	 * Which body of water (X, Y) lies in, from the same carving the terrain is built with: an upland lake's shore ellipse, else the ocean basin (land below sea level), else
	 * the deepest river channel / pond / lake basin that took the ground under the sea plane. None on dry land. Pure; ~3 base-height samples.
	 */
	static EMOWaterBodyKind ClassifyWaterBody(const FMOWorldGenParams& Params, double X, double Y, int32 Seed);

	/**
	 * How much of the ground at (X, Y) is an upland lake's BED: 1 inside the shore ellipse, falling to 0 over the first ~10% of a radius past the shore with a noise breakup (so the edge is a damp, ragged rim,
	 * never a clean curve); 0 away from every lake and when upland lakes are off. The terrain node paints the lake-bed surface (mud / rock instead of the biome's forest floor) where this is >= 0.5.
	 */
	static float UplandLakeBedWeight(const FMOWorldGenParams& Params, double X, double Y, int32 Seed);

	/**
	 * A good place to start a new game: land on the shore of a real ocean, looking across the water at relief on the far side. Searches a coarse grid (a few tens of ms), refines
	 * the best candidates on the real terrain. Pure and deterministic like the rest of the generator. False when the world has no open water within the search radius
	 * (the caller keeps its old spawn rules then).
	 */
	static bool FindCoastalSpawn(const FMOWorldGenParams& Params, int32 Seed, FMOCoastalSpawn& Out, const FMOCoastalSearch& Search = FMOCoastalSearch());

	/**
	 * How much connected land (8-connected cells of CellCm -- a land strip running diagonally between lattice points must not read as cut -- ground not under water; rivers and ponds do not cut land, they can be waded) is reachable from (X, Y) -- the size of the
	 * landmass the point stands on, in cells, counting at most MaxCells (it stops there: "big enough" is all a caller needs). 0 if (X, Y) is water. The world border ends land.
	 * The island test of the spawn search; tools and tests call it with their own lattice to cross-check.
	 */
	static int32 MeasureLandmassCells(const FMOWorldGenParams& Params, int32 Seed, double X, double Y, float CellCm, int32 MaxCells);

	/** Height + climate + biome (slope assumed 0). */
	static FMOTerrainSample SampleColumn(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float StepCm = 100.0f);

	/**
	 * The biome relief multipliers at a climate (see FMOWorldGenTuning::ReliefClimateBlend): a soft first-match-wins over the climate-only biomes, exactly 1 and 1 when no biome changes
	 * the relief. Continuous in both arguments (no cliffs at biome borders).
	 */
	static FMOBiomeRelief BiomeReliefAt(const FMOWorldGenParams& Params, float Moisture, float TemperatureSeaLevel);

	/** Elevation cools the air: T = T0 - lapse * heightAboveSea(km). Result clamped 0..1. */
	static float TemperatureAtHeight(const FMOWorldGenParams& Params, float TemperatureSeaLevel, float HeightCm);

	/** Highest-Priority biome whose bands contain the sample, or INDEX_NONE. */
	static int32 ResolveBiome(const FMOWorldGenParams& Params, float HeightCm, float SlopeDeg, float Moisture, float Temperature);

	/**
	 * THE soft threshold of every ground-material split: 0 below the edge, 1 above it, a Hermite ramp of total width Width centred on Edge + Breakup (the caller's
	 * deterministic noise offset, so the edge wanders instead of following a contour). Width <= 0 = a hard step at Edge + Breakup. Pure, IEEE-exact.
	 */
	static float SoftThreshold(float Value, float Edge, float Width, float Breakup = 0.0f);

	struct FBiomeWeight
	{
		int32 Biome = INDEX_NONE;
		float Weight = 0.0f;
	};

	/**
	 * Which biomes' GROUND SURFACE shows at a column, and how much: each biome's height / moisture / temperature band through SoftThreshold (band widths and the edge
	 * breakup from the Surface* tuning), composited first-match-wins in priority order (the same rule ResolveBiome applies hard), so a boundary is a band of mixed
	 * ground with a noisy edge. Writes the strongest MaxOut biomes (weights sum to 1), returns how many. Ground looks and PickSurfaceBiome (PCG) use it; gameplay keeps ResolveBiome.
	 * Slope-banded biomes follow SampleColumn's slope 0 (they never apply to the column surface).
	 */
	static int32 SampleSurfaceBiomeWeights(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float HeightCm, float Moisture, float Temperature, FBiomeWeight* Out, int32 MaxOut);

	/**
	 * The biome a PCG point (a tree, a grass clump) belongs to, so foliage thins out across the SAME soft band the ground fades over instead of stopping at ResolveBiome's line:
	 * a deterministic per-position roll (10 cm grid, seeded) against SampleSurfaceBiomeWeights, restricted to the biomes whose slope range holds SlopeDeg. A slope-banded biome
	 * (cliffs) that ResolveBiome picks wins outright -- slope edges stay crisp. Same inputs -> same answer on every machine. INDEX_NONE when no biome applies.
	 */
	static int32 PickSurfaceBiome(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float HeightCm, float SlopeDeg, float Moisture, float Temperature);
};
