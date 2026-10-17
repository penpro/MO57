// Tests for the geology half of the world generator (bedrock provinces, ore bodies) and the cave-surface finder the cave spawner scatters on.
// Headless-safe: no world, no PIE. The realism rules asserted here are the ones in MOWorldGenGeology.cpp's header (Wes, Oct 17 2026: "I still want realism").

#include "Misc/AutomationTest.h"
#include "MOWorldGen.h"
#include "MOGeologyYield.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 GeoTestSeed = 424242;

	FMOWorldGenParamsRef MakeGeoParams()
	{
		TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> P = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
		P->Finalize();
		return FMOWorldGenParamsRef(P);
	}

	const TCHAR* BedrockName(EMOBedrock B)
	{
		switch (B)
		{
		case EMOBedrock::Limestone: return TEXT("limestone");
		case EMOBedrock::CoalMeasures: return TEXT("coal measures");
		case EMOBedrock::Granite: return TEXT("granite");
		case EMOBedrock::Basalt: return TEXT("basalt");
		}
		return TEXT("?");
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_Geology,
	"MOFramework.WorldGen.Geology",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_Geology::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = MakeGeoParams();

	// 1. Provinces: every rock has a real share of the land, and the bedrock is regional (a neighbour 100 m away is almost always the same rock).
	int32 Land = 0, Same = 0, Pairs = 0;
	int32 Count[4] = { 0, 0, 0, 0 };
	for (double X = -3.0e6; X < 3.0e6; X += 50000.0)
	{
		for (double Y = -3.0e6; Y < 3.0e6; Y += 50000.0)
		{
			if (FMOWorldGen::SampleHeightCm(*P, X, Y, GeoTestSeed) <= P->Tuning.SeaLevelCm)
			{
				continue;
			}
			++Land;
			const EMOBedrock B = FMOWorldGen::SampleBedrock(*P, X, Y, GeoTestSeed);
			++Count[(int32)B];
			++Pairs;
			Same += FMOWorldGen::SampleBedrock(*P, X + 10000.0, Y, GeoTestSeed) == B ? 1 : 0;
		}
	}
	FString Shares;
	for (int32 B = 0; B < 4; ++B)
	{
		Shares += FString::Printf(TEXT("%s%s %.0f%%"), B ? TEXT(", ") : TEXT(""), BedrockName((EMOBedrock)B), Land ? 100.0 * Count[B] / Land : 0.0);
	}
	AddInfo(FString::Printf(TEXT("bedrock over %d land samples (60 x 60 km at 500 m): %s; same rock 100 m away: %.1f%%"), Land, *Shares, Pairs ? 100.0 * Same / Pairs : 0.0));
	for (int32 B = 0; B < 4; ++B)
	{
		TestTrue(FString::Printf(TEXT("%s covers 5..60%% of the land"), BedrockName((EMOBedrock)B)), Land > 0 && Count[B] >= 0.05 * Land && Count[B] <= 0.6 * Land);
	}
	TestTrue(TEXT("bedrock is regional: >= 97% of neighbours 100 m apart share it"), Pairs > 0 && Same >= 0.97 * Pairs);

	// 2. Each ore only in its host rock, and how much of each rock is ore (in the cave band: 10..140 m under the ground).
	FRandomStream R(31);
	int32 Wrong = 0;
	FString FirstWrong;
	int32 Samples[4] = { 0, 0, 0, 0 };
	int32 Ores[4][7] = {};
	for (int32 Try = 0; Try < 400000; ++Try)
	{
		const double X = R.FRandRange(-3.0e6f, 3.0e6f), Y = R.FRandRange(-3.0e6f, 3.0e6f);
		const EMOBedrock B = FMOWorldGen::SampleBedrock(*P, X, Y, GeoTestSeed);
		const double Z = R.FRandRange(1000.0f, 30000.0f);
		const FMOOreSample O = FMOWorldGen::SampleOreIn(*P, B, X, Y, Z, GeoTestSeed);
		++Samples[(int32)B];
		++Ores[(int32)B][(int32)O.Kind];
		const bool bAllowed = O.Kind == EMOOreKind::None
			|| (O.Kind == EMOOreKind::Coal && B == EMOBedrock::CoalMeasures)
			|| ((O.Kind == EMOOreKind::Hematite || O.Kind == EMOOreKind::Flint || O.Kind == EMOOreKind::Galena) && B == EMOBedrock::Limestone)
			|| (O.Kind == EMOOreKind::Copper && (B == EMOBedrock::Basalt || B == EMOBedrock::Granite))
			|| (O.Kind == EMOOreKind::Tin && B == EMOBedrock::Granite);
		if (!bAllowed)
		{
			++Wrong;
			if (FirstWrong.IsEmpty())
			{
				FirstWrong = FString::Printf(TEXT("ore %d in %s at (%.0f, %.0f, %.0f)"), (int32)O.Kind, BedrockName(B), X, Y, Z);
			}
		}
	}
	static const TCHAR* OreNames[7] = { TEXT("none"), TEXT("coal"), TEXT("hematite"), TEXT("copper"), TEXT("tin"), TEXT("flint"), TEXT("galena") };
	for (int32 B = 0; B < 4; ++B)
	{
		FString Line;
		for (int32 K = 1; K < 7; ++K)
		{
			if (Ores[B][K] > 0)
			{
				Line += FString::Printf(TEXT(" %s %.2f%%"), OreNames[K], 100.0 * Ores[B][K] / FMath::Max(Samples[B], 1));
			}
		}
		AddInfo(FString::Printf(TEXT("%s (%d samples):%s"), BedrockName((EMOBedrock)B), Samples[B], Line.IsEmpty() ? TEXT(" no ore") : *Line));
	}
	TestEqual(FString::Printf(TEXT("every ore sits in its host rock%s%s"), FirstWrong.IsEmpty() ? TEXT("") : TEXT(": first "), *FirstWrong), Wrong, 0);
	TestTrue(TEXT("coal is common in the coal measures (>= 1% of the rock)"), Ores[(int32)EMOBedrock::CoalMeasures][(int32)EMOOreKind::Coal] >= 0.01 * Samples[(int32)EMOBedrock::CoalMeasures]);
	for (const TPair<EMOBedrock, EMOOreKind>& Need : { TPair<EMOBedrock, EMOOreKind>(EMOBedrock::Limestone, EMOOreKind::Hematite), TPair<EMOBedrock, EMOOreKind>(EMOBedrock::Limestone, EMOOreKind::Flint),
		TPair<EMOBedrock, EMOOreKind>(EMOBedrock::Limestone, EMOOreKind::Galena),
		TPair<EMOBedrock, EMOOreKind>(EMOBedrock::Basalt, EMOOreKind::Copper), TPair<EMOBedrock, EMOOreKind>(EMOBedrock::Granite, EMOOreKind::Tin) })
	{
		TestTrue(FString::Printf(TEXT("%s holds %s (> 0.05%% of the rock)"), BedrockName(Need.Key), OreNames[(int32)Need.Value]),
			Ores[(int32)Need.Key][(int32)Need.Value] > 0.0005 * Samples[(int32)Need.Key]);
	}

	TestTrue(TEXT("galena is rarer than hematite in the limestone"),
		Ores[(int32)EMOBedrock::Limestone][(int32)EMOOreKind::Galena] < Ores[(int32)EMOBedrock::Limestone][(int32)EMOOreKind::Hematite]);

	// 3. Coal lies in BEDS: from a coal point, 2 m sideways is mostly still coal, 3 m up mostly is not.
	int32 CoalPoints = 0, Side = 0, Up = 0;
	for (int32 Try = 0; Try < 2000000 && CoalPoints < 400; ++Try)
	{
		const double X = R.FRandRange(-3.0e6f, 3.0e6f), Y = R.FRandRange(-3.0e6f, 3.0e6f), Z = R.FRandRange(1000.0f, 30000.0f);
		if (FMOWorldGen::SampleOreIn(*P, EMOBedrock::CoalMeasures, X, Y, Z, GeoTestSeed).Kind != EMOOreKind::Coal)
		{
			continue;
		}
		++CoalPoints;
		const double A = R.FRandRange(0.0f, 2.0f * UE_PI);
		Side += FMOWorldGen::SampleOreIn(*P, EMOBedrock::CoalMeasures, X + 200.0 * FMath::Cos(A), Y + 200.0 * FMath::Sin(A), Z, GeoTestSeed).Kind == EMOOreKind::Coal ? 1 : 0;
		Up += FMOWorldGen::SampleOreIn(*P, EMOBedrock::CoalMeasures, X, Y, Z + 300.0, GeoTestSeed).Kind == EMOOreKind::Coal ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("coal seams: %d coal points; 2 m sideways still coal %.0f%%, 3 m up %.0f%%"), CoalPoints, CoalPoints ? 100.0 * Side / CoalPoints : 0.0, CoalPoints ? 100.0 * Up / CoalPoints : 0.0));
	TestTrue(TEXT("coal is a bed: >= 75% of coal points continue 2 m sideways"), CoalPoints >= 100 && Side >= 0.75 * CoalPoints);
	TestTrue(TEXT("coal is a bed: <= 20% of coal points are still coal 3 m up"), CoalPoints >= 100 && Up <= 0.2 * CoalPoints);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_CaveSurfaces,
	"MOFramework.WorldGen.CaveSurfaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_CaveSurfaces::RunTest(const FString& Parameters)
{
	// The points the cave spawner scatters on: ON the surface, their normal pointing into the air, their kind matching the normal. Find a 64 m cell with caves first
	// (karst rule off: surface finding is the same in any rock, and the search stays short).
	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> Any = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	Any->Tuning.bCavesOnlyInLimestone = false;
	Any->Finalize();
	const FMOWorldGenParamsRef P = Any;
	TArray<FMOCaveSurfacePoint> Points;
	FBox2D Cell(ForceInit);
	double Ms = 0.0;
	for (int32 K = 0; K < 400 && Points.Num() < 50; ++K)
	{
		Points.Reset();
		const double X0 = -1.0e6 + (K % 20) * 100000.0, Y0 = -1.0e6 + (K / 20) * 100000.0;
		Cell = FBox2D(FVector2D(X0, Y0), FVector2D(X0 + 6400.0, Y0 + 6400.0));
		const double T0 = FPlatformTime::Seconds();
		FMOWorldGen::FindCaveSurfaces(*P, GeoTestSeed, Cell, 100.0f, Points);
		Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
	}
	int32 Kinds[3] = { 0, 0, 0 }, OffSurface = 0, BadSide = 0;
	for (const FMOCaveSurfacePoint& Pt : Points)
	{
		++Kinds[(int32)Pt.Kind];
		OffSurface += FMath::Abs(FMOWorldGen::SampleCaveDistanceAtGround(*P, Pt.Location.X, Pt.Location.Y, Pt.Location.Z, Pt.GroundCm, GeoTestSeed, 50.0f)) > 10.0f ? 1 : 0;
		const FVector Air = Pt.Location + Pt.AirNormal * 30.0, Rock = Pt.Location - Pt.AirNormal * 30.0;
		const bool bAir = FMOWorldGen::SampleCaveDistanceCm(*P, Air.X, Air.Y, Air.Z, GeoTestSeed) < 0.0f || Air.Z > Pt.GroundCm;
		const bool bRock = FMOWorldGen::SampleCaveDistanceCm(*P, Rock.X, Rock.Y, Rock.Z, GeoTestSeed) > 0.0f;
		BadSide += (bAir && bRock) ? 0 : 1;
	}
	AddInfo(FString::Printf(TEXT("64 m cell at (%.0f, %.0f): %d surface points in %.0f ms (1 m columns) -- %d floor, %d wall, %d ceiling; %d off the surface, %d with air / rock on the wrong side"),
		Cell.Min.X, Cell.Min.Y, Points.Num(), Ms, Kinds[0], Kinds[1], Kinds[2], OffSurface, BadSide));
	TestTrue(TEXT("a cell with caves has surface points (>= 50)"), Points.Num() >= 50);
	TestTrue(TEXT("floors, walls and ceilings are all found"), Kinds[0] > 0 && Kinds[1] > 0 && Kinds[2] > 0);
	TestTrue(TEXT("points lie on the surface (|field| <= 10 cm) -- 99%"), OffSurface <= Points.Num() / 100);
	TestTrue(TEXT("air 30 cm along the normal, rock 30 cm against it -- 95%"), BadSide <= Points.Num() / 20);

	// Deterministic: the same call gives the same points in the same order.
	TArray<FMOCaveSurfacePoint> Again;
	FMOWorldGen::FindCaveSurfaces(*P, GeoTestSeed, Cell, 100.0f, Again);
	bool bSame = Again.Num() == Points.Num();
	for (int32 I = 0; bSame && I < Points.Num(); ++I)
	{
		bSame = Again[I].Location == Points[I].Location && Again[I].Kind == Points[I].Kind;
	}
	TestTrue(TEXT("deterministic (same points, same order)"), bSame);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_DigYields,
	"MOFramework.WorldGen.DigYields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_DigYields::RunTest(const FString& Parameters)
{
	// Wes, Oct 17 2026: "mining yields from the geology ... they should be able to collect pretty much any ore". What a dig cuts through is what it yields.
	const FMOWorldGenParamsRef P = MakeGeoParams();
	const float Voxel = P->Tuning.VoxelSizeCm;
	FRandomStream R(77);

	// 1. Soil over the bedrock: 30..315 cm everywhere; the surface is soil, 4 m down never is.
	float MinDepth = 1.0e9f, MaxDepth = 0.0f;
	int32 SurfaceNotSoil = 0, DeepSoil = 0;
	for (int32 I = 0; I < 2000; ++I)
	{
		const double X = R.FRandRange(-2.0e6f, 2.0e6f), Y = R.FRandRange(-2.0e6f, 2.0e6f);
		const float Ground = FMOWorldGen::SampleHeightCm(*P, X, Y, GeoTestSeed, Voxel);
		const float Depth = FMOWorldGen::SoilDepthCm(*P, X, Y, GeoTestSeed);
		MinDepth = FMath::Min(MinDepth, Depth);
		MaxDepth = FMath::Max(MaxDepth, Depth);
		SurfaceNotSoil += FMOWorldGen::SampleDig(*P, X, Y, Ground - 10.0f, GeoTestSeed).Material != EMODigMaterial::Soil ? 1 : 0;
		DeepSoil += FMOWorldGen::SampleDig(*P, X, Y, Ground - 400.0f, GeoTestSeed).Material == EMODigMaterial::Soil ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("soil depth over 2000 columns: %.0f..%.0f cm"), MinDepth, MaxDepth));
	TestTrue(TEXT("soil depth stays 30..315 cm"), MinDepth >= 30.0f && MaxDepth <= 315.0f);
	TestEqual(TEXT("10 cm under the ground is soil everywhere"), SurfaceNotSoil, 0);
	TestEqual(TEXT("4 m under the ground is rock or ore everywhere"), DeepSoil, 0);

	// 2. A dig at the surface yields earth; its counts add up to floor(volume x items per m3).
	{
		double X = 0.0, Y = 0.0;
		for (int32 Try = 0; Try < 1000; ++Try)
		{
			X = R.FRandRange(-2.0e6f, 2.0e6f); Y = R.FRandRange(-2.0e6f, 2.0e6f);
			if (FMOWorldGen::SampleHeightCm(*P, X, Y, GeoTestSeed, Voxel) > P->Tuning.SeaLevelCm + 500.0f && FMOWorldGen::SoilDepthCm(*P, X, Y, GeoTestSeed) > 100.0f)
			{
				break;
			}
		}
		const FVector Surface(X, Y, FMOWorldGen::SampleHeightCm(*P, X, Y, GeoTestSeed, Voxel));
		TArray<FMOItemYield> Yields;
		FMOGeologyYield::ForDig(*P, GeoTestSeed, Surface, 150.0f, 0.3f, 80.0f, /*bSlab=*/true, FName(TEXT("Dirt01")), Yields);
		int32 Total = 0, Dirt = 0;
		for (const FMOItemYield& Y2 : Yields) { Total += Y2.Count; Dirt += Y2.ItemId == FName(TEXT("Dirt01")) ? Y2.Count : 0; }
		AddInfo(FString::Printf(TEXT("a 0.3 m3 dig at the surface: %s"), *FMOGeologyYield::Describe(Yields)));
		TestEqual(TEXT("a dig's counts add up to floor(0.3 m3 x 80)"), Total, 24);
		TestTrue(TEXT("a shallow dig on deep soil is earth"), Dirt == Total);
	}

	// 3. A pick into a coal seam yields coal; into plain limestone, limestone.
	auto FindDeep = [&](EMOBedrock Want, TFunctionRef<bool(const FMODigSample&)> Pred, FVector& Out)
	{
		for (int32 Try = 0; Try < 4000000; ++Try)
		{
			const double X = R.FRandRange(-3.0e6f, 3.0e6f), Y = R.FRandRange(-3.0e6f, 3.0e6f);
			if (FMOWorldGen::SampleBedrock(*P, X, Y, GeoTestSeed) != Want)
			{
				continue;
			}
			const float Ground = FMOWorldGen::SampleHeightCm(*P, X, Y, GeoTestSeed, Voxel);
			const double Z = Ground - R.FRandRange(800.0f, 5000.0f);
			if (FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Z, Ground, GeoTestSeed, 50.0f) > 200.0f && Pred(FMOWorldGen::SampleDig(*P, X, Y, Z, GeoTestSeed)))
			{
				Out = FVector(X, Y, Z);
				return true;
			}
		}
		return false;
	};
	FVector Coal, Lime;
	if (TestTrue(TEXT("a coal point in the coal measures is found"), FindDeep(EMOBedrock::CoalMeasures, [](const FMODigSample& S) { return S.Ore == EMOOreKind::Coal && S.Grade > 0.6f; }, Coal)))
	{
		TArray<FMOItemYield> Yields;
		FMOGeologyYield::ForDig(*P, GeoTestSeed, Coal, 40.0f, 0.1f, 80.0f, /*bSlab=*/false, NAME_None, Yields);
		const FMOItemYield* Got = Yields.FindByPredicate([](const FMOItemYield& Y2) { return Y2.ItemId == FName(TEXT("Coal01")); });
		AddInfo(FString::Printf(TEXT("a pick into a coal seam at (%.0f, %.0f, %.0f): %s"), Coal.X, Coal.Y, Coal.Z, *FMOGeologyYield::Describe(Yields)));
		TestTrue(TEXT("a pick into a coal seam yields coal (not fill)"), Got && Got->Count > 0 && !Got->bFill);
	}
	if (TestTrue(TEXT("a plain limestone point is found"), FindDeep(EMOBedrock::Limestone, [](const FMODigSample& S) { return S.Material == EMODigMaterial::Rock; }, Lime)))
	{
		TArray<FMOItemYield> Yields;
		FMOGeologyYield::ForDig(*P, GeoTestSeed, Lime, 40.0f, 0.1f, 80.0f, /*bSlab=*/false, NAME_None, Yields);
		const FMOItemYield* Got = Yields.FindByPredicate([](const FMOItemYield& Y2) { return Y2.ItemId == FName(TEXT("Limestone01")); });
		AddInfo(FString::Printf(TEXT("a pick into limestone: %s"), *FMOGeologyYield::Describe(Yields)));
		TestTrue(TEXT("a pick into limestone yields limestone (fill)"), Got && Got->Count > 0 && Got->bFill);
	}
	return true;
}

#endif
