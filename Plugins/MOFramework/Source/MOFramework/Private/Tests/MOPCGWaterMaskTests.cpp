// Nothing is planted in water (FMOPCGWaterMask) and an upland lake's bed is its own ground (FMOWorldGen::UplandLakeBedWeight). Headless-safe: generator only.

#include "Misc/AutomationTest.h"
#include "MOPCGWaterMask.h"
#include "MOWorldGen.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FMOWorldGenParamsRef MakeWaterParams()
	{
		TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> P = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
		P->Finalize();
		return FMOWorldGenParamsRef(P);
	}

	/** The lakes of a stretch of the world, found the way the game finds them. */
	TArray<FMOWorldGen::FLake> FindLakes(const FMOWorldGenParams& P, int32 Seed, int32 Max)
	{
		TArray<FMOWorldGen::FLake> Lakes;
		const int32 HalfCells = (int32)(P.Tuning.WorldHalfSizeCm / P.Tuning.UplandLakeCellCm);
		for (int64 Cy = -HalfCells; Cy < HalfCells && Lakes.Num() < Max; ++Cy)
		{
			for (int64 Cx = -HalfCells; Cx < HalfCells && Lakes.Num() < Max; ++Cx)
			{
				FMOWorldGen::FLake L;
				if (FMOWorldGen::GetLakeInCell(P, 2, Cx, Cy, Seed, L))
				{
					Lakes.Add(L);
				}
			}
		}
		return Lakes;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOPCG_WaterMask,
	"MOFramework.PCG.WaterMask",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOPCG_WaterMask::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = MakeWaterParams();
	constexpr int32 Seed = 424242;
	const FMOPCGWaterMask Mask(P, Seed);
	const FMOPCGWaterMask Off;

	TestFalse(TEXT("a default mask is inactive (the main menu, no seed): it filters nothing"), Off.IsActive());
	TestFalse(TEXT("... so nothing is water to it"), Off.IsInWater(0.0, 0.0, -5000.0));
	TestTrue(TEXT("a mask built from params and a seed is active"), Mask.IsActive());

	// The sea, rivers, ponds and sea-level lakes are terrain below the sea plane.
	const float Sea = P->Tuning.SeaLevelCm;
	TestTrue(TEXT("ground well under the sea plane is water"), Mask.IsInWater(1000.0, 1000.0, Sea - 300.0));
	TestTrue(TEXT("ground a hair under the sea plane (shallows, +15 cm) is water"), Mask.IsInWater(1000.0, 1000.0, Sea + 10.0));
	TestFalse(TEXT("dry ground a few metres above the sea plane is not water"), Mask.IsInWater(1000.0, 1000.0, Sea + 400.0));

	// Upland lakes sit ABOVE the sea plane: only the lake test can see them.
	const TArray<FMOWorldGen::FLake> Lakes = FindLakes(*P, Seed, 30);
	if (!TestTrue(TEXT("the test world has upland lakes"), Lakes.Num() >= 10))
	{
		return false;
	}
	int32 CentreMissed = 0, ShoreMissed = 0, BankMissed = 0, FarFalse = 0;
	for (const FMOWorldGen::FLake& L : Lakes)
	{
		const double HighZ = (double)L.SurfaceCm + 50.0;   // well above sea level, as the lake is: the sea test cannot be what answers
		CentreMissed += Mask.IsInWater(L.Center.X, L.Center.Y, HighZ) ? 0 : 1;
		// 90% of the way to the shore along the long axis, and 3% past it (the default bank margin is 5%).
		ShoreMissed += Mask.IsInWater(L.Center.X + 0.9 * L.RadiusCm * L.CosAngle, L.Center.Y + 0.9 * L.RadiusCm * L.SinAngle, HighZ) ? 0 : 1;
		BankMissed += Mask.IsInWater(L.Center.X + 1.03 * L.RadiusCm * L.CosAngle, L.Center.Y + 1.03 * L.RadiusCm * L.SinAngle, HighZ) ? 0 : 1;
		// 2.4 radii out is dry land (the cell margin keeps neighbours' lakes away).
		FarFalse += Mask.IsInWater(L.Center.X + 2.4 * L.RadiusCm * L.CosAngle, L.Center.Y + 2.4 * L.RadiusCm * L.SinAngle, HighZ) ? 1 : 0;
	}
	TestEqual(TEXT("the centre of every upland lake is water, however high it lies"), CentreMissed, 0);
	TestEqual(TEXT("... and so is 90% of the way to the shore"), ShoreMissed, 0);
	TestEqual(TEXT("... and the first metres of the bank (103% of the shore)"), BankMissed, 0);
	TestEqual(TEXT("2.4 shore radii out is not water"), FarFalse, 0);

	// A wider bank margin reaches further.
	const FMOPCGWaterMask Wide(P, Seed, 1.3f);
	const FMOWorldGen::FLake& L0 = Lakes[0];
	const double Zhigh = (double)L0.SurfaceCm + 50.0;
	const double Px = L0.Center.X + 1.2 * L0.RadiusCm * L0.CosAngle, Py = L0.Center.Y + 1.2 * L0.RadiusCm * L0.SinAngle;
	TestFalse(TEXT("120% of the shore is outside the default 105% margin"), Mask.IsInWater(Px, Py, Zhigh));
	TestTrue(TEXT("... and inside a 130% margin"), Wide.IsInWater(Px, Py, Zhigh));

	// Water switched off in the tuning: nothing is water (the legacy worlds).
	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> NoWater = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	NoWater->Tuning.bEnableWater = false;
	NoWater->Finalize();
	TestFalse(TEXT("a world without water has no water to keep out of"), FMOPCGWaterMask(FMOWorldGenParamsRef(NoWater), Seed).IsInWater(L0.Center.X, L0.Center.Y, Sea - 500.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_UplandLakeBed,
	"MOFramework.WorldGen.UplandLakeBed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_UplandLakeBed::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = MakeWaterParams();
	constexpr int32 Seed = 424242;
	const TArray<FMOWorldGen::FLake> Lakes = FindLakes(*P, Seed, 40);
	if (!TestTrue(TEXT("the test world has upland lakes"), Lakes.Num() >= 10))
	{
		return false;
	}
	int32 InsideWrong = 0, FarWrong = 0, RimBoth = 0, Nondeterministic = 0;
	double RimSum = 0.0;
	int32 RimN = 0;
	for (const FMOWorldGen::FLake& L : Lakes)
	{
		// Anywhere inside the shore ellipse the whole bed is lake bed.
		for (int32 K = 0; K < 8; ++K)
		{
			const double Angle = (double)K * UE_PI / 4.0;
			const double Lx = 0.95 * L.RadiusCm * FMath::Cos(Angle), Ly = 0.95 * L.RadiusCm * L.Aspect * FMath::Sin(Angle);
			const double X = L.Center.X + L.CosAngle * Lx - L.SinAngle * Ly, Y = L.Center.Y + L.SinAngle * Lx + L.CosAngle * Ly;
			InsideWrong += FMOWorldGen::UplandLakeBedWeight(*P, X, Y, Seed) < 0.999f ? 1 : 0;
			Nondeterministic += FMOWorldGen::UplandLakeBedWeight(*P, X, Y, Seed) != FMOWorldGen::UplandLakeBedWeight(*P, X, Y, Seed) ? 1 : 0;
		}
		// 2.4 radii out (inside the lake's own cell): none.
		FarWrong += FMOWorldGen::UplandLakeBedWeight(*P, L.Center.X + 2.4 * L.RadiusCm * L.CosAngle, L.Center.Y + 2.4 * L.RadiusCm * L.SinAngle, Seed) > 0.0f ? 1 : 0;
		// The rim (shore .. 20% past it along the long axis): sampled finely, it fades out and is ragged (some samples above, some below the 0.5 paint threshold within the band).
		int32 Above = 0, Below = 0;
		for (int32 I = 0; I <= 60; ++I)
		{
			const double Q = 1.0 + 0.20 * (double)I / 60.0;
			const float W = FMOWorldGen::UplandLakeBedWeight(*P, L.Center.X + Q * L.RadiusCm * L.CosAngle, L.Center.Y + Q * L.RadiusCm * L.SinAngle, Seed);
			RimSum += W;
			++RimN;
			(W >= 0.5f ? Above : Below) += 1;
		}
		RimBoth += (Above > 0 && Below > 0) ? 1 : 0;
	}
	TestEqual(TEXT("95% of the way to the shore is lake bed everywhere on every lake"), InsideWrong, 0);
	TestEqual(TEXT("2.4 radii from a lake there is no lake bed"), FarWrong, 0);
	TestEqual(TEXT("the weight is deterministic"), Nondeterministic, 0);
	TestEqual(TEXT("the rim of every lake crosses the paint threshold (bed -> biome ground)"), RimBoth, Lakes.Num());
	AddInfo(FString::Printf(TEXT("mean bed weight over the 20%% past the shore: %.2f"), RimN > 0 ? RimSum / (double)RimN : 0.0));

	// Upland lakes off: no bed anywhere.
	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> NoLakes = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	NoLakes->Tuning.bEnableUplandLakes = false;
	NoLakes->Finalize();
	TestEqual(TEXT("without upland lakes there is no lake bed"), FMOWorldGen::UplandLakeBedWeight(*NoLakes, Lakes[0].Center.X, Lakes[0].Center.Y, Seed), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOPCG_MayHaveShore,
	"MOFramework.PCG.MayHaveShore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOPCG_MayHaveShore::RunTest(const FString& Parameters)
{
	// MayHaveShore is the shore tier's cheap per-cell skip. It must be SOUND: a cell it rejects may not contain a single point with a shore factor, or the reed belt would silently miss stretches of bank.
	const FMOWorldGenParamsRef P = MakeWaterParams();
	constexpr int32 Seed = 90210;
	const FMOPCGWaterMask Mask(P, Seed);
	const FMOPCGWaterMask Off;
	TestFalse(TEXT("an inactive mask has no shore"), Off.MayHaveShore(0.0, 0.0, 6400.0, 6400.0, 3000.0));

	const TArray<FMOWorldGen::FLake> Lakes = FindLakes(*P, Seed, 12);
	if (!TestTrue(TEXT("the test world has upland lakes"), Lakes.Num() >= 5))
	{
		return false;
	}
	for (const FMOWorldGen::FLake& L : Lakes)
	{
		TestTrue(TEXT("a cell on a lake's shore may have shore"), Mask.MayHaveShore(L.Center.X + 0.9 * L.RadiusCm, L.Center.Y, L.Center.X + 0.9 * L.RadiusCm + 6400.0, L.Center.Y + 6400.0, 3000.0));
	}

	FRandomStream R(31);
	const double Half = (double)P->Tuning.WorldHalfSizeCm * 0.8;
	int32 Rejected = 0, Accepted = 0, UnsoundCells = 0;
	for (int32 I = 0; I < 160; ++I)
	{
		const double X0 = R.FRandRange(-Half, Half), Y0 = R.FRandRange(-Half, Half);
		const double Size = 6400.0;
		if (Mask.MayHaveShore(X0, Y0, X0 + Size, Y0 + Size, 3000.0))
		{
			++Accepted;
			continue;
		}
		++Rejected;
		for (int32 K = 0; K < 48; ++K)
		{
			const double X = X0 + R.FRand() * Size, Y = Y0 + R.FRand() * Size;
			const double Z = FMOWorldGen::SampleHeightCm(*P, X, Y, Seed, P->Tuning.VoxelSizeCm);
			if (Mask.ShoreFactor(X, Y, Z, 120.0) > 0.0f)
			{
				++UnsoundCells;
				break;
			}
		}
	}
	TestEqual(TEXT("no rejected cell contains a point with a shore factor"), UnsoundCells, 0);
	TestTrue(TEXT("the pre-test rejects most dry land (it is a performance skip)"), Rejected >= 40);
	AddInfo(FString::Printf(TEXT("160 random 64 m cells: %d rejected as shore-free, %d kept"), Rejected, Accepted));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
