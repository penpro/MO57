// How common caves are where a player walks (N-170; Wes, Oct 18 2026: "caves need to be decently common since they're ALSO hard to spot").
// MOFramework.WorldGen.CaveEntrances judges the hilliest karst tile only; this one samples ordinary 1 km tiles of cave country on three real worlds
// (Wes's seed included) with the game's own snapshot (UMOWorldGenSettings::BuildParams: biomes and all), and reports, for the shipped tuning and
// denser candidates, the share of tiles holding at least one walk-in mouth and the mouths per km2. Headless-safe: no world, no PIE.

#include "Misc/AutomationTest.h"
#include "MOWorldGen.h"
#include "MOWorldGenSettings.h"
#include "MOWorldSeedSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MOCaveReachTest
{
	struct FTile
	{
		double X0 = 0.0, Y0 = 0.0;   // corner (cm)
		bool bLimestone = false;     // the bedrock at the tile's centre
		TArray<float> Ground;        // N x N at Step
		TArray<uint8> Steep;         // a candidate column: steep enough that a mouth could open there
	};

	constexpr double TileCm = 100000.0;   // 1 km
	constexpr double Step = 400.0;        // 4 m: a mouth is a few metres wide (a 25 m scan finds almost none)
	constexpr int32 N = 250;

	/** Up to MaxTiles 1 km tiles whose centre is land in the cave band (above the coast's keep-out height, below the high-ground taper), spread
	 *  over a 60 x 60 km area in a fixed scattered order (the seed decides where land is, not the test). */
	TArray<FTile> PickTiles(const FMOWorldGenParams& P, int32 Seed, int32 MaxTiles)
	{
		const float CaveLow = P.Tuning.SeaLevelCm + P.Tuning.CaveMinGroundAboveSeaCm + 600.0f;
		const float CaveHigh = P.Tuning.CaveMaxGroundCm - P.Tuning.CaveHighFadeCm;
		TArray<FTile> Tiles;
		for (int32 I = 0; I < 3600 && Tiles.Num() < MaxTiles; ++I)
		{
			// A fixed scatter over a 60 x 60 grid of 1 km cells (multiplier coprime to 3600).
			const int32 Cell = (I * 1879) % 3600;
			const double X0 = ((Cell % 60) - 30) * TileCm, Y0 = ((Cell / 60) - 30) * TileCm;
			const float Z = FMOWorldGen::SampleHeightCm(P, X0 + TileCm * 0.5, Y0 + TileCm * 0.5, Seed, 50.0f);
			if (Z <= CaveLow || Z >= CaveHigh)
			{
				continue;
			}
			FTile& T = Tiles.AddDefaulted_GetRef();
			T.X0 = X0;
			T.Y0 = Y0;
			T.bLimestone = FMOWorldGen::SampleBedrock(P, X0 + TileCm * 0.5, Y0 + TileCm * 0.5, Seed) == EMOBedrock::Limestone;
		}
		return Tiles;
	}

	/** Ground on the 4 m grid once per tile (the cave tuning does not move the ground); the steep prefilter uses 8 m central differences and a
	 *  margin under the entrance band's start (flat ground never opens: the generator keeps CaveCoverCm of rock there). */
	void SampleGround(const FMOWorldGenParams& P, int32 Seed, FTile& T)
	{
		T.Ground.SetNumUninitialized(N * N);
		for (int32 Iy = 0; Iy < N; ++Iy)
		{
			for (int32 Ix = 0; Ix < N; ++Ix)
			{
				T.Ground[Iy * N + Ix] = FMOWorldGen::SampleHeightCm(P, T.X0 + (Ix + 0.5) * Step, T.Y0 + (Iy + 0.5) * Step, Seed, 50.0f);
			}
		}
		const float MinTan = FMath::Tan(FMath::DegreesToRadians(FMath::Max(1.0f, P.Tuning.CaveEntranceSlopeMinDeg - 4.0f)));
		T.Steep.SetNumZeroed(N * N);
		for (int32 Iy = 1; Iy < N - 1; ++Iy)
		{
			for (int32 Ix = 1; Ix < N - 1; ++Ix)
			{
				const float Gx = (T.Ground[Iy * N + Ix + 1] - T.Ground[Iy * N + Ix - 1]) / (2.0f * Step);
				const float Gy = (T.Ground[(Iy + 1) * N + Ix] - T.Ground[(Iy - 1) * N + Ix]) / (2.0f * Step);
				T.Steep[Iy * N + Ix] = FMath::Sqrt(Gx * Gx + Gy * Gy) >= MinTan ? 1 : 0;
			}
		}
	}

	/** Walk-in mouths in a tile: open columns (cave air 0.5-2.5 m under the ground) grouped by 30 m cell, as WorldGen.CaveEntrances counts them. */
	int32 CountMouths(const FMOWorldGenParams& Q, int32 Seed, const FTile& T)
	{
		TSet<FIntPoint> Mouths;
		for (int32 Iy = 0; Iy < N; ++Iy)
		{
			for (int32 Ix = 0; Ix < N; ++Ix)
			{
				if (!T.Steep[Iy * N + Ix])
				{
					continue;
				}
				const double X = T.X0 + (Ix + 0.5) * Step, Y = T.Y0 + (Iy + 0.5) * Step;
				const float G = T.Ground[Iy * N + Ix];
				for (float Depth = 50.0f; Depth <= 250.0f; Depth += 50.0f)
				{
					if (FMOWorldGen::SampleCaveDistanceAtGround(Q, X, Y, G - Depth, G, Seed, 50.0f) < 0.0f)
					{
						Mouths.Add(FIntPoint(FMath::FloorToInt32(X / 3000.0), FMath::FloorToInt32(Y / 3000.0)));
						break;
					}
				}
			}
		}
		return Mouths.Num();
	}

	FMOWorldGenParamsRef Variant(const FMOWorldGenParamsRef& Base, TFunctionRef<void(FMOWorldGenTuning&)> Modify)
	{
		TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> Q = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>(*Base);
		Modify(Q->Tuning);
		Q->Finalize();
		return FMOWorldGenParamsRef(Q);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_CaveReach,
	"MOFramework.WorldGen.CaveReach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_CaveReach::RunTest(const FString& Parameters)
{
	using namespace MOCaveReachTest;
	const FMOWorldGenParamsRef Base = UMOWorldGenSettings::BuildParams();
	const int32 WorldSeeds[] = { 670711536, 31415926, 12345 };   // Wes's world, the coastal-spawn world of Oct 17, the DumpCaveMaps world
	constexpr int32 TilesPerSeed = 8;

	struct FCandidate { const TCHAR* Name; TFunction<void(FMOWorldGenTuning&)> Modify; };
	const FCandidate Candidates[] = {
		{ TEXT("shipped"), [](FMOWorldGenTuning&) {} },
		{ TEXT("meander 150 m"), [](FMOWorldGenTuning& T) { T.TunnelWavelengthCm = 15000.0f; } },
		{ TEXT("meander 150 m, mask -0.45"), [](FMOWorldGenTuning& T) { T.TunnelWavelengthCm = 15000.0f; T.GalleryMaskThreshold = -0.45f; } },
		{ TEXT("meander 110 m, mask -0.45"), [](FMOWorldGenTuning& T) { T.TunnelWavelengthCm = 11000.0f; T.GalleryMaskThreshold = -0.45f; } },
		{ TEXT("meander 150 m, mask -0.45, levels 22 m"), [](FMOWorldGenTuning& T) { T.TunnelWavelengthCm = 15000.0f; T.GalleryMaskThreshold = -0.45f;
			T.GalleryLevelSpacingCm = 2200.0f; T.GalleryLevelAmplitudeCm = 900.0f; } },
		// A dense, multi-level karst (Mammoth Cave's ~14 km of passage per km2 is ~70 m between passages on one level).
		{ TEXT("dense karst: meander 70 m, levels 15 m"), [](FMOWorldGenTuning& T) { T.TunnelWavelengthCm = 7000.0f; T.GalleryMaskThreshold = -0.45f;
			T.GalleryLevelSpacingCm = 1500.0f; T.GalleryLevelAmplitudeCm = 600.0f; } },
		// The rules Wes set (Oct 16-17), relaxed one at a time, to show what each costs / buys:
		{ TEXT("mouths from 10 deg (trenches)"), [](FMOWorldGenTuning& T) { T.CaveEntranceSlopeMinDeg = 10.0f; T.CaveEntranceSlopeFullDeg = 16.0f; } },
		{ TEXT("any rock (not only limestone)"), [](FMOWorldGenTuning& T) { T.bCavesOnlyInLimestone = false; } },
		{ TEXT("dense karst + any rock"), [](FMOWorldGenTuning& T) { T.TunnelWavelengthCm = 7000.0f; T.GalleryMaskThreshold = -0.45f;
			T.GalleryLevelSpacingCm = 1500.0f; T.GalleryLevelAmplitudeCm = 600.0f; T.bCavesOnlyInLimestone = false; } },
		{ TEXT("dense karst + mouths from 10 deg"), [](FMOWorldGenTuning& T) { T.TunnelWavelengthCm = 7000.0f; T.GalleryMaskThreshold = -0.45f;
			T.GalleryLevelSpacingCm = 1500.0f; T.GalleryLevelAmplitudeCm = 600.0f; T.CaveEntranceSlopeMinDeg = 10.0f; T.CaveEntranceSlopeFullDeg = 16.0f; } },
		// F30 A: built entrances (a doline hollow with a passage down into a chamber), Wes's rules kept.
		{ TEXT("built entrances: 500 m cells, 70 %"), [](FMOWorldGenTuning& T) { T.CaveEntranceCellCm = 50000.0f; T.CaveEntranceChance = 0.7f; } },
		{ TEXT("built entrances: 400 m cells, 100 %"), [](FMOWorldGenTuning& T) { T.CaveEntranceCellCm = 40000.0f; T.CaveEntranceChance = 1.0f; } },
		{ TEXT("built entrances 500 m 70 % + any rock"), [](FMOWorldGenTuning& T) { T.CaveEntranceCellCm = 50000.0f; T.CaveEntranceChance = 0.7f;
			T.bCavesOnlyInLimestone = false; } },
	};

	// The tiles (and their ground) once per world.
	struct FWorld { int32 Seed; TArray<FTile> Tiles; };
	TArray<FWorld> Worlds;
	for (const int32 WorldSeed : WorldSeeds)
	{
		FWorld& W = Worlds.AddDefaulted_GetRef();
		W.Seed = UMOWorldSeedSubsystem::TerrainSeedFromWorldSeed(WorldSeed);
		W.Tiles = PickTiles(*Base, W.Seed, TilesPerSeed);
		for (FTile& T : W.Tiles)
		{
			SampleGround(*Base, W.Seed, T);
		}
		int32 Lime = 0;
		for (const FTile& T : W.Tiles) { Lime += T.bLimestone ? 1 : 0; }
		AddInfo(FString::Printf(TEXT("world %d: %d tile(s) of cave country, %d on limestone"), WorldSeed, W.Tiles.Num(), Lime));
		// The funnel per tile, on a 20 m grid: the share of the tile that is >= 16 deg (where a mouth can open), on limestone, and with gallery
		// air 15 m under the ground (the passage network), and the share that has all three.
		const float Tan16 = FMath::Tan(FMath::DegreesToRadians(16.0f));
		for (const FTile& T : W.Tiles)
		{
			int32 Cells = 0, Steep = 0, LimeCells = 0, Air = 0, All = 0;
			for (int32 Iy = 2; Iy < N - 2; Iy += 5)
			{
				for (int32 Ix = 2; Ix < N - 2; Ix += 5)
				{
					const double X = T.X0 + (Ix + 0.5) * Step, Y = T.Y0 + (Iy + 0.5) * Step;
					const float Gx = (T.Ground[Iy * N + Ix + 2] - T.Ground[Iy * N + Ix - 2]) / (4.0f * Step);
					const float Gy = (T.Ground[(Iy + 2) * N + Ix] - T.Ground[(Iy - 2) * N + Ix]) / (4.0f * Step);
					const bool bSteep = FMath::Sqrt(Gx * Gx + Gy * Gy) >= Tan16;
					const bool bLime = FMOWorldGen::SampleBedrock(*Base, X, Y, W.Seed) == EMOBedrock::Limestone;
					const float G = T.Ground[Iy * N + Ix];
					const bool bAir = FMOWorldGen::SampleCaveDistanceAtGround(*Base, X, Y, G - 1500.0f, G, W.Seed, 50.0f) < 0.0f
						|| FMOWorldGen::SampleCaveDistanceAtGround(*Base, X, Y, G - 3000.0f, G, W.Seed, 50.0f) < 0.0f;
					++Cells; Steep += bSteep; LimeCells += bLime; Air += bAir; All += (bSteep && bLime && bAir);
				}
			}
			AddInfo(FString::Printf(TEXT("   tile (%.0f, %.0f) km: ground %.0f m; >=16 deg %.0f%%, limestone %.0f%%, gallery air 15/30 m down %.1f%%, all three %.2f%%"),
				T.X0 / 100000.0, T.Y0 / 100000.0, T.Ground[(N / 2) * N + N / 2] / 100.0, 100.0 * Steep / Cells, 100.0 * LimeCells / Cells, 100.0 * Air / Cells, 100.0 * All / Cells));
		}
	}

	double ShippedOccupancy = 0.0;
	for (const FCandidate& C : Candidates)
	{
		const FMOWorldGenParamsRef Q = Variant(Base, C.Modify);
		int32 Tiles = 0, WithMouth = 0, Mouths = 0, LimeTiles = 0, LimeWithMouth = 0;
		FString PerWorld;
		for (const FWorld& W : Worlds)
		{
			int32 WorldWith = 0;
			for (const FTile& T : W.Tiles)
			{
				const int32 M = CountMouths(*Q, W.Seed, T);
				++Tiles;
				Mouths += M;
				WithMouth += M > 0 ? 1 : 0;
				WorldWith += M > 0 ? 1 : 0;
				LimeTiles += T.bLimestone ? 1 : 0;
				LimeWithMouth += (T.bLimestone && M > 0) ? 1 : 0;
			}
			PerWorld += FString::Printf(TEXT(" %d/%d"), WorldWith, W.Tiles.Num());
		}
		const double Occupancy = Tiles > 0 ? 100.0 * WithMouth / Tiles : 0.0;
		AddInfo(FString::Printf(TEXT("%-40s: %5.1f%% of 1 km tiles hold a mouth (per world:%s), %.1f mouths per km2; limestone tiles %d/%d"),
			C.Name, Occupancy, *PerWorld, Tiles > 0 ? (double)Mouths / Tiles : 0.0, LimeWithMouth, LimeTiles));
		if (FCString::Strcmp(C.Name, TEXT("shipped")) == 0)
		{
			ShippedOccupancy = Occupancy;
		}
	}
	// N-170's bar: with caves only in limestone (Wes, v11) the bedrock caps this; the shipped tuning is judged on its own numbers above.
	// Measurement mode until N-170's tuning lands (then the bar: a walk-in mouth in at least a third of 1 km tiles of cave country).
	AddInfo(FString::Printf(TEXT("N-170 bar (not asserted yet): a walk-in mouth in >= 33%% of 1 km tiles of cave country; shipped %.1f%%"), ShippedOccupancy));
	return true;
}


// F30 A, built entrances (on in shipped worlds since generator v12, Wes Oct 19 2026; every cell on here). With them on: the hollow is open, a passage and chamber lie 8-16 m under it, the rock
// 1-3 m under the ground stays whole outside the hollow (no hole in flat ground), and the field has no cliff (the other cave tests' gradient limit).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_CaveEntranceBuilt,
	"MOFramework.WorldGen.CaveEntranceBuilt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_CaveEntranceBuilt::RunTest(const FString& Parameters)
{
	using namespace MOCaveReachTest;
	const FMOWorldGenParamsRef Shipped = UMOWorldGenSettings::BuildParams();
	const FMOWorldGenParamsRef P = Variant(Shipped, [](FMOWorldGenTuning& T) { T.CaveEntranceCellCm = 50000.0f; T.CaveEntranceChance = 1.0f; });
	const FMOWorldGenParamsRef Off = Variant(Shipped, [](FMOWorldGenTuning& T) { T.CaveEntranceChance = 0.0f; });   // the field without the feature
	const int32 Seed = UMOWorldSeedSubsystem::TerrainSeedFromWorldSeed(670711536);
	// On by default since generator v12 (F30 A, Wes Oct 19 2026: "turn on built cave entrances in limestone").
	TestEqual(TEXT("on by default: 70 % of the 500 m cells"), Shipped->Tuning.CaveEntranceChance, 0.7f);
	TestNotEqual(TEXT("on by default: the feature is part of the shipped params hash"), Shipped->GetHash(),
		Variant(Shipped, [](FMOWorldGenTuning& T) { T.CaveEntranceCellCm = 12345678.0f; })->GetHash());

	// Find entrances: scan 50 x 50 cells (25 km) at each cell's 9 candidate points for a negative hollow distance, then walk to the hollow's centre.
	int32 Found = 0, Checked = 0;
	for (int32 Cy = -25; Cy < 25 && Checked < 6; ++Cy)
	{
		for (int32 Cx = -25; Cx < 25 && Checked < 6; ++Cx)
		{
			double Bx = 0.0, By = 0.0;
			float Best = FMOWorldGen::NoSinkholeDistanceCm;
			for (int32 K = 0; K < 81; ++K)
			{
				const double X = (Cx + 0.1 + 0.1 * (K % 9)) * 50000.0, Y = (Cy + 0.1 + 0.1 * (K / 9)) * 50000.0;
				const float D = FMOWorldGen::DistanceToCaveEntranceHollow(*P, X, Y, Seed);
				if (D < Best) { Best = D; Bx = X; By = Y; }
			}
			if (Best >= FMOWorldGen::NoSinkholeDistanceCm * 0.5f)
			{
				continue;
			}
			++Found;
			// The hollow's centre: its distance is a cone (|p - c| - 1.2 R), so the minimum is the centre. A 2 m grid over 120 m, then 10 cm over +-2 m
			// (a gradient descent overshot the cone's tip by metres and read the hollow itself as "outside").
			for (const TPair<double, int32> Pass : { TPair<double, int32>(200.0, 30), TPair<double, int32>(10.0, 20) })
			{
				const double Cx0 = Bx, Cy0 = By;
				float Min = FMOWorldGen::DistanceToCaveEntranceHollow(*P, Bx, By, Seed);
				for (int32 Iy = -Pass.Value; Iy <= Pass.Value; ++Iy)
				{
					for (int32 Ix = -Pass.Value; Ix <= Pass.Value; ++Ix)
					{
						const double X = Cx0 + Ix * Pass.Key, Y = Cy0 + Iy * Pass.Key;
						const float D = FMOWorldGen::DistanceToCaveEntranceHollow(*P, X, Y, Seed);
						if (D < Min) { Min = D; Bx = X; By = Y; }
					}
				}
			}
			const float HollowR = -FMOWorldGen::DistanceToCaveEntranceHollow(*P, Bx, By, Seed) / 1.2f;
			const float Ground = FMOWorldGen::SampleHeightCm(*P, Bx, By, Seed, 50.0f);
			++Checked;

			// 1. The hollow is open: air a metre under the ground at its middle.
			TestTrue(FString::Printf(TEXT("entrance at (%.0f, %.0f): the hollow is open"), Bx, By),
				FMOWorldGen::SampleCaveDistanceAtGround(*P, Bx, By, Ground - 100.0f, Ground, Seed, 50.0f) < 0.0f);

			// 2. A passage / chamber 8-16 m below the hollow's floor within 45 m.
			bool bPassage = false;
			for (int32 R = 600; R <= 4500 && !bPassage; R += 300)
			{
				for (int32 A = 0; A < 32 && !bPassage; ++A)
				{
					const double X = Bx + R * FMath::Cos(A * PI / 16.0), Y = By + R * FMath::Sin(A * PI / 16.0);
					const float G = FMOWorldGen::SampleHeightCm(*P, X, Y, Seed, 50.0f);
					for (float Below = 800.0f; Below <= 1800.0f && !bPassage; Below += 100.0f)
					{
						bPassage = FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Ground - Below, G, Seed, 50.0f) < 0.0f;
					}
				}
			}
			TestTrue(FString::Printf(TEXT("entrance at (%.0f, %.0f): a passage leads down out of the hollow"), Bx, By), bPassage);

			// 3. No hole in flat ground: outside the hollow (1.3 x its radius on) the top 1-3 m stay rock out to the chamber's reach.
			int32 Holes = 0;
			for (int32 R = (int32)(HollowR * 1.3f); R <= 6000; R += 200)
			{
				for (int32 A = 0; A < 64; ++A)
				{
					const double X = Bx + R * FMath::Cos(A * PI / 32.0), Y = By + R * FMath::Sin(A * PI / 32.0);
					const float G = FMOWorldGen::SampleHeightCm(*P, X, Y, Seed, 50.0f);
					Holes += FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, G - 200.0f, G, Seed, 50.0f) < 0.0f
						&& FMOWorldGen::SampleCaveDistanceAtGround(*Off, X, Y, G - 200.0f, G, Seed, 50.0f) >= 0.0f ? 1 : 0;   // only openings the feature added
				}
			}
			TestEqual(FString::Printf(TEXT("entrance at (%.0f, %.0f): no opening in the ground outside the hollow"), Bx, By), Holes, 0);

			// 4. No cliff in the field: |gradient| (1 m differences on a 2 m lattice through the hollow and down the passage) under the cave tests' limit (40).
			float MaxGrad = 0.0f;
			for (int32 Ix = -30; Ix <= 30; Ix += 2)
			{
				for (int32 Iy = -30; Iy <= 30; Iy += 2)
				{
					const double X = Bx + Ix * 100.0, Y = By + Iy * 100.0;
					const float G = FMOWorldGen::SampleHeightCm(*P, X, Y, Seed, 50.0f);
					for (float Below = 50.0f; Below <= 1800.0f; Below += 250.0f)
					{
						const float Z = G - Below;
						const float D0 = FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Z, G, Seed, 50.0f);
						const float Dz = FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Z + 100.0f, G, Seed, 50.0f) - D0;
						const float Dx = FMOWorldGen::SampleCaveDistanceAtGround(*P, X + 100.0, Y, Z, FMOWorldGen::SampleHeightCm(*P, X + 100.0, Y, Seed, 50.0f), Seed, 50.0f) - D0;
						MaxGrad = FMath::Max3(MaxGrad, FMath::Abs(Dz) / 100.0f, FMath::Abs(Dx) / 100.0f);   // cm per cm, as MOFramework.WorldGen.Caves
					}
				}
			}
			AddInfo(FString::Printf(TEXT("entrance at (%.0f, %.0f): hollow radius %.0f cm, ground %.0f m, max |gradient| %.1f"), Bx, By, HollowR, Ground / 100.0f, MaxGrad));
			TestTrue(FString::Printf(TEXT("entrance at (%.0f, %.0f): the field has no cliff (max |gradient| %.2f < 40)"), Bx, By, MaxGrad), MaxGrad < 40.0f);
		}
	}
	AddInfo(FString::Printf(TEXT("%d cell(s) with an entrance in 2,500 (chance 1: the rest failed the dry / karst / slope / height gates); %d checked"), Found, Checked));
	TestTrue(TEXT("some entrances exist with the feature on"), Found > 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
