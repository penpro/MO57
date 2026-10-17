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

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 CaveTestSeed = 424242;

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
	const FMOWorldGenParamsRef P = MakeCaveParams();
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
		int32 AboveWrong = 0, CoverWrong = 0, CoverChecked = 0, SteepColumns = 0, SteepOpen = 0;
		const float FlatTan = FMath::Tan(FMath::DegreesToRadians(T.CaveEntranceSlopeMinDeg - 2.0f));
		for (int32 I = 0; I < 6000; ++I)
		{
			double X, Y;
			RandomColumn(X, Y);
			const float Ground = FMOWorldGen::SampleHeightCm(*P, X, Y, CaveTestSeed);
			AboveWrong += (FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Ground + R.FRandRange(150.0f, 1500.0f), Ground, CaveTestSeed) != FMOWorldGen::CaveFarCm) ? 1 : 0;
			// The same slope the field uses: forward differences over 3 m, both ways (a column on a slope's edge counts as steep if either side is).
			const float Gx = (FMOWorldGen::SampleHeightCm(*P, X + 300.0, Y, CaveTestSeed) - Ground) / 300.0f;
			const float Gy = (FMOWorldGen::SampleHeightCm(*P, X, Y + 300.0, CaveTestSeed) - Ground) / 300.0f;
			const float Gx2 = (Ground - FMOWorldGen::SampleHeightCm(*P, X - 300.0, Y, CaveTestSeed)) / 300.0f;
			const float Gy2 = (Ground - FMOWorldGen::SampleHeightCm(*P, X, Y - 300.0, CaveTestSeed)) / 300.0f;
			const float Depth = R.FRandRange(0.0f, T.CaveCoverCm - 50.0f);
			const bool bOpen = FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Ground - Depth, Ground, CaveTestSeed) <= 0.0f;
			if (FMath::Max(FMath::Sqrt(Gx * Gx + Gy * Gy), FMath::Sqrt(Gx2 * Gx2 + Gy2 * Gy2)) < FlatTan)
			{
				++CoverChecked;
				CoverWrong += bOpen ? 1 : 0;
			}
			else
			{
				++SteepColumns;
				SteepOpen += bOpen ? 1 : 0;
			}
		}
		AddInfo(FString::Printf(TEXT("steep columns: %d, of which open inside the cover (a gallery entrance): %d"), SteepColumns, SteepOpen));
		TestEqual(TEXT("no cave above the ground (6000 columns)"), AboveWrong, 0);
		TestEqual(*FString::Printf(TEXT("under ground flatter than %.0f deg no gallery / cavern breaks the %.0f cm cover (%d columns): no hole to fall into"),
			T.CaveEntranceSlopeMinDeg - 2.0f, T.CaveCoverCm, CoverChecked), CoverWrong, 0);
	}

	// 4. The rock really is carved, but not swiss cheese.
	{
		int32 Inside = 0;
		const int32 N = 60000;
		for (int32 I = 0; I < N; ++I)
		{
			double X, Y;
			RandomColumn(X, Y);
			const float Ground = FMOWorldGen::SampleHeightCm(*P, X, Y, CaveTestSeed);
			Inside += (FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Ground - R.FRandRange(1000.0f, 12000.0f), Ground, CaveTestSeed) < 0.0f) ? 1 : 0;
		}
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
		for (int32 I = 0; I < 200000; ++I)   // v9's galleries are half the radius: more samples to find their walls
		{
			double X, Y;
			RandomColumn(X, Y);
			const float Ground = FMOWorldGen::SampleHeightCm(*P, X, Y, CaveTestSeed);
			const double Z = Ground - R.FRandRange(1500.0f, 12000.0f);
			const float D0 = FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Z, Ground, CaveTestSeed);
			// Forward differences along the three axes (the column's ground is reused: the gate depends on Z only through Depth, which Z + h changes by h).
			const float Gx = (FMOWorldGen::SampleCaveDistanceCm(*P, X + 25.0, Y, Z, CaveTestSeed) - D0) / 25.0f;
			const float Gy = (FMOWorldGen::SampleCaveDistanceCm(*P, X, Y + 25.0, Z, CaveTestSeed) - D0) / 25.0f;
			const float Gz = (FMOWorldGen::SampleCaveDistanceCm(*P, X, Y, Z + 25.0, CaveTestSeed) - D0) / 25.0f;
			const float Magnitude = FMath::Sqrt(Gx * Gx + Gy * Gy + Gz * Gz);
			if (Magnitude > MaxGradient)
			{
				MaxGradient = Magnitude;
				WorstText = FString::Printf(TEXT("worst: at (%.0f, %.0f, %.0f) depth %.0f cm, d=%.1f, gradient (%.1f, %.1f, %.1f)"), X, Y, Z, Ground - Z, D0, Gx, Gy, Gz);
			}
			// At a wall (|d| < 60 cm), away from the shallow / deep taper zones where the penalty ramps steepen the field.
			if (FMath::Abs(D0) < 60.0f && Ground - Z > T.CavernMinDepthCm + T.CaveFadeCm && Ground - Z < T.CaveMaxDepthCm - 500.0f)
			{
				GradientSum += Magnitude;
				++WallCount;
			}
		}
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
		for (int32 I = 0; I < 30000; ++I)
		{
			double X, Y;
			RandomColumn(X, Y);
			const float Ground = FMOWorldGen::SampleHeightCm(*P, X, Y, CaveTestSeed);
			const double Z = Ground - R.FRandRange(1200.0f, 12000.0f);
			FineInside += (FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Z, Ground, CaveTestSeed, 50.0f) < 0.0f) ? 1 : 0;
			CoarseInside += (FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Z, Ground, CaveTestSeed, 3200.0f) < 0.0f) ? 1 : 0;
		}
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
		const FMOWorldGenParamsRef Sink = MakeCaveParams([](FMOWorldGenTuning& Tuning) { Tuning.SinkholeChance = 0.6f; });
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
	const FMOWorldGenParamsRef P = MakeCaveParams();
	FRandomStream R(97);
	int32 Measured = 0, Level = 0;
	float Worst = 0.0f;
	const float MaxTan = FMath::Tan(FMath::DegreesToRadians(15.0f));
	for (int32 Try = 0; Try < 2000000 && Measured < 250; ++Try)
	{
		const double X = R.FRandRange(-2.0e6f, 2.0e6f), Y = R.FRandRange(-2.0e6f, 2.0e6f);
		const float Ground = FMOWorldGen::SampleHeightCm(*P, X, Y, CaveTestSeed);
		const double Z = Ground - R.FRandRange(2200.0f, 3300.0f);
		if (FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Z, Ground, CaveTestSeed, 50.0f) > -60.0f)
		{
			continue;
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
			continue;   // a gallery's tapered end: nowhere to follow it
		}
		++Measured;
		const float Tan = FMath::Abs(BestDz) / 300.0f;
		Level += Tan <= MaxTan ? 1 : 0;
		Worst = FMath::Max(Worst, Tan);
	}
	AddInfo(FString::Printf(TEXT("%d passages followed, %d within 15 deg, steepest %.0f deg"), Measured, Level, FMath::RadiansToDegrees(FMath::Atan(Worst))));
	TestTrue(TEXT("enough gallery air found to measure (>= 100 passages)"), Measured >= 100);
	TestTrue(FString::Printf(TEXT("95%% of passages run within 15 deg of level (%d of %d)"), Level, Measured), Measured > 0 && Level >= FMath::CeilToInt(0.95f * Measured));

	// Entrances: a scan of a 20 x 20 km region at 25 m spacing -- columns on ground steep enough for an entrance with cave air within 2 m of the
	// surface. They exist (a cave you can walk into), and only on the steep columns (the flat-ground cover is tested in MOFramework.WorldGen.Caves).
	{
		const float MinTan = FMath::Tan(FMath::DegreesToRadians(P->Tuning.CaveEntranceSlopeMinDeg));
		int32 Steep = 0, Entrances = 0;
		for (double X = -1.0e6; X < 1.0e6; X += 2500.0)
		{
			for (double Y = -1.0e6; Y < 1.0e6; Y += 2500.0)
			{
				const float Ground = FMOWorldGen::SampleHeightCm(*P, X, Y, CaveTestSeed);
				const float Gx = (FMOWorldGen::SampleHeightCm(*P, X + 300.0, Y, CaveTestSeed) - Ground) / 300.0f;
				const float Gy = (FMOWorldGen::SampleHeightCm(*P, X, Y + 300.0, CaveTestSeed) - Ground) / 300.0f;
				if (FMath::Sqrt(Gx * Gx + Gy * Gy) < MinTan)
				{
					continue;
				}
				++Steep;
				bool bOpen = false;
				for (float Depth = 50.0f; Depth <= 200.0f && !bOpen; Depth += 50.0f)
				{
					bOpen = FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Ground - Depth, Ground, CaveTestSeed, 50.0f) < 0.0f;
				}
				Entrances += bOpen ? 1 : 0;
			}
		}
		AddInfo(FString::Printf(TEXT("entrance scan, 20 x 20 km at 25 m: %d steep column(s), %d with cave air within 2 m of the surface"), Steep, Entrances));
		TestTrue(TEXT("hillside entrances exist (a cave you walk into)"), Entrances > 0);
	}
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
	TestTrue(TEXT("bounds cover the deepest possible cave (minimum ground - max depth)"), Bounds.Min.Z <= (double)P->MinHeightCm - P->Tuning.CaveMaxDepthCm);
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
