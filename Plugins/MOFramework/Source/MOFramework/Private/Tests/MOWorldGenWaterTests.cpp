// Tests for the water half of the world generator: rivers and lake basins carved into low ground at sea level. Headless-safe. The PNG dump (DumpMaps) shows them
// as the blue below-sea-level areas of the relief maps.

#include "Misc/AutomationTest.h"
#include "MOWorldGen.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 WaterTestSeed = 424242;

	FMOWorldGenParamsRef MakeWaterParams(TFunctionRef<void(FMOWorldGenTuning&)> Modify = [](FMOWorldGenTuning&) {})
	{
		TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> P = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
		P->Tuning.bEnableUplandLakes = false;   // this test is about the SEA-LEVEL water (it asserts high ground is never touched); upland lakes have their own test, MOWorldGenCoastTests.cpp
		Modify(P->Tuning);
		P->Finalize();
		return FMOWorldGenParamsRef(P);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_Water,
	"MOFramework.WorldGen.Water",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_Water::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = MakeWaterParams();
	const FMOWorldGenTuning& T = P->Tuning;
	FRandomStream R(31);

	// 1. Water only ever LOWERS the ground, never touches high ground, and is exact when switched off.
	{
		const FMOWorldGenParamsRef Off = MakeWaterParams([](FMOWorldGenTuning& Tuning) { Tuning.bEnableWater = false; });
		int32 Raised = 0, HighTouched = 0, OffDiffers = 0, Carved = 0, Low = 0;
		for (int32 I = 0; I < 60000; ++I)
		{
			const double X = R.FRandRange(-2.5e6f, 2.5e6f), Y = R.FRandRange(-2.5e6f, 2.5e6f);
			const float Base = FMOWorldGen::SampleBaseHeightCm(*P, X, Y, WaterTestSeed);
			const float H = FMOWorldGen::SampleHeightCm(*P, X, Y, WaterTestSeed);
			Raised += (H > Base) ? 1 : 0;
			HighTouched += (Base > T.SeaLevelCm + 6000.0f && H != Base) ? 1 : 0;
			OffDiffers += (FMOWorldGen::SampleHeightCm(*Off, X, Y, WaterTestSeed) != Base) ? 1 : 0;
			if (Base > T.SeaLevelCm && Base < T.SeaLevelCm + 1100.0f)
			{
				++Low;
				Carved += (H < T.SeaLevelCm) ? 1 : 0;
			}
		}
		TestEqual(TEXT("water never raises the ground"), Raised, 0);
		TestEqual(TEXT("ground more than 60 m above sea level is never touched"), HighTouched, 0);
		TestEqual(TEXT("bEnableWater = false: SampleHeightCm equals SampleBaseHeightCm exactly"), OffDiffers, 0);
		const float Fraction = Low > 0 ? (float)Carved / Low : 0.0f;
		AddInfo(FString::Printf(TEXT("%.1f%% of the low land (0..11 m above sea) is carved under water (%d of %d samples)"), Fraction * 100.0f, Carved, Low));
		TestTrue(FString::Printf(TEXT("between 1%% and 35%% of low land becomes water (got %.1f%%)"), Fraction * 100.0f), Fraction > 0.01f && Fraction < 0.35f);
	}

	// 2. Rivers alone (basins off): they exist, are narrow channels, and only in lowland.
	{
		const FMOWorldGenParamsRef RiversOnly = MakeWaterParams([](FMOWorldGenTuning& Tuning) { Tuning.PondChance = 0.0f; Tuning.LakeChance = 0.0f; });
		int32 Wet = 0, Low = 0, WetHigh = 0;
		for (int32 I = 0; I < 80000; ++I)
		{
			const double X = R.FRandRange(-2.0e6f, 2.0e6f), Y = R.FRandRange(-2.0e6f, 2.0e6f);
			const float Base = FMOWorldGen::SampleBaseHeightCm(*RiversOnly, X, Y, WaterTestSeed);
			const float H = FMOWorldGen::SampleHeightCm(*RiversOnly, X, Y, WaterTestSeed);
			if (Base > T.SeaLevelCm && Base < T.SeaLevelCm + T.RiverMaxGroundAboveSeaCm + 600.0f)
			{
				++Low;
				Wet += (H < T.SeaLevelCm) ? 1 : 0;
			}
			WetHigh += (Base > T.SeaLevelCm + T.RiverMaxGroundAboveSeaCm + 600.0f && H != Base) ? 1 : 0;
		}
		const float Fraction = Low > 0 ? (float)Wet / Low : 0.0f;
		AddInfo(FString::Printf(TEXT("rivers cover %.2f%% of the low land (%d of %d samples)"), Fraction * 100.0f, Wet, Low));
		TestTrue(FString::Printf(TEXT("rivers exist but are narrow: 0.3%% .. 12%% of low land (got %.2f%%)"), Fraction * 100.0f), Fraction > 0.003f && Fraction < 0.12f);
		TestEqual(TEXT("no river above the headwater height"), WetHigh, 0);
	}

	// 3. Basins: found, a bowl under sea level with a shore at sea level, fit rim, banks above.
	{
		int32 Found[2] = { 0, 0 };
		int32 CentreWet = 0, ShoreOk = 0, RimDry = 0, Checked = 0;
		for (int32 Tier = 0; Tier < 2; ++Tier)
		{
			const int32 Half = Tier == 0 ? 20 : 8;
			for (int64 Cy = -Half; Cy < Half; ++Cy)
			{
				for (int64 Cx = -Half; Cx < Half; ++Cx)
				{
					FMOWorldGen::FLake L;
					if (!FMOWorldGen::GetLakeInCell(*P, Tier, Cx, Cy, WaterTestSeed, L))
					{
						continue;
					}
					++Found[Tier];
					++Checked;
					CentreWet += (FMOWorldGen::SampleHeightCm(*P, L.Center.X, L.Center.Y, WaterTestSeed) <= T.SeaLevelCm - L.DepthCm * 0.95f) ? 1 : 0;
					// On the long axis a hair inside the shore: still water; a hair outside: the bank just above sea level.
					const double Ax = L.Center.X + L.CosAngle * L.RadiusCm * 0.98, Ay = L.Center.Y + L.SinAngle * L.RadiusCm * 0.98;
					const double Bx = L.Center.X + L.CosAngle * L.RadiusCm * 1.06, By = L.Center.Y + L.SinAngle * L.RadiusCm * 1.06;
					const float Inside = FMOWorldGen::SampleHeightCm(*P, Ax, Ay, WaterTestSeed), Outside = FMOWorldGen::SampleHeightCm(*P, Bx, By, WaterTestSeed);
					ShoreOk += (Inside < T.SeaLevelCm && Outside >= T.SeaLevelCm) ? 1 : 0;
					// Every rim point of a basin was fit (low land): the carved rim stays dry.
					const double Cxw = L.Center.X - L.CosAngle * L.RadiusCm * 1.15, Cyw = L.Center.Y - L.SinAngle * L.RadiusCm * 1.15;
					RimDry += (FMOWorldGen::SampleHeightCm(*P, Cxw, Cyw, WaterTestSeed) >= T.SeaLevelCm) ? 1 : 0;
				}
			}
		}
		AddInfo(FString::Printf(TEXT("%d ponds in 40 x 40 cells (%.0f m) and %d lakes in 16 x 16 cells (%.0f m) have a basin"), Found[0], T.PondCellCm / 100.0f, Found[1], T.LakeCellCm / 100.0f));
		TestTrue(TEXT("ponds are found"), Found[0] > 0);
		TestTrue(TEXT("lakes are found"), Found[1] > 0);
		TestEqual(TEXT("every basin centre is carved to ~its full depth below sea level"), CentreWet, Checked);
		// (A river can cross a basin, so the shore / bank checks tolerate a few exceptions.)
		TestTrue(FString::Printf(TEXT("at least 90%% of basins have water just inside the shore and dry bank just outside it (%d of %d)"), ShoreOk, Checked), ShoreOk * 10 >= Checked * 9);
		TestTrue(FString::Printf(TEXT("at least 90%% of basins keep the far bank above water (%d of %d)"), RimDry, Checked), RimDry * 10 >= Checked * 9);
	}

	// 4. Continuous: no cliff where a channel or bowl meets the land (the same 10 cm step test as the base terrain, but across wet ground).
	{
		float MaxStep = 0.0f;
		int32 Wet = 0;
		for (int32 Line = 0; Line < 200; ++Line)
		{
			double X0 = R.FRandRange(-2.0e6f, 2.0e6f), Y0 = R.FRandRange(-2.0e6f, 2.0e6f);
			if (FMOWorldGen::SampleBaseHeightCm(*P, X0, Y0, WaterTestSeed) > T.SeaLevelCm + 1200.0f)
			{
				continue;
			}
			float Prev = FMOWorldGen::SampleHeightCm(*P, X0, Y0, WaterTestSeed);
			for (int32 I = 1; I <= 300; ++I)
			{
				const float H = FMOWorldGen::SampleHeightCm(*P, X0 + 10.0 * I, Y0, WaterTestSeed);
				MaxStep = FMath::Max(MaxStep, FMath::Abs(H - Prev));
				Wet += (H < T.SeaLevelCm) ? 1 : 0;
				Prev = H;
			}
		}
		TestTrue(TEXT("the test lines cross water"), Wet > 0);
		TestTrue(FString::Printf(TEXT("no cliff at a shore: max height change over a 10 cm step is %.1f cm (limit 60)"), MaxStep), MaxStep < 60.0f);
	}

	// 5. Pure.
	{
		bool bIdentical = true;
		for (int32 I = 0; I < 3000; ++I)
		{
			const double X = R.FRandRange(-2.0e6f, 2.0e6f), Y = R.FRandRange(-2.0e6f, 2.0e6f);
			bIdentical &= FMOWorldGen::SampleHeightCm(*P, X, Y, WaterTestSeed) == FMOWorldGen::SampleHeightCm(*P, X, Y, WaterTestSeed);
		}
		TestTrue(TEXT("the same (params, seed, position) is bit-identical every call (the thread-local basin cache never changes a result)"), bIdentical);
	}
	return true;
}

// A coarse LOD skips the basins it cannot resolve (FMOWorldGen::IsWaterTierResolved). Before the gate, the far voxel chunks of every new game looked up an upland lake per
// sample -- each one hundreds of full-detail base samples -- for lakes narrower than one of their voxels: 5 s per 32 km chunk, the 10 s second voxel state (Oct 13 2026).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_WaterLodGate,
	"MOFramework.WorldGen.WaterLodGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_WaterLodGate::RunTest(const FString& Parameters)
{
	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> Lakes = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	Lakes->Finalize();
	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> NoUpland = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	NoUpland->Tuning.bEnableUplandLakes = false;
	NoUpland->Finalize();
	const FMOWorldGenTuning& T = Lakes->Tuning;

	// 1. The rule: every tier is resolved at the voxel size and just under its largest radius, none at a spacing as wide as its largest radius.
	for (int32 Tier = 0; Tier < 3; ++Tier)
	{
		const float MaxRadiusCm = Tier == 0 ? T.PondRadiusMaxCm : Tier == 1 ? T.LakeRadiusMaxCm : T.UplandLakeRadiusMaxCm;
		TestTrue(FString::Printf(TEXT("tier %d is resolved at the voxel size"), Tier), FMOWorldGen::IsWaterTierResolved(*Lakes, Tier, T.VoxelSizeCm));
		TestTrue(FString::Printf(TEXT("tier %d is resolved just under its largest radius"), Tier), FMOWorldGen::IsWaterTierResolved(*Lakes, Tier, 0.99f * MaxRadiusCm));
		TestFalse(FString::Printf(TEXT("tier %d is skipped at a spacing as wide as its largest radius"), Tier), FMOWorldGen::IsWaterTierResolved(*Lakes, Tier, MaxRadiusCm));
	}

	// 2. A real upland lake: carved at the voxel size; at a spacing past every tier the height is exactly the height without upland lakes (rivers still apply to both).
	TArray<FMOWorldGen::FLake> Found;
	for (double RadiusCm = 1.0e6; Found.Num() == 0 && RadiusCm <= 1.6e7; RadiusCm *= 2.0)
	{
		FMOWorldGen::GetUplandLakesNear(*Lakes, 0.0, 0.0, RadiusCm, WaterTestSeed, Found);
	}
	if (!TestTrue(TEXT("the seed has an upland lake"), Found.Num() > 0))
	{
		return false;
	}
	const FMOWorldGen::FLake& L = Found[0];
	const float Fine = T.VoxelSizeCm;
	const float Coarse = 2.0f * FMath::Max3(T.PondRadiusMaxCm, T.LakeRadiusMaxCm, T.UplandLakeRadiusMaxCm);
	const float FineWith = FMOWorldGen::SampleHeightCm(*Lakes, L.Center.X, L.Center.Y, WaterTestSeed, Fine);
	const float FineWithout = FMOWorldGen::SampleHeightCm(*NoUpland, L.Center.X, L.Center.Y, WaterTestSeed, Fine);
	TestTrue(FString::Printf(TEXT("at the voxel size the lake carves its centre (%.0f cm with, %.0f without)"), FineWith, FineWithout), FineWith < FineWithout - 100.0f);
	TestEqual(TEXT("past every tier's spacing the lake is skipped: the same height as without upland lakes"),
		FMOWorldGen::SampleHeightCm(*Lakes, L.Center.X, L.Center.Y, WaterTestSeed, Coarse), FMOWorldGen::SampleHeightCm(*NoUpland, L.Center.X, L.Center.Y, WaterTestSeed, Coarse));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_WaterBodyKind,
	"MOFramework.WorldGen.WaterBodyKind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_WaterBodyKind::RunTest(const FString& Parameters)
{
	// Drinking reads ClassifyWaterBody (salt or fresh, how risky): it must say "water" exactly where the terrain is under its water surface, and every kind
	// must exist somewhere -- else a river would drink as sea (or nothing) and the illness odds would be wrong.
	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> Built = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	Built->Finalize();   // the shipping tuning, upland lakes on
	const FMOWorldGenParamsRef P(Built);
	const FMOWorldGenTuning& T = P->Tuning;
	FRandomStream R(77);
	int32 Count[6] = {}, Disagree = 0, Samples = 0;
	for (int32 I = 0; I < 40000; ++I)
	{
		const double X = R.FRandRange(-3.0e6f, 3.0e6f), Y = R.FRandRange(-3.0e6f, 3.0e6f);
		const EMOWaterBodyKind Kind = FMOWorldGen::ClassifyWaterBody(*P, X, Y, WaterTestSeed);
		float Surface = 0.0f;
		FMOWorldGen::GetWaterSurfaceCm(*P, X, Y, WaterTestSeed, Surface);
		const float Ground = FMOWorldGen::SampleHeightCm(*P, X, Y, WaterTestSeed, T.VoxelSizeCm);
		const bool bWet = Ground < Surface;
		// An upland lake's ellipse is water by definition (its bed is under the plane except a sliver at the shore line): only the sea-plane kinds must match the ground.
		if (Kind != EMOWaterBodyKind::UplandLake)
		{
			++Samples;
			Disagree += (Kind != EMOWaterBodyKind::None) != bWet ? 1 : 0;
		}
		Count[static_cast<int32>(Kind)]++;
	}
	AddInfo(FString::Printf(TEXT("none %d, sea %d, river %d, pond %d, lake %d, upland lake %d"), Count[0], Count[1], Count[2], Count[3], Count[4], Count[5]));
	TestTrue(FString::Printf(TEXT("water exactly where the ground is under the sea plane (%d of %d disagree)"), Disagree, Samples), Disagree <= Samples / 1000);
	TestTrue(TEXT("there is sea"), Count[static_cast<int32>(EMOWaterBodyKind::Sea)] > 0);
	TestTrue(TEXT("there are rivers"), Count[static_cast<int32>(EMOWaterBodyKind::River)] > 0);
	TestTrue(TEXT("there are lakes or ponds"), Count[static_cast<int32>(EMOWaterBodyKind::Lake)] + Count[static_cast<int32>(EMOWaterBodyKind::Pond)] > 0);
	TestTrue(TEXT("there are upland lakes"), Count[static_cast<int32>(EMOWaterBodyKind::UplandLake)] > 0);
	TestTrue(TEXT("most of the world is dry land or sea"), Count[0] + Count[1] > 30000);

	// CONTROL: with water off nothing is water.
	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> Dry = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	Dry->Tuning.bEnableWater = false;
	Dry->Finalize();
	int32 Wet = 0;
	for (int32 I = 0; I < 2000; ++I)
	{
		Wet += FMOWorldGen::ClassifyWaterBody(*Dry, R.FRandRange(-3.0e6f, 3.0e6f), R.FRandRange(-3.0e6f, 3.0e6f), WaterTestSeed) != EMOWaterBodyKind::None ? 1 : 0;
	}
	TestEqual(TEXT("CONTROL: water off -> no water anywhere"), Wet, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
