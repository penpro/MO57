// Tests for the geology half of the world generator (bedrock provinces, ore bodies) and the cave-surface finder the cave spawner scatters on.
// Headless-safe: no world, no PIE. The realism rules asserted here are the ones in MOWorldGenGeology.cpp's header (Wes, Oct 17 2026: "I still want realism").

#include "Misc/AutomationTest.h"
#include "MOWorldGen.h"

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
	int32 Ores[4][6] = {};
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
			|| ((O.Kind == EMOOreKind::Hematite || O.Kind == EMOOreKind::Flint) && B == EMOBedrock::Limestone)
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
	static const TCHAR* OreNames[6] = { TEXT("none"), TEXT("coal"), TEXT("hematite"), TEXT("copper"), TEXT("tin"), TEXT("flint") };
	for (int32 B = 0; B < 4; ++B)
	{
		FString Line;
		for (int32 K = 1; K < 6; ++K)
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
		TPair<EMOBedrock, EMOOreKind>(EMOBedrock::Basalt, EMOOreKind::Copper), TPair<EMOBedrock, EMOOreKind>(EMOBedrock::Granite, EMOOreKind::Tin) })
	{
		TestTrue(FString::Printf(TEXT("%s holds %s (> 0.05%% of the rock)"), BedrockName(Need.Key), OreNames[(int32)Need.Value]),
			Ores[(int32)Need.Key][(int32)Need.Value] > 0.0005 * Samples[(int32)Need.Key]);
	}

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
	// The points the cave spawner scatters on: ON the surface, their normal pointing into the air, their kind matching the normal. Find a 64 m cell with caves first.
	const FMOWorldGenParamsRef P = MakeGeoParams();
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

#endif
