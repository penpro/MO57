// Grounding a placed composition (a world feature's PCG assembly) on the terrain it actually landed on.
//
// Electric Dreams' assemblies are point data baked from Epic's own levels: every rock and plant keeps the height it had on Epic's hillside. Placed on our
// different ground, plants float or sink and rocks hover over the downhill side (Steam playtest Oct 14 / Oct 16 2026: "floating trees", "wonky cliff").
// FMOCompositionGrounding measures every generated instance against the ground under it and, applied, puts it there:
//   1. rocks (cliffs, embankments, boulders) with a gap under them sink until it closes (never more than most of their height);
//   2. plants (trees, bushes, ferns, logs) stand on whatever is under their base -- the composition's own rocks, else the terrain;
//   3. clinging pieces (vines, ivy, moss) move with the rock or plant they hang on.
// The terrain height comes from the world generator (GroundZ): the same on every machine and known far beyond any terrain collision (compositions
// generate ~400 m out, voxel collision exists only near players). Pure measurement with bApply = false: the census `MO.WorldFeatures.Grounding` prints.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UStaticMesh;

enum class EMOCompositionPiece : uint8
{
	Foliage,    // stands on the ground: trees, bushes, ferns, logs, stumps
	Rock,       // the structure: cliffs, embankments, boulders -- everything not named otherwise
	Clinging,   // hangs on something: vines, ivy, moss
	Ignore      // engine basic shapes (editor markers baked into Epic's data, hidden by UMOWorldFeatureSubsystem)
};

/** What one grounding pass over a composition found -- and, applied, how many instances it moved. */
struct MOFRAMEWORK_API FMOCompositionGroundingStats
{
	int32 Foliage = 0;
	int32 FoliageFloating = 0;   // base more than FloatToleranceCm above what is under it
	int32 FoliageBuried = 0;     // base more than BuryToleranceCm under the ground
	float MaxFoliageFloatCm = 0.0f;
	float MaxFoliageBuryCm = 0.0f;
	int32 Rocks = 0;
	int32 RocksFloating = 0;     // somewhere on its footprint a visible gap under it
	float MaxRockGapCm = 0.0f;
	int32 Clinging = 0;
	int32 Moved = 0;

	FString Describe() const;
};

struct MOFRAMEWORK_API FMOCompositionGrounding
{
	/** A plant floating more than this over its support is "floating" (and is moved when applied). */
	static constexpr float FloatToleranceCm = 15.0f;
	/** A plant buried deeper than this is moved up. Deeper than BuryDetectCm is not seen (its base is inside the ground). */
	static constexpr float BuryToleranceCm = 40.0f;
	static constexpr float BuryDetectCm = 250.0f;
	/** A plant stands this far into its support (scaled): no hairline gap on uneven ground. */
	static constexpr float FoliageSinkCm = 5.0f;
	/** A rock with a gap bigger than this under it somewhere sinks; it ends this far into the ground at its worst point, never deeper than the fraction
	 *  of its height (a boulder on a steep slope shows its uphill side buried rather than hovering on the downhill side). */
	static constexpr float RockGapToleranceCm = 20.0f;
	static constexpr float RockSinkCm = 10.0f;
	static constexpr float RockMaxSinkFraction = 0.6f;

	/** What kind of piece a composition mesh is, from its asset name (Electric Dreams / Megascans naming). */
	static EMOCompositionPiece Classify(const UStaticMesh* Mesh);

	/** Measure every visible generated instance of Owner's composition; with bApply, ground them (rocks, then plants, then clinging pieces). */
	static FMOCompositionGroundingStats Ground(AActor* Owner, const TFunctionRef<float(double X, double Y)>& GroundZ, bool bApply);
};
