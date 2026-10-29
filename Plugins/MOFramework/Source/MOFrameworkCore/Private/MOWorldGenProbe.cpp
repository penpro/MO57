// The fixed fingerprint of the world generator. Same rules as MOWorldGen.h. Do NOT change the sample sets or hashing here casually: the expected values live in
// MOFramework.WorldGen.GoldenChecksum / CaveGoldenChecksum and in whatever notes people took when they compared machines.

#include "MOWorldGen.h"

#include <atomic>

namespace
{
	constexpr int32 ProbeSeed = 424242;

	FORCEINLINE uint32 FnvFloat(uint32 H, float V)
	{
		const uint8* B = reinterpret_cast<const uint8*>(&V);
		for (int32 I = 0; I < 4; ++I)
		{
			H = (H ^ B[I]) * 16777619u;
		}
		return H;
	}

	/** Default tuning + the three stock biome bands (what DT_Biomes held when the probe was written). */
	FMOWorldGenParamsRef MakeProbeParams(bool bWater)
	{
		TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> P = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
		P->Tuning.bEnableWater = bWater;
		if (!bWater)
		{
			// The water-off probe is the v1 ANCHOR (PinnedProbeHeightNoWater): the base math as it shipped, before any later feature.
			P->Tuning.CoastalRangeAmplitudeM = 0.0f;
			P->Tuning.VerticalScale = 1.0f;
			// v5 moved these defaults; the anchor keeps the v1 values (the biomes below have ReliefScale 1 here, so the relief term is skipped entirely).
			P->Tuning.MountainAmplitudeM = 320.0f;
			P->Tuning.MountainWavelengthCm = 800000.0f;
			P->Tuning.MountainGain = 0.5f;
			P->Tuning.ErosionWavelengthCm = 900000.0f;
			P->Tuning.WeirdnessWavelengthCm = 600000.0f;
			P->Tuning.MoistureWavelengthCm = 700000.0f;
			P->Tuning.TemperatureWavelengthCm = 1500000.0f;
			P->Tuning.EdgeFalloffStart = 2.0f;
			P->Tuning.EdgeFalloffEnd = 2.0f;
			P->Tuning.HillSeaFadeLow = 2.0f;
			P->Tuning.HillSeaFadeHigh = 2.0f;
			P->Tuning.bRangeMountains = false;   // v6 moved the mountains to range scale; the anchor keeps the v1-v5 mountain math
			// The v1 continent spline: a sea floor at -6 m.
			P->Tuning.ContinentSplineM = {
				FVector2D(-1.00, -6.0), FVector2D(-0.55, 0.0), FVector2D(-0.30, 6.0), FVector2D(-0.15, 12.0),
				FVector2D(0.00, 18.0), FVector2D(0.25, 32.0), FVector2D(0.55, 55.0), FVector2D(1.00, 90.0) };
		}
		auto Add = [&P](const TCHAR* Id, int32 Priority, float SlopeMin, float SlopeMax, float MoistureMin, float ReliefScale, float MountainScale)
		{
			FMOWorldGenBiome& B = P->Biomes.AddDefaulted_GetRef();
			B.Id = FName(Id);
			B.Priority = Priority;
			B.SlopeMinDeg = SlopeMin;
			B.SlopeMaxDeg = SlopeMax;
			B.MoistureMin = MoistureMin;
			B.ReliefScale = ReliefScale;
			B.MountainScale = MountainScale;
		};
		// The full probe also pins the v5 biome relief (a prairie stays flat); the anchor leaves it at 1.
		Add(TEXT("Meadow"), 1, 0.0f, 90.0f, 0.0f, bWater ? 0.30f : 1.0f, bWater ? 0.03f : 1.0f);
		Add(TEXT("TemperateForest"), 10, 0.0f, 45.0f, 0.4f, bWater ? 1.2f : 1.0f, bWater ? 1.3f : 1.0f);
		Add(TEXT("RockyHighland"), 20, 28.0f, 90.0f, 0.0f, 1.0f, 1.0f);
		P->Finalize();
		return FMOWorldGenParamsRef(P);
	}
}

FMOWorldGenProbe FMOWorldGenDomain::RunProbe(bool bWater)
{
	const FMOWorldGenParamsRef Params = MakeProbeParams(bWater);
	const FMOWorldGenParams& P = *Params;

	FMOWorldGenProbe Probe;
	Probe.ParamsHash = P.ComputeHash();
	Probe.GeneratorVersion = P.GeneratorVersion;

	uint32 H = 2166136261u;
	for (int32 Y = 0; Y < 64; ++Y)
	{
		for (int32 X = 0; X < 64; ++X)
		{
			const FMOTerrainSample S = SampleColumn(P, -3.0e6 + 97003.0 * X, -3.0e6 + 97001.0 * Y, ProbeSeed, 100.0f);
			H = FnvFloat(H, S.HeightCm);
			H = FnvFloat(H, S.Moisture);
			H = FnvFloat(H, S.Temperature);
			H = (H ^ (uint32)(S.BiomeIndex + 1)) * 16777619u;
		}
	}
	if (bWater)
	{
		// The water half: every lake / pond / upland-lake site in a 26 x 26 block of each tier's cells (the sea-level tiers are in the grid above only where it happens to land).
		for (int32 Tier = 0; Tier < 3; ++Tier)
		{
			for (int64 Cy = -13; Cy < 13; ++Cy)
			{
				for (int64 Cx = -13; Cx < 13; ++Cx)
				{
					FLake L;
					if (GetLakeInCell(P, Tier, Cx, Cy, ProbeSeed, L))
					{
						H = FnvFloat(FnvFloat(FnvFloat(H, (float)L.Center.X), (float)L.Center.Y), L.RadiusCm);
						H = FnvFloat(FnvFloat(FnvFloat(H, L.Aspect), L.SurfaceCm), L.DepthCm);
					}
				}
			}
		}
	}
	Probe.Height = H;

	uint32 C = 2166136261u;
	for (int32 Iz = 0; Iz < 8; ++Iz)
	{
		for (int32 Iy = 0; Iy < 24; ++Iy)
		{
			for (int32 Ix = 0; Ix < 24; ++Ix)
			{
				const double X = -400000.0 + 33331.0 * Ix, Y = -400000.0 + 33343.0 * Iy;
				const float Ground = SampleHeightCm(P, X, Y, ProbeSeed);
				C = FnvFloat(C, SampleCaveDistanceAtGround(P, X, Y, Ground - 1500.0 - 1500.0 * Iz, Ground, ProbeSeed));
			}
		}
	}
	for (int64 Cy = -8; Cy < 8; ++Cy)
	{
		for (int64 Cx = -8; Cx < 8; ++Cx)
		{
			FMOSinkhole S;
			if (GetSinkholeInCell(P, Cx, Cy, ProbeSeed, S))
			{
				C = FnvFloat(FnvFloat(FnvFloat(C, (float)S.Center.X), (float)S.Center.Y), S.GroundCm);
			}
		}
	}
	Probe.Cave = C;
	return Probe;
}

namespace
{
	std::atomic<const FMOWorldGenSelfCheck*> GSelfCheckDone{ nullptr };
}

const FMOWorldGenSelfCheck* FMOWorldGenDomain::SelfCheckIfDone()
{
	return GSelfCheckDone.load(std::memory_order_acquire);
}

const FMOWorldGenSelfCheck& FMOWorldGenDomain::SelfCheck()
{
	// A function-local static: worked out once, by whichever thread asks first; the others wait for it.
	static const FMOWorldGenSelfCheck Check = []()
	{
		FMOWorldGenSelfCheck Result;
		const double Start = FPlatformTime::Seconds();
		Result.Probe = RunProbe(true);
		Result.BaseHeight = RunProbe(false).Height;
		Result.Seconds = FPlatformTime::Seconds() - Start;
		Result.bHeightMatch = Result.Probe.Height == PinnedProbeHeight;
		Result.bBaseMatch = Result.BaseHeight == PinnedProbeHeightNoWater;
		Result.bCaveMatch = Result.Probe.Cave == PinnedProbeCave;
		return Result;
	}();
	GSelfCheckDone.store(&Check, std::memory_order_release);
	return Check;
}
