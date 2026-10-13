// Cost of each generator query the biome spawner makes per point, over a real 512 m tree-tier cell (the coast spawn of seed 1029024224). The spawner's per-point work was 13.5 us a
// point on the game thread -- the whole budget of a new-game load (packaged, Oct 13 2026); this is the table that says which query to make cheaper. Logs only (no thresholds: timings
// depend on the machine); it fails only when a query is non-finite.

#include "Misc/AutomationTest.h"
#include "MOWorldGen.h"
#include "MOWorldGenSettings.h"
#include "MOPCGWaterMask.h"
#include "MOWorldSeedSubsystem.h"
#include "MOFramework.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOPCG_SampleCosts,
	"MOFramework.PCG.SampleCosts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOPCG_SampleCosts::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef Params = UMOWorldGenSettings::RefreshPublishedParams();
	const int32 Seed = 1029024224;
	const FMOPCGWaterMask Water(Params, Seed);
	const FMOPCGWaterMask Edge(Params, Seed, 1.008f);
	const double X0 = 1024000.0, Y0 = -716800.0;   // a 512 m cell just inland of the coast spawn (1222000, -680000)
	const int32 N = 96;                            // 96 x 96 = 9216 points, ~5 m apart
	TArray<FVector> Pts;
	Pts.Reserve(N * N);
	for (int32 J = 0; J < N; ++J)
	{
		for (int32 I = 0; I < N; ++I)
		{
			Pts.Add(FVector(X0 + I * 533.0, Y0 + J * 533.0, 0.0));
		}
	}
	double Sink = 0.0;
	auto Time = [&](const TCHAR* Name, TFunctionRef<double(const FVector&)> Fn) -> double
	{
		const double T0 = FPlatformTime::Seconds();
		double Acc = 0.0;
		for (const FVector& P : Pts)
		{
			Acc += Fn(P);
		}
		const double Us = (FPlatformTime::Seconds() - T0) * 1.0e6 / Pts.Num();
		Sink += Acc;
		UE_LOG(LogMOFramework, Display, TEXT("[MOPCGCOST] %-34s %8.2f us/point"), Name, Us);
		AddInfo(FString::Printf(TEXT("%s: %.2f us/point"), Name, Us));
		return Us;
	};
	// Fill Z with the generator height so the water queries see real ground.
	for (FVector& P : Pts)
	{
		P.Z = FMOWorldGen::SampleHeightCm(*Params, P.X, P.Y, Seed, Params->Tuning.VoxelSizeCm);
	}
	const double HeightUs = Time(TEXT("SampleHeightCm"), [&](const FVector& P) { return FMOWorldGen::SampleHeightCm(*Params, P.X, P.Y, Seed, Params->Tuning.VoxelSizeCm); });
	Time(TEXT("SampleHeightCm + OutClimate"), [&](const FVector& P) { FMOClimateSample C; return FMOWorldGen::SampleHeightCm(*Params, P.X, P.Y, Seed, Params->Tuning.VoxelSizeCm, &C) + C.Moisture; });
	const double ClimateUs = Time(TEXT("SampleClimate"), [&](const FVector& P) { return FMOWorldGen::SampleClimate(*Params, P.X, P.Y, Seed).Moisture; });
	Time(TEXT("WaterMask.IsInWater (1.05)"), [&](const FVector& P) { return Water.IsInWater(P.X, P.Y, P.Z) ? 1.0 : 0.0; });
	Time(TEXT("EdgeMask.IsInWater (1.008)"), [&](const FVector& P) { return Edge.IsInWater(P.X, P.Y, P.Z) ? 1.0 : 0.0; });
	Time(TEXT("EdgeMask.ShoreFactor"), [&](const FVector& P) { return Edge.ShoreFactor(P.X, P.Y, P.Z, 300.0); });
	Time(TEXT("DistanceToSinkholeRimZone"), [&](const FVector& P) { return FMOWorldGen::DistanceToSinkholeRimZone(*Params, P.X, P.Y, Seed); });
	Time(TEXT("PerlinNoise2D (clump field)"), [&](const FVector& P) { return FMath::PerlinNoise2D(FVector2D(P.X / 12000.0, P.Y / 12000.0)); });
	UE_LOG(LogMOFramework, Display, TEXT("[MOPCGCOST] one point of the tree tier ~= height + 5 x climate (centre + 4 edge-blend taps) + 2 water + sinkhole: the edge blend alone is %.1f us"), 4.0 * ClimateUs);

	// The SLOW CELL of the packaged perf tour (Oct 13 2026): the tree-tier cell at (1024002, -767984) of world seed 1029024224 spent ~690 us a point in the
	// generator stage (mo.PCG.BiomeSpawnerProfile: "gen 5178 ms" for 7545 points) -- the same queries at the same place, with the game's own terrain seed.
	{
		const int32 SlowSeed = UMOWorldSeedSubsystem::TerrainSeedFromWorldSeed(1029024224);
		const FMOPCGWaterMask SlowWater(Params, SlowSeed);
		const FMOPCGWaterMask SlowEdge(Params, SlowSeed, 1.008f);
		TArray<FVector> Slow;
		for (int32 J = 0; J < 48; ++J)
		{
			for (int32 I = 0; I < 48; ++I)
			{
				const double X = 1024002.0 + I * 1070.0, Y = -767984.0 + J * 1070.0;
				Slow.Add(FVector(X, Y, FMOWorldGen::SampleHeightCm(*Params, X, Y, SlowSeed, Params->Tuning.VoxelSizeCm)));
			}
		}
		auto TimeSlow = [&](const TCHAR* Name, TFunctionRef<double(const FVector&)> Fn)
		{
			const double T0 = FPlatformTime::Seconds();
			double Acc = 0.0;
			for (const FVector& P : Slow)
			{
				Acc += Fn(P);
			}
			Sink += Acc;
			const double Us = (FPlatformTime::Seconds() - T0) * 1.0e6 / Slow.Num();
			UE_LOG(LogMOFramework, Display, TEXT("[MOPCGCOST] SLOW CELL %-28s %10.2f us/point"), Name, Us);
			AddInfo(FString::Printf(TEXT("SLOW CELL %s: %.2f us/point"), Name, Us));
		};
		TimeSlow(TEXT("SampleHeightCm"), [&](const FVector& P) { return FMOWorldGen::SampleHeightCm(*Params, P.X, P.Y, SlowSeed, Params->Tuning.VoxelSizeCm); });
		TimeSlow(TEXT("EdgeMask.IsInWater"), [&](const FVector& P) { return SlowEdge.IsInWater(P.X, P.Y, P.Z) ? 1.0 : 0.0; });
		TimeSlow(TEXT("WaterMask.IsInWater"), [&](const FVector& P) { return SlowWater.IsInWater(P.X, P.Y, P.Z) ? 1.0 : 0.0; });
		TimeSlow(TEXT("DistanceToSinkholeRimZone"), [&](const FVector& P) { return FMOWorldGen::DistanceToSinkholeRimZone(*Params, P.X, P.Y, SlowSeed); });
		TimeSlow(TEXT("SampleClimate"), [&](const FVector& P) { return FMOWorldGen::SampleClimate(*Params, P.X, P.Y, SlowSeed).Moisture; });
	}

	// SWEEP: the cost of a height sample cell by cell over a 15 km square around the coast spawn (30 x 30 cells of 512 m, 64 samples each, in raster order
	// like a spawner cell). A cache pathology shows as one cell hundreds of times slower than the rest (the basin collision above was 500x).
	{
		const int32 SweepSeed = UMOWorldSeedSubsystem::TerrainSeedFromWorldSeed(1029024224);
		TArray<TPair<double, FVector2D>> Cells;
		double Total = 0.0;
		for (int32 CY = 0; CY < 30; ++CY)
		{
			for (int32 CX = 0; CX < 30; ++CX)
			{
				const double X0c = 1222000.0 - 768000.0 + CX * 51200.0, Y0c = -680000.0 - 768000.0 + CY * 51200.0;
				const double T0 = FPlatformTime::Seconds();
				double Acc = 0.0;
				for (int32 J = 0; J < 8; ++J)
				{
					for (int32 I = 0; I < 8; ++I)
					{
						Acc += FMOWorldGen::SampleHeightCm(*Params, X0c + I * 6400.0, Y0c + J * 6400.0, SweepSeed, Params->Tuning.VoxelSizeCm);
					}
				}
				Sink += Acc;
				const double Us = (FPlatformTime::Seconds() - T0) * 1.0e6 / 64.0;
				Total += Us;
				Cells.Add({ Us, FVector2D(X0c, Y0c) });
			}
		}
		Cells.Sort([](const TPair<double, FVector2D>& A, const TPair<double, FVector2D>& B) { return A.Key > B.Key; });
		const double Mean = Total / Cells.Num();
		UE_LOG(LogMOFramework, Display, TEXT("[MOPCGCOST] SWEEP 900 cells: mean %.2f us/sample, worst %.2f at (%.0f, %.0f), 2nd %.2f, 10th %.2f"), Mean, Cells[0].Key,
			Cells[0].Value.X, Cells[0].Value.Y, Cells[1].Key, Cells[9].Key);
		AddInfo(FString::Printf(TEXT("SWEEP: mean %.2f us, worst %.2f us at (%.0f, %.0f), 2nd %.2f, 10th %.2f"), Mean, Cells[0].Key, Cells[0].Value.X, Cells[0].Value.Y, Cells[1].Key, Cells[9].Key));
	}

	// COARSE CHUNK: what the voxel mesher asks of a far LOD-11 chunk (32 km, a 1 km step) -- the chunk (-3276800, 0) of the coast spawn's world that took 5 s to mesh
	// while every sample looked up an upland lake (FMOWorldGen::IsWaterTierResolved now skips basins the step cannot resolve).
	{
		const int32 CoarseSeed = UMOWorldSeedSubsystem::TerrainSeedFromWorldSeed(1029024224);
		const float Step = Params->Tuning.VoxelSizeCm * 2048.0f;
		const double T0 = FPlatformTime::Seconds();
		double Acc = 0.0;
		for (int32 J = 0; J < 33; ++J)
		{
			for (int32 I = 0; I < 33; ++I)
			{
				Acc += FMOWorldGen::SampleColumn(*Params, -3276800.0 + I * (double)Step, J * (double)Step, CoarseSeed, Step).HeightCm;
			}
		}
		Sink += Acc;
		const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
		UE_LOG(LogMOFramework, Display, TEXT("[MOPCGCOST] COARSE CHUNK LOD 11: 1089 columns in %.1f ms (%.2f us/column)"), Ms, Ms * 1000.0 / 1089.0);
		AddInfo(FString::Printf(TEXT("COARSE CHUNK LOD 11: %.1f ms for 1089 columns"), Ms));
	}

	TestTrue(TEXT("the queries return finite values"), FMath::IsFinite(Sink));
	TestTrue(TEXT("the height query was timed"), HeightUs > 0.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
