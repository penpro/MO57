// The ground surface fades across a biome boundary instead of switching at a line (Wes, 2026-10-12: "every split should have a blend band AND a noise term that
// breaks the edge up ... plus a test that the transition has width and is deterministic"). FMOWorldGen::SoftThreshold is the one rule; SampleSurfaceBiomeWeights
// applies it to every biome band. Headless-safe.

#include "Misc/AutomationTest.h"
#include "MOWorldGen.h"
#include "MOWorldGenSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_SoftThreshold,
	"MOFramework.WorldGen.SoftThreshold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_SoftThreshold::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("0 below the band"), FMOWorldGen::SoftThreshold(89.0f, 100.0f, 20.0f), 0.0f);
	TestEqual(TEXT("1 above the band"), FMOWorldGen::SoftThreshold(111.0f, 100.0f, 20.0f), 1.0f);
	TestTrue(TEXT("one half at the edge"), FMath::IsNearlyEqual(FMOWorldGen::SoftThreshold(100.0f, 100.0f, 20.0f), 0.5f, 1.0e-6f));
	TestTrue(TEXT("the breakup moves the edge"), FMath::IsNearlyEqual(FMOWorldGen::SoftThreshold(105.0f, 100.0f, 20.0f, 5.0f), 0.5f, 1.0e-6f));
	TestEqual(TEXT("width 0 is a hard step (below)"), FMOWorldGen::SoftThreshold(99.9f, 100.0f, 0.0f), 0.0f);
	TestEqual(TEXT("width 0 is a hard step (at)"), FMOWorldGen::SoftThreshold(100.0f, 100.0f, 0.0f), 1.0f);
	float Last = -1.0f;
	bool bMonotonic = true;
	for (int32 I = 0; I <= 400; ++I)
	{
		const float V = FMOWorldGen::SoftThreshold(80.0f + 0.1f * (float)I, 100.0f, 20.0f);
		bMonotonic = bMonotonic && V >= Last;
		Last = V;
	}
	TestTrue(TEXT("monotonic across the band"), bMonotonic);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_HeightBlendBand,
	"MOFramework.WorldGen.HeightBlendBand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_HeightBlendBand::RunTest(const FString& Parameters)
{
	// Wes, Oct 23 2026 (BR23-19): "blend the height splits over 25-50m of elevation change for everything except beaches etc at water level". The shipped
	// tuning, against every height edge DT_Biomes has: the inland edges (300 m, 320 m, 950 m) blend over 25-50 m, the shore edges (4 and 4.5 m) stay narrow.
	const FMOWorldGenTuning T;
	const float Sea = T.SeaLevelCm;
	for (const float Edge : { 30000.0f, 32000.0f, 95000.0f })
	{
		const float Band = FMOWorldGen::HeightBlendBandCm(T, Sea + Edge);
		TestTrue(FString::Printf(TEXT("an inland edge %.0f m above the sea blends over 25-50 m (%.1f m)"), Edge / 100.0f, Band / 100.0f), Band >= 2500.0f && Band <= 5000.0f);
	}
	for (const float Edge : { 400.0f, 450.0f })
	{
		const float Band = FMOWorldGen::HeightBlendBandCm(T, Sea + Edge);
		TestTrue(FString::Printf(TEXT("a shore edge %.1f m above the sea stays narrow (%.2f m)"), Edge / 100.0f, Band / 100.0f), Band <= 200.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_SurfaceBlendBands,
	"MOFramework.WorldGen.SurfaceBlendBands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_SurfaceBlendBands::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = UMOWorldGenSettings::RefreshPublishedParams();
	if (!TestTrue(TEXT("the published params have biomes"), P->Biomes.Num() >= 2))
	{
		return false;
	}
	const int32 Seed = 1029024224;
	const float Step = P->Tuning.VoxelSizeCm;

	// 1. Weights are a partition and deterministic.
	{
		FRandomStream R(7);
		int32 BadSum = 0, Nondeterministic = 0, Checked = 0;
		for (int32 I = 0; I < 3000; ++I)
		{
			const double X = R.FRandRange(-2.0e6f, 2.0e6f), Y = R.FRandRange(-2.0e6f, 2.0e6f);
			const FMOTerrainSample S = FMOWorldGen::SampleColumn(*P, X, Y, Seed, Step);
			FMOWorldGen::FBiomeWeight A[3], B[3];
			const int32 NA = FMOWorldGen::SampleSurfaceBiomeWeights(*P, X, Y, Seed, S.HeightCm, S.Moisture, S.Temperature, A, 3);
			const int32 NB = FMOWorldGen::SampleSurfaceBiomeWeights(*P, X, Y, Seed, S.HeightCm, S.Moisture, S.Temperature, B, 3);
			if (NA == 0)
			{
				continue;
			}
			++Checked;
			float Sum = 0.0f;
			for (int32 K = 0; K < NA; ++K)
			{
				Sum += A[K].Weight;
				Nondeterministic += (NA != NB || A[K].Biome != B[K].Biome || A[K].Weight != B[K].Weight) ? 1 : 0;
			}
			BadSum += FMath::IsNearlyEqual(Sum, 1.0f, 1.0e-4f) ? 0 : 1;
		}
		AddInfo(FString::Printf(TEXT("%d columns checked"), Checked));
		TestTrue(TEXT("columns were checked"), Checked > 1000);
		TestEqual(TEXT("weights sum to 1"), BadSum, 0);
		TestEqual(TEXT("same column -> same weights"), Nondeterministic, 0);
	}

	// 2. A hard boundary is a BAND: walk lines, find where ResolveBiome changes, then measure (1 m steps) how far the strongest surface weight stays mixed (0.1..0.9).
	{
		FRandomStream R(11);
		int32 Boundaries = 0, Banded = 0;
		double WidthSum = 0.0;
		for (int32 Line = 0; Line < 400 && Boundaries < 30; ++Line)
		{
			const double X0 = R.FRandRange(-2.0e6f, 2.0e6f), Y0 = R.FRandRange(-2.0e6f, 2.0e6f);
			const float A = R.FRandRange(0.0f, 6.2831853f);
			const double Dx = FMath::Cos(A), Dy = FMath::Sin(A);   // test-only directions; the generator itself stays transcendental-free
			int32 PrevBiome = INDEX_NONE;
			for (int32 K = 0; K <= 300; ++K)   // 3 km in 10 m steps
			{
				const double X = X0 + Dx * K * 1000.0, Y = Y0 + Dy * K * 1000.0;
				const FMOTerrainSample S = FMOWorldGen::SampleColumn(*P, X, Y, Seed, Step);
				if (S.BiomeIndex == INDEX_NONE || S.HeightCm < P->Tuning.SeaLevelCm)
				{
					PrevBiome = INDEX_NONE;
					continue;
				}
				if (PrevBiome != INDEX_NONE && S.BiomeIndex != PrevBiome)
				{
					++Boundaries;
					int32 Mixed = 0;
					for (int32 M = -200; M <= 200; ++M)   // +-200 m around the change, 1 m steps
					{
						const double Mx = X + Dx * M * 100.0, My = Y + Dy * M * 100.0;
						const FMOTerrainSample Q = FMOWorldGen::SampleColumn(*P, Mx, My, Seed, Step);
						FMOWorldGen::FBiomeWeight W[3];
						const int32 N = FMOWorldGen::SampleSurfaceBiomeWeights(*P, Mx, My, Seed, Q.HeightCm, Q.Moisture, Q.Temperature, W, 3);
						Mixed += (N >= 2 && W[0].Weight > 0.1f && W[0].Weight < 0.9f) ? 1 : 0;
					}
					Banded += Mixed >= 3 ? 1 : 0;
					WidthSum += Mixed;
					break;
				}
				PrevBiome = S.BiomeIndex;
			}
		}
		AddInfo(FString::Printf(TEXT("%d boundaries: %d have a mixed band of 3 m or more; mean mixed width %.0f m"), Boundaries, Banded, Boundaries > 0 ? WidthSum / Boundaries : 0.0));
		TestTrue(TEXT("boundaries were found"), Boundaries >= 10);
		TestTrue(FString::Printf(TEXT("most boundaries are bands, not lines (%d of %d)"), Banded, Boundaries), Banded * 10 >= Boundaries * 8);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_PickSurfaceBiome,
	"MOFramework.WorldGen.PickSurfaceBiome",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_PickSurfaceBiome::RunTest(const FString& Parameters)
{
	// Foliage follows the ground: a PCG point's biome is a roll against the ground's soft weights. Over a boundary band the share of points that pick a biome must match its
	// ground weight; inside one biome the pick is that biome; a cliff (slope band) still wins outright; and the roll is a pure function of the position.
	const FMOWorldGenParamsRef P = UMOWorldGenSettings::RefreshPublishedParams();
	if (!TestTrue(TEXT("the published params have biomes"), P->Biomes.Num() >= 2))
	{
		return false;
	}
	const int32 Seed = 1029024224;
	const float Step = P->Tuning.VoxelSizeCm;
	FRandomStream R(23);
	int32 Single = 0, SingleWrong = 0, Nondeterministic = 0, Mixed = 0, CliffChecked = 0, CliffWrong = 0;
	double Expected = 0.0, Observed = 0.0;
	int32 Searched = 0;
	for (; Searched < 200000 && (Mixed < 150 || Single < 1000); ++Searched)
	{
		const double X = R.FRandRange(-2.0e6f, 2.0e6f), Y = R.FRandRange(-2.0e6f, 2.0e6f);
		const FMOTerrainSample S = FMOWorldGen::SampleColumn(*P, X, Y, Seed, Step);
		if (S.BiomeIndex == INDEX_NONE || S.HeightCm < P->Tuning.SeaLevelCm)
		{
			continue;
		}
		FMOWorldGen::FBiomeWeight W[3];
		const int32 N = FMOWorldGen::SampleSurfaceBiomeWeights(*P, X, Y, Seed, S.HeightCm, S.Moisture, S.Temperature, W, 3);
		const int32 Pick = FMOWorldGen::PickSurfaceBiome(*P, X, Y, Seed, S.HeightCm, 0.0f, S.Moisture, S.Temperature);
		Nondeterministic += Pick != FMOWorldGen::PickSurfaceBiome(*P, X, Y, Seed, S.HeightCm, 0.0f, S.Moisture, S.Temperature) ? 1 : 0;
		const int32 Hard80 = FMOWorldGen::ResolveBiome(*P, S.HeightCm, 80.0f, S.Moisture, S.Temperature);
		if (Hard80 != INDEX_NONE && P->Biomes[Hard80].SlopeMinDeg > 0.0f)
		{
			++CliffChecked;
			CliffWrong += FMOWorldGen::PickSurfaceBiome(*P, X, Y, Seed, S.HeightCm, 80.0f, S.Moisture, S.Temperature) != Hard80 ? 1 : 0;
		}
		if (N == 1 && Single < 1000)
		{
			++Single;
			SingleWrong += Pick != W[0].Biome ? 1 : 0;
		}
		else if (N >= 2 && W[0].Weight > 0.3f && W[0].Weight < 0.7f && Mixed < 150)   // (slope 0: every weighted biome holds it -- slope-banded biomes are never weighted)
		{
			// A 2 m x 2 m patch of points on the 10 cm roll grid, same climate: the share that picks the strongest biome is its weight (the breakup noise moves little in 2 m).
			++Mixed;
			Expected += W[0].Weight;
			int32 Hits = 0;
			for (int32 Iy = 0; Iy < 20; ++Iy)
			{
				for (int32 Ix = 0; Ix < 20; ++Ix)
				{
					Hits += FMOWorldGen::PickSurfaceBiome(*P, X + Ix * 10.0, Y + Iy * 10.0, Seed, S.HeightCm, 0.0f, S.Moisture, S.Temperature) == W[0].Biome ? 1 : 0;
				}
			}
			Observed += Hits / 400.0;
		}
	}
	AddInfo(FString::Printf(TEXT("%d columns searched; %d single-biome, %d mixed (mean weight %.3f, picked %.3f), %d cliff columns"), Searched, Single, Mixed,
		Mixed > 0 ? Expected / Mixed : 0.0, Mixed > 0 ? Observed / Mixed : 0.0, CliffChecked));
	TestEqual(TEXT("same position -> same biome"), Nondeterministic, 0);
	TestTrue(TEXT("single-biome columns were found"), Single >= 500);
	TestEqual(TEXT("inside one biome the pick is that biome"), SingleWrong, 0);
	TestTrue(TEXT("mixed columns were found"), Mixed >= 50);
	TestTrue(FString::Printf(TEXT("the pick follows the ground's mix (weight %.3f, picked %.3f)"), Mixed > 0 ? Expected / Mixed : 0.0, Mixed > 0 ? Observed / Mixed : 0.0),
		Mixed > 0 && FMath::Abs(Expected - Observed) / Mixed < 0.05);
	TestEqual(TEXT("a cliff (slope band) wins outright"), CliffWrong, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
