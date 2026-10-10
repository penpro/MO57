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
 * [2026-10] LOD: pass the sample spacing (cm) so octaves finer than 2x spacing are
 *   skipped; without it coarse LODs alias and cost the same as LOD 0.
 *
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "MOWorldGen.generated.h"

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

	/** The generated world covers +-this around the origin (cm). Outside it the height stamp does nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World", meta=(ClampMin="100000"))
	float WorldHalfSizeCm = 3000000.0f;

	/** World Z of sea level (cm). The continental spline is relative to this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World")
	float SeaLevelCm = 0.0f;

	/** Must match AVoxelWorld::VoxelSize (cm). Sample spacing at LOD n is VoxelSizeCm * 2^n, which decides which octaves are skipped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World", meta=(ClampMin="1"))
	float VoxelSizeCm = 100.0f;

	// --- Large-scale fields (wavelength of the lowest octave, cm) --------------------------------

	/** Continentalness: ocean/lowland <-> inland highland. The biggest structure in the world. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fields", meta=(ClampMin="10000"))
	float ContinentWavelengthCm = 3000000.0f;

	/** Erosion: flat plains <-> rugged. Decides WHERE mountains may rise. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fields", meta=(ClampMin="10000"))
	float ErosionWavelengthCm = 900000.0f;

	/** Weirdness (folded into peaks-and-valleys): modulates mountain height inside mountainous regions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fields", meta=(ClampMin="10000"))
	float WeirdnessWavelengthCm = 600000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fields", meta=(ClampMin="10000"))
	float MoistureWavelengthCm = 700000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fields", meta=(ClampMin="10000"))
	float TemperatureWavelengthCm = 1500000.0f;

	/** Contrast applied to continentalness/erosion/weirdness (gradient noise rarely leaves +-0.5; the splines want the full -1..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fields", meta=(ClampMin="1"))
	float FieldContrast = 1.9f;

	/** Domain warp: positions are displaced by low-frequency noise before every field is sampled (organic, non-grid shapes). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fields", meta=(ClampMin="0"))
	float WarpStrengthCm = 70000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fields", meta=(ClampMin="10000"))
	float WarpWavelengthCm = 500000.0f;

	// --- Height shaping --------------------------------------------------------------------------

	/** Peak relief of mountain ranges at full ruggedness (metres). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(ClampMin="0"))
	float MountainAmplitudeM = 320.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(ClampMin="1000"))
	float MountainWavelengthCm = 800000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(ClampMin="0"))
	float HillAmplitudeM = 26.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(ClampMin="1000"))
	float HillWavelengthCm = 150000.0f;

	/** Metre-scale roughness so the ground is not billiard-smooth. Skipped at coarse LODs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(ClampMin="0"))
	float DetailAmplitudeM = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Height", meta=(ClampMin="100"))
	float DetailWavelengthCm = 3500.0f;

	/** Continentalness (-1..1) -> base elevation in metres above sea level. Sorted by X. Catmull-Rom interpolated. */
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

	/** Asset path of the UVoxelSurfaceTypeInterface painted under this biome (resolved by the Voxel adapter, not here). */
	FSoftObjectPath GroundSurfaceType;

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
	static constexpr uint32 CurrentGeneratorVersion = 1;

	FMOWorldGenTuning Tuning;

	/** Biomes, highest Priority first after Finalize(). Index = the biome id the node/PCG pass around. */
	TArray<FMOWorldGenBiome> Biomes;

	/** Conservative bounds of SampleHeightCm; the generator clamps to them (the node feeds them to OutputHeight.HeightRange). */
	float MinHeightCm = 0.0f;
	float MaxHeightCm = 0.0f;

	uint32 GeneratorVersion = CurrentGeneratorVersion;

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

/** Large-scale climate at a position, before elevation is applied. */
struct FMOClimateSample
{
	float Continentalness = 0.0f;   // -1 ocean/lowland .. +1 deep inland
	float Erosion = 0.0f;           // -1 rugged .. +1 flat
	float PeaksValleys = 0.0f;      // -1 valley .. +1 peak
	float Moisture = 0.5f;          // 0..1
	float TemperatureSeaLevel = 0.5f; // 0..1 at sea level (apply lapse with TemperatureAtHeight)
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

struct MOFRAMEWORKCORE_API FMOWorldGen
{
	/** The terrain seed for an int world seed when no graph is involved (graphs get FVoxelExposedSeed::GetSeed(), see UMOWorldSeedSubsystem::GetTerrainSeed). */
	static int32 HashSeed(int32 Seed, uint32 Salt);

	/** Climate fields at world X/Y (cm). Cheap: no height. */
	static FMOClimateSample SampleClimate(const FMOWorldGenParams& Params, double X, double Y, int32 Seed);

	/** Ground height (cm, world Z). StepCm = sample spacing; octaves finer than 2x StepCm are skipped. */
	static float SampleHeightCm(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float StepCm = 100.0f, FMOClimateSample* OutClimate = nullptr);

	/** Height + climate + biome (slope assumed 0). */
	static FMOTerrainSample SampleColumn(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float StepCm = 100.0f);

	/** Elevation cools the air: T = T0 - lapse * heightAboveSea(km). Result clamped 0..1. */
	static float TemperatureAtHeight(const FMOWorldGenParams& Params, float TemperatureSeaLevel, float HeightCm);

	/** Highest-Priority biome whose bands contain the sample, or INDEX_NONE. */
	static int32 ResolveBiome(const FMOWorldGenParams& Params, float HeightCm, float SlopeDeg, float Moisture, float Temperature);
};
