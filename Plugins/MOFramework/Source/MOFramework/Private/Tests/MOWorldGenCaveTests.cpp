// Tests for the cave half of the world generator (tunnels, caverns, sinkholes) and the MO Terrain Caves voxel node. Headless-safe: no world, no PIE.
// MOFramework.WorldGen.DumpCaveMaps writes PNG slices of the carved terrain to Saved/WorldGen/ so caves can be LOOKED at, not just asserted.

#include "Misc/AutomationTest.h"
#include "MOWorldGen.h"
#include "MOWorldGenSettings.h"
#include "MOWorldSeedSubsystem.h"
#include "MOTerrainFunctionLibrary.h"
#include "VoxelFunctionLibrary.h"
#include "VoxelNode.h"
#include "Nodes/VoxelNode_UFunction.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Async/ParallelFor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 CaveTestSeed = 424242;

	/**
	 * Random trials on every core, with the serial loop's results (Oct 19 2026: three of these tests took 18 of the suite's 22 minutes on one thread).
	 * Draw(I) takes block slot I's random numbers from the stream IN THE SERIAL ORDER; Eval(I) runs in parallel (the generator is pure and worker-thread
	 * safe) and writes slot I's own result; Take(I) consumes the results in order and returns false to stop. A search that stops early evaluates at
	 * most the rest of its block past the stop; nothing it takes differs.
	 */
	constexpr int32 TrialBlock = 4096;
	template <typename TDraw, typename TEval, typename TTake>
	void RunTrials(int32 Count, TDraw&& Draw, TEval&& Eval, TTake&& Take)
	{
		for (int32 Base = 0; Base < Count; Base += TrialBlock)
		{
			const int32 N = FMath::Min(TrialBlock, Count - Base);
			for (int32 I = 0; I < N; ++I)
			{
				Draw(I);
			}
			ParallelFor(N, [&Eval](int32 I) { Eval(I); });
			for (int32 I = 0; I < N; ++I)
			{
				if (!Take(I))
				{
					return;
				}
			}
		}
	}

	/** Default tuning, no biomes: the cave field needs only heights. */
	FMOWorldGenParamsRef MakeCaveParams(TFunctionRef<void(FMOWorldGenTuning&)> Modify = [](FMOWorldGenTuning&) {})
	{
		TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> P = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
		Modify(P->Tuning);
		P->Finalize();
		return FMOWorldGenParamsRef(P);
	}

	uint32 FnvFloat(uint32 H, float V)
	{
		const uint8* B = reinterpret_cast<const uint8*>(&V);
		for (int32 I = 0; I < 4; ++I) { H = (H ^ B[I]) * 16777619u; }
		return H;
	}

	bool SavePng(const FString& Path, const TArray<FColor>& Pixels, int32 Width, int32 Height)
	{
		IImageWrapperModule& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
		TSharedPtr<IImageWrapper> Wrapper = Module.CreateImageWrapper(EImageFormat::PNG);
		if (!Wrapper.IsValid() || !Wrapper->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
		{
			return false;
		}
		return FFileHelper::SaveArrayToFile(Wrapper->GetCompressed(), *Path);
	}

	/** The cell grid window the sinkhole tests scan: 16 x 16 cells around the origin. */
	void ForEachSinkholeCell(const FMOWorldGenParams& P, int32 Seed, TFunctionRef<void(const FMOSinkhole&)> Fn)
	{
		for (int64 Cy = -8; Cy < 8; ++Cy)
		{
			for (int64 Cx = -8; Cx < 8; ++Cx)
			{
				FMOSinkhole S;
				if (FMOWorldGen::GetSinkholeInCell(P, Cx, Cy, Seed, S))
				{
					Fn(S);
				}
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_Caves,
	"MOFramework.WorldGen.Caves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_Caves::RunTest(const FString& Parameters)
{
	// The cave FIELD's own properties (purity, cover, air share, gradient, LOD, sinkholes) are measured with the karst rule off: since v11 caves exist only in
	// limestone (41% of the land), which MOFramework.WorldGen.CavesStayDry checks; measured over every rock these numbers would only be diluted.
	const FMOWorldGenParamsRef P = MakeCaveParams([](FMOWorldGenTuning& Tuning) { Tuning.bCavesOnlyInLimestone = false; });
	const FMOWorldGenTuning& T = P->Tuning;
	FRandomStream R(5);
	auto RandomColumn = [&R](double& X, double& Y) { X = R.FRandRange(-1.5e6f, 1.5e6f); Y = R.FRandRange(-1.5e6f, 1.5e6f); };

	// 1. Pure: the same inputs give the same bits.
	{
		bool bIdentical = true;
		for (int32 I = 0; I < 2000; ++I)
		{
			double X, Y;
			RandomColumn(X, Y);
			const double Z = R.FRandRange(-6000.0f, 4000.0f);
			bIdentical &= FMOWorldGen::SampleCaveDistanceCm(*P, X, Y, Z, CaveTestSeed) == FMOWorldGen::SampleCaveDistanceCm(*P, X, Y, Z, CaveTestSeed);
		}
		TestTrue(TEXT("the same (params, seed, position) is bit-identical every call"), bIdentical);
	}

	// 2. Air above the ground needs no carving; 3. rock under FLAT ground stays intact down to the cover (v9: a gallery opens only into a steep slope,
	//    an entrance -- never a hole in flat ground to fall into; Wes, Oct 16 2026).
	{
		int32 AboveWrong = 0, CoverWrong = 0, CoverChecked = 0, SteepColumns = 0, SteepOpen = 0, NearEntrance = 0;
		const float FlatTan = FMath::Tan(FMath::DegreesToRadians(T.CaveEntranceSlopeMinDeg - 2.0f));
		struct FTrial { double X = 0.0, Y = 0.0; float Above = 0.0f, Depth = 0.0f; bool bAboveWrong = false, bNearEntrance = false, bFlat = false, bOpen = false; };
		TArray<FTrial> Trials;
		Trials.SetNum(TrialBlock);
		RunTrials(6000,
			[&](int32 I) { FTrial& Tr = Trials[I]; Tr = FTrial(); RandomColumn(Tr.X, Tr.Y); Tr.Above = R.FRandRange(150.0f, 1500.0f); Tr.Depth = R.FRandRange(0.0f, T.CaveCoverCm - 50.0f); },
			[&](int32 I)
			{
				FTrial& Tr = Trials[I];
				const double X = Tr.X, Y = Tr.Y;
				const float Ground = FMOWorldGen::SampleHeightCm(*P, X, Y, CaveTestSeed);
				Tr.bAboveWrong = FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Ground + Tr.Above, Ground, CaveTestSeed) != FMOWorldGen::CaveFarCm;
				// The same slope the field uses: forward differences over 3 m, both ways (a column on a slope's edge counts as steep if either side is).
				const float Gx = (FMOWorldGen::SampleHeightCm(*P, X + 300.0, Y, CaveTestSeed) - Ground) / 300.0f;
				const float Gy = (FMOWorldGen::SampleHeightCm(*P, X, Y + 300.0, CaveTestSeed) - Ground) / 300.0f;
				const float Gx2 = (Ground - FMOWorldGen::SampleHeightCm(*P, X - 300.0, Y, CaveTestSeed)) / 300.0f;
				const float Gy2 = (Ground - FMOWorldGen::SampleHeightCm(*P, X, Y - 300.0, CaveTestSeed)) / 300.0f;
				Tr.bOpen = FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Ground - Tr.Depth, Ground, CaveTestSeed) <= 0.0f;
				// A built entrance (v12, F30 A) is a designed opening on gentle ground: its hollow is walked into and its passage runs a few metres under
				// the ground nearby. Its own test (WorldGen.CaveEntranceBuilt) holds the top 1-3 m whole out to 60 m; this one judges the rest of the field.
				Tr.bNearEntrance = FMOWorldGen::DistanceToCaveEntranceHollow(*P, X, Y, CaveTestSeed) < 6000.0f;
				Tr.bFlat = FMath::Max(FMath::Sqrt(Gx * Gx + Gy * Gy), FMath::Sqrt(Gx2 * Gx2 + Gy2 * Gy2)) < FlatTan;
			},
			[&](int32 I)
			{
				const FTrial& Tr = Trials[I];
				AboveWrong += Tr.bAboveWrong ? 1 : 0;
				if (Tr.bNearEntrance)
				{
					++NearEntrance;
				}
				else if (Tr.bFlat)
				{
					++CoverChecked;
					CoverWrong += Tr.bOpen ? 1 : 0;
				}
				else
				{
					++SteepColumns;
					SteepOpen += Tr.bOpen ? 1 : 0;
				}
				return true;
			});
		AddInfo(FString::Printf(TEXT("steep columns: %d, of which open inside the cover (a gallery entrance): %d; %d column(s) within 60 m of a built entrance left to its own test"), SteepColumns, SteepOpen, NearEntrance));
		TestEqual(TEXT("no cave above the ground (6000 columns)"), AboveWrong, 0);
		TestEqual(*FString::Printf(TEXT("under ground flatter than %.0f deg no gallery / cavern breaks the %.0f cm cover (%d columns): no hole to fall into"),
			T.CaveEntranceSlopeMinDeg - 2.0f, T.CaveCoverCm, CoverChecked), CoverWrong, 0);
	}

	// 4. The rock really is carved, but not swiss cheese.
	{
		int32 Inside = 0;
		const int32 N = 60000;
		struct FTrial { double X = 0.0, Y = 0.0; float Depth = 0.0f; bool bInside = false; };
		TArray<FTrial> Trials;
		Trials.SetNum(TrialBlock);
		RunTrials(N,
			[&](int32 I) { FTrial& Tr = Trials[I]; Tr = FTrial(); RandomColumn(Tr.X, Tr.Y); Tr.Depth = R.FRandRange(1000.0f, 12000.0f); },
			[&](int32 I)
			{
				FTrial& Tr = Trials[I];
				const float Ground = FMOWorldGen::SampleHeightCm(*P, Tr.X, Tr.Y, CaveTestSeed);
				Tr.bInside = FMOWorldGen::SampleCaveDistanceAtGround(*P, Tr.X, Tr.Y, Ground - Tr.Depth, Ground, CaveTestSeed) < 0.0f;
			},
			[&](int32 I) { Inside += Trials[I].bInside ? 1 : 0; return true; });
		const float Fraction = (float)Inside / N;
		AddInfo(FString::Printf(TEXT("cave air is %.2f%% of the rock between 10 and 120 m deep"), Fraction * 100.0f));
		TestTrue(FString::Printf(TEXT("between 0.2%% and 12%% of deep rock is cave air (got %.2f%%)"), Fraction * 100.0f), Fraction > 0.002f && Fraction < 0.12f);
	}

	// 5. Roughly a unit-gradient distance: bounded everywhere, ~1 at the wall of a cave (marching cubes interpolates the surface position on it).
	{
		float MaxGradient = 0.0f;
		FString WorstText;
		double GradientSum = 0.0;
		int32 WallCount = 0;
		struct FTrial { double X = 0.0, Y = 0.0, Z = 0.0; float Depth = 0.0f, Ground = 0.0f, D0 = 0.0f, Gx = 0.0f, Gy = 0.0f, Gz = 0.0f, Magnitude = 0.0f; };
		TArray<FTrial> Trials;
		Trials.SetNum(TrialBlock);
		RunTrials(200000,   // v9's galleries are half the radius: more samples to find their walls
			[&](int32 I) { FTrial& Tr = Trials[I]; Tr = FTrial(); RandomColumn(Tr.X, Tr.Y); Tr.Depth = R.FRandRange(1500.0f, 12000.0f); },
			[&](int32 I)
			{
				FTrial& Tr = Trials[I];
				const double X = Tr.X, Y = Tr.Y;
				Tr.Ground = FMOWorldGen::SampleHeightCm(*P, X, Y, CaveTestSeed);
				const double Z = Tr.Ground - Tr.Depth;
				Tr.Z = Z;
				Tr.D0 = FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Z, Tr.Ground, CaveTestSeed);
				// Forward differences along the three axes (the column's ground is reused: the gate depends on Z only through Depth, which Z + h changes by h).
				Tr.Gx = (FMOWorldGen::SampleCaveDistanceCm(*P, X + 25.0, Y, Z, CaveTestSeed) - Tr.D0) / 25.0f;
				Tr.Gy = (FMOWorldGen::SampleCaveDistanceCm(*P, X, Y + 25.0, Z, CaveTestSeed) - Tr.D0) / 25.0f;
				Tr.Gz = (FMOWorldGen::SampleCaveDistanceCm(*P, X, Y, Z + 25.0, CaveTestSeed) - Tr.D0) / 25.0f;
				Tr.Magnitude = FMath::Sqrt(Tr.Gx * Tr.Gx + Tr.Gy * Tr.Gy + Tr.Gz * Tr.Gz);
			},
			[&](int32 I)
			{
				const FTrial& Tr = Trials[I];
				if (Tr.Magnitude > MaxGradient)
				{
					MaxGradient = Tr.Magnitude;
					WorstText = FString::Printf(TEXT("worst: at (%.0f, %.0f, %.0f) depth %.0f cm, d=%.1f, gradient (%.1f, %.1f, %.1f)"), Tr.X, Tr.Y, Tr.Z, Tr.Ground - Tr.Z, Tr.D0, Tr.Gx, Tr.Gy, Tr.Gz);
				}
				// At a wall (|d| < 60 cm), away from the shallow / deep taper zones where the penalty ramps steepen the field.
				if (FMath::Abs(Tr.D0) < 60.0f && Tr.Ground - Tr.Z > T.CavernMinDepthCm + T.CaveFadeCm && Tr.Ground - Tr.Z < T.CaveMaxDepthCm - 500.0f)
				{
					GradientSum += Tr.Magnitude;
					++WallCount;
				}
				return true;
			});
		const float MeanGradient = WallCount > 0 ? (float)(GradientSum / WallCount) : 0.0f;
		AddInfo(FString::Printf(TEXT("cave field |gradient|: mean %.2f at cave walls (%d samples), max %.2f anywhere; %s"), MeanGradient, WallCount, MaxGradient, *WorstText));
		TestTrue(TEXT("there are samples at cave walls to measure"), WallCount > 100);
		TestTrue(FString::Printf(TEXT("mean |gradient| at cave walls is ~1 (0.6..1.5, got %.2f)"), MeanGradient), MeanGradient > 0.6f && MeanGradient < 1.5f);
		// The taper ramps legitimately reach ~10 per cm of DEPTH change (a 15 000 cm penalty over a 1 600 cm ramp), and a horizontal step changes the depth by the ground's slope: on the v5 mountains
		// (slopes to ~75 degrees = 3.7 cm per cm) that is ~35 at the very worst. A gate that closed with a jump instead of saturating would show 100+ here (the first version of this field had one: 146).
		TestTrue(FString::Printf(TEXT("no cliff in the field (every gate saturates smoothly): max |gradient| %.2f < 40"), MaxGradient), MaxGradient < 40.0f);
	}

	// 6. LOD: features smaller than the sample spacing are skipped, so far chunks see solid ground. CONTROL: the same samples at LOD0 do hit caves.
	{
		int32 FineInside = 0, CoarseInside = 0;
		struct FTrial { double X = 0.0, Y = 0.0; float Depth = 0.0f; bool bFine = false, bCoarse = false; };
		TArray<FTrial> Trials;
		Trials.SetNum(TrialBlock);
		RunTrials(30000,
			[&](int32 I) { FTrial& Tr = Trials[I]; Tr = FTrial(); RandomColumn(Tr.X, Tr.Y); Tr.Depth = R.FRandRange(1200.0f, 12000.0f); },
			[&](int32 I)
			{
				FTrial& Tr = Trials[I];
				const float Ground = FMOWorldGen::SampleHeightCm(*P, Tr.X, Tr.Y, CaveTestSeed);
				const double Z = Ground - Tr.Depth;
				Tr.bFine = FMOWorldGen::SampleCaveDistanceAtGround(*P, Tr.X, Tr.Y, Z, Ground, CaveTestSeed, 50.0f) < 0.0f;
				Tr.bCoarse = FMOWorldGen::SampleCaveDistanceAtGround(*P, Tr.X, Tr.Y, Z, Ground, CaveTestSeed, 3200.0f) < 0.0f;
			},
			[&](int32 I) { FineInside += Trials[I].bFine ? 1 : 0; CoarseInside += Trials[I].bCoarse ? 1 : 0; return true; });
		TestTrue(FString::Printf(TEXT("CONTROL: LOD0 samples hit caves (%d)"), FineInside), FineInside > 0);
		TestEqual(TEXT("a 32 m sample spacing sees no caves"), CoarseInside, 0);
	}

	// 7. Sinkholes: off by default since v9 (a shaft in flat ground is a hole to fall into); still a working feature for a world that turns them on --
	//    found, open to the sky at the rim, a shaft down the middle, keep-out zone around them.
	{
		int32 DefaultFound = 0;
		ForEachSinkholeCell(*P, CaveTestSeed, [&DefaultFound](const FMOSinkhole&) { ++DefaultFound; });
		TestEqual(TEXT("the default world has no sinkholes (v9)"), DefaultFound, 0);
	}
	{
		const FMOWorldGenParamsRef Sink = MakeCaveParams([](FMOWorldGenTuning& Tuning) { Tuning.SinkholeChance = 0.6f; Tuning.bCavesOnlyInLimestone = false; });
		const FMOWorldGenTuning& ST = Sink->Tuning;
		int32 Found = 0, RimOpen = 0, ShaftOpen = 0, ZoneOk = 0, FarClear = 0;
		ForEachSinkholeCell(*Sink, CaveTestSeed, [&](const FMOSinkhole& S)
		{
			++Found;
			const double Cx = S.Center.X, Cy = S.Center.Y;
			ShaftOpen += (FMOWorldGen::SampleCaveDistanceCm(*Sink, Cx, Cy, S.GroundCm - 500.0f, CaveTestSeed) < 0.0f) ? 1 : 0;
			// Half a radius from the axis, 40 cm below the ground THERE: inside the cut, i.e. the shaft reaches the surface. (The ground at the offset, not at the axis: on a slope -- the v6 ranges
			// have gentle ones everywhere -- the two differ by radius x slope, and a point 40 cm under the AXIS ground is in the air on the downhill side.)
			const float LocalGroundCm = FMOWorldGen::SampleHeightCm(*Sink, Cx + S.RadiusCm * 0.5, Cy, CaveTestSeed);
			RimOpen += (FMOWorldGen::SampleCaveDistanceCm(*Sink, Cx + S.RadiusCm * 0.5, Cy, LocalGroundCm - 40.0f, CaveTestSeed) < 0.0f) ? 1 : 0;
			ZoneOk += (FMOWorldGen::DistanceToSinkholeRimZone(*Sink, Cx, Cy, CaveTestSeed) < 0.0f) ? 1 : 0;
			// Well away from the sinkhole (but the same cell) the shaft is not there: 3 radii out, just under the ground, no cave.
			FarClear += (FMOWorldGen::SampleCaveDistanceCm(*Sink, Cx + S.RadiusCm * 4.0, Cy, S.GroundCm - 400.0f, CaveTestSeed) > 0.0f) ? 1 : 0;
		});
		AddInfo(FString::Printf(TEXT("%d sinkholes in 16 x 16 cells (%.0f m cells)"), Found, ST.SinkholeCellCm / 100.0f));
		// A sinkhole is evaluated only in its own cell, so its reach (2.4x the largest radius + the saturation distance) must fit the 18% jitter margin.
		TestTrue(TEXT("a sinkhole's reach fits inside its cell's jitter margin"),
			ST.SinkholeRadiusCm * 1.3f * 2.4f + FMOWorldGen::CaveFarCm + 300.0f < 0.18f * ST.SinkholeCellCm);
		TestTrue(FString::Printf(TEXT("a plausible number of sinkholes in 256 cells at chance %.2f (got %d)"), ST.SinkholeChance, Found), Found > 80 && Found < 220);
		TestEqual(TEXT("the shaft is open 5 m below the ground at every sinkhole"), ShaftOpen, Found);
		TestEqual(TEXT("the shaft reaches the surface at every sinkhole"), RimOpen, Found);
		TestEqual(TEXT("DistanceToSinkholeRimZone is negative at every sinkhole centre"), ZoneOk, Found);
		TestEqual(TEXT("4 radii from the axis the ground is intact at every sinkhole"), FarClear, Found);
	}

	// 8. The switch really switches it off.
	{
		const FMOWorldGenParamsRef Off = MakeCaveParams([](FMOWorldGenTuning& Tuning) { Tuning.bEnableCaves = false; });
		int32 Inside = 0;
		for (int32 I = 0; I < 5000; ++I)
		{
			double X, Y;
			RandomColumn(X, Y);
			const float Ground = FMOWorldGen::SampleHeightCm(*Off, X, Y, CaveTestSeed);
			Inside += (FMOWorldGen::SampleCaveDistanceAtGround(*Off, X, Y, Ground - R.FRandRange(0.0f, 12000.0f), Ground, CaveTestSeed) < FMOWorldGen::CaveFarCm) ? 1 : 0;
		}
		TestEqual(TEXT("bEnableCaves = false: no cave anywhere"), Inside, 0);
		TestNotEqual(TEXT("the cave settings are part of the params hash (host/client handshake)"), Off->GetHash(), P->GetHash());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_GalleriesStayLevel,
	"MOFramework.WorldGen.GalleriesStayLevel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMOWorldGen_GalleriesStayLevel::RunTest(const FString& Parameters)
{
	// Wes, Oct 16 2026: "caves should be more or less horizontal so we don't just fall into them". From points well inside galleries (22-33 m deep:
	// past where they fade in, above the caverns' 35 m), follow the passage 3 m sideways -- the horizontal direction where the field is deepest, at
	// the best height within +-1.5 m. The height change along it is the passage's slope: 95% must stay under 15 deg.
	// Passage geometry, measured with the karst rule off (v11 keeps caves to limestone; the random search for gallery air would only get slower).
	const FMOWorldGenParamsRef P = MakeCaveParams([](FMOWorldGenTuning& Tuning) { Tuning.bCavesOnlyInLimestone = false; });
	FRandomStream R(97);
	int32 Measured = 0, Level = 0;
	float Worst = 0.0f;
	const float MaxTan = FMath::Tan(FMath::DegreesToRadians(15.0f));
	struct FTrial { double X = 0.0, Y = 0.0; float Depth = 0.0f; bool bMeasured = false; float Tan = 0.0f; };
	TArray<FTrial> Trials;
	Trials.SetNum(TrialBlock);
	RunTrials(2000000,
		[&](int32 I) { FTrial& Tr = Trials[I]; Tr = FTrial(); Tr.X = R.FRandRange(-2.0e6f, 2.0e6f); Tr.Y = R.FRandRange(-2.0e6f, 2.0e6f); Tr.Depth = R.FRandRange(2200.0f, 3300.0f); },
		[&](int32 I)
		{
			FTrial& Tr = Trials[I];
			const double X = Tr.X, Y = Tr.Y;
			const float Ground = FMOWorldGen::SampleHeightCm(*P, X, Y, CaveTestSeed);
			const double Z = Ground - Tr.Depth;
			if (FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Z, Ground, CaveTestSeed, 50.0f) > -60.0f)
			{
				return;
			}
			// The passage's centre at this column first (the deepest point within +-1.5 m): starting from a point near the tube's roof or floor would
			// count the offset to the next column's centre as slope.
			double Z0 = Z;
			float CentreD = FMOWorldGen::CaveFarCm;
			for (float Dz = -150.0f; Dz <= 150.0f; Dz += 10.0f)
			{
				const float D = FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Z + Dz, Ground, CaveTestSeed, 50.0f);
				if (D < CentreD)
				{
					CentreD = D;
					Z0 = Z + Dz;
				}
			}
			float BestD = FMOWorldGen::CaveFarCm, BestDz = 0.0f;
			for (int32 K = 0; K < 24; ++K)
			{
				const double A = K * 2.0 * UE_DOUBLE_PI / 24.0;
				const double Qx = X + 300.0 * FMath::Cos(A), Qy = Y + 300.0 * FMath::Sin(A);
				const float QGround = FMOWorldGen::SampleHeightCm(*P, Qx, Qy, CaveTestSeed);
				for (float Dz = -150.0f; Dz <= 150.0f; Dz += 10.0f)
				{
					const float D = FMOWorldGen::SampleCaveDistanceAtGround(*P, Qx, Qy, Z0 + Dz, QGround, CaveTestSeed, 50.0f);
					if (D < BestD)
					{
						BestD = D;
						BestDz = Dz;
					}
				}
			}
			if (BestD >= 0.0f)
			{
				return;   // a gallery's tapered end: nowhere to follow it
			}
			Tr.bMeasured = true;
			Tr.Tan = FMath::Abs(BestDz) / 300.0f;
		},
		[&](int32 I)
		{
			const FTrial& Tr = Trials[I];
			if (Tr.bMeasured)
			{
				++Measured;
				Level += Tr.Tan <= MaxTan ? 1 : 0;
				Worst = FMath::Max(Worst, Tr.Tan);
			}
			return Measured < 250;
		});
	AddInfo(FString::Printf(TEXT("%d passages followed, %d within 15 deg, steepest %.0f deg"), Measured, Level, FMath::RadiansToDegrees(FMath::Atan(Worst))));
	TestTrue(TEXT("enough gallery air found to measure (>= 100 passages)"), Measured >= 100);
	TestTrue(FString::Printf(TEXT("95%% of passages run within 15 deg of level (%d of %d)"), Level, Measured), Measured > 0 && Level >= FMath::CeilToInt(0.95f * Measured));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_CaveEntrances,
	"MOFramework.WorldGen.CaveEntrances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_CaveEntrances::RunTest(const FString& Parameters)
{
	// Caves you can walk into: a gallery opens where it runs into a hillside. The first v9 test asked only that one exists -- it found 3 open columns
	// in 20 x 20 km at 25 m, and a fresh world had no mouth within 2 km of an inland spot (Oct 17 2026): the caves were unreachable. Measured here:
	// the hilliest 2 x 2 km of a 20 x 20 km region, scanned at 4 m (a mouth is a few metres wide), mouths = open columns grouped by 30 m cell.
	const FMOWorldGenParamsRef P = MakeCaveParams();
	const float Voxel = P->Tuning.VoxelSizeCm;
	auto Ground = [&P, Voxel](double X, double Y) { return FMOWorldGen::SampleHeightCm(*P, X, Y, CaveTestSeed, Voxel); };
	// The generator's slope: 12 m central differences (GroundGradient in MOWorldGenCaves.cpp).
	auto SlopeTan = [&Ground](double X, double Y)
	{
		const float Gx = (Ground(X + 600.0, Y) - Ground(X - 600.0, Y)) / 1200.0f, Gy = (Ground(X, Y + 600.0) - Ground(X, Y - 600.0)) / 1200.0f;
		return FMath::Sqrt(Gx * Gx + Gy * Gy);
	};

	// The hilliest 2 km tile of cave country (most columns over 14 deg on ground where caves exist: above the coast, below the high-ground taper --
	// the mountain ranges have none by design) of the region, at 50 m.
	const double Tile = 200000.0;
	const float HillTan = FMath::Tan(FMath::DegreesToRadians(14.0f));
	const float CaveLow = P->Tuning.SeaLevelCm + P->Tuning.CaveMinGroundAboveSeaCm + 600.0f, CaveHigh = P->Tuning.CaveMaxGroundCm - P->Tuning.CaveHighFadeCm;
	TMap<FIntPoint, int32> SteepPerTile;
	for (double X = -1.0e6; X < 1.0e6; X += 5000.0)
	{
		for (double Y = -1.0e6; Y < 1.0e6; Y += 5000.0)
		{
			const float Z = Ground(X, Y);
			if (Z > CaveLow && Z < CaveHigh && SlopeTan(X, Y) >= HillTan && FMOWorldGen::SampleBedrock(*P, X, Y, CaveTestSeed) == EMOBedrock::Limestone)
			{
				++SteepPerTile.FindOrAdd(FIntPoint(FMath::FloorToInt32(X / Tile), FMath::FloorToInt32(Y / Tile)));
			}
		}
	}
	FIntPoint Best(0, 0);
	int32 BestCount = -1;
	for (const TPair<FIntPoint, int32>& It : SteepPerTile)
	{
		if (It.Value > BestCount) { BestCount = It.Value; Best = It.Key; }
	}
	const double X0 = Best.X * Tile, Y0 = Best.Y * Tile;
	const double Step = 400.0;
	const int32 N = FMath::RoundToInt32(Tile / Step);

	// Ground and slope once (the cave tuning does not move the ground), then each candidate entrance band.
	TArray<float> G, S;
	G.SetNumUninitialized(N * N);
	S.SetNumUninitialized(N * N);
	int32 Hilly = 0;
	for (int32 Iy = 0; Iy < N; ++Iy)
	{
		for (int32 Ix = 0; Ix < N; ++Ix)
		{
			const double X = X0 + (Ix + 0.5) * Step, Y = Y0 + (Iy + 0.5) * Step;
			G[Iy * N + Ix] = Ground(X, Y);
			S[Iy * N + Ix] = SlopeTan(X, Y);
			Hilly += S[Iy * N + Ix] >= HillTan && G[Iy * N + Ix] > CaveLow && G[Iy * N + Ix] < CaveHigh && FMOWorldGen::SampleBedrock(*P, X, Y, CaveTestSeed) == EMOBedrock::Limestone ? 1 : 0;
		}
	}
	const double TileKm2 = (Tile / 100000.0) * (Tile / 100000.0);
	AddInfo(FString::Printf(TEXT("hilliest 2 km tile of karst cave country at (%.0f, %.0f): %.0f%% of it over 14 deg, limestone, in the cave band"), X0, Y0, 100.0 * Hilly / (N * N)));

	auto CountMouths = [&](const FMOWorldGenParams& Q, int32& OutOpen)
	{
		const float MinTan = FMath::Tan(FMath::DegreesToRadians(Q.Tuning.CaveEntranceSlopeMinDeg));
		TSet<FIntPoint> Mouths;
		OutOpen = 0;
		for (int32 Iy = 0; Iy < N; ++Iy)
		{
			for (int32 Ix = 0; Ix < N; ++Ix)
			{
				if (S[Iy * N + Ix] < MinTan)
				{
					continue;
				}
				const double X = X0 + (Ix + 0.5) * Step, Y = Y0 + (Iy + 0.5) * Step;
				bool bOpen = false;
				for (float Depth = 50.0f; Depth <= 250.0f && !bOpen; Depth += 50.0f)
				{
					bOpen = FMOWorldGen::SampleCaveDistanceAtGround(Q, X, Y, G[Iy * N + Ix] - Depth, G[Iy * N + Ix], CaveTestSeed, 50.0f) < 0.0f;
				}
				if (bOpen)
				{
					++OutOpen;
					Mouths.Add(FIntPoint(FMath::FloorToInt32(X / 3000.0), FMath::FloorToInt32(Y / 3000.0)));
				}
			}
		}
		return Mouths.Num();
	};

	// The candidates (information: what the band does to the density), then the default -- the assertion.
	const FVector2f Bands[] = { FVector2f(16.0f, 32.0f), FVector2f(14.0f, 24.0f), FVector2f(12.0f, 20.0f), FVector2f(10.0f, 16.0f) };
	for (const FVector2f& Band : Bands)
	{
		const FMOWorldGenParamsRef Q = MakeCaveParams([&Band](FMOWorldGenTuning& T) { T.CaveEntranceSlopeMinDeg = Band.X; T.CaveEntranceSlopeFullDeg = Band.Y; });
		int32 Open = 0;
		const int32 Mouths = CountMouths(*Q, Open);
		AddInfo(FString::Printf(TEXT("  band %.0f-%.0f deg: %d mouth(s) (%.1f per km2), %d open column(s)"), Band.X, Band.Y, Mouths, Mouths / TileKm2, Open));
	}
	int32 Open = 0;
	const int32 Mouths = CountMouths(*P, Open);
	const double PerKm2 = Mouths / TileKm2;
	// v11: caves only in limestone, so the hilliest KARST tile has less hillside than the hilliest tile did (18% vs 38%): judge the mouths per km2 of hillside.
	const double HillKm2 = TileKm2 * Hilly / (double)(N * N);
	const double PerHillKm2 = HillKm2 > 0.0 ? Mouths / HillKm2 : 0.0;
	AddInfo(FString::Printf(TEXT("default band %.0f-%.0f deg: %d mouth(s) in the hilliest 2 km tile (%.1f per km2), %d open column(s)"),
		P->Tuning.CaveEntranceSlopeMinDeg, P->Tuning.CaveEntranceSlopeFullDeg, Mouths, PerKm2, Open));
	AddInfo(FString::Printf(TEXT("%.2f km2 of karst hillside (>= 14 deg): %.1f mouths per km2 of it"), HillKm2, PerHillKm2));
	TestTrue(FString::Printf(TEXT("karst hillsides have caves to walk into: >= 8 mouths per km2 of hillside in the hilliest karst tile (got %.1f)"), PerHillKm2), PerHillKm2 >= 8.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_CavesStayDry,
	"MOFramework.WorldGen.CavesStayDry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_CavesStayDry::RunTest(const FString& Parameters)
{
	// Wes, Oct 17 2026: caves must not reach sea level (the ocean mesh fills every point under it: water and the underwater post process inside the cave),
	// and none right by a shore -- "they'd immediately be below water table". Random cave air anywhere in the cave band of a 40 x 40 km region: every point
	// above the water floor, and no water (ground at sea level: the sea, a river, a basin -- or an upland lake) within 100 m of it in 16 directions.
	const FMOWorldGenParamsRef P = MakeCaveParams();
	const FMOWorldGenTuning& T = P->Tuning;
	const float Voxel = T.VoxelSizeCm;
	FRandomStream R(1017);
	int32 Found = 0, BelowFloor = 0, NearWater = 0, NotKarst = 0;
	float LowestZ = 1.0e9f;
	FString FirstWet;
	const float Floor = T.SeaLevelCm + T.CaveWaterClearanceCm;
	struct FTrial { double X = 0.0, Y = 0.0, Z = 0.0; float Depth = 0.0f; bool bFound = false, bNotKarst = false; EMOWaterBodyKind WetKind = EMOWaterBodyKind::None; double WetDist = 0.0; };
	TArray<FTrial> Trials;
	Trials.SetNum(TrialBlock);
	RunTrials(3000000,
		[&](int32 I) { FTrial& Tr = Trials[I]; Tr = FTrial(); Tr.X = R.FRandRange(-2.0e6f, 2.0e6f); Tr.Y = R.FRandRange(-2.0e6f, 2.0e6f); Tr.Depth = R.FRandRange(150.0f, T.CaveMaxDepthCm); },
		[&](int32 I)
		{
			FTrial& Tr = Trials[I];
			const double X = Tr.X, Y = Tr.Y;
			const float Ground = FMOWorldGen::SampleHeightCm(*P, X, Y, CaveTestSeed, Voxel);
			const double Z = Ground - Tr.Depth;
			if (FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Z, Ground, CaveTestSeed, 50.0f) >= 0.0f)
			{
				return;
			}
			Tr.bFound = true;
			Tr.Z = Z;
			Tr.bNotKarst = FMOWorldGen::SampleBedrock(*P, X, Y, CaveTestSeed) != EMOBedrock::Limestone;
			for (int32 K = 0; K < 16 && Tr.WetKind == EMOWaterBodyKind::None; ++K)
			{
				const double A = K * UE_DOUBLE_PI / 8.0;
				for (double Dist = 2500.0; Dist <= 10000.0 && Tr.WetKind == EMOWaterBodyKind::None; Dist += 2500.0)
				{
					const double Qx = X + Dist * FMath::Cos(A), Qy = Y + Dist * FMath::Sin(A);
					Tr.WetKind = FMOWorldGen::ClassifyWaterBody(*P, Qx, Qy, CaveTestSeed);   // the generator's own "is there water here"
					Tr.WetDist = Dist;
				}
			}
		},
		[&](int32 I)
		{
			const FTrial& Tr = Trials[I];
			if (Tr.bFound)
			{
				++Found;
				NotKarst += Tr.bNotKarst ? 1 : 0;
				LowestZ = FMath::Min(LowestZ, (float)Tr.Z);
				BelowFloor += Tr.Z < Floor ? 1 : 0;
				const bool bWet = Tr.WetKind != EMOWaterBodyKind::None;
				if (bWet && FirstWet.IsEmpty())
				{
					FirstWet = FString::Printf(TEXT("cave air at (%.0f, %.0f, %.0f) has water (%s) %.0f m away"), Tr.X, Tr.Y, Tr.Z, *UEnum::GetValueAsString(Tr.WetKind), Tr.WetDist / 100.0);
				}
				NearWater += bWet ? 1 : 0;
			}
			return Found < 300;
		});
	AddInfo(FString::Printf(TEXT("%d cave-air samples: lowest at z = %.0f cm (floor %.0f), %d below the floor, %d with water within 100 m%s%s"), Found, LowestZ, Floor,
		BelowFloor, NearWater, FirstWet.IsEmpty() ? TEXT("") : TEXT("; first: "), *FirstWet));
	TestTrue(TEXT("enough cave air found to judge (>= 200 samples)"), Found >= 200);
	TestEqual(TEXT("no cave air below sea level + CaveWaterClearanceCm"), BelowFloor, 0);
	TestEqual(TEXT("no cave air within 100 m of water"), NearWater, 0);
	// v11 (Wes, Oct 17 2026: "caves only in limestone"): the gate reads the bedrock on a 16 m lattice, so a sample right at a contact may stand a few metres past it.
	AddInfo(FString::Printf(TEXT("cave samples outside limestone: %d of %d"), NotKarst, Found));
	TestTrue(TEXT("caves are in limestone (>= 99% of cave-air samples)"), Found > 0 && NotKarst <= Found / 100);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_NoCavesOnHighGround,
	"MOFramework.WorldGen.NoCavesOnHighGround",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_NoCavesOnHighGround::RunTest(const FString& Parameters)
{
	// Wes, 2026-10-13: caves under the mountain ranges left thin rock the far voxel LODs cannot represent, so the summits tore at a distance. Under ground at or above CaveMaxGroundCm there is no cave at
	// any depth (tunnel, cavern or sinkhole); the sinkhole mouth would breach the ground itself. The same sampler still carves below the taper band (the other cave tests).
	const FMOWorldGenParamsRef P = MakeCaveParams();
	const FMOWorldGenTuning& T = P->Tuning;
	FRandomStream R(11);
	int32 HighColumns = 0, CavesFound = 0, CavesFoundBelow = 0, BelowChecked = 0;
	struct FHighSample { double X, Y; float Z, Ground; };
	TArray<FHighSample> HighSamples;   // replayed by the negative control below
	for (int32 I = 0; I < 120000 && HighColumns < 400; ++I)
	{
		const double X = R.FRandRange(-1.5e6f, 1.5e6f), Y = R.FRandRange(-1.5e6f, 1.5e6f);
		const float Ground = FMOWorldGen::SampleHeightCm(*P, X, Y, CaveTestSeed);
		if (Ground >= T.CaveMaxGroundCm)
		{
			++HighColumns;
			for (int32 K = 0; K < 12; ++K)
			{
				const float Depth = R.FRandRange(-100.0f, T.CaveMaxDepthCm);
				HighSamples.Add({ X, Y, Ground - Depth, Ground });
				CavesFound += (FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Ground - Depth, Ground, CaveTestSeed) < 0.0f) ? 1 : 0;
			}
		}
		else if (Ground < T.CaveMaxGroundCm - T.CaveHighFadeCm && BelowChecked < 4000)
		{
			++BelowChecked;
			CavesFoundBelow += (FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Ground - R.FRandRange(1000.0f, 12000.0f), Ground, CaveTestSeed) < 0.0f) ? 1 : 0;
		}
	}
	AddInfo(FString::Printf(TEXT("high columns checked: %d (ceiling %.0f cm), cave samples found there: %d; below the taper band: %d of %d samples are cave"), HighColumns, T.CaveMaxGroundCm, CavesFound, CavesFoundBelow, BelowChecked));
	TestTrue(TEXT("the sample found enough high ground to mean something"), HighColumns >= 50);
	TestEqual(TEXT("no cave under ground at or above the ceiling"), CavesFound, 0);
	TestTrue(TEXT("caves still exist below the taper band"), CavesFoundBelow > 0);

	// NEGATIVE CONTROL: the same high-ground samples with the ceiling switched off (CaveMaxGroundCm <= 0 disables the gate: HighGround is 0 in SampleCaveDistanceAtGround)
	// must find caves -- otherwise "no cave up there" above would pass for a reason other than the gate (Codex H07: this test was never negative-controlled).
	const FMOWorldGenParamsRef NoCeiling = MakeCaveParams([](FMOWorldGenTuning& Tuning) { Tuning.CaveMaxGroundCm = 0.0f; });
	int32 ControlCaves = 0;
	for (const FHighSample& S : HighSamples)
	{
		ControlCaves += (FMOWorldGen::SampleCaveDistanceAtGround(*NoCeiling, S.X, S.Y, S.Z, S.Ground, CaveTestSeed) < 0.0f) ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("control (ceiling off): %d of the same %d high-ground samples are cave"), ControlCaves, HighSamples.Num()));
	TestTrue(TEXT("CONTROL: with the ceiling off, caves appear under the same high ground"), ControlCaves > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_CaveMaxLOD,
	"MOFramework.WorldGen.CaveMaxLOD",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_CaveMaxLOD::RunTest(const FString& Parameters)
{
	// The cave stamp is limited to LODs 0..CaveMaxLOD (playtest 2026-10-14 #10: the whole-world cave stamp routed far mountain chunks through the plugin's volume
	// pass, which holed them). That is only free if the field past CaveMaxLOD carves nothing: assert it is +CaveFarCm at every sample in the cave band one LOD
	// coarser, and that caves still exist AT CaveMaxLOD (the range is not cut one LOD too early).
	const FMOWorldGenParamsRef P = MakeCaveParams();
	const FMOWorldGenTuning& T = P->Tuning;
	const int32 MaxLOD = FMOWorldGen::CaveMaxLOD(*P);
	const float StepAt = T.VoxelSizeCm * (float)(1 << MaxLOD);
	const float StepPast = T.VoxelSizeCm * (float)(1 << (MaxLOD + 1));
	AddInfo(FString::Printf(TEXT("CaveMaxLOD = %d (spacing %.0f cm; voxel %.0f cm)"), MaxLOD, StepAt, T.VoxelSizeCm));
	TestTrue(TEXT("the last LOD that carries caves is no coarser than the cavern limit"), StepAt <= FMOWorldGen::CavernMaxStepCm);
	TestTrue(TEXT("one LOD coarser is past every cave feature's limit"), StepPast > FMOWorldGen::CavernMaxStepCm);

	FRandomStream R(23);
	int32 Samples = 0, CarvedPast = 0, CarvedAt = 0;
	for (int32 I = 0; I < 30000; ++I)
	{
		const double X = R.FRandRange(-1.5e6f, 1.5e6f), Y = R.FRandRange(-1.5e6f, 1.5e6f);
		const float Ground = FMOWorldGen::SampleHeightCm(*P, X, Y, CaveTestSeed);
		const float Z = Ground - R.FRandRange(-100.0f, T.CaveMaxDepthCm + T.CaveFadeCm);
		++Samples;
		CarvedPast += FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Z, Ground, CaveTestSeed, StepPast) < FMOWorldGen::CaveFarCm ? 1 : 0;
		CarvedAt += FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Z, Ground, CaveTestSeed, StepAt) < 0.0f ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("%d samples in the cave band: %d below +CaveFarCm one LOD past, %d inside cave air at CaveMaxLOD"), Samples, CarvedPast, CarvedAt));
	TestEqual(TEXT("past CaveMaxLOD the cave field is +CaveFarCm everywhere (the stamp would carve nothing)"), CarvedPast, 0);
	TestTrue(TEXT("CONTROL: at CaveMaxLOD caves are still carved"), CarvedAt > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_CaveGoldenChecksum,
	"MOFramework.WorldGen.CaveGoldenChecksum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_CaveGoldenChecksum::RunTest(const FString& Parameters)
{
	// Same rules as WorldGen.GoldenChecksum: an intentional change to the cave math bumps CurrentGeneratorVersion and pastes the printed value here;
	// a mismatch on another machine with the same binary means the cave field is not deterministic across CPUs, which would desync co-op worlds.
	constexpr uint32 Expected = FMOWorldGen::PinnedProbeCave;   // pinned in MOWorldGen.h; on an intentional change: bump CurrentGeneratorVersion, rerun, paste the printed value there

	const FMOWorldGenProbe Probe = FMOWorldGen::RunProbe();
	const uint32 H = Probe.Cave;
	AddInfo(FString::Printf(TEXT("CAVE GOLDEN checksum = 0x%08X, params hash = 0x%08X"), H, Probe.ParamsHash));
	TestTrue(FString::Printf(TEXT("golden cave checksum is 0x%08X (got 0x%08X)"), Expected, H), H == Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_CaveNode,
	"MOFramework.WorldGen.CaveNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_CaveNode::RunTest(const FString& Parameters)
{
	const UFunction* Function = UMOTerrainFunctionLibrary::StaticClass()->FindFunctionByName(TEXT("MOTerrainCaves"));
	if (TestNotNull(TEXT("the MOTerrainCaves UFUNCTION is reflected"), Function))
	{
		TestTrue(TEXT("registered with the plugin (VOXEL_REGISTER_FUNCTION)"), FVoxelFunctionLibraryRegistry::FindFunction(*Function) != nullptr);
		const TSharedRef<FVoxelNode_UFunction> Node = FVoxelNode_UFunction::Make(const_cast<UFunction*>(Function));
		TSet<FName> Inputs, Outputs;
		bool bAllValid = true;
		for (const FVoxelPin& Pin : Node->GetPins())
		{
			bAllValid &= Pin.GetType().IsValid();
			(Pin.bIsInput ? Inputs : Outputs).Add(Pin.Name);
		}
		TestTrue(TEXT("every pin has a valid Voxel pin type"), bAllValid);
		TestTrue(TEXT("input pin WorldPosition"), Inputs.Contains(FName("WorldPosition")));
		TestTrue(TEXT("input pin Seed"), Inputs.Contains(FName("Seed")));
		TestTrue(TEXT("output pin Distance"), Outputs.Contains(FName("Distance")));
		TestEqual(TEXT("exactly 2 input pins"), Inputs.Num(), 2);
		TestEqual(TEXT("exactly 1 output pin"), Outputs.Num(), 1);
	}

	// Bounds: no inputs (evaluated at stamp initialisation, where no position exists -- see the terrain node's PITFALLS).
	const UFunction* BoundsFunction = UMOTerrainFunctionLibrary::StaticClass()->FindFunctionByName(TEXT("MOTerrainCavesBounds"));
	if (TestNotNull(TEXT("the MOTerrainCavesBounds UFUNCTION is reflected"), BoundsFunction))
	{
		TestTrue(TEXT("registered with the plugin"), FVoxelFunctionLibraryRegistry::FindFunction(*BoundsFunction) != nullptr);
		const TSharedRef<FVoxelNode_UFunction> Node = FVoxelNode_UFunction::Make(const_cast<UFunction*>(BoundsFunction));
		int32 NumIn = 0, NumOut = 0;
		for (const FVoxelPin& Pin : Node->GetPins()) { (Pin.bIsInput ? NumIn : NumOut)++; }
		TestEqual(TEXT("no input pins"), NumIn, 0);
		TestEqual(TEXT("one output pin (Bounds)"), NumOut, 1);
	}

	// The node body equals the generator at every position, including repeated XY columns (the node memoises ground heights) and positions outside the Z band.
	const FMOWorldGenParamsRef P = FMOWorldGenParamsProvider::Get();
	constexpr int32 Columns = 40, Layers = 12;
	const int32 Num = Columns * Layers + 5;
	FVoxelDoubleVectorBuffer Position;
	Position.X.Allocate(Num);
	Position.Y.Allocate(Num);
	Position.Z.Allocate(Num);
	FRandomStream R(321);
	int32 Index = 0;
	for (int32 C = 0; C < Columns; ++C)
	{
		const double X = R.FRandRange(-1.0e6f, 1.0e6f), Y = R.FRandRange(-1.0e6f, 1.0e6f);
		const float Ground = FMOWorldGen::SampleHeightCm(*P, X, Y, CaveTestSeed, P->Tuning.VoxelSizeCm);
		for (int32 L = 0; L < Layers; ++L, ++Index)
		{
			Position.X.Set(Index, X);
			Position.Y.Set(Index, Y);
			Position.Z.Set(Index, Ground - 300.0 - 1100.0 * L);
		}
	}
	for (int32 I = 0; I < 5; ++I, ++Index)   // out of band: far above and far below
	{
		Position.X.Set(Index, R.FRandRange(-1.0e6f, 1.0e6f));
		Position.Y.Set(Index, R.FRandRange(-1.0e6f, 1.0e6f));
		Position.Z.Set(Index, (I % 2 == 0) ? (double)P->CaveMaxZCm + 1000.0 : (double)P->CaveMinZCm - 1000.0);
	}
	FVoxelFloatBuffer Distance;
	UMOTerrainFunctionLibrary::ComputeCaves(Position, CaveTestSeed, 0, Distance);
	TestEqual(TEXT("one distance per position"), Distance.Num(), Num);
	int32 Wrong = 0, Carved = 0;
	for (int32 I = 0; I < Num; ++I)
	{
		const float Expected = FMOWorldGen::SampleCaveDistanceCm(*P, Position.X[I], Position.Y[I], Position.Z[I], CaveTestSeed, P->Tuning.VoxelSizeCm);
		Wrong += (Distance[I] == Expected) ? 0 : 1;
		Carved += (Distance[I] < 0.0f) ? 1 : 0;
	}
	TestEqual(TEXT("node output equals FMOWorldGen::SampleCaveDistanceCm at every position"), Wrong, 0);
	AddInfo(FString::Printf(TEXT("%d of %d node samples are inside cave air"), Carved, Num));

	FVoxelBox Bounds;
	UMOTerrainFunctionLibrary::ComputeCavesBounds(Bounds);
	TestEqual(TEXT("bounds min X is the world footprint"), Bounds.Min.X, -(double)P->Tuning.WorldHalfSizeCm);
	TestEqual(TEXT("bounds max Y is the world footprint"), Bounds.Max.Y, (double)P->Tuning.WorldHalfSizeCm);
	TestEqual(TEXT("bounds min Z is CaveMinZCm"), Bounds.Min.Z, (double)P->CaveMinZCm);
	TestEqual(TEXT("bounds max Z is CaveMaxZCm"), Bounds.Max.Z, (double)P->CaveMaxZCm);
	// v10: no cave air below the water floor, so the bounds start at the deeper of (minimum ground - max depth) and that floor -- never below the floor.
	const double Floor = (double)(P->Tuning.SeaLevelCm + P->Tuning.CaveWaterClearanceCm);
	TestTrue(TEXT("bounds cover every cave the water floor allows (min Z <= max(minimum ground - max depth, the floor))"),
		Bounds.Min.Z <= FMath::Max((double)P->MinHeightCm - P->Tuning.CaveMaxDepthCm, Floor));
	TestTrue(TEXT("bounds sample nothing below the water floor"), Bounds.Min.Z >= Floor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_CavePerformance,
	"MOFramework.WorldGen.CavePerformance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_CavePerformance::RunTest(const FString& Parameters)
{
	// What a chunk of the volume path pays: a 3D grid of samples, one memoised ground height per column (see UMOTerrainFunctionLibrary::ComputeCaves).
	const FMOWorldGenParamsRef P = MakeCaveParams();
	constexpr int32 Side = 34;   // a 32^3 chunk plus its border
	FVoxelDoubleVectorBuffer Position;
	const int32 Num = Side * Side * Side;
	Position.X.Allocate(Num);
	Position.Y.Allocate(Num);
	Position.Z.Allocate(Num);
	auto FillChunk = [&](double Ox, double Oy, double Oz)
	{
		int32 I = 0;
		for (int32 Z = 0; Z < Side; ++Z) { for (int32 Y = 0; Y < Side; ++Y) { for (int32 X = 0; X < Side; ++X, ++I)
		{
			Position.X.Set(I, Ox + X * 50.0); Position.Y.Set(I, Oy + Y * 50.0); Position.Z.Set(I, Oz + Z * 50.0);
		} } }
	};

	FVoxelFloatBuffer Distance;
	auto TimeChunkMs = [&](double Oz)
	{
		const double T0 = FPlatformTime::Seconds();
		constexpr int32 Reps = 6;
		for (int32 Rep = 0; Rep < Reps; ++Rep)
		{
			FillChunk(100000.0 + Rep * 2000.0, 50000.0, Oz);
			UMOTerrainFunctionLibrary::ComputeCaves(Position, CaveTestSeed, 0, Distance);
		}
		return (FPlatformTime::Seconds() - T0) * 1000.0 / Reps;
	};

	const float Ground = FMOWorldGen::SampleHeightCm(*P, 100000.0, 50000.0, CaveTestSeed);
	const double Underground = TimeChunkMs(Ground - 6000.0);   // inside the cave band: 3D noise per sample
	const double InTheAir = TimeChunkMs(Ground + 3000.0);      // a chunk of sky above the surface: ground height only
	AddInfo(FString::Printf(TEXT("cave chunk (34^3 samples): %.1f ms underground, %.1f ms in the air (single thread, Development editor build)"), Underground, InTheAir));
	TestTrue(FString::Printf(TEXT("an underground chunk costs under 250 ms on one thread (got %.1f)"), Underground), Underground < 250.0);

	// A voxel NAVIGATION chunk: 32 m at 50 cm = a 66^3 grid. Each AI pawn's navigation invoker asks for a few dozen of them, mostly solid ground or air (Oct 13 2026: ~0.5 s each
	// in the packaged game, 224 of them when the first survivor and deer spawn).
	{
		constexpr int32 NavSide = 66;
		FVoxelDoubleVectorBuffer NavPosition;
		const int32 NavNum = NavSide * NavSide * NavSide;
		NavPosition.X.Allocate(NavNum);
		NavPosition.Y.Allocate(NavNum);
		NavPosition.Z.Allocate(NavNum);
		auto TimeNavMs = [&](double Oz)
		{
			int32 I = 0;
			for (int32 Z = 0; Z < NavSide; ++Z) { for (int32 Y = 0; Y < NavSide; ++Y) { for (int32 X = 0; X < NavSide; ++X, ++I)
			{
				NavPosition.X.Set(I, 100000.0 + X * 50.0); NavPosition.Y.Set(I, 50000.0 + Y * 50.0); NavPosition.Z.Set(I, Oz + Z * 50.0);
			} } }
			const double T0 = FPlatformTime::Seconds();
			UMOTerrainFunctionLibrary::ComputeCaves(NavPosition, CaveTestSeed, 0, Distance);
			return (FPlatformTime::Seconds() - T0) * 1000.0;
		};
		const double NavUnder = TimeNavMs(Ground - 6400.0);
		const double NavAir = TimeNavMs(Ground + 3200.0);
		AddInfo(FString::Printf(TEXT("navigation chunk (66^3 samples): %.1f ms underground, %.1f ms in the air (single thread)"), NavUnder, NavAir));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_DumpCaveMaps,
	"MOFramework.WorldGen.DumpCaveMaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_DumpCaveMaps::RunTest(const FString& Parameters)
{
	// Visual QA: Saved/WorldGen/caves_*.png. Horizontal slices at fixed depths below the ground and a vertical cross-section, from the REAL snapshot.
	const FMOWorldGenParamsRef P = UMOWorldGenSettings::BuildParams();
	const int32 Seed = UMOWorldSeedSubsystem::TerrainSeedFromWorldSeed(12345);
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("WorldGen");
	IFileManager::Get().MakeDirectory(*Dir, true);

	const double CenterX = 0.0, CenterY = 0.0, Span = 400000.0;   // 4 km square
	const int32 Res = 800;
	const double Cell = Span / Res;

	// Horizontal slices: white = cave air at that depth under the local ground, grey = rock, blue = low ground where caves are not allowed; red = sinkhole centres.
	for (const float DepthM : { 12.0f, 30.0f, 60.0f, 100.0f })
	{
		TArray<FColor> Pixels;
		Pixels.SetNumUninitialized(Res * Res);
		for (int32 Y = 0; Y < Res; ++Y)
		{
			for (int32 X = 0; X < Res; ++X)
			{
				const double Wx = CenterX - Span * 0.5 + (X + 0.5) * Cell, Wy = CenterY - Span * 0.5 + (Y + 0.5) * Cell;
				const float Ground = FMOWorldGen::SampleHeightCm(*P, Wx, Wy, Seed, 50.0f);
				const float D = FMOWorldGen::SampleCaveDistanceAtGround(*P, Wx, Wy, Ground - DepthM * 100.0f, Ground, Seed, 50.0f);
				FColor C = FColor(70, 66, 62);
				if (Ground < P->Tuning.SeaLevelCm + P->Tuning.CaveMinGroundAboveSeaCm) { C = FColor(60, 90, 140); }
				if (D < 0.0f) { C = FColor(245, 240, 225); }
				Pixels[(Res - 1 - Y) * Res + X] = C;
			}
		}
		for (int64 Cy = -4; Cy < 4; ++Cy)
		{
			for (int64 Cx = -4; Cx < 4; ++Cx)
			{
				FMOSinkhole S;
				if (!FMOWorldGen::GetSinkholeInCell(*P, Cx, Cy, Seed, S)) { continue; }
				const int32 Px = (int32)((S.Center.X - (CenterX - Span * 0.5)) / Cell), Py = (int32)((S.Center.Y - (CenterY - Span * 0.5)) / Cell);
				for (int32 Dy = -2; Dy <= 2; ++Dy) { for (int32 Dx = -2; Dx <= 2; ++Dx)
				{
					const int32 Qx = Px + Dx, Qy = Py + Dy;
					if (Qx >= 0 && Qx < Res && Qy >= 0 && Qy < Res) { Pixels[(Res - 1 - Qy) * Res + Qx] = FColor(230, 40, 40); }
				} }
			}
		}
		const FString Path = Dir / FString::Printf(TEXT("caves_slice_%03.0fm_seed12345.png"), DepthM);
		TestTrue(*FString::Printf(TEXT("slice at %.0f m written"), DepthM), SavePng(Path, Pixels, Res, Res));
		AddInfo(Path);
	}

	// Vertical cross-section along +X through the origin: 4 km wide, 220 m tall around the ground. Sky = blue, rock = brown, cave = near-white.
	{
		const int32 W = 1000, H = 330;
		const double Dx = 400000.0 / W, Dz = 22000.0 / H;
		TArray<FColor> Pixels;
		Pixels.SetNumUninitialized(W * H);
		const double Wy = 0.0;
		for (int32 X = 0; X < W; ++X)
		{
			const double Wx = -200000.0 + (X + 0.5) * Dx;
			const float Ground = FMOWorldGen::SampleHeightCm(*P, Wx, Wy, Seed, 50.0f);
			for (int32 Row = 0; Row < H; ++Row)
			{
				const double Z = Ground + 2000.0 - (Row + 0.5) * Dz;
				const float D = FMOWorldGen::SampleCaveDistanceAtGround(*P, Wx, Wy, Z, Ground, Seed, 50.0f);
				FColor C = (Z > Ground) ? FColor(150, 190, 235) : FColor(110, 84, 60);
				if (Z <= Ground && D < 0.0f) { C = FColor(245, 240, 225); }
				Pixels[Row * W + X] = C;
			}
		}
		const FString Path = Dir / TEXT("caves_section_seed12345.png");
		TestTrue(TEXT("cross-section written"), SavePng(Path, Pixels, W, H));
		AddInfo(Path);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
