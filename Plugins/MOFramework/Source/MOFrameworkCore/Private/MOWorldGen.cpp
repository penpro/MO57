// See MOWorldGen.h for the rules this file must obey (pure, deterministic, thread-safe, IEEE-exact ops only).

#include "MOWorldGen.h"

#include "Misc/ScopeRWLock.h"
#include "HAL/IConsoleManager.h"
#include "MOWorldGenNoise.h"

// DEBUG ONLY (2026-10-11): 1 = the base height ignores the LOD (every octave at every LOD). Two chunks of neighbouring LODs meet at a seam that is only watertight if the coarse samples equal the
// fine values at the same positions; the octave cut makes them differ. This switch exists to SEE whether that is what tears the far mountains. Never set it in a shipped build: it changes the ground.
static TAutoConsoleVariable<int32> CVarMOWaterLodGate(TEXT("MO.WorldGen.WaterLodGate"), 1, TEXT("1 = a coarse LOD skips the water basins it cannot resolve (FMOWorldGen::IsWaterTierResolved); 0 = every basin at every LOD (A/B only: far chunks then take seconds each). Set before the world generates."), ECVF_Default);
static TAutoConsoleVariable<int32> CVarMODebugNoLodCull(TEXT("MO.WorldGen.DebugNoLodCull"), 0, TEXT("Debug: 1 = the base height ignores the LOD (no octave culling). Changes the generated ground; test seams only."), ECVF_Default);

// ============================================================================
// TUNING DEFAULTS
// ============================================================================

FMOWorldGenTuning::FMOWorldGenTuning()
{
	// Continentalness -> metres above sea level (before VerticalScale). Above the waterline the land rises gently (beaches, lowland); below it the ground drops away FAST: 12 m under at -0.58,
	// 28 m at -0.62, 50 m at -0.70, then a deep ocean floor (80 m, doubled by VerticalScale = 160 m). The first version had a sea floor at -6 m, i.e. a shelf so flat that noise made the shoreline wander out into the sea.
	ContinentSplineM = {
		FVector2D(-1.00, -80.0), FVector2D(-0.85, -72.0), FVector2D(-0.70, -50.0), FVector2D(-0.62, -28.0), FVector2D(-0.58, -12.0), FVector2D(-0.55, 0.0),
		FVector2D(-0.30, 6.0), FVector2D(-0.15, 12.0),
		FVector2D(0.00, 18.0), FVector2D(0.25, 32.0), FVector2D(0.55, 55.0), FVector2D(1.00, 90.0) };

	// Erosion -> ruggedness. Low erosion = mountains.
	ErosionReliefSpline = {
		FVector2D(-1.00, 1.15), FVector2D(-0.50, 0.95), FVector2D(0.00, 0.55), FVector2D(0.45, 0.20), FVector2D(1.00, 0.05) };
}

// ============================================================================
// PARAMS SNAPSHOT
// ============================================================================

namespace
{
	/** Catmull-Rom through knots sorted by X (non-uniform tangents), clamped at both ends. */
	float EvalSpline(const TArray<FVector2D>& Knots, float X)
	{
		const int32 N = Knots.Num();
		if (N == 0)
		{
			return 0.0f;
		}
		if (N == 1 || X <= (float)Knots[0].X)
		{
			return (float)Knots[0].Y;
		}
		if (X >= (float)Knots[N - 1].X)
		{
			return (float)Knots[N - 1].Y;
		}

		int32 I = 0;
		while (I < N - 2 && X > (float)Knots[I + 1].X)
		{
			++I;
		}

		const float X0 = (float)Knots[I].X, Y0 = (float)Knots[I].Y;
		const float X1 = (float)Knots[I + 1].X, Y1 = (float)Knots[I + 1].Y;
		const float H = FMath::Max(X1 - X0, 1e-6f);
		const float T = (X - X0) / H;
		const float Secant = (Y1 - Y0) / H;

		const float M0 = (I > 0) ? ((float)(Knots[I + 1].Y - Knots[I - 1].Y) / FMath::Max((float)(Knots[I + 1].X - Knots[I - 1].X), 1e-6f)) : Secant;
		const float M1 = (I + 2 < N) ? ((float)(Knots[I + 2].Y - Knots[I].Y) / FMath::Max((float)(Knots[I + 2].X - Knots[I].X), 1e-6f)) : Secant;

		const float T2 = T * T;
		const float T3 = T2 * T;
		return (2.0f * T3 - 3.0f * T2 + 1.0f) * Y0 + (T3 - 2.0f * T2 + T) * H * M0
			+ (-2.0f * T3 + 3.0f * T2) * Y1 + (T3 - T2) * H * M1;
	}

	/** Min/max of a spline over its whole domain (dense scan: Catmull-Rom can overshoot its knots). */
	void SplineRange(const TArray<FVector2D>& Knots, float& OutMin, float& OutMax)
	{
		OutMin = 0.0f;
		OutMax = 0.0f;
		if (Knots.Num() == 0)
		{
			return;
		}
		OutMin = OutMax = (float)Knots[0].Y;
		const float X0 = (float)Knots[0].X;
		const float X1 = (float)Knots.Last().X;
		for (int32 I = 0; I <= 512; ++I)
		{
			const float V = EvalSpline(Knots, X0 + (X1 - X0) * (float)I / 512.0f);
			OutMin = FMath::Min(OutMin, V);
			OutMax = FMath::Max(OutMax, V);
		}
	}

	FORCEINLINE uint32 FnvAdd(uint32 Hash, const void* Data, int32 NumBytes)
	{
		const uint8* Bytes = static_cast<const uint8*>(Data);
		for (int32 I = 0; I < NumBytes; ++I)
		{
			Hash = (Hash ^ Bytes[I]) * 16777619u;
		}
		return Hash;
	}
	FORCEINLINE uint32 FnvAddFloat(uint32 Hash, float V) { return FnvAdd(Hash, &V, sizeof(V)); }
	FORCEINLINE uint32 FnvAddInt(uint32 Hash, int32 V) { return FnvAdd(Hash, &V, sizeof(V)); }
}

void FMOWorldGenParams::Finalize()
{
	// Highest priority first so ResolveBiome's first match wins (same rule the spawner always used).
	Biomes.StableSort([](const FMOWorldGenBiome& A, const FMOWorldGenBiome& B) { return A.Priority > B.Priority; });
	ForcedBiomeIndex = ForcedBiomeId.IsNone() ? INDEX_NONE : FindBiomeIndex(ForcedBiomeId);

	FMOWorldGenTuning& T = Tuning;
	T.ContinentSplineM.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X; });
	T.ErosionReliefSpline.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X; });

	// An upland basin reaches 2.5 radii from its centre and the centre is jittered to within 0.25 cell of the cell border: the reach must stay inside the cell, because
	// a column only ever asks its OWN cell for a lake (see MOWorldGenWater.cpp).
	T.UplandLakeRadiusMaxCm = FMath::Min(T.UplandLakeRadiusMaxCm, 0.1f * T.UplandLakeCellCm);
	T.UplandLakeRadiusMinCm = FMath::Min(T.UplandLakeRadiusMinCm, T.UplandLakeRadiusMaxCm);

	float MinBase = 0.0f, MaxBase = 0.0f, MinRelief = 0.0f, MaxRelief = 0.0f;
	SplineRange(T.ContinentSplineM, MinBase, MaxBase);
	SplineRange(T.ErosionReliefSpline, MinRelief, MaxRelief);
	MaxRelief = FMath::Max(MaxRelief, 0.0f);

	// Biome relief: only climate biomes shape it (see FMOWorldGenBiome::IsClimateBiome); with every scale at 1 the relief term is skipped entirely so the v1-v4 math stays bit for bit.
	bBiomeRelief = false;
	MaxHillScale = 1.0f;
	MaxMountainScale = 1.0f;
	for (const FMOWorldGenBiome& B : Biomes)
	{
		if (B.IsClimateBiome() && (B.ReliefScale != 1.0f || B.MountainScale != 1.0f))
		{
			bBiomeRelief = true;
			MaxHillScale = FMath::Max(MaxHillScale, B.ReliefScale);
			MaxMountainScale = FMath::Max(MaxMountainScale, B.MountainScale);
		}
	}

	// Every noise term is bounded by 1 (see the generator), so these are hard bounds, not estimates.
	// v6: the range term is mask (<= 1) x (massif + peaks), each <= its amplitude; the gullies only ever subtract.
	const float MountainBoundM = T.bRangeMountains ? (T.MassifAmplitudeM + T.PeakAmplitudeM + T.CragAmplitudeM) : (T.MountainAmplitudeM * MaxRelief);
	MaxHeightCm = T.SeaLevelCm + 100.0f * T.VerticalScale * (MaxBase + (MountainBoundM + T.CoastalRangeAmplitudeM) * MaxMountainScale + T.HillAmplitudeM * MaxHillScale + T.DetailAmplitudeM);
	MinHeightCm = T.SeaLevelCm + 100.0f * T.VerticalScale * (MinBase - T.HillAmplitudeM * MaxHillScale - T.DetailAmplitudeM);
	if (T.bEnableWater)
	{
		// Rivers and basins dig below sea level: the deepest is a lake centre (base + per-radius) or the river bed.
		const float DeepestWater = FMath::Max(T.RiverDepthCm, T.BasinDepthBaseCm + T.BasinDepthPerRadius * FMath::Max(T.LakeRadiusMaxCm, T.PondRadiusMaxCm));
		MinHeightCm = FMath::Min(MinHeightCm, T.SeaLevelCm - DeepestWater - 10.0f);
	}

	// Caves live between the deepest point under the lowest ground and a little above the highest ground (a flared sinkhole rim can poke above the surface).
	CaveMaxZCm = MaxHeightCm + 500.0f;
	// ... and never below the water floor (v10): the volume stamp's bounds start there, so nothing under the sea is even sampled.
	CaveMinZCm = FMath::Max(MinHeightCm - (T.CaveMaxDepthCm + T.CaveFadeCm), T.SeaLevelCm + T.CaveWaterClearanceCm);
	GeneratorVersion = CurrentGeneratorVersion;
	CachedHash = ComputeHash();
}

uint32 FMOWorldGenParams::ComputeHash() const
{
	// Extend this when a field that affects the output is added to FMOWorldGenTuning / FMOWorldGenBiome.
	uint32 H = 2166136261u;
	H = FnvAddInt(H, (int32)GeneratorVersion);
	const FMOWorldGenTuning& T = Tuning;
	for (const float V : { T.WorldHalfSizeCm, T.SeaLevelCm, T.VoxelSizeCm, T.ContinentWavelengthCm, T.ErosionWavelengthCm, T.WeirdnessWavelengthCm, T.MoistureWavelengthCm,
		T.TemperatureWavelengthCm, T.FieldContrast, T.WarpStrengthCm, T.WarpWavelengthCm, T.MountainAmplitudeM, T.MountainWavelengthCm, T.HillAmplitudeM,
		T.HillWavelengthCm, T.DetailAmplitudeM, T.DetailWavelengthCm, T.TemperatureLapsePerKm, T.CoastalMoistureBoost, T.LatitudeTemperatureGradient })
	{
		H = FnvAddFloat(H, V);
	}
	H = FnvAddInt(H, T.bEnableWater ? 1 : 0);
	for (const float V : { T.RiverWavelengthCm, T.RiverWidthCm, T.RiverDepthCm, T.RiverBankSlope, T.RiverMaxGroundAboveSeaCm, T.PondCellCm, T.PondChance, T.PondRadiusMinCm,
		T.PondRadiusMaxCm, T.LakeCellCm, T.LakeChance, T.LakeRadiusMinCm, T.LakeRadiusMaxCm, T.BasinRimMinAboveSeaCm, T.BasinRimMaxAboveSeaCm, T.BasinDepthBaseCm,
		T.BasinDepthPerRadius, T.BasinBankSlope })
	{
		H = FnvAddFloat(H, V);
	}
	// Upland lakes (v4).
	for (const float V : { T.CoastalRangeAmplitudeM, T.CoastalRangeWavelengthCm, T.CoastalRangePresenceWavelengthCm, T.VerticalScale })
	{
		H = FnvAddFloat(H, V);
	}
	// v5: relief blend and the island edge.
	for (const float V : { T.ReliefClimateBlend, T.EdgeFalloffStart, T.EdgeFalloffEnd, T.MountainGain, T.LowlandCapM, T.HillSeaFadeLow, T.HillSeaFadeHigh })
	{
		H = FnvAddFloat(H, V);
	}
	// v6: mountain ranges.
	H = FnvAddInt(H, T.bRangeMountains ? 1 : 0);
	for (const float V : { T.MountainRegionWavelengthCm, T.MountainRegionLow, T.MountainRegionHigh, T.MountainRegionGain, T.ReliefRegionWavelengthCm, T.MassifAmplitudeM, T.MassifWavelengthCm,
		T.PeakAmplitudeM, T.PeakWavelengthCm, (float)T.PeakOctaves, T.PeakGain, T.PeakSoftness, T.GullyAmplitudeM, T.GullyWavelengthCm, (float)T.GullyOctaves, T.GullySlopeLow, T.GullySlopeHigh,
		T.CragAmplitudeM, T.CragWavelengthCm, T.CragSlopeLow, T.CragSlopeHigh, T.CragEdge })
	{
		H = FnvAddFloat(H, V);
	}
	H = FnvAddInt(H, T.bEnableUplandLakes ? 1 : 0);
	for (const float V : { T.UplandLakeCellCm, T.UplandLakeChance, T.UplandLakeRadiusMinCm, T.UplandLakeRadiusMaxCm, (float)T.UplandLakeCandidates, T.UplandLakeMinGroundAboveSeaCm,
		T.UplandLakeMaxReliefCm, T.UplandLakeFreeboardCm, T.UplandLakeDepthBaseCm, T.UplandLakeDepthPerRadius })
	{
		H = FnvAddFloat(H, V);
	}
	H = FnvAddInt(H, T.bEnableCaves ? 1 : 0);
	H = FnvAddInt(H, T.bCavesOnlyInLimestone ? 1 : 0);
	for (const float V : { T.CaveCoverCm, T.CaveFadeCm, T.CaveMaxDepthCm, T.CaveMinGroundAboveSeaCm, T.TunnelWavelengthCm, T.TunnelRadiusCm, T.TunnelRadiusVariation,
		T.TunnelVerticalSquash, T.CavernWavelengthCm, T.CavernThreshold, T.CavernMinDepthCm, T.SinkholeCellCm, T.SinkholeChance, T.SinkholeRadiusCm, T.SinkholeDepthCm,
		T.GalleryLevelSpacingCm, T.GalleryLevelAmplitudeCm, T.GalleryLevelWavelengthCm, T.GalleryMaskThreshold, T.CaveEntranceSlopeMinDeg, T.CaveEntranceSlopeFullDeg,
		T.CaveWaterClearanceCm, T.CaveWaterFadeCm, T.CaveShoreKeepOutCm })
	{
		H = FnvAddFloat(H, V);
	}
	// Built cave entrances (F30): only while they are on, so every world made without them keeps its identity (and its pinned checksums).
	if (T.CaveEntranceChance > 0.0f)
	{
		H = FnvAddFloat(FnvAddFloat(H, T.CaveEntranceCellCm), T.CaveEntranceChance);
	}
	for (const FVector2D& K : T.ContinentSplineM) { H = FnvAddFloat(FnvAddFloat(H, (float)K.X), (float)K.Y); }
	for (const FVector2D& K : T.ErosionReliefSpline) { H = FnvAddFloat(FnvAddFloat(H, (float)K.X), (float)K.Y); }
	for (const FMOWorldGenBiome& B : Biomes)
	{
		const FString Name = B.Id.ToString();
		H = FnvAdd(H, *Name, Name.Len() * (int32)sizeof(TCHAR));
		for (const float V : { B.HeightMin, B.HeightMax, B.SlopeMinDeg, B.SlopeMaxDeg, B.MoistureMin, B.MoistureMax, B.TemperatureMin, B.TemperatureMax, B.EdgeBlendWidth })
		{
			H = FnvAddFloat(H, V);
		}
		H = FnvAddInt(H, B.Priority);
		H = FnvAddFloat(FnvAddFloat(H, B.ReliefScale), B.MountainScale);
		// The ground surface does not change the generated numbers but it does change what the Voxel adapter must rebuild (its surface table).
		const FString Surface = B.GroundSurfaceType.ToString();
		H = FnvAdd(H, *Surface, Surface.Len() * (int32)sizeof(TCHAR));
	}
	if (ForcedBiomeIndex != INDEX_NONE)
	{
		H = FnvAddInt(H, 0x0F0C0000 + ForcedBiomeIndex);   // a biome test world (only then: an ordinary world keeps its hash)
	}
	return H;
}

int32 FMOWorldGenParams::FindBiomeIndex(FName BiomeId) const
{
	return Biomes.IndexOfByPredicate([BiomeId](const FMOWorldGenBiome& B) { return B.Id == BiomeId; });
}

// ============================================================================
// PUBLISHED SNAPSHOT
// ============================================================================

namespace
{
	FRWLock GParamsLock;
	TSharedPtr<const FMOWorldGenParams, ESPMode::ThreadSafe> GPublishedParams;
}

FMOWorldGenParamsRef FMOWorldGenParamsProvider::MakeDefault()
{
	static const FMOWorldGenParamsRef Default = []
	{
		TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> P = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
		P->Finalize();
		return FMOWorldGenParamsRef(P);
	}();
	return Default;
}

void FMOWorldGenParamsProvider::Publish(const FMOWorldGenParamsRef& NewParams)
{
	FWriteScopeLock Lock(GParamsLock);
	GPublishedParams = NewParams;
}

FMOWorldGenParamsRef FMOWorldGenParamsProvider::Get()
{
	{
		FReadScopeLock Lock(GParamsLock);
		if (GPublishedParams.IsValid())
		{
			return GPublishedParams.ToSharedRef();
		}
	}
	return MakeDefault();
}

bool FMOWorldGenParamsProvider::HasPublished()
{
	FReadScopeLock Lock(GParamsLock);
	return GPublishedParams.IsValid();
}

// ============================================================================
// NOISE (gradient noise with analytic derivatives, integer-hashed, literal gradient table)
// ============================================================================

namespace
{
	// 2D gradient noise with analytic derivatives lives in MOWorldGenNoise.h (shared with the cave and water halves of the generator).
	using FNoise = MOWorldGenNoise::FNoise2;
	FORCEINLINE FNoise GradientNoise(double X, double Y, uint32 Salt) { return MOWorldGenNoise::GradientNoise2(X, Y, Salt); }

	FORCEINLINE uint32 OctaveSalt(uint32 Base, int32 Octave)
	{
		return Base + (uint32)Octave * 0x9E3779B1u;
	}

	FORCEINLINE float SmoothStep(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	/** Rotate + double the lattice coordinates between octaves (breaks axis alignment). */
	FORCEINLINE void NextOctave(double& Px, double& Py)
	{
		const double Nx = 0.8 * Px - 0.6 * Py;
		const double Ny = 0.6 * Px + 0.8 * Py;
		Px = Nx * 2.0;
		Py = Ny * 2.0;
	}

	/**
	 * Plain fBm, normalised to ~unit std-dev regardless of octave count (sum / sqrt(sum of squared amplitudes)).
	 * Used for the large-scale climate fields (no LOD cut: they are low frequency).
	 */
	float Fbm(double X, double Y, uint32 Salt, int32 Octaves, float Gain, float WavelengthCm, float MinWavelengthCm = 0.0f)
	{
		double Px = X / WavelengthCm;
		double Py = Y / WavelengthCm;
		float Sum = 0.0f, Amp = 1.0f, SumSq = 0.0f, Wavelength = WavelengthCm;
		for (int32 K = 0; K < Octaves; ++K)
		{
			if (Wavelength >= MinWavelengthCm)   // a culled octave only loses its own energy (normalisation stays full): LOD-stable
			{
				Sum += Amp * GradientNoise(Px, Py, OctaveSalt(Salt, K)).Value;
			}
			SumSq += Amp * Amp;
			Amp *= Gain;
			Wavelength *= 0.5f;
			NextOctave(Px, Py);
		}
		return Sum / FMath::Sqrt(SumSq);
	}

	/**
	 * "Erosion" fBm after Inigo Quilez: each octave's contribution is damped by the accumulated slope, so slopes stay smooth
	 * and valley floors fill in. Octaves with wavelength < MinWavelengthCm are skipped (LOD). Normalised by the FULL amplitude
	 * sum so a culled octave only loses its own energy (LOD-stable). Result in [-1,1] (plain) or [0,1] (ridged).
	 */
	float ErodedFbm(double X, double Y, uint32 Salt, int32 Octaves, float Gain, float WavelengthCm, float MinWavelengthCm, bool bRidged, float Soft = 0.0f)
	{
		double Px = X / WavelengthCm;
		double Py = Y / WavelengthCm;
		float Sum = 0.0f, Amp = 1.0f, NormFull = 0.0f, Dx = 0.0f, Dy = 0.0f;
		float Wavelength = WavelengthCm;
		bool bCut = false;
		for (int32 K = 0; K < Octaves; ++K)
		{
			NormFull += Amp;
			if (!bCut && Wavelength < MinWavelengthCm)
			{
				bCut = true;
			}
			if (!bCut)
			{
				const FNoise N = GradientNoise(Px, Py, OctaveSalt(Salt, K));
				// Ridged: fold the value (1 - |v|) but accumulate the RAW gradient. Flipping the gradient's sign across the crease made the
				// damping term -- and therefore the height -- jump there (found by MOFramework.WorldGen.ContinuityAndRange).
				// Soft > 0 rounds the crest (pseudo-Huber |v|): a ridge gets a width instead of a knife edge. Soft == 0 is the exact old math.
				const float V = bRidged ? 1.0f - (Soft > 0.0f ? FMath::Sqrt(N.Value * N.Value + Soft * Soft) - Soft : FMath::Abs(N.Value)) : N.Value;
				Dx += N.Dx;
				Dy += N.Dy;
				Sum += Amp * V / (1.0f + Dx * Dx + Dy * Dy);
			}
			Amp *= Gain;
			Wavelength *= 0.5f;
			NextOctave(Px, Py);
		}
		return Sum / NormFull;
	}

	// Per-field salts (decorrelation hygiene: adding a field never shifts an existing one).
	enum : uint32
	{
		SaltWarpX = 0xA101u, SaltWarpY = 0xA102u, SaltContinent = 0xC001u, SaltErosion = 0xE002u, SaltWeird = 0xD003u,
		SaltMoisture = 0xB004u, SaltTemperature = 0xF005u, SaltMountain = 0x7006u, SaltHills = 0x5007u, SaltDetail = 0x3008u,
		SaltCoastRange = 0x4009u, SaltCoastPresence = 0x400Au,
		SaltMountainRegion = 0x7201u, SaltMassif = 0x7301u, SaltPeak = 0x7302u, SaltGully = 0x7303u, SaltCrag = 0x7304u, SaltMoistureRegion = 0xB105u, SaltTemperatureRegion = 0xF106u
	};

	/** Soft box membership: 1 inside [Lo, Hi], falling smoothly to 0 over Blend outside it (a band that spans all of 0..1 is 1 everywhere). */
	FORCEINLINE float SoftBand(float V, float Lo, float Hi, float Blend)
	{
		if (Blend <= 0.0f)
		{
			return (V >= Lo && V <= Hi) ? 1.0f : 0.0f;
		}
		return SmoothStep(Lo - Blend, Lo, V) * (1.0f - SmoothStep(Hi, Hi + Blend, V));
	}

	void Warp(const FMOWorldGenTuning& T, double& X, double& Y, int32 Seed)
	{
		if (T.WarpStrengthCm <= 0.0f)
		{
			return;
		}
		const double Px = X / T.WarpWavelengthCm;
		const double Py = Y / T.WarpWavelengthCm;
		const uint32 SX = (uint32)FMOWorldGen::HashSeed(Seed, SaltWarpX);
		const uint32 SY = (uint32)FMOWorldGen::HashSeed(Seed, SaltWarpY);
		// two octaves: a big lazy swirl plus a half-scale ripple
		const float Wx = GradientNoise(Px, Py, SX).Value + 0.5f * GradientNoise(Px * 2.0 + 11.7, Py * 2.0 - 5.3, SX + 1u).Value;
		const float Wy = GradientNoise(Px + 7.31, Py - 3.17, SY).Value + 0.5f * GradientNoise(Px * 2.0 - 4.1, Py * 2.0 + 9.9, SY + 1u).Value;
		X += (double)(T.WarpStrengthCm * Wx * 0.66f);
		Y += (double)(T.WarpStrengthCm * Wy * 0.66f);
	}

	FMOClimateSample ClimateWarped(const FMOWorldGenParams& P, double Xw, double Yw, int32 Seed)
	{
		const FMOWorldGenTuning& T = P.Tuning;
		auto Field = [&](uint32 Salt, int32 Octaves, float WavelengthCm)
		{
			return FMath::Clamp(Fbm(Xw, Yw, (uint32)FMOWorldGen::HashSeed(Seed, Salt), Octaves, 0.5f, WavelengthCm) * T.FieldContrast, -1.0f, 1.0f);
		};

		FMOClimateSample S;
		S.Continentalness = Field(SaltContinent, 4, T.ContinentWavelengthCm);
		if (T.EdgeFalloffEnd > T.EdgeFalloffStart)
		{
			// Island shaping: a squarish distance to the centre (p = 4 norm from two square roots: IEEE-exact), 0 at the middle, 1 at the border. The continent is pulled toward
			// open ocean (-1) between Start and End, so the land is gone before the border and the shore is organic (the position is the warped one).
			const float Nx = (float)(Xw / (double)T.WorldHalfSizeCm), Ny = (float)(Yw / (double)T.WorldHalfSizeCm);
			const float Nx2 = Nx * Nx, Ny2 = Ny * Ny;
			const float Edge = FMath::Sqrt(FMath::Sqrt(Nx2 * Nx2 + Ny2 * Ny2));
			const float Pull = SmoothStep(T.EdgeFalloffStart, T.EdgeFalloffEnd, Edge);
			S.Continentalness += (-1.0f - S.Continentalness) * Pull;
			S.EdgeLand = 1.0f - Pull;
		}
		S.Erosion = Field(SaltErosion, 3, T.ErosionWavelengthCm);
		const float Weird = Field(SaltWeird, 3, T.WeirdnessWavelengthCm);
		// Minecraft-style peaks & valleys: fold weirdness so both its extremes and its zero crossings differ.
		S.PeaksValleys = 1.0f - FMath::Abs(3.0f * FMath::Abs(Weird) - 2.0f);

		const float Coastal = 1.0f - SmoothStep(-0.25f, 0.25f, S.Continentalness);
		const float Moist = 0.5f + 0.45f * Fbm(Xw, Yw, (uint32)FMOWorldGen::HashSeed(Seed, SaltMoisture), 3, 0.5f, T.MoistureWavelengthCm) * T.FieldContrast;
		S.Moisture = FMath::Clamp(Moist + T.CoastalMoistureBoost * Coastal, 0.0f, 1.0f);

		float Temp = 0.5f + 0.45f * Fbm(Xw, Yw, (uint32)FMOWorldGen::HashSeed(Seed, SaltTemperature), 3, 0.5f, T.TemperatureWavelengthCm) * T.FieldContrast;
		Temp += 0.5f * T.LatitudeTemperatureGradient * (float)FMath::Clamp(Yw / (double)T.WorldHalfSizeCm, -1.0, 1.0);
		S.TemperatureSeaLevel = FMath::Clamp(Temp, 0.0f, 1.0f);

		// v6: the climate at RANGE scale, for the biome's hill / mountain scale only (the local patch is 2-3 km: a relief scale that follows it makes a cliff at every biome border).
		S.MoistureRegion = S.Moisture;
		S.TemperatureRegion = S.TemperatureSeaLevel;
		if (T.bRangeMountains && T.ReliefRegionWavelengthCm > 0.0f)
		{
			const float MoistRegion = 0.5f + 0.45f * Fbm(Xw, Yw, (uint32)FMOWorldGen::HashSeed(Seed, SaltMoistureRegion), 2, 0.5f, T.ReliefRegionWavelengthCm) * T.FieldContrast;
			S.MoistureRegion = FMath::Clamp(MoistRegion + T.CoastalMoistureBoost * Coastal, 0.0f, 1.0f);
			float TempRegion = 0.5f + 0.45f * Fbm(Xw, Yw, (uint32)FMOWorldGen::HashSeed(Seed, SaltTemperatureRegion), 2, 0.5f, T.ReliefRegionWavelengthCm) * T.FieldContrast;
			TempRegion += 0.5f * T.LatitudeTemperatureGradient * (float)FMath::Clamp(Yw / (double)T.WorldHalfSizeCm, -1.0, 1.0);
			S.TemperatureRegion = FMath::Clamp(TempRegion, 0.0f, 1.0f);
		}
		return S;
	}
}

// ============================================================================
// GENERATOR
// ============================================================================

namespace
{
	/** The v6 range term WITHOUT the gullies: massif + peaks, metres before VerticalScale (see FMOWorldGenTuning::bRangeMountains). */
	float RangeBaseM(const FMOWorldGenTuning& T, double X, double Y, int32 Seed, float MinWavelengthCm)
	{
		const float Mass = SmoothStep(-0.5f, 1.0f, Fbm(X, Y, (uint32)FMOWorldGen::HashSeed(Seed, SaltMassif), 2, 0.5f, T.MassifWavelengthCm, MinWavelengthCm));
		float Peak = 0.0f;
		if (T.PeakAmplitudeM > 0.0f)
		{
			Peak = FMath::Max(ErodedFbm(X, Y, (uint32)FMOWorldGen::HashSeed(Seed, SaltPeak), T.PeakOctaves, T.PeakGain, T.PeakWavelengthCm, MinWavelengthCm, true, T.PeakSoftness), 0.0f);
		}
		// Peaks need the massif under them: a thin skin of them stands on the plain, the full height only on the massif.
		const float Support = SmoothStep(0.05f, 0.75f, Mass);
		return T.MassifAmplitudeM * Mass + T.PeakAmplitudeM * Peak * (0.15f + 0.85f * Support);
	}

	/**
	 * The v6 mountain term in metres before VerticalScale and before the range mask / biome scale: massif + peaks, minus erosion gullies. The gullies are ridged valley lines cut where the
	 * base is steep (slope from a short finite difference of the same base terms), so they run down the flanks and leave the summits and the plains alone. Every fine octave (peak octaves,
	 * gullies) is skipped when its wavelength is below the LOD's resolution (2 x StepCm), so at a coarse LOD only the massif and the big peaks remain: the mass never collapses.
	 */
	float RangeShapeM(const FMOWorldGenTuning& T, double X, double Y, int32 Seed, float MinWavelengthCm)
	{
		const float Base0 = RangeBaseM(T, X, Y, Seed, MinWavelengthCm);
		float Base = Base0;
		// Gullies fade out with the LOD (see FMOWorldGenTuning::GullyAmplitudeM) and only exist where there is a mountain to cut (on the plain the base is ~0 and so is its slope: skip the extra
		// base evaluations, the hot path of every lowland column).
		const float GullyFade = T.GullyAmplitudeM > 0.0f ? SmoothStep(1.0f, 6.0f, T.GullyWavelengthCm / MinWavelengthCm) : 0.0f;
		const float CragFade = T.CragAmplitudeM > 0.0f ? SmoothStep(1.0f, 6.0f, T.CragWavelengthCm / MinWavelengthCm) : 0.0f;
		if ((GullyFade > 0.0f || CragFade > 0.0f) && Base0 > 25.0f)
		{
			constexpr double E = 6000.0;   // a 60 m baseline: the gullies are 600 m wide
			const float Bx = RangeBaseM(T, X + E, Y, Seed, MinWavelengthCm) - Base0;
			const float By = RangeBaseM(T, X, Y + E, Seed, MinWavelengthCm) - Base0;
			const float Slope = FMath::Sqrt(Bx * Bx + By * By) / (float)E * 100.0f * T.VerticalScale;   // rise over run as the player sees it (VerticalScale included)
			const float Steep = SmoothStep(T.GullySlopeLow, T.GullySlopeHigh, Slope);
			if (GullyFade > 0.0f && Steep > 0.0f)
			{
				const float R = ErodedFbm(X, Y, (uint32)FMOWorldGen::HashSeed(Seed, SaltGully), T.GullyOctaves, 0.5f, T.GullyWavelengthCm, MinWavelengthCm, true, 0.05f);
				Base -= T.GullyAmplitudeM * GullyFade * Steep * (1.0f - FMath::Clamp(R * 1.3f, 0.0f, 1.0f));
			}
			const float Ledge = SmoothStep(T.CragSlopeLow, T.CragSlopeHigh, Slope);
			if (CragFade > 0.0f && Ledge > 0.0f && T.CragAmplitudeM > 0.0f)
			{
				// v8 cliff bands: TERRACES of the ground's own height, so every riser faces DOWNHILL -- a drop-off, like rock strata. The v6/v7 crags were raised blobs with a
				// sharp rim all round: on a slope the rim's UPHILL side was a wall facing uphill with a trench behind it (playtest 2026-10-14 #27 / #28, "cliffs facing uphill").
				// Each step = a gentle bench (a quarter of the slope) then a steep riser over its top Riser fraction; continuous everywhere (no floor() seam). The band levels are
				// tilted by a broad noise (strata dip, not contour rings) and the terracing comes in soft-edged patches on the steep flanks only, never region-wide.
				// Natural, not a staircase: how sheer a riser is varies along the flank (some bands are sheer rock, some only a steeper roll) and the benches keep
				// 40 % of the slope.
				const float RiserNoise = 0.5f + 0.5f * Fbm(X, Y, (uint32)FMOWorldGen::HashSeed(Seed, SaltCrag + 2u), 2, 0.5f, T.CragWavelengthCm * 2.0f, MinWavelengthCm);
				const float Riser = FMath::Lerp(0.14f, 0.45f, FMath::Clamp(RiserNoise, 0.0f, 1.0f));
				const float Step = T.CragAmplitudeM;
				const float Tilt = Fbm(X, Y, (uint32)FMOWorldGen::HashSeed(Seed, SaltCrag + 1u), 2, 0.5f, T.CragWavelengthCm * 4.0f, MinWavelengthCm) * Step * 1.5f;
				const float Hs = (Base + Tilt) / Step;
				const float K = FMath::FloorToFloat(Hs);
				const float F = Hs - K;
				const float Terraced = (K + 0.4f * F + 0.6f * SmoothStep(1.0f - Riser, 1.0f, F)) * Step - Tilt;
				const float N = 0.5f + 0.5f * Fbm(X, Y, (uint32)FMOWorldGen::HashSeed(Seed, SaltCrag), 2, 0.5f, T.CragWavelengthCm, MinWavelengthCm);
				const float Patch = SmoothStep(T.CragEdge - 0.08f, T.CragEdge + 0.08f, N);
				Base += CragFade * Ledge * Patch * (Terraced - Base);
			}
		}
		return Base;
	}
}

int32 FMOWorldGen::HashSeed(int32 Seed, uint32 Salt)
{
	uint32 H = (uint32)Seed ^ (Salt * 0x9E3779B1u);
	H ^= H >> 16;
	H *= 0x85EBCA6Bu;
	H ^= H >> 13;
	H *= 0xC2B2AE35u;
	H ^= H >> 16;
	return (int32)H;
}

FMOClimateSample FMOWorldGen::SampleClimate(const FMOWorldGenParams& Params, double X, double Y, int32 Seed)
{
	double Xw = X, Yw = Y;
	Warp(Params.Tuning, Xw, Yw, Seed);
	return ClimateWarped(Params, Xw, Yw, Seed);
}

FMOBiomeRelief FMOWorldGen::BiomeReliefAt(const FMOWorldGenParams& Params, float Moisture, float TemperatureSeaLevel)
{
	FMOBiomeRelief Out;
	if (!Params.bBiomeRelief)
	{
		return Out;   // exactly 1 and 1: the v1-v4 height math is untouched
	}
	// Soft first-match-wins over the climate biomes (Params.Biomes is sorted highest Priority first): each claims what is left of the unit weight in proportion to how well the climate
	// fits its band; whatever nobody claims is neutral (1). Continuous in (Moisture, Temperature), so relief changes over the blend width, never at a line.
	const float Blend = Params.Tuning.ReliefClimateBlend;
	float Remaining = 1.0f, HillSum = 0.0f, MountainSum = 0.0f;
	for (const FMOWorldGenBiome& B : Params.Biomes)
	{
		if (!B.IsClimateBiome())
		{
			continue;
		}
		const float Fit = SoftBand(Moisture, B.MoistureMin, B.MoistureMax, Blend) * SoftBand(TemperatureSeaLevel, B.TemperatureMin, B.TemperatureMax, Blend);
		HillSum += Remaining * Fit * B.ReliefScale;
		MountainSum += Remaining * Fit * B.MountainScale;
		Remaining *= 1.0f - Fit;
		if (Remaining <= 1.0e-4f)
		{
			Remaining = 0.0f;
			break;
		}
	}
	Out.Hill = HillSum + Remaining;
	Out.Mountain = MountainSum + Remaining;
	return Out;
}

float FMOWorldGen::SampleBaseHeightCm(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float StepCm, FMOClimateSample* OutClimate)
{
	const FMOWorldGenTuning& T = Params.Tuning;

	double Xw = X, Yw = Y;
	Warp(T, Xw, Yw, Seed);
	const FMOClimateSample Clim = ClimateWarped(Params, Xw, Yw, Seed);
	if (OutClimate)
	{
		*OutClimate = Clim;
	}

	const float MinWavelengthCm = CVarMODebugNoLodCull.GetValueOnAnyThread() != 0 ? 2.0f : 2.0f * FMath::Max(StepCm, 1.0f);

	float BaseM = EvalSpline(T.ContinentSplineM, Clim.Continentalness);
	const float Inland = SmoothStep(-0.15f, 0.4f, Clim.Continentalness);
	const float Relief = FMath::Max(EvalSpline(T.ErosionReliefSpline, Clim.Erosion), 0.0f);
	const float PvFactor = FMath::Clamp(0.5f + 0.5f * Clim.PeaksValleys, 0.0f, 1.0f);
	// The climate's say in how hilly / how mountainous this ground is, faded to nothing at the world border (so the sea there is open water, not a field of islets). Exactly 1 away from both effects.
	const FMOBiomeRelief BiomeRelief = BiomeReliefAt(Params, Clim.MoistureRegion, Clim.TemperatureRegion);
	const float HillScale = BiomeRelief.Hill * Clim.EdgeLand, MountainScale = BiomeRelief.Mountain * Clim.EdgeLand;
	if (BiomeRelief.Hill < 1.0f)
	{
		// A calm biome flattens the continent too: the land above the lowland cap is pulled toward it (never when the scale is exactly 1: the v1-v4 bits stay).
		const float FlatM = FMath::Min(BaseM, T.LowlandCapM);
		BaseM = FlatM + BiomeRelief.Hill * (BaseM - FlatM);
	}

	// Mountains are a REGIONAL feature: where the land is inland, unworn (low erosion) and the peaks-and-valleys field agrees, a smoothstep turns
	// the ranges fully on (full amplitude + a raised base); elsewhere there are none at all. (A plain product of the fields averaged ~7% of the
	// amplitude everywhere -- hills, never mountains.)
	float MountainM = 0.0f;
	if (T.bRangeMountains)
	{
		// v6: ranges stand where a broad regional field says so (wide soft ramps, never a wall), only inland, scaled by the biome's REGIONAL mountain scale.
		const float RegionField = FMath::Clamp(Fbm(Xw, Yw, (uint32)HashSeed(Seed, SaltMountainRegion), 2, 0.5f, T.MountainRegionWavelengthCm) * T.MountainRegionGain, -1.0f, 1.0f);
		const float RangeMask = SmoothStep(T.MountainRegionLow, T.MountainRegionHigh, RegionField) * Inland;
		if (RangeMask > 0.0f && MountainScale > 0.0f)
		{
			MountainM = RangeMask * MountainScale * RangeShapeM(T, Xw, Yw, Seed, MinWavelengthCm);
		}
	}
	else
	{
		const float Mountainous = SmoothStep(0.2f, 0.75f, Inland * Relief * (0.6f + 0.4f * PvFactor));
		if (T.MountainAmplitudeM > 0.0f && Mountainous > 0.0f)
		{
			const float Ridge = ErodedFbm(Xw, Yw, (uint32)HashSeed(Seed, SaltMountain), 6, T.MountainGain, T.MountainWavelengthCm, MinWavelengthCm, true);
			MountainM = T.MountainAmplitudeM * Mountainous * Ridge * MountainScale;
		}
	}

	// Coastal ranges: a band that starts at the shore (continentalness ~ -0.55, where the spline crosses sea level) and fades out as the continental mountains take over, a slow
	// presence field that decides which stretches of coast have ranges at all, and the same ridged, slope-damped fBm the mountains use. All smooth: no cliff anywhere.
	float CoastalM = 0.0f;
	if (T.CoastalRangeAmplitudeM > 0.0f)
	{
		const float Band = SmoothStep(-0.52f, -0.28f, Clim.Continentalness) * (1.0f - SmoothStep(-0.05f, 0.3f, Clim.Continentalness));
		if (Band > 0.0f)
		{
			const float Presence = SmoothStep(-0.3f, 0.6f, Fbm(Xw, Yw, (uint32)HashSeed(Seed, SaltCoastPresence), 2, 0.5f, T.CoastalRangePresenceWavelengthCm));
			if (Presence > 0.0f)
			{
				const float Ridge = ErodedFbm(Xw, Yw, (uint32)HashSeed(Seed, SaltCoastRange), 6, 0.5f, T.CoastalRangeWavelengthCm, MinWavelengthCm, true);
				CoastalM = T.CoastalRangeAmplitudeM * Band * Presence * Ridge * MountainScale;
			}
		}
	}

	float HillM = 0.0f;
	if (T.HillAmplitudeM > 0.0f)
	{
		// Over open sea the hill noise fades out (HillSeaFadeLow/High, see the tuning): exactly 1 when the fade is off, so the v1-v4 bits stay.
		const float HillSeaFade = T.HillSeaFadeHigh > T.HillSeaFadeLow ? SmoothStep(T.HillSeaFadeLow, T.HillSeaFadeHigh, Clim.Continentalness) : 1.0f;
		HillM = T.HillAmplitudeM * (0.35f + 0.65f * FMath::Min(Relief, 1.0f)) * HillScale * HillSeaFade
			* ErodedFbm(Xw, Yw, (uint32)HashSeed(Seed, SaltHills), 4, 0.5f, T.HillWavelengthCm, MinWavelengthCm, false);
	}

	float DetailM = 0.0f;
	if (T.DetailAmplitudeM > 0.0f)
	{
		DetailM = T.DetailAmplitudeM * Fbm(Xw, Yw, (uint32)HashSeed(Seed, SaltDetail), 3, 0.5f, T.DetailWavelengthCm, MinWavelengthCm) * 1.6f;
		DetailM = FMath::Clamp(DetailM, -T.DetailAmplitudeM, T.DetailAmplitudeM);
	}

	const float HeightCm = T.SeaLevelCm + 100.0f * T.VerticalScale * (BaseM + MountainM + CoastalM + HillM + DetailM);
	return FMath::Clamp(HeightCm, Params.MinHeightCm, Params.MaxHeightCm);
}

float FMOWorldGen::SampleHeightCm(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float StepCm, FMOClimateSample* OutClimate)
{
	return ApplyWater(Params, X, Y, Seed, SampleBaseHeightCm(Params, X, Y, Seed, StepCm, OutClimate), StepCm);
}

bool FMOWorldGen::IsWaterTierResolved(const FMOWorldGenParams& Params, int32 Tier, float StepCm)
{
	if (CVarMODebugNoLodCull.GetValueOnAnyThread() != 0 || CVarMOWaterLodGate.GetValueOnAnyThread() == 0)
	{
		return true;
	}
	const FMOWorldGenTuning& T = Params.Tuning;
	const float MaxRadiusCm = Tier == 0 ? T.PondRadiusMaxCm : Tier == 1 ? T.LakeRadiusMaxCm : T.UplandLakeRadiusMaxCm;
	return StepCm < MaxRadiusCm;
}

float FMOWorldGen::TemperatureAtHeight(const FMOWorldGenParams& Params, float TemperatureSeaLevel, float HeightCm)
{
	const float AboveSeaKm = FMath::Max(HeightCm - Params.Tuning.SeaLevelCm, 0.0f) / 100000.0f;
	return FMath::Clamp(TemperatureSeaLevel - Params.Tuning.TemperatureLapsePerKm * AboveSeaKm, 0.0f, 1.0f);
}

int32 FMOWorldGen::ResolveBiome(const FMOWorldGenParams& Params, float HeightCm, float SlopeDeg, float Moisture, float Temperature)
{
	if (Params.ForcedBiomeIndex != INDEX_NONE)
	{
		return Params.ForcedBiomeIndex;   // a biome test world (mo.WorldGen.ForceBiome)
	}
	for (int32 I = 0; I < Params.Biomes.Num(); ++I)
	{
		if (Params.Biomes[I].Contains(HeightCm, SlopeDeg, Moisture, Temperature))
		{
			return I;
		}
	}
	return INDEX_NONE;
}

float FMOWorldGen::SoftThreshold(float Value, float Edge, float Width, float Breakup)
{
	const float E = Edge + Breakup;
	if (Width <= 0.0f)
	{
		return Value >= E ? 1.0f : 0.0f;
	}
	return MOWorldGenNoise::Smooth01(E - 0.5f * Width, E + 0.5f * Width, Value);
}

int32 FMOWorldGen::SampleSurfaceBiomeWeights(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float HeightCm, float Moisture, float Temperature, FBiomeWeight* Out, int32 MaxOut)
{
	if (!Out || MaxOut <= 0 || Params.Biomes.Num() == 0)
	{
		return 0;
	}
	if (Params.ForcedBiomeIndex != INDEX_NONE)
	{
		Out[0] = { Params.ForcedBiomeIndex, 1.0f };   // a biome test world: one biome everywhere
		return 1;
	}
	const FMOWorldGenTuning& T = Params.Tuning;
	// One breakup sample per column, shared by every edge (a second, offset read for the climate edges so height and climate edges do not wander in lockstep).
	// Three octaves: the first bends the edge at the scale of the band, the finer two fray it into fingers and islands (one octave left a smooth curve).
	constexpr uint32 SaltSurfaceBreakup = 0x5A01u;
	const double Inv = 1.0 / FMath::Max((double)T.SurfaceBreakupWavelengthCm, 100.0);
	const auto Breakup = [Seed, Inv](double Px, double Py, uint32 Salt)
	{
		return 0.62f * MOWorldGenNoise::GradientNoise2(Px * Inv, Py * Inv, (uint32)HashSeed(Seed, Salt)).Value
			+ 0.27f * MOWorldGenNoise::GradientNoise2(Px * Inv * 2.6 + 41.3, Py * Inv * 2.6 - 17.9, (uint32)HashSeed(Seed, Salt + 0x100u)).Value
			+ 0.11f * MOWorldGenNoise::GradientNoise2(Px * Inv * 6.8 - 3.1, Py * Inv * 6.8 + 29.5, (uint32)HashSeed(Seed, Salt + 0x200u)).Value;
	};
	const float NoiseH = Breakup(X, Y, SaltSurfaceBreakup);
	const float NoiseC = Breakup(X * 0.7 + 13.7 / Inv, Y * 0.7 - 5.3 / Inv, SaltSurfaceBreakup + 1u);
	const float WC = T.SurfaceClimateBlend;
	const float BreakC = NoiseC * T.SurfaceBreakup * 0.5f * WC;
	// A height edge's band scales with how high above the sea it lies (see SurfaceHeightBlendFraction); its breakup scales with its band.
	const auto HeightBand = [&T](float Edge)
	{
		return FMath::Clamp(T.SurfaceHeightBlendFraction * FMath::Max(Edge - T.SeaLevelCm, 0.0f), T.SurfaceHeightBlendMinCm, FMath::Max(T.SurfaceHeightBlendCm, T.SurfaceHeightBlendMinCm));
	};
	const auto HeightInside = [&](float V, float Lo, float Hi, bool bLoOpen, bool bHiOpen)
	{
		const float WLo = HeightBand(Lo), WHi = HeightBand(Hi);
		const float AboveLo = bLoOpen ? 1.0f : SoftThreshold(V, Lo, WLo, NoiseH * T.SurfaceBreakup * 0.5f * WLo);
		const float BelowHi = bHiOpen ? 1.0f : 1.0f - SoftThreshold(V, Hi, WHi, NoiseH * T.SurfaceBreakup * 0.5f * WHi);
		return AboveLo * BelowHi;
	};

	// Soft "inside [Lo, Hi]": an open end (the band's default sentinel) is no edge.
	const auto Inside = [](float V, float Lo, float Hi, bool bLoOpen, bool bHiOpen, float W, float Break)
	{
		const float AboveLo = bLoOpen ? 1.0f : SoftThreshold(V, Lo, W, Break);
		const float BelowHi = bHiOpen ? 1.0f : 1.0f - SoftThreshold(V, Hi, W, Break);
		return AboveLo * BelowHi;
	};

	// First-match-wins, softly: in priority order each biome claims its membership of what is left (ResolveBiome's hard rule is the W -> 0 limit).
	TArray<FBiomeWeight, TInlineAllocator<16>> All;
	float Remaining = 1.0f;
	for (int32 I = 0; I < Params.Biomes.Num() && Remaining > 1.0e-4f; ++I)
	{
		const FMOWorldGenBiome& B = Params.Biomes[I];
		if (B.SlopeMinDeg > 0.0f)
		{
			continue;   // a slope band never applies at SampleColumn's slope 0
		}
		const float M = HeightInside(HeightCm, B.HeightMin, B.HeightMax, B.HeightMin <= -50000.0f, B.HeightMax >= 50000.0f)
			* Inside(Moisture, B.MoistureMin, B.MoistureMax, B.MoistureMin <= 0.0f, B.MoistureMax >= 1.0f, WC, BreakC)
			* Inside(Temperature, B.TemperatureMin, B.TemperatureMax, B.TemperatureMin <= 0.0f, B.TemperatureMax >= 1.0f, WC, -BreakC);
		const float W = Remaining * M;
		if (W > 1.0e-4f)
		{
			All.Add({ I, W });
			Remaining -= W;
		}
	}
	if (All.Num() == 0)
	{
		const int32 Hard = ResolveBiome(Params, HeightCm, 0.0f, Moisture, Temperature);
		if (Hard == INDEX_NONE)
		{
			return 0;
		}
		Out[0] = { Hard, 1.0f };
		return 1;
	}
	All.Sort([](const FBiomeWeight& A, const FBiomeWeight& Bw) { return A.Weight > Bw.Weight || (A.Weight == Bw.Weight && A.Biome < Bw.Biome); });
	const int32 N = FMath::Min(All.Num(), MaxOut);
	float Sum = 0.0f;
	for (int32 K = 0; K < N; ++K)
	{
		Sum += All[K].Weight;
	}
	for (int32 K = 0; K < N; ++K)
	{
		Out[K] = { All[K].Biome, All[K].Weight / Sum };
	}
	return N;
}

int32 FMOWorldGen::PickSurfaceBiome(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float HeightCm, float SlopeDeg, float Moisture, float Temperature)
{
	const int32 Hard = ResolveBiome(Params, HeightCm, SlopeDeg, Moisture, Temperature);
	if (Hard != INDEX_NONE && Params.Biomes[Hard].SlopeMinDeg > 0.0f)
	{
		return Hard;   // a slope band (cliff rock) keeps its crisp edge
	}
	FBiomeWeight W[3];
	const int32 N = SampleSurfaceBiomeWeights(Params, X, Y, Seed, HeightCm, Moisture, Temperature, W, 3);
	float Sum = 0.0f;
	for (int32 K = 0; K < N; ++K)
	{
		const FMOWorldGenBiome& B = Params.Biomes[W[K].Biome];
		if (SlopeDeg < B.SlopeMinDeg || SlopeDeg > B.SlopeMaxDeg)
		{
			W[K].Weight = 0.0f;   // a biome that would never hold this slope does not get the point
		}
		Sum += W[K].Weight;
	}
	if (Sum <= 0.0f)
	{
		return Hard;
	}
	constexpr uint32 SaltSurfacePick = 0x5A10u;
	const uint32 Bits = MOWorldGenNoise::HashLattice2((int64)FMath::FloorToDouble(X * 0.1), (int64)FMath::FloorToDouble(Y * 0.1), (uint32)HashSeed(Seed, SaltSurfacePick));
	float Roll = (float)(Bits >> 8) * (1.0f / 16777216.0f) * Sum;   // [0, Sum)
	for (int32 K = 0; K < N; ++K)
	{
		if (W[K].Weight > 0.0f && Roll < W[K].Weight)
		{
			return W[K].Biome;
		}
		Roll -= W[K].Weight;
	}
	for (int32 K = N - 1; K >= 0; --K)   // float rounding left the roll just past the last share
	{
		if (W[K].Weight > 0.0f)
		{
			return W[K].Biome;
		}
	}
	return Hard;
}

FMOTerrainSample FMOWorldGen::SampleColumn(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float StepCm)
{
	FMOClimateSample Clim;
	const float Height = SampleHeightCm(Params, X, Y, Seed, StepCm, &Clim);

	FMOTerrainSample S;
	S.HeightCm = Height;
	S.Moisture = Clim.Moisture;
	S.Temperature = TemperatureAtHeight(Params, Clim.TemperatureSeaLevel, Height);
	S.BiomeIndex = ResolveBiome(Params, Height, 0.0f, S.Moisture, S.Temperature);
	S.Continentalness = Clim.Continentalness;
	S.Erosion = Clim.Erosion;
	return S;
}
