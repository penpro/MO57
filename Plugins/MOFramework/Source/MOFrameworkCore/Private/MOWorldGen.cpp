// See MOWorldGen.h for the rules this file must obey (pure, deterministic, thread-safe, IEEE-exact ops only).

#include "MOWorldGen.h"

#include "Misc/ScopeRWLock.h"

// ============================================================================
// TUNING DEFAULTS
// ============================================================================

FMOWorldGenTuning::FMOWorldGenTuning()
{
	// Continentalness -> metres above sea level. Basins are modest (there is no water in the game yet; they are lake-ready).
	ContinentSplineM = {
		FVector2D(-1.00, -6.0), FVector2D(-0.55, 0.0), FVector2D(-0.30, 6.0), FVector2D(-0.15, 12.0),
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

	FMOWorldGenTuning& T = Tuning;
	T.ContinentSplineM.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X; });
	T.ErosionReliefSpline.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X; });

	float MinBase = 0.0f, MaxBase = 0.0f, MinRelief = 0.0f, MaxRelief = 0.0f;
	SplineRange(T.ContinentSplineM, MinBase, MaxBase);
	SplineRange(T.ErosionReliefSpline, MinRelief, MaxRelief);
	MaxRelief = FMath::Max(MaxRelief, 0.0f);

	// Every noise term is bounded by 1 (see the generator), so these are hard bounds, not estimates.
	MaxHeightCm = T.SeaLevelCm + 100.0f * (MaxBase + T.MountainAmplitudeM * MaxRelief + T.HillAmplitudeM + T.DetailAmplitudeM);
	MinHeightCm = T.SeaLevelCm + 100.0f * (MinBase - T.HillAmplitudeM - T.DetailAmplitudeM);
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
		// The ground surface does not change the generated numbers but it does change what the Voxel adapter must rebuild (its surface table).
		const FString Surface = B.GroundSurfaceType.ToString();
		H = FnvAdd(H, *Surface, Surface.Len() * (int32)sizeof(TCHAR));
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
	// 16 unit gradient directions, literal constants (no sin/cos at runtime: see RULES in the header).
	constexpr float GGrad[16][2] = {
		{ 1.0f, 0.0f }, { 0.92387953f, 0.38268343f }, { 0.70710678f, 0.70710678f }, { 0.38268343f, 0.92387953f },
		{ 0.0f, 1.0f }, { -0.38268343f, 0.92387953f }, { -0.70710678f, 0.70710678f }, { -0.92387953f, 0.38268343f },
		{ -1.0f, 0.0f }, { -0.92387953f, -0.38268343f }, { -0.70710678f, -0.70710678f }, { -0.38268343f, -0.92387953f },
		{ 0.0f, -1.0f }, { 0.38268343f, -0.92387953f }, { 0.70710678f, -0.70710678f }, { 0.92387953f, -0.38268343f } };

	FORCEINLINE uint32 HashLattice(int64 X, int64 Y, uint32 Salt)
	{
		uint64 H = (uint64)X * 0x9E3779B97F4A7C15ull;
		H ^= ((uint64)Y + 0xC2B2AE3D27D4EB4Full) * 0xD6E8FEB86659FD93ull;
		H ^= (uint64)Salt * 0xFF51AFD7ED558CCDull;
		H ^= H >> 32;
		H *= 0xD6E8FEB86659FD93ull;
		H ^= H >> 32;
		H *= 0xD6E8FEB86659FD93ull;
		H ^= H >> 32;
		return (uint32)H;
	}

	struct FNoise
	{
		float Value;
		float Dx;
		float Dy;
	};

	/** Output range ~[-1, 1] (unit-gradient 2D noise peaks at sqrt(2)/2, scaled up). Derivatives are w.r.t. the input coordinates. */
	FNoise GradientNoise(double X, double Y, uint32 Salt)
	{
		const double FloorX = FMath::FloorToDouble(X);
		const double FloorY = FMath::FloorToDouble(Y);
		const int64 Xi = (int64)FloorX;
		const int64 Yi = (int64)FloorY;
		const float Fx = (float)(X - FloorX);
		const float Fy = (float)(Y - FloorY);

		const float Ux = Fx * Fx * Fx * (Fx * (Fx * 6.0f - 15.0f) + 10.0f);
		const float Uy = Fy * Fy * Fy * (Fy * (Fy * 6.0f - 15.0f) + 10.0f);
		const float Dux = 30.0f * Fx * Fx * (Fx - 1.0f) * (Fx - 1.0f);
		const float Duy = 30.0f * Fy * Fy * (Fy - 1.0f) * (Fy - 1.0f);

		const float* Ga = GGrad[HashLattice(Xi, Yi, Salt) & 15u];
		const float* Gb = GGrad[HashLattice(Xi + 1, Yi, Salt) & 15u];
		const float* Gc = GGrad[HashLattice(Xi, Yi + 1, Salt) & 15u];
		const float* Gd = GGrad[HashLattice(Xi + 1, Yi + 1, Salt) & 15u];

		const float Va = Ga[0] * Fx + Ga[1] * Fy;
		const float Vb = Gb[0] * (Fx - 1.0f) + Gb[1] * Fy;
		const float Vc = Gc[0] * Fx + Gc[1] * (Fy - 1.0f);
		const float Vd = Gd[0] * (Fx - 1.0f) + Gd[1] * (Fy - 1.0f);

		const float K = Va - Vb - Vc + Vd;
		constexpr float Scale = 1.41421356f;

		FNoise Result;
		Result.Value = Scale * (Va + Ux * (Vb - Va) + Uy * (Vc - Va) + Ux * Uy * K);
		Result.Dx = Scale * (Ga[0] + Ux * (Gb[0] - Ga[0]) + Uy * (Gc[0] - Ga[0]) + Ux * Uy * (Ga[0] - Gb[0] - Gc[0] + Gd[0]) + Dux * (Uy * K + Vb - Va));
		Result.Dy = Scale * (Ga[1] + Ux * (Gb[1] - Ga[1]) + Uy * (Gc[1] - Ga[1]) + Ux * Uy * (Ga[1] - Gb[1] - Gc[1] + Gd[1]) + Duy * (Ux * K + Vc - Va));
		return Result;
	}

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
	float ErodedFbm(double X, double Y, uint32 Salt, int32 Octaves, float Gain, float WavelengthCm, float MinWavelengthCm, bool bRidged)
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
				const float V = bRidged ? 1.0f - FMath::Abs(N.Value) : N.Value;
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
		SaltMoisture = 0xB004u, SaltTemperature = 0xF005u, SaltMountain = 0x7006u, SaltHills = 0x5007u, SaltDetail = 0x3008u
	};

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
		return S;
	}
}

// ============================================================================
// GENERATOR
// ============================================================================

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

float FMOWorldGen::SampleHeightCm(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float StepCm, FMOClimateSample* OutClimate)
{
	const FMOWorldGenTuning& T = Params.Tuning;

	double Xw = X, Yw = Y;
	Warp(T, Xw, Yw, Seed);
	const FMOClimateSample Clim = ClimateWarped(Params, Xw, Yw, Seed);
	if (OutClimate)
	{
		*OutClimate = Clim;
	}

	const float MinWavelengthCm = 2.0f * FMath::Max(StepCm, 1.0f);

	const float BaseM = EvalSpline(T.ContinentSplineM, Clim.Continentalness);
	const float Inland = SmoothStep(-0.15f, 0.4f, Clim.Continentalness);
	const float Relief = FMath::Max(EvalSpline(T.ErosionReliefSpline, Clim.Erosion), 0.0f);
	const float PvFactor = FMath::Clamp(0.5f + 0.5f * Clim.PeaksValleys, 0.0f, 1.0f);

	// Mountains are a REGIONAL feature: where the land is inland, unworn (low erosion) and the peaks-and-valleys field agrees, a smoothstep turns
	// the ranges fully on (full amplitude + a raised base); elsewhere there are none at all. (A plain product of the fields averaged ~7% of the
	// amplitude everywhere -- hills, never mountains.)
	float MountainM = 0.0f;
	const float Mountainous = SmoothStep(0.2f, 0.75f, Inland * Relief * (0.6f + 0.4f * PvFactor));
	if (T.MountainAmplitudeM > 0.0f && Mountainous > 0.0f)
	{
		const float Ridge = ErodedFbm(Xw, Yw, (uint32)HashSeed(Seed, SaltMountain), 6, 0.5f, T.MountainWavelengthCm, MinWavelengthCm, true);
		MountainM = T.MountainAmplitudeM * Mountainous * Ridge;
	}

	float HillM = 0.0f;
	if (T.HillAmplitudeM > 0.0f)
	{
		HillM = T.HillAmplitudeM * (0.35f + 0.65f * FMath::Min(Relief, 1.0f))
			* ErodedFbm(Xw, Yw, (uint32)HashSeed(Seed, SaltHills), 4, 0.5f, T.HillWavelengthCm, MinWavelengthCm, false);
	}

	float DetailM = 0.0f;
	if (T.DetailAmplitudeM > 0.0f)
	{
		DetailM = T.DetailAmplitudeM * Fbm(Xw, Yw, (uint32)HashSeed(Seed, SaltDetail), 3, 0.5f, T.DetailWavelengthCm, MinWavelengthCm) * 1.6f;
		DetailM = FMath::Clamp(DetailM, -T.DetailAmplitudeM, T.DetailAmplitudeM);
	}

	const float HeightCm = T.SeaLevelCm + 100.0f * (BaseM + MountainM + HillM + DetailM);
	return FMath::Clamp(HeightCm, Params.MinHeightCm, Params.MaxHeightCm);
}

float FMOWorldGen::TemperatureAtHeight(const FMOWorldGenParams& Params, float TemperatureSeaLevel, float HeightCm)
{
	const float AboveSeaKm = FMath::Max(HeightCm - Params.Tuning.SeaLevelCm, 0.0f) / 100000.0f;
	return FMath::Clamp(TemperatureSeaLevel - Params.Tuning.TemperatureLapsePerKm * AboveSeaKm, 0.0f, 1.0f);
}

int32 FMOWorldGen::ResolveBiome(const FMOWorldGenParams& Params, float HeightCm, float SlopeDeg, float Moisture, float Temperature)
{
	for (int32 I = 0; I < Params.Biomes.Num(); ++I)
	{
		if (Params.Biomes[I].Contains(HeightCm, SlopeDeg, Moisture, Temperature))
		{
			return I;
		}
	}
	return INDEX_NONE;
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
