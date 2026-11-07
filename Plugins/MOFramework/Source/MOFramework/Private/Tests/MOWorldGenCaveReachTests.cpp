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
	// Every cell a shallow one (the mix has its own test, CaveEntranceMix).
	const FMOWorldGenParamsRef P = Variant(Shipped, [](FMOWorldGenTuning& T) { T.CaveEntranceCellCm = 50000.0f; T.CaveEntranceChance = 1.0f; T.CaveOverhangShare = 0.0f; T.CaveDeepShare = 0.0f; });
	const FMOWorldGenParamsRef Off = Variant(Shipped, [](FMOWorldGenTuning& T) { T.CaveEntranceChance = 0.0f; });   // the field without the feature
	const int32 Seed = UMOWorldSeedSubsystem::TerrainSeedFromWorldSeed(670711536);
	// On by default since generator v12 (F30 A, Wes Oct 19 2026: "turn on built cave entrances in limestone").
	TestEqual(TEXT("on by default: 90 % of the 400 m cells (v15)"), Shipped->Tuning.CaveEntranceChance, 0.9f);
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
					if (FMOWorldGen::DistanceToCaveEntranceHollow(*P, X, Y, Seed) <= 0.0f)
					{
						continue;   // one of its bowls (N-355: they chain), which are openings it means to have
					}
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

// BR23-18 (Wes, Oct 23: "a mix: ~2/4 small overhangs (room for a fire, partial shelter), 1/4 deeper branching caves, 1/4 shallow caves as now"). What each kind must be, read back from the cave field itself:
// an overhang has a level floor with standing room, roofed by rock you cannot see sky through, and you walk into it from the bowl; a deep cave's branches are air you can stand in, under rock, off a chamber.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_CaveEntranceMix,
	"MOFramework.WorldGen.CaveEntranceMix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_CaveEntranceMix::RunTest(const FString& Parameters)
{
	using namespace MOCaveReachTest;
	const FMOWorldGenParamsRef P = UMOWorldGenSettings::BuildParams();
	const FMOWorldGenParamsRef Off = Variant(P, [](FMOWorldGenTuning& T) { T.CaveEntranceChance = 0.0f; });
	const int32 Seed = UMOWorldSeedSubsystem::TerrainSeedFromWorldSeed(670711536);
	const float Voxel = P->Tuning.VoxelSizeCm;
	const auto Field = [&P, Seed, Voxel](double X, double Y, double Z) { return FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Z, FMOWorldGen::SampleHeightCm(*P, X, Y, Seed, Voxel), Seed, Voxel); };

	int32 Counts[3] = {};
	int32 OverhangsChecked = 0, DeepsChecked = 0, RoomsTotal = 0, PassagesTotal = 0;
	float DeepestUnderHollow = 0.0f, LongestDeep = 0.0f, RoofThinnest = 1.0e6f;
	for (int64 Cy = -60; Cy < 60; ++Cy)
	{
		for (int64 Cx = -60; Cx < 60; ++Cx)
		{
			FMOCaveEntranceHollow E;
			if (!FMOWorldGen::GetCaveEntranceInCell(*P, Cx, Cy, Seed, E))
			{
				continue;
			}
			++Counts[(int32)E.Kind];
			const FString At = FString::Printf(TEXT("(%.0f, %.0f)"), E.Center.X, E.Center.Y);
			const FVector2D Across(-E.Uphill.Y, E.Uphill.X);

			// No kind opens the ground beyond its hollow (1.3 x the radius on; a hollow's uphill cut is measured by OpeningKeepOut), and the field keeps the cave tests' gradient limit round it.
			const bool bCheck = (E.Kind == EMOCaveEntranceKind::Overhang && OverhangsChecked < 8) || (E.Kind == EMOCaveEntranceKind::Deep && DeepsChecked < 6);
			if (bCheck)
			{
				int32 Holes = 0;
				for (int32 R = (int32)(E.RadiusCm * 1.5f); R <= 10000; R += 250)
				{
					for (int32 A = 0; A < 48; ++A)
					{
						const double X = E.Center.X + R * FMath::Cos(A * PI / 24.0), Y = E.Center.Y + R * FMath::Sin(A * PI / 24.0);
						// One of its bowls (N-355: they chain), which are openings it means to have, with the uphill cut this check has always allowed the main one: 1.5 x the radius, at the outline's widest.
						bool bInABowl = false;
						for (const FMOCaveEntranceHollow::FBowl& Bowl : E.Bowls)
						{
							bInABowl |= FVector2D::Distance(FVector2D(X, Y), Bowl.Center) < Bowl.RadiusCm * 1.5f * 1.22f;
						}
						for (const FMOCaveEntranceHollow::FShelter& Shelter : E.Shelters)
						{
							bInABowl |= FVector2D::Distance(FVector2D(X, Y), Shelter.Middle) < FMath::Max(Shelter.HalfDepthCm, Shelter.HalfWidthCm) * 1.3f;
						}
						if (bInABowl)
						{
							continue;
						}
						const float G = FMOWorldGen::SampleHeightCm(*P, X, Y, Seed, Voxel);
						Holes += FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, G - 60.0f, G, Seed, Voxel) < 0.0f && FMOWorldGen::SampleCaveDistanceAtGround(*Off, X, Y, G - 60.0f, G, Seed, Voxel) >= 0.0f ? 1 : 0;
					}
				}
				TestEqual(FString::Printf(TEXT("%s: no opening in the ground beyond its hollow"), *At), Holes, 0);
				float MaxGrad = 0.0f;
				for (int32 Ix = -40; Ix <= 40; Ix += 3)
				{
					for (int32 Iy = -40; Iy <= 40; Iy += 3)
					{
						const double X = E.Center.X + Ix * 100.0, Y = E.Center.Y + Iy * 100.0;
						const float G = FMOWorldGen::SampleHeightCm(*P, X, Y, Seed, Voxel);
						for (float Below = 50.0f; Below <= 3000.0f; Below += 350.0f)
						{
							const float Z = G - Below;
							const float D0 = FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Z, G, Seed, Voxel);
							const float Dz = FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Z + 100.0f, G, Seed, Voxel) - D0;
							const float Dx = Field(X + 100.0, Y, Z) - D0;
							MaxGrad = FMath::Max3(MaxGrad, FMath::Abs(Dz) / 100.0f, FMath::Abs(Dx) / 100.0f);
						}
					}
				}
				TestTrue(FString::Printf(TEXT("%s: the field has no cliff (max |gradient| %.2f < 40)"), *At, MaxGrad), MaxGrad < 40.0f);
			}

			if (E.Kind == EMOCaveEntranceKind::Overhang && OverhangsChecked < 8)
			{
				++OverhangsChecked;
				const float Floor = E.GroundCm - E.DepthCm;
				// A level floor with standing room, 3 x 3 m round the shelter's middle: rock 60 cm under the floor, air 180 cm over it.
				int32 Standing = 0, Level = 0, Spots = 0;
				for (int32 Ia = -1; Ia <= 1; ++Ia)
				{
					for (int32 Ib = -1; Ib <= 1; ++Ib)
					{
						const FVector2D Spot = E.ShelterMiddle + E.Uphill * (Ia * 150.0) + Across * (Ib * 150.0);
						++Spots;
						Standing += Field(Spot.X, Spot.Y, Floor + 180.0f) < 0.0f ? 1 : 0;
						Level += Field(Spot.X, Spot.Y, Floor - 60.0f) > 0.0f && Field(Spot.X, Spot.Y, Floor + 60.0f) < 0.0f ? 1 : 0;
					}
				}
				TestEqual(FString::Printf(TEXT("%s: standing room over 3 x 3 m of the shelter"), *At), Standing, Spots);
				TestEqual(FString::Printf(TEXT("%s: the shelter's floor is level, at the hollow's floor"), *At), Level, Spots);
				// Roofed: 2.5 m further in, going up from the floor there is air to crouch in, then rock up to the hillside's own ground: the hill, between the shelter's ceiling and the sky.
				const FVector2D Back = E.ShelterMiddle + E.Uphill * 250.0;
				const float BackGround = FMOWorldGen::SampleHeightCm(*P, Back.X, Back.Y, Seed, Voxel);
				TestTrue(FString::Printf(TEXT("%s: air at the back of the shelter"), *At), Field(Back.X, Back.Y, Floor + 100.0f) < 0.0f);
				float RoofCm = 0.0f;
				for (float Z = Floor + 100.0f; Z <= BackGround; Z += 20.0f)
				{
					RoofCm += Field(Back.X, Back.Y, Z) >= 0.0f ? 20.0f : 0.0f;
				}
				RoofThinnest = FMath::Min(RoofThinnest, RoofCm);
				TestTrue(FString::Printf(TEXT("%s: the roof over the back of the shelter is rock (%.0f cm)"), *At, RoofCm), RoofCm >= 60.0f);
				// You walk in level from the hillside: every metre from the mouth to the middle has air at knee height over the floor, whether cut or the open air over the slope below it.
				int32 Blocked = 0;
				const double Way = FVector2D::Distance(E.Center, E.ShelterMiddle);
				for (double S = 100.0; S <= Way; S += 100.0)
				{
					const FVector2D WayPoint = E.Center + E.Uphill * S;
					const float WayGround = FMOWorldGen::SampleHeightCm(*P, WayPoint.X, WayPoint.Y, Seed, Voxel);
					Blocked += Floor + 60.0f > WayGround || Field(WayPoint.X, WayPoint.Y, Floor + 60.0f) < 0.0f ? 0 : 1;
				}
				TestEqual(FString::Printf(TEXT("%s: the way in from the hillside is clear"), *At), Blocked, 0);
			}

			if (E.Kind == EMOCaveEntranceKind::Deep)
			{
				PassagesTotal += E.Passages.Num();
				RoomsTotal += E.NumRooms;
				if (DeepsChecked < 6)
				{
					++DeepsChecked;
					TestTrue(FString::Printf(TEXT("%s: a deep cave branches"), *At), E.Passages.Num() >= 1);
					float Length = 0.0f;
					int32 Samples = 0, Air = 0, Covered = 0;
					for (const TPair<FVector, FVector>& Pass : E.Passages)
					{
						Length += (float)FVector::Distance(Pass.Key, Pass.Value);
						for (int32 K = 0; K <= 10; ++K)
						{
							const FVector On = FMath::Lerp(Pass.Key, Pass.Value, K / 10.0);
							const float G = FMOWorldGen::SampleHeightCm(*P, On.X, On.Y, Seed, Voxel);
							++Samples;
							Air += Field(On.X, On.Y, On.Z) < 0.0f ? 1 : 0;
							Covered += G - On.Z >= 300.0f ? 1 : 0;
							DeepestUnderHollow = FMath::Max(DeepestUnderHollow, E.GroundCm - (float)On.Z);
						}
					}
					LongestDeep = FMath::Max(LongestDeep, Length);
					// The field's own gates (a limestone contact, water near) may close a stretch; most of it must be there.
					TestTrue(FString::Printf(TEXT("%s: its branches are air (%d of %d samples)"), *At, Air, Samples), Air * 10 >= Samples * 8);
					TestEqual(FString::Printf(TEXT("%s: its branches are under rock"), *At), Covered, Samples);
				}
			}
		}
	}
	const int32 Total = Counts[0] + Counts[1] + Counts[2];
	AddInfo(FString::Printf(TEXT("%d built entrance(s) in 48 x 48 km: %d overhangs (thinnest roof checked %.0f cm), %d shallow, %d deep; deep: %d passages, %d rooms, at most %.0f m of branches, %.0f m under the hollow's rim"),
		Total, Counts[0], RoofThinnest, Counts[1], Counts[2], PassagesTotal, RoomsTotal, LongestDeep / 100.0f, DeepestUnderHollow / 100.0f));
	TestTrue(TEXT("enough entrances to measure the mix (>= 40)"), Total >= 40);
	// The caves' half of Wes's mix is the hash's: as many deep as shallow. Overhangs go by the land now (a cell of flat country holds none, N-357), so only that some stand.
	TestTrue(FString::Printf(TEXT("overhangs stand (%d)"), Counts[0]), Counts[0] > 0);
	TestTrue(FString::Printf(TEXT("about as many deep as shallow (%d, %d)"), Counts[2], Counts[1]), Counts[2] * 100 >= Counts[1] * 60 && Counts[1] * 100 >= Counts[2] * 60);
	TestTrue(TEXT("overhangs were checked"), OverhangsChecked > 0);
	TestTrue(TEXT("deep caves were checked"), DeepsChecked > 0);

	// The same cell gives the same cave whatever was generated before it on this thread (the settled entrance lives in a per-thread cache whose slots are reused: a passage once kept the room of the cell
	// before it, and the same world counted 2,322 rooms in one run and 2,316 in the next). Backwards through the cells, so every slot has held something else: a co-op machine meets them in its own order.
	TMap<FIntPoint, TPair<int32, int32>> FirstPass;
	for (int64 Cy = -60; Cy < 60; ++Cy)
	{
		for (int64 Cx = -60; Cx < 60; ++Cx)
		{
			FMOCaveEntranceHollow E;
			if (FMOWorldGen::GetCaveEntranceInCell(*P, Cx, Cy, Seed, E))
			{
				FirstPass.Add(FIntPoint((int32)Cx, (int32)Cy), TPair<int32, int32>(E.Passages.Num(), E.NumRooms));
			}
		}
	}
	int32 Differ = 0;
	for (int64 Cy = 59; Cy >= -60; --Cy)
	{
		for (int64 Cx = 59; Cx >= -60; --Cx)
		{
			FMOCaveEntranceHollow E;
			if (FMOWorldGen::GetCaveEntranceInCell(*P, Cx, Cy, Seed, E))
			{
				const TPair<int32, int32>* Was = FirstPass.Find(FIntPoint((int32)Cx, (int32)Cy));
				Differ += !Was || Was->Key != E.Passages.Num() || Was->Value != E.NumRooms ? 1 : 0;
			}
		}
	}
	TestEqual(TEXT("each cell's cave is the same whatever order the cells are asked in"), Differ, 0);

	// What it costs: one 34^3 chunk of the cave field at 100 cm round a cave's chamber, by kind (the number is printed, not gated: it depends on the machine).
	for (const EMOCaveEntranceKind Kind : { EMOCaveEntranceKind::Shallow, EMOCaveEntranceKind::Deep, EMOCaveEntranceKind::Overhang })
	{
		bool bDone = false;
		for (int64 Cy = -60; Cy < 60 && !bDone; ++Cy)
		{
			for (int64 Cx = -60; Cx < 60 && !bDone; ++Cx)
			{
				FMOCaveEntranceHollow E;
				if (!FMOWorldGen::GetCaveEntranceInCell(*P, Cx, Cy, Seed, E) || E.Kind != Kind)
				{
					continue;
				}
				bDone = true;
				const FVector At = Kind == EMOCaveEntranceKind::Overhang ? FVector(E.ShelterMiddle.X, E.ShelterMiddle.Y, E.GroundCm - E.DepthCm) : E.MainChamber;
				const double Started = FPlatformTime::Seconds();
				float Sum = 0.0f;
				for (int32 Iy = -17; Iy < 17; ++Iy)
				{
					for (int32 Ix = -17; Ix < 17; ++Ix)
					{
						const double X = At.X + Ix * 100.0, Y = At.Y + Iy * 100.0;
						const float G = FMOWorldGen::SampleHeightCm(*P, X, Y, Seed, Voxel);
						for (int32 Iz = -17; Iz < 17; ++Iz)
						{
							Sum += FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, At.Z + Iz * 100.0, G, Seed, 100.0f);
						}
					}
				}
				AddInfo(FString::Printf(TEXT("kind %d: a 34^3 chunk of the field round its chamber %.1f ms (sum %.0f)"), (int32)Kind, (FPlatformTime::Seconds() - Started) * 1000.0, Sum));
			}
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
