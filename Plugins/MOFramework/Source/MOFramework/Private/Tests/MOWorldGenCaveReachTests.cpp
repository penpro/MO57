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

#endif // WITH_DEV_AUTOMATION_TESTS
