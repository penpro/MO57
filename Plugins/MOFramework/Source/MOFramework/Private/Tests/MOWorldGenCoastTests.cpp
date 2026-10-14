// Tests for the new-game coastal spawn (FMOWorldGen::FindCoastalSpawn) and the upland lakes (tier 2 basins above sea level). Headless-safe.

#include "Misc/AutomationTest.h"
#include "HAL/PlatformTime.h"
#include "MOWorldGen.h"
#include "MOUplandLakeSubsystem.h"
#include "MOWorldSeedSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FMOWorldGenParamsRef MakeCoastParams(TFunctionRef<void(FMOWorldGenTuning&)> Modify = [](FMOWorldGenTuning&) {})
	{
		TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> P = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
		Modify(P->Tuning);
		P->Finalize();
		return FMOWorldGenParamsRef(P);
	}

	float Percentile(TArray<float> V, float Fraction)
	{
		if (V.IsEmpty())
		{
			return 0.0f;
		}
		V.Sort();
		return V[FMath::Clamp((int32)(Fraction * (float)(V.Num() - 1) + 0.5f), 0, V.Num() - 1)];
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_CoastalSpawn,
	"MOFramework.WorldGen.CoastalSpawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_CoastalSpawn::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = MakeCoastParams();
	const FMOWorldGenTuning& T = P->Tuning;

	constexpr int32 NumSeeds = 40;
	int32 Found = 0, BadGround = 0, BadWater = 0, Nondeterministic = 0, WithSkyline = 0, OnIsland = 0;
	TArray<float> DistancesKm, ReliefM, OpenKm, FarKm, ElevationDeg, LandKm2;
	double TotalMs = 0.0, WorstMs = 0.0;

	for (int32 I = 0; I < NumSeeds; ++I)
	{
		const int32 Seed = 1000 + 7919 * I;
		FMOCoastalSpawn A;
		const double T0 = FPlatformTime::Seconds();
		const bool bFound = FMOWorldGen::FindCoastalSpawn(*P, Seed, A);
		const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
		TotalMs += Ms;
		WorstMs = FMath::Max(WorstMs, Ms);
		if (!bFound)
		{
			AddInfo(FString::Printf(TEXT("seed %d: no coastal spawn within the search radius"), Seed));
			continue;
		}
		++Found;

		FMOCoastalSpawn B;
		FMOWorldGen::FindCoastalSpawn(*P, Seed, B);
		Nondeterministic += (A.Location != B.Location || A.YawDegrees != B.YawDegrees) ? 1 : 0;

		// Standing on dry, gentle ground the generator agrees about.
		const float Ground = FMOWorldGen::SampleHeightCm(*P, A.Location.X, A.Location.Y, Seed, T.VoxelSizeCm);
		BadGround += (Ground != A.GroundCm || Ground < T.SeaLevelCm + 150.0f || Ground > T.SeaLevelCm + 3000.0f) ? 1 : 0;

		// Looking at open water: walking along the view direction from the spawn, the first water comes within 400 m and then 85% of the next 1.5 km is water.
		const double Dx = FMath::Cos(FMath::DegreesToRadians(A.YawDegrees)), Dy = FMath::Sin(FMath::DegreesToRadians(A.YawDegrees));
		double FirstWater = -1.0;
		for (double S = 0.0; S <= 40000.0; S += 500.0)
		{
			if (FMOWorldGen::SampleHeightCm(*P, A.Location.X + Dx * S, A.Location.Y + Dy * S, Seed, T.VoxelSizeCm) < T.SeaLevelCm - 20.0f)
			{
				FirstWater = S;
				break;
			}
		}
		int32 Wet = 0, Total = 0;
		if (FirstWater >= 0.0)
		{
			for (double S = FirstWater; S <= FirstWater + 150000.0; S += 2500.0)
			{
				++Total;
				Wet += (FMOWorldGen::SampleHeightCm(*P, A.Location.X + Dx * S, A.Location.Y + Dy * S, Seed, T.VoxelSizeCm) < T.SeaLevelCm - 20.0f) ? 1 : 0;
			}
		}
		const bool bBadWater = (FirstWater < 0.0 || Total == 0 || Wet * 100 < Total * 85);
		BadWater += bBadWater ? 1 : 0;
		if (bBadWater)
		{
			AddInfo(FString::Printf(TEXT("seed %d: spawn (%.0f, %.0f) yaw %.1f: first water at %.0f cm, %d of %d samples wet"), Seed, A.Location.X, A.Location.Y, A.YawDegrees, FirstWater, Wet, Total));
		}

		// Never an island: cross-check with a DIFFERENT lattice (225 m, not the search's 175 / 250 / 350 m) and a looser bar (80% of the required area; the lattices cut the coast differently).
		const FMOCoastalSearch Defaults;
		const int32 NeedCells = (int32)(0.8f * Defaults.MinLandmassKm2 / FMath::Square(22500.0f / 100000.0f));
		const int32 Measured = FMOWorldGen::MeasureLandmassCells(*P, Seed, A.Location.X, A.Location.Y, 22500.0f, NeedCells);
		OnIsland += (Measured < NeedCells) ? 1 : 0;
		if (Measured < NeedCells)
		{
			AddInfo(FString::Printf(TEXT("seed %d: spawn (%.0f, %.0f) measures %d of %d cells of land at 225 m (%d at 200 m, %d at 300 m)"), Seed, A.Location.X, A.Location.Y, Measured, NeedCells,
				FMOWorldGen::MeasureLandmassCells(*P, Seed, A.Location.X, A.Location.Y, 20000.0f, 100000), FMOWorldGen::MeasureLandmassCells(*P, Seed, A.Location.X, A.Location.Y, 30000.0f, 100000)));
		}
		LandKm2.Add(A.LandmassKm2);

		DistancesKm.Add(A.DistanceFromOriginCm / 100000.0f);
		ReliefM.Add(A.FarReliefCm / 100.0f);
		OpenKm.Add(A.OpenWaterCm / 100000.0f);
		FarKm.Add(A.FarDistanceCm / 100000.0f);
		const float Elevation = FMath::RadiansToDegrees(FMath::Atan2(A.FarReliefCm - 200.0f, A.FarDistanceCm));
		ElevationDeg.Add(Elevation);
		WithSkyline += (Elevation >= 1.0f) ? 1 : 0;
	}

	AddInfo(FString::Printf(TEXT("%d of %d seeds found a coast. distance from origin km p50/p90 = %.1f / %.1f; open water km p50 = %.1f; the highest ground in view: %.0f m (p10 %.0f, p90 %.0f) "
		"at %.1f km, standing %.1f deg above the horizon (p10 %.1f, p90 %.1f); %d seeds with >= 1 deg of skyline; search %.0f ms mean, %.0f ms worst"),
		Found, NumSeeds, Percentile(DistancesKm, 0.5f), Percentile(DistancesKm, 0.9f), Percentile(OpenKm, 0.5f), Percentile(ReliefM, 0.5f), Percentile(ReliefM, 0.1f),
		Percentile(ReliefM, 0.9f), Percentile(FarKm, 0.5f), Percentile(ElevationDeg, 0.5f), Percentile(ElevationDeg, 0.1f), Percentile(ElevationDeg, 0.9f), WithSkyline,
		TotalMs / NumSeeds, WorstMs));

	TestTrue(FString::Printf(TEXT("at least 90%% of seeds have a coastal spawn (%d of %d)"), Found, NumSeeds), Found * 10 >= NumSeeds * 9);
	TestEqual(TEXT("every spawn stands on dry, gentle ground that matches the generator"), BadGround, 0);
	TestEqual(TEXT("every spawn looks at open water (first water near, 85% water beyond)"), BadWater, 0);
	TestEqual(TEXT("the search is deterministic"), Nondeterministic, 0);
	TestEqual(TEXT("no spawn stands on an island (a landmass under the minimum area)"), OnIsland, 0);
	TestTrue(FString::Printf(TEXT("most coasts look across the water at a real skyline: ground standing >= 1 degree above the horizon on at least 3 of 4 seeds (%d of %d)"), WithSkyline, Found), WithSkyline * 4 >= Found * 3);
	TestTrue(FString::Printf(TEXT("the search is fast enough for a loading screen: worst %.0f ms (limit 1500)"), WorstMs), WorstMs < 1500.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_CoastalSpawnOnVegetatedGround,
	"MOFramework.WorldGen.CoastalSpawnOnVegetatedGround",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// The spawn stands on CLIMATE-biome ground (forest / meadow / scrub), not on the shore strand: on a wide flat coast the old search took the first dry spot 50 m back from the
// water, which is all shore sand (world seed -1675528670: the nearest tree 141 m away and nothing but sand in view, reported as "zero PCG"). Real biomes (published params).
bool FMOWorldGen_CoastalSpawnOnVegetatedGround::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = FMOWorldGenParamsProvider::Get();
	if (!TestTrue(TEXT("the published params have biomes"), P->Biomes.Num() >= 2))
	{
		return false;
	}
	auto ClimateShare = [&P](const FMOCoastalSpawn& S, int32 TerrainSeed)
	{
		FMOClimateSample Climate;
		const float Height = FMOWorldGen::SampleHeightCm(*P, S.Location.X, S.Location.Y, TerrainSeed, P->Tuning.VoxelSizeCm, &Climate);
		FMOWorldGen::FBiomeWeight Weights[3];
		const int32 Num = FMOWorldGen::SampleSurfaceBiomeWeights(*P, S.Location.X, S.Location.Y, TerrainSeed, Height, Climate.Moisture,
			FMOWorldGen::TemperatureAtHeight(*P, Climate.TemperatureSeaLevel, Height), Weights, 3);
		float Share = 0.0f;
		for (int32 I = 0; I < Num; ++I)
		{
			Share += P->Biomes.IsValidIndex(Weights[I].Biome) && P->Biomes[Weights[I].Biome].IsClimateBiome() ? Weights[I].Weight : 0.0f;
		}
		return Share;
	};

	TArray<int32> WorldSeeds = { -1675528670, 1029024224, 7, 2024, 42 };
	for (int32 I = 0; I < 15; ++I)
	{
		WorldSeeds.Add(1000 + 7919 * I);
	}
	const FMOCoastalSearch Search;
	FMOCoastalSearch Old = Search;
	Old.MinClimateGroundWeight = 0.0f;
	int32 Found = 0, OnBare = 0, OldOnBare = 0;
	bool bReportedSeedOld = false;
	for (const int32 WorldSeed : WorldSeeds)
	{
		const int32 TerrainSeed = UMOWorldSeedSubsystem::TerrainSeedFromWorldSeed(WorldSeed);
		FMOCoastalSpawn S, O;
		if (FMOWorldGen::FindCoastalSpawn(*P, TerrainSeed, O, Old))
		{
			const float OldShare = ClimateShare(O, TerrainSeed);
			OldOnBare += OldShare < Search.MinClimateGroundWeight ? 1 : 0;
			if (WorldSeed == -1675528670)
			{
				bReportedSeedOld = OldShare < Search.MinClimateGroundWeight;
				AddInfo(FString::Printf(TEXT("world seed %d without the rule: (%.0f, %.0f), climate ground %.2f"), WorldSeed, O.Location.X, O.Location.Y, OldShare));
			}
		}
		if (!FMOWorldGen::FindCoastalSpawn(*P, TerrainSeed, S, Search))
		{
			AddInfo(FString::Printf(TEXT("world seed %d: no coastal spawn on vegetated ground"), WorldSeed));
			continue;
		}
		++Found;
		const float Share = ClimateShare(S, TerrainSeed);
		OnBare += Share < Search.MinClimateGroundWeight ? 1 : 0;
		AddInfo(FString::Printf(TEXT("world seed %d: spawn (%.0f, %.0f), climate ground %.2f, %.0f m from the old spawn"), WorldSeed, S.Location.X, S.Location.Y, Share,
			FVector2D::Distance(S.Location, O.Location) / 100.0f));
	}
	TestTrue(FString::Printf(TEXT("at least 90%% of seeds still find a coastal spawn (%d of %d)"), Found, WorldSeeds.Num()), Found * 10 >= WorldSeeds.Num() * 9);
	TestEqual(TEXT("every spawn stands on climate-biome ground (>= MinClimateGroundWeight)"), OnBare, 0);
	TestTrue(TEXT("CONTROL: the reported seed's spawn WITHOUT the rule is on shore ground (so the check above can fail)"), bReportedSeedOld);
	AddInfo(FString::Printf(TEXT("without the rule %d of %d spawns stood on band-biome ground"), OldOnBare, WorldSeeds.Num()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_UplandLakes,
	"MOFramework.WorldGen.UplandLakes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_UplandLakes::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = MakeCoastParams();
	const FMOWorldGenTuning& T = P->Tuning;
	constexpr int32 Seed = 424242;
	const int32 HalfCells = (int32)(T.WorldHalfSizeCm / T.UplandLakeCellCm);   // 13

	// 1. Density: about one lake per 5 square kilometres, give or take (the fit test thins the cells out).
	TArray<FMOWorldGen::FLake> Lakes;
	const double T0 = FPlatformTime::Seconds();
	for (int64 Cy = -HalfCells; Cy < HalfCells; ++Cy)
	{
		for (int64 Cx = -HalfCells; Cx < HalfCells; ++Cx)
		{
			FMOWorldGen::FLake L;
			if (FMOWorldGen::GetLakeInCell(*P, 2, Cx, Cy, Seed, L))
			{
				Lakes.Add(L);
			}
		}
	}
	const double ResolveMs = (FPlatformTime::Seconds() - T0) * 1000.0;
	const double CellsKm2 = (double)(2 * HalfCells) * (double)(2 * HalfCells) * FMath::Square((double)T.UplandLakeCellCm / 100000.0);
	const double PerLake = Lakes.Num() > 0 ? CellsKm2 / Lakes.Num() : 0.0;
	AddInfo(FString::Printf(TEXT("%d upland lakes in %.0f km^2: one per %.1f km^2; resolving all %d cells cold took %.0f ms (%.2f ms per cell)"),
		Lakes.Num(), CellsKm2, PerLake, 4 * HalfCells * HalfCells, ResolveMs, ResolveMs / (4.0 * HalfCells * HalfCells)));
	TestTrue(FString::Printf(TEXT("about one lake per 5 km^2 of the world: between 1 per 3 and 1 per 8 km^2 (got 1 per %.1f)"), PerLake), Lakes.Num() > 0 && PerLake > 3.0 && PerLake < 8.0);

	// 2. Every lake: above sea level, walled in, with a dry bank all round and a bowl under the surface.
	const FMOWorldGenParamsRef SinkOn = MakeCoastParams([](FMOWorldGenTuning& Tuning) { Tuning.SinkholeChance = 1.0f; });
	const FMOWorldGenParamsRef SinkOff = MakeCoastParams([](FMOWorldGenTuning& Tuning) { Tuning.SinkholeChance = 1.0f; Tuning.bEnableUplandLakes = false; });
	int32 BelowSea = 0, OutsideMargin = 0, CentreDry = 0, ShoreBad = 0, WetOutside = 0, SinkholeInside = 0, SinkholesBlocked = 0, ZoneBad = 0;
	float MinSurfaceM = 1.0e9f, MaxSurfaceM = -1.0e9f, MeanRadiusM = 0.0f;
	for (const FMOWorldGen::FLake& L : Lakes)
	{
		MinSurfaceM = FMath::Min(MinSurfaceM, L.SurfaceCm / 100.0f);
		MaxSurfaceM = FMath::Max(MaxSurfaceM, L.SurfaceCm / 100.0f);
		MeanRadiusM += L.RadiusCm / 100.0f / Lakes.Num();
		BelowSea += (L.SurfaceCm < T.SeaLevelCm + T.UplandLakeMinGroundAboveSeaCm - T.UplandLakeFreeboardCm * T.VerticalScale - 1.0f) ? 1 : 0;

		// The reach (2.5 radii) stays inside the cell: a column only asks its own cell.
		const double Cell = (double)T.UplandLakeCellCm;
		const double MinX = ((double)L.CellX + 0.0) * Cell, MaxX = ((double)L.CellX + 1.0) * Cell, MinY = ((double)L.CellY + 0.0) * Cell, MaxY = ((double)L.CellY + 1.0) * Cell;
		const double Reach = 2.5 * (double)L.RadiusCm;
		OutsideMargin += (L.Center.X - Reach < MinX || L.Center.X + Reach > MaxX || L.Center.Y - Reach < MinY || L.Center.Y + Reach > MaxY) ? 1 : 0;

		// The centre of the bowl is under the surface by (about) the lake depth.
		CentreDry += (FMOWorldGen::SampleHeightCm(*P, L.Center.X, L.Center.Y, Seed) > L.SurfaceCm - 0.9f * L.DepthCm) ? 1 : 0;

		// Along the long axis: water a hair inside the shore, dry bank a hair outside. (The cut is exact at q = 1, so test q = 0.97 and 1.06.)
		auto AtQ = [&](float Q, float Angle)
		{
			const float Ca = FMath::Cos(Angle), Sa = FMath::Sin(Angle);
			const float Lx = Q * Ca * L.RadiusCm, Ly = Q * Sa * L.RadiusCm * L.Aspect;
			return FMOWorldGen::SampleHeightCm(*P, L.Center.X + L.CosAngle * Lx - L.SinAngle * Ly, L.Center.Y + L.SinAngle * Lx + L.CosAngle * Ly, Seed);
		};
		bool bShore = true;
		for (int32 K = 0; K < 8; ++K)
		{
			const float Angle = K * UE_PI / 4.0f;
			bShore = bShore && AtQ(0.97f, Angle) < L.SurfaceCm && AtQ(1.06f, Angle) >= L.SurfaceCm;
		}
		ShoreBad += bShore ? 0 : 1;

		// The water plane is the +-1 box; outside the ellipse it must be buried: dense grid, ground >= surface + 20 cm wherever q >= 1.
		bool bBuried = true;
		for (int32 J = -20; J <= 20 && bBuried; ++J)
		{
			for (int32 I = -20; I <= 20 && bBuried; ++I)
			{
				const float U = (float)I / 20.0f, V = (float)J / 20.0f;
				if (U * U + V * V < 1.06f)   // q >= 1.03: right at the shore the bank has not risen 20 cm yet
				{
					continue;
				}
				const float Lx = U * L.RadiusCm, Ly = V * L.RadiusCm * L.Aspect;
				const float H = FMOWorldGen::SampleHeightCm(*P, L.Center.X + L.CosAngle * Lx - L.SinAngle * Ly, L.Center.Y + L.SinAngle * Lx + L.CosAngle * Ly, Seed);
				bBuried = H >= L.SurfaceCm + 20.0f;
			}
		}
		WetOutside += bBuried ? 0 : 1;

		// Zone helper agrees with the geometry.
		ZoneBad += (!FMOWorldGen::IsInUplandLakeZone(*P, L.Center.X, L.Center.Y, Seed, 1.0f) || FMOWorldGen::IsInUplandLakeZone(*P, L.Center.X + 2.6 * L.RadiusCm * L.CosAngle, L.Center.Y + 2.6 * L.RadiusCm * L.SinAngle, Seed, 1.5f)) ? 1 : 0;

		// No sinkhole opens in the bowl or on the bank. (Every 300 m cell gets one with SinkholeChance = 1; the same scan with the lakes switched off shows the gate has work to do.)
		const double Cs = (double)T.SinkholeCellCm, SinkReach = 1.9 * (double)L.RadiusCm;
		for (int64 Sy = (int64)FMath::FloorToDouble((L.Center.Y - SinkReach) / Cs); Sy <= (int64)FMath::FloorToDouble((L.Center.Y + SinkReach) / Cs); ++Sy)
		{
			for (int64 Sx = (int64)FMath::FloorToDouble((L.Center.X - SinkReach) / Cs); Sx <= (int64)FMath::FloorToDouble((L.Center.X + SinkReach) / Cs); ++Sx)
			{
				FMOSinkhole S;
				if (FMOWorldGen::GetSinkholeInCell(*SinkOn, Sx, Sy, Seed, S) && FMOWorldGen::IsInUplandLakeZone(*SinkOn, S.Center.X, S.Center.Y, Seed, 1.8f))
				{
					++SinkholeInside;
				}
				if (FMOWorldGen::GetSinkholeInCell(*SinkOff, Sx, Sy, Seed, S) && FMOWorldGen::IsInUplandLakeZone(*SinkOn, S.Center.X, S.Center.Y, Seed, 1.8f))
				{
					++SinkholesBlocked;
				}
			}
		}
	}
	AddInfo(FString::Printf(TEXT("surface %.0f..%.0f m above sea level; mean radius %.0f m"), MinSurfaceM, MaxSurfaceM, MeanRadiusM));
	TestEqual(TEXT("every lake surface is above the upland limit"), BelowSea, 0);
	TestEqual(TEXT("every lake's reach stays inside its cell"), OutsideMargin, 0);
	TestEqual(TEXT("every bowl is cut to ~its full depth"), CentreDry, 0);
	TestEqual(TEXT("water just inside the shore and dry bank just outside it, all round"), ShoreBad, 0);
	TestEqual(TEXT("ground outside the ellipse (inside the water plane's box) is buried 20 cm above the surface"), WetOutside, 0);
	TestEqual(TEXT("the zone helper matches the geometry"), ZoneBad, 0);
	TestEqual(TEXT("no sinkhole inside a lake zone"), SinkholeInside, 0);
	TestTrue(FString::Printf(TEXT("the sinkhole gate had work to do: %d sinkholes would have opened in a lake zone"), SinkholesBlocked), SinkholesBlocked > 0);

	// 3. Continuous: no cliff across a lake, its bank, or the outer reach where the bank ends.
	{
		float MaxStep = 0.0f;
		for (int32 N = 0; N < FMath::Min(Lakes.Num(), 40); ++N)
		{
			const FMOWorldGen::FLake& L = Lakes[N];
			for (int32 Dir = 0; Dir < 4; ++Dir)
			{
				const float Angle = Dir * UE_PI / 2.0f + 0.3f;
				const double Ux = FMath::Cos(Angle), Uy = FMath::Sin(Angle);
				float Prev = 0.0f;
				for (int32 I = -300; I <= 300; ++I)
				{
					const double R = (double)L.RadiusCm * 3.0 * (double)I / 300.0;   // through the centre, out past the 2.5 radii reach on both sides
					const float H = FMOWorldGen::SampleHeightCm(*P, L.Center.X + Ux * R, L.Center.Y + Uy * R, Seed);
					if (I > -300)
					{
						MaxStep = FMath::Max(MaxStep, FMath::Abs(H - Prev) / (float)(L.RadiusCm * 3.0 / 300.0));   // slope (rise per cm run)
					}
					Prev = H;
				}
			}
		}
		TestTrue(FString::Printf(TEXT("no cliff: steepest slope across a lake and its bank is %.2f (limit 1.0)"), MaxStep), MaxStep < 1.0f);
	}

	// 4. Deterministic, and the switch really switches them off.
	{
		int32 Differs = 0, OffMoved = 0;
		const FMOWorldGenParamsRef Off = MakeCoastParams([](FMOWorldGenTuning& Tuning) { Tuning.bEnableUplandLakes = false; });
		for (int32 N = 0; N < FMath::Min(Lakes.Num(), 20); ++N)
		{
			FMOWorldGen::FLake Again;
			FMOWorldGen::GetLakeInCell(*P, 2, Lakes[N].CellX, Lakes[N].CellY, Seed, Again);
			Differs += (Again.Center != Lakes[N].Center || Again.SurfaceCm != Lakes[N].SurfaceCm || Again.RadiusCm != Lakes[N].RadiusCm) ? 1 : 0;
			const double X = Lakes[N].Center.X, Y = Lakes[N].Center.Y;
			OffMoved += (FMOWorldGen::SampleHeightCm(*Off, X, Y, Seed) != FMOWorldGen::SampleBaseHeightCm(*Off, X, Y, Seed)) ? 1 : 0;
		}
		TestEqual(TEXT("upland lakes are deterministic"), Differs, 0);
		TestEqual(TEXT("bEnableUplandLakes = false leaves the lake sites untouched (height = base height there)"), OffMoved, 0);
	}

	// 5. GetUplandLakesNear finds a lake by its centre.
	if (!Lakes.IsEmpty())
	{
		TArray<FMOWorldGen::FLake> Near;
		FMOWorldGen::GetUplandLakesNear(*P, Lakes[0].Center.X, Lakes[0].Center.Y, 1000.0, Seed, Near);
		bool bHasIt = false;
		for (const FMOWorldGen::FLake& L : Near)
		{
			bHasIt = bHasIt || (L.CellX == Lakes[0].CellX && L.CellY == Lakes[0].CellY);
		}
		TestTrue(TEXT("GetUplandLakesNear returns the lake at its own centre"), bHasIt);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_Landmass,
	"MOFramework.WorldGen.Landmass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_Landmass::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = MakeCoastParams();
	constexpr int32 Cap = 15000;   // 600 km^2 at 200 m

	// The v5.1 sea is deep and the hill noise fades out over it, so the world no longer has the shoal islets the old shallow shelf produced. The measure still has to tell an island from a
	// mainland, so the positive control runs on the same world with the OLD shallow sea floor and the fade off (islets everywhere), as well as on the real one.
	FMOWorldGenParams Shallow = *P;
	Shallow.Tuning.ContinentSplineM = { FVector2D(-1.00, -6.0), FVector2D(-0.55, 0.0), FVector2D(-0.30, 6.0), FVector2D(-0.15, 12.0), FVector2D(0.00, 18.0), FVector2D(0.25, 32.0), FVector2D(0.55, 55.0), FVector2D(1.00, 90.0) };
	Shallow.Tuning.HillSeaFadeLow = 2.0f;
	Shallow.Tuning.HillSeaFadeHigh = 2.0f;
	Shallow.Finalize();

	struct FSurvey { int32 Water = 0, WaterNonZero = 0, Islands = 0, Mainland = 0, Land = 0, SmallestIsland = Cap; };
	auto Survey = [&](const FMOWorldGenParams& Q)
	{
		FSurvey R;
		for (int32 SeedIndex = 0; SeedIndex < 10; ++SeedIndex)
		{
			const int32 Seed = 1000 + 7919 * SeedIndex;
			for (int32 Iy = -3; Iy <= 3; ++Iy)
			{
				for (int32 Ix = -3; Ix <= 3; ++Ix)
				{
					const double X = Ix * 800000.0 + 12345.0, Y = Iy * 800000.0 - 6789.0;
					const float H = FMOWorldGen::SampleBaseHeightCm(Q, X, Y, Seed, 10000.0f);   // the step MeasureLandmassCells uses at a 200 m lattice
					const int32 Cells = FMOWorldGen::MeasureLandmassCells(Q, Seed, X, Y, 20000.0f, Cap);
					if (H < Q.Tuning.SeaLevelCm - 20.0f)
					{
						++R.Water;
						R.WaterNonZero += Cells != 0 ? 1 : 0;
						continue;
					}
					++R.Land;
					R.Mainland += (Cells >= Cap) ? 1 : 0;
					if (Cells > 0 && Cells < Cap)
					{
						++R.Islands;
						R.SmallestIsland = FMath::Min(R.SmallestIsland, Cells);
					}
				}
			}
		}
		return R;
	};

	const FSurvey Real = Survey(*P), Old = Survey(Shallow);
	AddInfo(FString::Printf(TEXT("real world: %d water points, %d land points: %d on a mainland (>= 600 km^2), %d on smaller land"), Real.Water, Real.Land, Real.Mainland, Real.Islands));
	AddInfo(FString::Printf(TEXT("shallow-sea control: %d water points, %d land points: %d on a mainland, %d on smaller land (smallest %.1f km^2)"), Old.Water, Old.Land, Old.Mainland, Old.Islands, Old.SmallestIsland * 0.04f));
	TestEqual(TEXT("open water measures as no land at all"), Real.WaterNonZero + Old.WaterNonZero, 0);
	TestTrue(TEXT("the world has a mainland, and the measure sees it"), Real.Mainland > 0 && Old.Mainland > 0);
	TestTrue(TEXT("CONTROL: where islands exist the measure sees them and does not call them mainland"), Old.Islands > 0);

	// The cap: counting stops there.
	TestTrue(TEXT("counting stops at the cap"), FMOWorldGen::MeasureLandmassCells(*P, 1000, 12345.0, -6789.0, 20000.0f, 10) <= 10);
	return true;
}

// The water of an upland lake is an Oceanology lake actor, and its mesh is a quad tree over WORLD-aligned bounds: it cannot be rotated to the ellipse, so the plane is the ellipse's axis-aligned
// bounding box (UMOUplandLakeSubsystem::ComputePlaneHalfSize), a different rectangle from the rotated one the generator buries (UplandLakes above). Outside the ellipse the ground must still hide
// that plane, or water hovers over a hillside. Measured over the same lakes: how much of the plane's outside area is ground below the surface (water showing), and the lowest margin.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_UplandLakePlane,
	"MOFramework.WorldGen.UplandLakePlane",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_UplandLakePlane::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = MakeCoastParams();
	const FMOWorldGenTuning& T = P->Tuning;
	constexpr int32 Seed = 424242;
	const int32 HalfCells = (int32)(T.WorldHalfSizeCm / T.UplandLakeCellCm);

	int32 NumLakes = 0, LakesExposed = 0;
	double SamplesOutside = 0.0, SamplesExposed = 0.0;
	float WorstExposedCm = 0.0f;
	FVector2D WorstAt = FVector2D::ZeroVector;
	for (int64 Cy = -HalfCells; Cy < HalfCells; ++Cy)
	{
		for (int64 Cx = -HalfCells; Cx < HalfCells; ++Cx)
		{
			FMOWorldGen::FLake L;
			if (!FMOWorldGen::GetLakeInCell(*P, 2, Cx, Cy, Seed, L))
			{
				continue;
			}
			++NumLakes;
			const FVector2D Half = UMOUplandLakeSubsystem::ComputePlaneHalfSize(L);
			constexpr int32 N = 24;   // 49 x 49 over the whole plane
			bool bExposed = false;
			for (int32 J = -N; J <= N; ++J)
			{
				for (int32 I = -N; I <= N; ++I)
				{
					const double Dx = Half.X * (double)I / (double)N, Dy = Half.Y * (double)J / (double)N;
					// The ellipse metric of this point: 1 at the shore.
					const double Lx = (L.CosAngle * Dx + L.SinAngle * Dy) / L.RadiusCm, Ly = (-L.SinAngle * Dx + L.CosAngle * Dy) / (L.RadiusCm * L.Aspect);
					if (Lx * Lx + Ly * Ly < 1.06)   // q >= 1.03: right at the shore the bank has not risen yet (the same cut as UplandLakes)
					{
						continue;
					}
					SamplesOutside += 1.0;
					const float H = FMOWorldGen::SampleHeightCm(*P, L.Center.X + Dx, L.Center.Y + Dy, Seed);
					if (H < L.SurfaceCm)
					{
						SamplesExposed += 1.0;
						bExposed = true;
						if (L.SurfaceCm - H > WorstExposedCm)
						{
							WorstExposedCm = L.SurfaceCm - H;
							WorstAt = FVector2D(L.Center.X + Dx, L.Center.Y + Dy);
						}
					}
				}
			}
			LakesExposed += bExposed ? 1 : 0;
		}
	}
	AddInfo(FString::Printf(TEXT("%d lakes: %d have ground below the water surface somewhere on their (axis-aligned) plane outside the ellipse; %.3f%% of %.0f plane samples; worst %.0f cm below the surface at (%.0f, %.0f)"),
		NumLakes, LakesExposed, SamplesOutside > 0.0 ? 100.0 * SamplesExposed / SamplesOutside : 0.0, SamplesOutside, WorstExposedCm, WorstAt.X, WorstAt.Y));
	TestTrue(TEXT("lakes exist to check"), NumLakes > 20);
	TestEqual(TEXT("no lake's water plane shows over ground outside its ellipse"), LakesExposed, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
