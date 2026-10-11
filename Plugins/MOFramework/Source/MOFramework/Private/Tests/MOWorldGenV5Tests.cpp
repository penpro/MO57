// Tests for the generator-v5 terrain shaping (requested 2026-10-10): the island edge (land is gone before the world border), per-biome relief (a prairie stays flat, mountains stand
// many times higher), and half-size biomes. Pure and headless like the other WorldGen tests.

#include "Misc/AutomationTest.h"
#include "MOWorldGen.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 V5Seeds[] = { 7, 18, 424242, 12345 };

	/** Meadow (fallback, climate-only, flat), Forest (moist, rugged), Rocky (slope-banded: shapes nothing). */
	FMOWorldGenParamsRef MakeReliefParams(float PrairieHill = 0.30f, float PrairieMountain = 0.03f, float ForestHill = 1.0f, float ForestMountain = 1.0f)
	{
		TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> P = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
		auto Add = [&P](const TCHAR* Id, int32 Priority, float SlopeMin, float MoistureMin, float Hill, float Mountain)
		{
			FMOWorldGenBiome& B = P->Biomes.AddDefaulted_GetRef();
			B.Id = FName(Id);
			B.Priority = Priority;
			B.SlopeMinDeg = SlopeMin;
			B.MoistureMin = MoistureMin;
			B.ReliefScale = Hill;
			B.MountainScale = Mountain;
		};
		Add(TEXT("Meadow"), 1, 0.0f, 0.0f, PrairieHill, PrairieMountain);
		Add(TEXT("Forest"), 10, 0.0f, 0.4f, ForestHill, ForestMountain);
		Add(TEXT("Rocky"), 20, 28.0f, 0.0f, 3.0f, 3.0f);   // slope-banded: must NOT change the relief
		P->Finalize();
		return FMOWorldGenParamsRef(P);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_IslandEdge,
	"MOFramework.WorldGen.IslandEdge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_IslandEdge::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = MakeReliefParams();
	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> Open = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	Open->Tuning.EdgeFalloffStart = 2.0f;   // the falloff off: what v4 did
	Open->Tuning.EdgeFalloffEnd = 2.0f;
	Open->Finalize();

	const double Half = (double)P->Tuning.WorldHalfSizeCm;
	const float Sea = P->Tuning.SeaLevelCm;
	float WorstAboveSea = -1.0e9f;
	int32 Samples = 0, ControlLand = 0;
	for (const int32 Seed : V5Seeds)
	{
		for (int32 I = 0; I <= 400; ++I)
		{
			const double T = -1.0 + 2.0 * I / 400.0;
			for (const double Scale : { 0.94, 0.97, 1.0 })
			{
				const double A = Half * Scale, B = Half * T * Scale;
				const double Pts[4][2] = { { A, B }, { -A, B }, { B, A }, { B, -A } };
				for (const double* Pt : Pts)
				{
					WorstAboveSea = FMath::Max(WorstAboveSea, FMOWorldGen::SampleHeightCm(*P, Pt[0], Pt[1], Seed, 400.0f) - Sea);
					ControlLand += FMOWorldGen::SampleHeightCm(*Open, Pt[0], Pt[1], Seed, 400.0f) > Sea + 100.0f ? 1 : 0;
					++Samples;
				}
			}
		}
	}
	TestTrue(FString::Printf(TEXT("every point of the outer 6%% of the world is open water (the highest is %.0f cm above sea level; need <= -500 over %d samples)"), WorstAboveSea, Samples), WorstAboveSea < -500.0f);
	TestTrue(FString::Printf(TEXT("CONTROL: without the falloff the same border has land (%d of the samples)"), ControlLand), ControlLand > 0);

	// The inside must still be a land: not an all-ocean world.
	float LandSum = 0.0f;
	for (const int32 Seed : V5Seeds)
	{
		int32 Land = 0, Total = 0;
		for (int32 Y = 0; Y < 60; ++Y)
		{
			for (int32 X = 0; X < 60; ++X)
			{
				const double Wx = -0.6 * Half + 1.2 * Half * (X + 0.5) / 60.0, Wy = -0.6 * Half + 1.2 * Half * (Y + 0.5) / 60.0;
				Land += FMOWorldGen::SampleHeightCm(*P, Wx, Wy, Seed, 800.0f) > Sea ? 1 : 0;
				++Total;
			}
		}
		AddInfo(FString::Printf(TEXT("seed %d: %.0f%% of the inner 60%% is land"), Seed, 100.0f * Land / Total));
		LandSum += (float)Land / Total;
	}
	const float MeanLand = LandSum / UE_ARRAY_COUNT(V5Seeds);
	TestTrue(FString::Printf(TEXT("the inner world is %.0f%% land on average (needs 30-90%%)"), 100.0f * MeanLand), MeanLand > 0.30f && MeanLand < 0.90f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_BiomeRelief,
	"MOFramework.WorldGen.BiomeRelief",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_BiomeRelief::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = MakeReliefParams();

	// No biome changes the relief -> exactly 1 and 1 (the v1-v4 height math is untouched, bit for bit).
	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> Plain = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	FMOWorldGenBiome& Only = Plain->Biomes.AddDefaulted_GetRef();
	Only.Id = FName(TEXT("Meadow"));
	Plain->Finalize();
	bool bExactlyOne = true;
	for (float M = 0.0f; M <= 1.0f; M += 0.05f)
	{
		const FMOBiomeRelief R = FMOWorldGen::BiomeReliefAt(*Plain, M, 0.5f);
		bExactlyOne &= R.Hill == 1.0f && R.Mountain == 1.0f;
	}
	TestTrue(TEXT("with every scale at 1 the relief multipliers are exactly 1"), bExactlyOne);

	// The slope-banded biome (scale 3) shapes nothing; the dry and wet climates get their own scales.
	const FMOBiomeRelief Dry = FMOWorldGen::BiomeReliefAt(*P, 0.10f, 0.5f), Wet = FMOWorldGen::BiomeReliefAt(*P, 0.90f, 0.5f);
	TestEqual(TEXT("a dry climate gets the prairie's hills"), Dry.Hill, 0.30f);
	TestEqual(TEXT("a dry climate gets the prairie's (almost no) mountains"), Dry.Mountain, 0.03f);
	TestEqual(TEXT("a moist climate gets the forest's hills"), Wet.Hill, 1.0f);
	TestEqual(TEXT("a moist climate gets the forest's mountains"), Wet.Mountain, 1.0f);
	TestTrue(TEXT("the rocky (slope) biome never leaks into the relief: the largest scales stay at 1"), P->MaxHillScale == 1.0f && P->MaxMountainScale == 1.0f);

	// Continuity: relief changes over the blend width, never at a line (a step here would be a cliff in the land).
	float MaxStep = 0.0f;
	for (float T = 0.0f; T <= 1.0f; T += 0.25f)
	{
		FMOBiomeRelief Prev = FMOWorldGen::BiomeReliefAt(*P, 0.0f, T);
		for (float M = 0.001f; M <= 1.0f; M += 0.001f)
		{
			const FMOBiomeRelief V = FMOWorldGen::BiomeReliefAt(*P, M, T);
			MaxStep = FMath::Max(MaxStep, FMath::Max(FMath::Abs(V.Hill - Prev.Hill), FMath::Abs(V.Mountain - Prev.Mountain)));
			Prev = V;
		}
	}
	TestTrue(FString::Printf(TEXT("the relief multipliers are continuous: largest change over a 0.001 climate step is %.4f (limit 0.03)"), MaxStep), MaxStep < 0.03f);

	// The terrain really is flat where the climate is dry: mean height change over 800 m, inland, along stretches that stay inside ONE climate class the whole way (start, middle and end all
	// dry / all moist). A stretch that crosses a biome border measures the border (prairie next to mountains), not the prairie.
	const double Baseline = 80000.0;
	double DrySum = 0.0, WetSum = 0.0;
	int32 DryN = 0, WetN = 0;
	FRandomStream R(31);
	for (int32 I = 0; I < 400000 && (DryN < 1500 || WetN < 1500); ++I)
	{
		const int32 Seed = V5Seeds[I % UE_ARRAY_COUNT(V5Seeds)];
		const double X = R.FRandRange(-1.5e6f, 1.5e6f), Y = R.FRandRange(-1.5e6f, 1.5e6f);
		const bool bAlongX = (I & 1) == 0;
		const double Mx = bAlongX ? 0.5 * Baseline : 0.0, My = bAlongX ? 0.0 : 0.5 * Baseline;
		const FMOClimateSample C0 = FMOWorldGen::SampleClimate(*P, X, Y, Seed), C1 = FMOWorldGen::SampleClimate(*P, X + Mx, Y + My, Seed), C2 = FMOWorldGen::SampleClimate(*P, X + 2.0 * Mx, Y + 2.0 * My, Seed);
		if (C0.Continentalness < -0.1f || C1.Continentalness < -0.1f || C2.Continentalness < -0.1f)
		{
			continue;   // inland only: the shore and the sea floor do not say anything about prairies
		}
		// v6: the biome's relief is looked up from the REGIONAL climate (a biome patch is ~2-3 km, a range tens of km: a scale that followed the local patch made a cliff at every border), so the
		// class of a stretch is its regional moisture, not the local one.
		const bool bDry = C0.MoistureRegion < 0.22f && C1.MoistureRegion < 0.22f && C2.MoistureRegion < 0.22f, bWet = C0.MoistureRegion > 0.62f && C1.MoistureRegion > 0.62f && C2.MoistureRegion > 0.62f;
		if ((!bDry || DryN >= 1500) && (!bWet || WetN >= 1500))
		{
			continue;
		}
		const float Delta = FMath::Abs(FMOWorldGen::SampleBaseHeightCm(*P, X + 2.0 * Mx, Y + 2.0 * My, Seed, 400.0f) - FMOWorldGen::SampleBaseHeightCm(*P, X, Y, Seed, 400.0f));
		if (bDry) { DrySum += Delta; ++DryN; }
		else { WetSum += Delta; ++WetN; }
	}
	const double DryMean = DryN ? DrySum / DryN : 0.0, WetMean = WetN ? WetSum / WetN : 1.0;
	AddInfo(FString::Printf(TEXT("mean height change over 800 m inside one climate, inland: dry %.0f cm (%d samples), moist %.0f cm (%d samples)"), DryMean, DryN, WetMean, WetN));
	TestTrue(TEXT("enough dry and moist inland stretches were found"), DryN >= 300 && WetN >= 300);
	TestTrue(FString::Printf(TEXT("a dry (prairie) climate is flat: %.0f cm of change over 800 m (limit 12 m, under 1 degree)"), DryMean), DryMean < 1200.0);
	TestTrue(FString::Printf(TEXT("and far flatter than a moist one: %.0f vs %.0f cm (limit 0.3x)"), DryMean, WetMean), DryMean < 0.30 * WetMean);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_MountainsAreTall,
	"MOFramework.WorldGen.MountainsAreTall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_MountainsAreTall::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = MakeReliefParams();
	float HighestEver = 0.0f;
	int32 LandTotal = 0, Over45 = 0, Over60 = 0, Over70 = 0;
	TArray<float> Slopes;
	for (const int32 Seed : V5Seeds)
	{
		float Highest = 0.0f;
		TArray<float> Land;
		for (int32 Y = 0; Y < 150; ++Y)
		{
			for (int32 X = 0; X < 150; ++X)
			{
				const double Wx = -2.7e6 + 5.4e6 * (X + 0.5) / 150.0, Wy = -2.7e6 + 5.4e6 * (Y + 0.5) / 150.0;
				const float H = FMOWorldGen::SampleBaseHeightCm(*P, Wx, Wy, Seed, 800.0f) - P->Tuning.SeaLevelCm;
				Highest = FMath::Max(Highest, H);
				if (H > 0.0f)
				{
					Land.Add(H);
					// Steepness over a 40 m baseline (the scale a walking pawn feels), on a third of the land points.
					if (((X + Y) % 3) == 0)
					{
						const float Dx = FMOWorldGen::SampleBaseHeightCm(*P, Wx + 2000.0, Wy, Seed, 400.0f) - FMOWorldGen::SampleBaseHeightCm(*P, Wx - 2000.0, Wy, Seed, 400.0f);
						const float Dy = FMOWorldGen::SampleBaseHeightCm(*P, Wx, Wy + 2000.0, Seed, 400.0f) - FMOWorldGen::SampleBaseHeightCm(*P, Wx, Wy - 2000.0, Seed, 400.0f);
						const float SlopeDeg = FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(Dx * Dx + Dy * Dy) / 4000.0f));
						Slopes.Add(SlopeDeg);
						++LandTotal;
						Over45 += SlopeDeg > 45.0f ? 1 : 0;
						Over60 += SlopeDeg > 60.0f ? 1 : 0;
						Over70 += SlopeDeg > 70.0f ? 1 : 0;
					}
				}
			}
		}
		Land.Sort();
		AddInfo(FString::Printf(TEXT("seed %d: highest ground %.0f m, land p50 %.0f m, p95 %.0f m, p99 %.0f m"), Seed, Highest / 100.0f,
			Land.Num() ? Land[Land.Num() / 2] / 100.0f : 0.0f, Land.Num() ? Land[(int32)(0.95f * (Land.Num() - 1))] / 100.0f : 0.0f, Land.Num() ? Land[(int32)(0.99f * (Land.Num() - 1))] / 100.0f : 0.0f));
		HighestEver = FMath::Max(HighestEver, Highest);
	}
	Slopes.Sort();
	AddInfo(FString::Printf(TEXT("land slope over 40 m: p50 %.1f, p90 %.1f, p99 %.1f degrees; steeper than 45 deg: %.1f%%, 60 deg: %.2f%%, 70 deg: %.2f%% (of %d land samples)"),
		Slopes[Slopes.Num() / 2], Slopes[(int32)(0.90f * (Slopes.Num() - 1))], Slopes[(int32)(0.99f * (Slopes.Num() - 1))],
		100.0f * Over45 / LandTotal, 100.0f * Over60 / LandTotal, 100.0f * Over70 / LandTotal, LandTotal));
	TestTrue(FString::Printf(TEXT("some mountain stands at least 1000 m above the sea (%.0f m); v4's tallest was ~400 m"), HighestEver / 100.0f), HighestEver >= 100000.0f);
	TestTrue(FString::Printf(TEXT("and none passes the conservative bound MaxHeightCm (%.0f m)"), P->MaxHeightCm / 100.0f), HighestEver + P->Tuning.SeaLevelCm <= P->MaxHeightCm);
	// v6: ranges, not towers. v5 measured 6.9% of the land steeper than 45 degrees and 3.4% steeper than 60 (p99 78 degrees): a biome border was a cliff as tall as the mountain. Now the
	// steepness is the gullies' and the peaks', a small share of the land.
	TestTrue(FString::Printf(TEXT("the mountains are broad masses, not walls: under 2.5%% of the land is steeper than 45 degrees (%.2f%%)"), 100.0f * Over45 / LandTotal), Over45 * 1000 < LandTotal * 25);
	TestTrue(FString::Printf(TEXT("and under 0.5%% is steeper than 60 degrees (%.2f%%)"), 100.0f * Over60 / LandTotal), Over60 * 1000 < LandTotal * 5);
	TestTrue(FString::Printf(TEXT("the 99th percentile of the slope stays under 55 degrees (%.1f)"), Slopes[(int32)(0.99f * (Slopes.Num() - 1))]), Slopes[(int32)(0.99f * (Slopes.Num() - 1))] < 55.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_BiomesAreSmaller,
	"MOFramework.WorldGen.BiomesAreSmaller",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_BiomesAreSmaller::RunTest(const FString& Parameters)
{
	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> Now = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	Now->Finalize();
	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> V4 = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	V4->Tuning.MoistureWavelengthCm = 700000.0f;     // what the biomes were before v5
	V4->Tuning.TemperatureWavelengthCm = 1500000.0f;
	V4->Finalize();

	auto Crossings = [](const FMOWorldGenParams& P)
	{
		int32 Count = 0;
		for (int32 Line = 0; Line < 48; ++Line)
		{
			const int32 Seed = V5Seeds[Line % UE_ARRAY_COUNT(V5Seeds)];
			const double Y = -2.4e6 + 100000.0 * Line;
			bool bPrev = FMOWorldGen::SampleClimate(P, -2.4e6, Y, Seed).Moisture >= 0.5f;
			for (int32 I = 1; I <= 160; ++I)   // a 48 km line, a sample every 300 m
			{
				const bool bNow = FMOWorldGen::SampleClimate(P, -2.4e6 + 30000.0 * I, Y, Seed).Moisture >= 0.5f;
				Count += (bNow != bPrev) ? 1 : 0;
				bPrev = bNow;
			}
		}
		return Count;
	};
	const int32 CrossNow = Crossings(*Now), CrossV4 = Crossings(*V4);
	TestTrue(FString::Printf(TEXT("a walk crosses at least 3x as many moist/dry borders as before v5 (%d vs %d): the biomes are a quarter of their original size (halved twice)"), CrossNow, CrossV4), CrossNow >= CrossV4 * 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_OceanDepth,
	"MOFramework.WorldGen.OceanDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_OceanDepth::RunTest(const FString& Parameters)
{
	// Reported 2026-10-10: "the ocean depth is really shallow and the shoreline meanders WAAAY out into the water". The sea floor was -12 m and the hill noise rode on it. Now the ground drops away
	// steeply from the shore to a deep ocean floor, and the hill noise fades out over the sea.
	const FMOWorldGenParamsRef P = MakeReliefParams();
	FMOWorldGenParams Old = *P;
	Old.Tuning.ContinentSplineM = { FVector2D(-1.00, -6.0), FVector2D(-0.55, 0.0), FVector2D(-0.30, 6.0), FVector2D(-0.15, 12.0), FVector2D(0.00, 18.0), FVector2D(0.25, 32.0), FVector2D(0.55, 55.0), FVector2D(1.00, 90.0) };
	Old.Tuning.HillSeaFadeLow = 2.0f;
	Old.Tuning.HillSeaFadeHigh = 2.0f;
	Old.Finalize();

	struct FSea { int32 Samples = 0, Shallow = 0, Deep = 0; float Deepest = 0.0f; };
	auto Survey = [](const FMOWorldGenParams& Q)
	{
		FSea R;
		for (const int32 Seed : V5Seeds)
		{
			for (int32 Y = 0; Y < 90; ++Y)
			{
				for (int32 X = 0; X < 90; ++X)
				{
					const double Wx = -2.6e6 + 5.2e6 * (X + 0.5) / 90.0, Wy = -2.6e6 + 5.2e6 * (Y + 0.5) / 90.0;
					const float H = FMOWorldGen::SampleBaseHeightCm(Q, Wx, Wy, Seed, 800.0f) - Q.Tuning.SeaLevelCm;
					if (H >= 0.0f)
					{
						continue;
					}
					++R.Samples;
					R.Shallow += H > -2000.0f ? 1 : 0;   // within 20 m of the surface
					R.Deep += H < -10000.0f ? 1 : 0;     // deeper than 100 m
					R.Deepest = FMath::Min(R.Deepest, H);
				}
			}
		}
		return R;
	};
	const FSea Now = Survey(*P), Before = Survey(Old);
	const float NowShallow = 100.0f * Now.Shallow / FMath::Max(Now.Samples, 1), BeforeShallow = 100.0f * Before.Shallow / FMath::Max(Before.Samples, 1);
	AddInfo(FString::Printf(TEXT("sea (%d samples): shallower than 20 m %.0f%% (was %.0f%%), deeper than 100 m %.0f%%, deepest %.0f m (was %.0f m)"), Now.Samples, NowShallow, BeforeShallow,
		100.0f * Now.Deep / FMath::Max(Now.Samples, 1), Now.Deepest / 100.0f, Before.Deepest / 100.0f));
	TestTrue(TEXT("CONTROL: the old sea was a shallow shelf (most of it within 20 m of the surface)"), BeforeShallow > 70.0f);
	TestTrue(FString::Printf(TEXT("the sea is mostly deep now: under 30%% of it is within 20 m of the surface (%.0f%%)"), NowShallow), NowShallow < 30.0f);
	TestTrue(FString::Printf(TEXT("the ocean has a real depth: the deepest water is at least 120 m down (%.0f m)"), Now.Deepest / 100.0f), Now.Deepest < -12000.0f);
	TestTrue(TEXT("and no ground rises above sea level in the middle of the sea: the world still stays inside its bounds"), Now.Deepest >= P->MinHeightCm - P->Tuning.SeaLevelCm);

	// The shore is DEFINED: from the waterline the ground falls 20 m within 400 m, seaward, on most coasts.
	int32 Transects = 0, Steep = 0;
	FRandomStream R(77);
	for (int32 I = 0; I < 40000 && Transects < 300; ++I)
	{
		const int32 Seed = V5Seeds[I % UE_ARRAY_COUNT(V5Seeds)];
		const double X = R.FRandRange(-2.0e6f, 2.0e6f), Y = R.FRandRange(-2.0e6f, 2.0e6f);
		const double Angle = R.FRandRange(0.0f, 6.2831853f);
		const double Dx = FMath::Cos(Angle), Dy = FMath::Sin(Angle);
		// find a land->sea crossing along 4 km at 20 m steps
		float Prev = FMOWorldGen::SampleBaseHeightCm(*P, X, Y, Seed, 400.0f);
		for (int32 K = 1; K <= 200; ++K)
		{
			const float H = FMOWorldGen::SampleBaseHeightCm(*P, X + Dx * 2000.0 * K, Y + Dy * 2000.0 * K, Seed, 400.0f);
			if (Prev > 0.0f && H <= 0.0f)
			{
				const double Cx = X + Dx * 2000.0 * K, Cy = Y + Dy * 2000.0 * K;
				// A shore's steepness is in its SEAWARD direction (a bearing that grazes the coast says nothing): the deepest of 8 bearings, 400 m out.
				float Deepest = 0.0f;
				for (int32 B = 0; B < 8; ++B)
				{
					const double Bx = FMath::Cos(B * UE_PI / 4.0), By = FMath::Sin(B * UE_PI / 4.0);
					Deepest = FMath::Min(Deepest, FMOWorldGen::SampleBaseHeightCm(*P, Cx + Bx * 40000.0, Cy + By * 40000.0, Seed, 400.0f));
				}
				++Transects;
				Steep += Deepest < -2000.0f ? 1 : 0;
				break;
			}
			Prev = H;
		}
	}
	AddInfo(FString::Printf(TEXT("%d shore points: %d drop 20 m within 400 m in their steepest seaward direction"), Transects, Steep));
	TestTrue(TEXT("enough shore transects were found"), Transects >= 100);
	TestTrue(FString::Printf(TEXT("most shores drop 20 m within 400 m of the waterline, seaward (%d of %d)"), Steep, Transects), Steep * 100 >= Transects * 70);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
