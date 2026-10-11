// Generator v6 mountains: ranges, not towers. Why these tests exist (2026-10-11, "the mountains are super fucked looking ... they collapse to points or empty cells at the far LODs"):
// in v5 the mask that decides WHERE mountains stand and the biome's mountain scale were looked up at biome scale (2-3 km), so a mountain went from nothing to full height over ~150-600 m: a
// wall. At a coarse LOD 6% of adjacent samples were steeper than 45 degrees (max 84): a wall the mesher cannot represent. These tests measure exactly that, at the LODs the voxel world uses.

#include "Misc/AutomationTest.h"
#include "MOWorldGen.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 RangeSeeds[] = { 7, 18, 424242, 12345 };

	struct FWindowStats
	{
		int64 Pairs = 0;
		int64 Steep = 0;    // adjacent samples steeper than 45 degrees (rise > run)
		int64 Wall = 0;     // steeper than 63 degrees (rise > 2 x run): a wall the mesher cannot represent at that spacing
		float MaxHeightCm = 0.0f;
	};

	/** The ground over a square window sampled at one LOD's own spacing (StepCm also tells the generator which octaves that LOD can resolve), and how steep it is between neighbouring samples. */
	FWindowStats SampleWindow(const FMOWorldGenParams& P, int32 Seed, double Cx, double Cy, double HalfCm, double StepCm)
	{
		const int32 N = FMath::Max(3, (int32)(2.0 * HalfCm / StepCm));
		TArray<float> H;
		H.SetNumUninitialized(N * N);
		FWindowStats S;
		for (int32 Y = 0; Y < N; ++Y)
		{
			for (int32 X = 0; X < N; ++X)
			{
				const float V = FMOWorldGen::SampleBaseHeightCm(P, Cx - HalfCm + X * StepCm, Cy - HalfCm + Y * StepCm, Seed, (float)StepCm);
				H[Y * N + X] = V;
				S.MaxHeightCm = FMath::Max(S.MaxHeightCm, V);
			}
		}
		for (int32 Y = 0; Y < N; ++Y)
		{
			for (int32 X = 0; X < N; ++X)
			{
				if (X + 1 < N)
				{
					const float D = FMath::Abs(H[Y * N + X + 1] - H[Y * N + X]);
					++S.Pairs;
					S.Steep += D > (float)StepCm ? 1 : 0;
					S.Wall += D > 2.0f * (float)StepCm ? 1 : 0;
				}
				if (Y + 1 < N)
				{
					const float D = FMath::Abs(H[(Y + 1) * N + X] - H[Y * N + X]);
					++S.Pairs;
					S.Steep += D > (float)StepCm ? 1 : 0;
					S.Wall += D > 2.0f * (float)StepCm ? 1 : 0;
				}
			}
		}
		return S;
	}

	/** The highest ground in the +-27 km window (450 m grid). */
	FVector2D FindSummit(const FMOWorldGenParams& P, int32 Seed)
	{
		FVector2D Best(0.0, 0.0);
		float BestH = -1.0e9f;
		for (int32 Y = 0; Y < 120; ++Y)
		{
			for (int32 X = 0; X < 120; ++X)
			{
				const double Wx = -2.7e6 + 5.4e6 * (X + 0.5) / 120.0, Wy = -2.7e6 + 5.4e6 * (Y + 0.5) / 120.0;
				const float H = FMOWorldGen::SampleBaseHeightCm(P, Wx, Wy, Seed, 450.0f);
				if (H > BestH)
				{
					BestH = H;
					Best = FVector2D(Wx, Wy);
				}
			}
		}
		return Best;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_RangeCoarseLOD,
	"MOFramework.WorldGen.RangeCoarseLOD",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_RangeCoarseLOD::RunTest(const FString& Parameters)
{
	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> Built = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	Built->Tuning.bEnableUplandLakes = false;
	Built->Finalize();
	const FMOWorldGenParams& P = *Built;

	// Around the summit of each seed's window (the worst place: tall, steep ground), sample 12 km at the spacing of three LODs. The 32 m sampling is the reference for the mass.
	int64 Pairs128 = 0, Steep128 = 0, Wall128 = 0, Pairs512 = 0, Steep512 = 0, Wall512 = 0;
	float WorstMass128 = 1.0e9f, WorstMass512 = 1.0e9f;
	for (const int32 Seed : RangeSeeds)
	{
		const FVector2D Summit = FindSummit(P, Seed);
		const FWindowStats Fine = SampleWindow(P, Seed, Summit.X, Summit.Y, 600000.0, 3200.0);
		const FWindowStats L128 = SampleWindow(P, Seed, Summit.X, Summit.Y, 600000.0, 12800.0);
		const FWindowStats L512 = SampleWindow(P, Seed, Summit.X, Summit.Y, 600000.0, 51200.0);
		Pairs128 += L128.Pairs;
		Steep128 += L128.Steep;
		Wall128 += L128.Wall;
		Pairs512 += L512.Pairs;
		Steep512 += L512.Steep;
		Wall512 += L512.Wall;
		WorstMass128 = FMath::Min(WorstMass128, L128.MaxHeightCm / FMath::Max(Fine.MaxHeightCm, 1.0f));
		WorstMass512 = FMath::Min(WorstMass512, L512.MaxHeightCm / FMath::Max(Fine.MaxHeightCm, 1.0f));
		AddInfo(FString::Printf(TEXT("seed %d summit at (%.0f, %.0f): highest sample %.0f m at 32 m spacing, %.0f m at 128 m, %.0f m at 512 m; adjacent samples steeper than 45 deg: %.2f%% at 32 m, %.2f%% at 128 m, %.2f%% at 512 m; steeper than 63 deg (walls): %.2f%% at 128 m, %.2f%% at 512 m"),
			Seed, Summit.X, Summit.Y, Fine.MaxHeightCm / 100.0f, L128.MaxHeightCm / 100.0f, L512.MaxHeightCm / 100.0f,
			100.0 * Fine.Steep / FMath::Max<int64>(Fine.Pairs, 1), 100.0 * L128.Steep / FMath::Max<int64>(L128.Pairs, 1), 100.0 * L512.Steep / FMath::Max<int64>(L512.Pairs, 1),
			100.0 * L128.Wall / FMath::Max<int64>(L128.Pairs, 1), 100.0 * L512.Wall / FMath::Max<int64>(L512.Pairs, 1)));
	}
	const double Share128 = 100.0 * Steep128 / FMath::Max<int64>(Pairs128, 1), Share512 = 100.0 * Steep512 / FMath::Max<int64>(Pairs512, 1);
	const double WallShare128 = 100.0 * Wall128 / FMath::Max<int64>(Pairs128, 1), WallShare512 = 100.0 * Wall512 / FMath::Max<int64>(Pairs512, 1);
	// v5 measured, around a summit, 6% of adjacent samples steeper than 45 degrees at a 128 m step (the steepest 84 degrees) and 4% at 512 m. A wall higher than twice the sample spacing is
	// what the mesher turns into shards and holes; 45-degree faces are ordinary mountain flanks.
	TestTrue(FString::Printf(TEXT("at a 128 m LOD step under 0.5%% of adjacent samples are walls (steeper than 63 degrees): %.2f%%"), WallShare128), WallShare128 < 0.5);
	TestTrue(FString::Printf(TEXT("at a 512 m LOD step under 0.2%% are walls: %.2f%%"), WallShare512), WallShare512 < 0.2);
	TestTrue(FString::Printf(TEXT("and the steep (45 degree) flanks stay a small share around a summit: %.2f%% at 128 m (limit 4%%), %.2f%% at 512 m (limit 1.5%%)"), Share128, Share512), Share128 < 4.0 && Share512 < 1.5);
	// The mass: a coarse LOD keeps the massif, so the summit does not shrink to a point (the culled octaves only take the fine detail).
	TestTrue(FString::Printf(TEXT("the summit keeps at least 90%% of its height at a 128 m LOD step (worst seed %.0f%%)"), 100.0f * WorstMass128), WorstMass128 >= 0.90f);
	TestTrue(FString::Printf(TEXT("and at least 80%% at a 512 m step (worst seed %.0f%%)"), 100.0f * WorstMass512), WorstMass512 >= 0.80f);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_RangeLODSeams,
	"MOFramework.WorldGen.RangeLODSeams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_RangeLODSeams::RunTest(const FString& Parameters)
{
	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> Built = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	Built->Tuning.bEnableUplandLakes = false;
	Built->Finalize();
	const FMOWorldGenParams& P = *Built;

	// Two chunks of neighbouring LODs meet at a seam, and each sampled the ground at its own spacing: if the fine octaves a coarse LOD drops are deep, the two surfaces differ and the seam shows a
	// crack. Measured with a numpy replica of the generator: v6 with 70 m gullies cut hard jumped 95 m between the 256 m and 512 m LODs (v5: 18 m); faded 25 m gullies keep it small.
	// Mountain ground only (above 400 m): that is where there is detail to lose.
	FRandomStream R(77);
	for (const double FineStep : { 6400.0, 12800.0, 25600.0 })
	{
		TArray<float> Diffs;
		for (int32 I = 0; I < 120000 && Diffs.Num() < 3000; ++I)
		{
			const int32 Seed = RangeSeeds[I % UE_ARRAY_COUNT(RangeSeeds)];
			const double X = R.FRandRange(-2.7e6f, 2.7e6f), Y = R.FRandRange(-2.7e6f, 2.7e6f);
			const float Fine = FMOWorldGen::SampleBaseHeightCm(P, X, Y, Seed, (float)FineStep);
			if (Fine < 40000.0f)
			{
				continue;
			}
			Diffs.Add(FMath::Abs(Fine - FMOWorldGen::SampleBaseHeightCm(P, X, Y, Seed, (float)(2.0 * FineStep))));
		}
		Diffs.Sort();
		TestTrue(TEXT("enough mountain samples were found"), Diffs.Num() >= 1000);
		const float P99 = Diffs.Num() ? Diffs[(int32)(0.99f * (Diffs.Num() - 1))] : 0.0f;
		const float Limit = 0.25f * (float)(2.0 * FineStep);
		AddInfo(FString::Printf(TEXT("mountain ground, LOD step %.0f m vs %.0f m: the surface differs by p50 %.1f m, p99 %.1f m (limit %.0f m = 25%% of the coarser step) over %d samples"),
			FineStep / 100.0, 2.0 * FineStep / 100.0, Diffs.Num() ? Diffs[Diffs.Num() / 2] / 100.0f : 0.0f, P99 / 100.0f, Limit / 100.0f, Diffs.Num()));
		TestTrue(FString::Printf(TEXT("neighbouring LODs (%.0f m / %.0f m steps) agree to within 25%% of the coarser step at p99 (%.1f m)"), FineStep / 100.0, 2.0 * FineStep / 100.0, P99 / 100.0f), P99 <= Limit);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
