// A built cave entrance's bowls and what stands round them (Wes's votes, Nov 6: "make them non uniform, right now they're like perfect little circles, chain some together"; "keep the gravel from the edges and bias more
// closed geometry rocks around the edges ... smaller nodes scattered around the poi center in cascades"). Read back from the generator: the outlines, the chains, the rim you walk, and the rocks' places.

#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "MOPCGCaveSpawnerSettings.h"
#include "MOBiomeDefinitionRow.h"
#include "Materials/MaterialInterface.h"
#include "MOWorldGen.h"
#include "MOWorldGenSettings.h"
#include "MOWorldSeedSubsystem.h"
#if WITH_EDITOR
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS

namespace MOCaveBowlTest
{
	constexpr int64 CellSpan = 40;

	int32 TestSeed()
	{
		return UMOWorldSeedSubsystem::TerrainSeedFromWorldSeed(670711536);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_CaveBowls,
	"MOFramework.WorldGen.CaveBowls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_CaveBowls::RunTest(const FString& Parameters)
{
	using namespace MOCaveBowlTest;
	const FMOWorldGenParamsRef P = UMOWorldGenSettings::BuildParams();
	const int32 Seed = TestSeed();
	const double Cell = P->Tuning.CaveEntranceCellCm;

	int32 Entrances = 0, Chained = 0, Bowls = 0, MostChained = 0, OutOfCell = 0, Measured = 0, Roundish = 0;
	int32 ChainSizes[6] = {};
	float SmallestRatio = 1.0e6f, RatioSum = 0.0f;
	TMap<FIntPoint, uint32> Shapes;
	TArray<FMOCaveEntranceHollow> Shallow;
	for (int64 Cy = -CellSpan; Cy < CellSpan; ++Cy)
	{
		for (int64 Cx = -CellSpan; Cx < CellSpan; ++Cx)
		{
			FMOCaveEntranceHollow E;
			if (!FMOWorldGen::GetCaveEntranceInCell(*P, Cx, Cy, Seed, E) || E.Kind == EMOCaveEntranceKind::Overhang)
			{
				continue;   // an overhang has no bowls: MOFramework.WorldGen.OverhangsNative
			}
			++Entrances;
			Bowls += E.Bowls.Num();
			if (!TestTrue(TEXT("every entrance has its main bowl first"), E.Bowls.Num() > 0 && E.Bowls[0].Center == E.Center && E.Bowls[0].RadiusCm == E.RadiusCm))
			{
				return false;
			}
			uint32 Shape = 0;
			int32 Beyond = 0;
			for (const FMOCaveEntranceHollow::FBowl& Bowl : E.Bowls)
			{
				Shape = HashCombineFast(Shape, GetTypeHash(Bowl.Center.X) ^ GetTypeHash(Bowl.Center.Y) ^ GetTypeHash(Bowl.RadiusCm) ^ GetTypeHash(Bowl.DepthCm) ^ GetTypeHash(Bowl.GroundCm));
				Beyond += FVector2D::Distance(Bowl.Center, E.Center) > E.RadiusCm ? 1 : 0;
				// The whole of a bowl stays in its entrance's cell: the field only asks one cell.
				for (const FVector2D Way : { FVector2D(1.0, 0.0), FVector2D(-1.0, 0.0), FVector2D(0.0, 1.0), FVector2D(0.0, -1.0) })
				{
					const FVector2D Edge = Bowl.Center + Way * (double)(Bowl.RadiusCm * 1.22f);
					int64 Ex = 0, Ey = 0;
					FMOWorldGen::WorldToCell(*P, Edge.X, Edge.Y, Cell, Ex, Ey);
					OutOfCell += Ex != Cx || Ey != Cy ? 1 : 0;
				}
			}
			Shapes.Add(FIntPoint((int32)Cx, (int32)Cy), Shape);
			Chained += Beyond > 0 ? 1 : 0;
			MostChained = FMath::Max(MostChained, Beyond);
			++ChainSizes[FMath::Min(Beyond, 5)];
			if (E.Kind == EMOCaveEntranceKind::Shallow && Shallow.Num() < 12)
			{
				Shallow.Add(E);
			}
			// The outline round the main bowl: how far out the rim lies by heading. A circle would read 1.00.
			if (Measured < 300)
			{
				++Measured;
				float Near = 1.0e6f, Far = 0.0f;
				for (int32 A = 0; A < 32; ++A)
				{
					const double Cos = FMath::Cos(A * UE_PI / 16.0), Sin = FMath::Sin(A * UE_PI / 16.0);
					float Run = 200.0f;
					while (Run < 9000.0f && FMOWorldGen::CaveBowlShare(*P, E.Center.X + Cos * Run, E.Center.Y + Sin * Run, Seed) <= 1.0f)
					{
						Run += 25.0f;
					}
					Near = FMath::Min(Near, Run);
					Far = FMath::Max(Far, Run);
				}
				const float Ratio = Far / FMath::Max(Near, 1.0f);
				SmallestRatio = FMath::Min(SmallestRatio, Ratio);
				RatioSum += Ratio;
				Roundish += Ratio < 1.2f ? 1 : 0;
			}
		}
	}
	AddInfo(FString::Printf(TEXT("%d entrances: %.1f bowls each, %d with a bowl chained past the main one's rim (most: %d). Chained bowls: 0 x%d, 1 x%d, 2 x%d, 3 x%d, 4 x%d, 5+ x%d"),
		Entrances, Bowls / (float)FMath::Max(Entrances, 1), Chained, MostChained, ChainSizes[0], ChainSizes[1], ChainSizes[2], ChainSizes[3], ChainSizes[4], ChainSizes[5]));
	AddInfo(FString::Printf(TEXT("the rim's farthest over its nearest round the main bowl: mean %.2f, least %.2f (a circle reads 1.00); %d of %d under 1.2"), RatioSum / FMath::Max(Measured, 1), SmallestRatio, Roundish, Measured));
	TestTrue(TEXT("entrances exist"), Entrances > 100);
	TestTrue(TEXT("most entrances chain at least one more bowl"), Chained > Entrances / 2);
	TestTrue(TEXT("some chains run to 3 or more past the main bowl"), MostChained >= 3);
	TestTrue(TEXT("no main bowl reads as a circle: the rim's farthest over its nearest is at least 1.1 in all"), SmallestRatio >= 1.1f);
	TestTrue(TEXT("on average it is well off a circle"), RatioSum / FMath::Max(Measured, 1) >= 1.3f);
	TestEqual(TEXT("every bowl's outline stays in its entrance's cell"), OutOfCell, 0);

	// The switch: one round bowl, the whole entrance otherwise the same.
	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> RoundParams = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>(*P);
	RoundParams->Tuning.bCaveBowlsVaried = false;
	RoundParams->Finalize();
	TestTrue(TEXT("the switch moves the params hash"), RoundParams->GetHash() != P->GetHash());
	int32 RoundChecked = 0, NotOne = 0, NotRound = 0;
	for (int64 Cy = -CellSpan; Cy < CellSpan && RoundChecked < 50; ++Cy)
	{
		for (int64 Cx = -CellSpan; Cx < CellSpan && RoundChecked < 50; ++Cx)
		{
			FMOCaveEntranceHollow E;
			if (FMOWorldGen::GetCaveEntranceInCell(*RoundParams, Cx, Cy, Seed, E) && E.Kind != EMOCaveEntranceKind::Overhang)
			{
				++RoundChecked;
				NotOne += E.Bowls.Num() != 1 ? 1 : 0;
				for (int32 A = 0; A < 8; ++A)
				{
					const double Way = A * UE_PI / 4.0;
					NotRound += !FMath::IsNearlyEqual(FMOWorldGen::CaveBowlShare(*RoundParams, E.Center.X + FMath::Cos(Way) * E.RadiusCm, E.Center.Y + FMath::Sin(Way) * E.RadiusCm, Seed), 1.0f, 0.001f) ? 1 : 0;
				}
			}
		}
	}
	TestTrue(TEXT("the switch: entrances were checked"), RoundChecked > 0);
	TestEqual(TEXT("switched off, an entrance has one bowl"), NotOne, 0);
	TestEqual(TEXT("switched off, its rim is a circle"), NotRound, 0);

	// The same bowls whatever order the cells are asked in (per-thread cache slots are reused).
	int32 Differ = 0;
	for (int64 Cy = CellSpan - 1; Cy >= -CellSpan; --Cy)
	{
		for (int64 Cx = CellSpan - 1; Cx >= -CellSpan; --Cx)
		{
			FMOCaveEntranceHollow E;
			if (FMOWorldGen::GetCaveEntranceInCell(*P, Cx, Cy, Seed, E) && E.Kind != EMOCaveEntranceKind::Overhang)
			{
				uint32 Shape = 0;
				for (const FMOCaveEntranceHollow::FBowl& Bowl : E.Bowls)
				{
					Shape = HashCombineFast(Shape, GetTypeHash(Bowl.Center.X) ^ GetTypeHash(Bowl.Center.Y) ^ GetTypeHash(Bowl.RadiusCm) ^ GetTypeHash(Bowl.DepthCm) ^ GetTypeHash(Bowl.GroundCm));
				}
				const uint32* Was = Shapes.Find(FIntPoint((int32)Cx, (int32)Cy));
				Differ += !Was || *Was != Shape ? 1 : 0;
			}
		}
	}
	TestEqual(TEXT("each cell's bowls are the same whatever order the cells are asked in"), Differ, 0);

	// You walk them: the floor the field gives inside the bowls, by slope. A pawn walks to 44.8 deg.
	int32 InBowls = 0, TooSteep = 0;
	float Steepest = 0.0f;
	for (const FMOCaveEntranceHollow& E : Shallow)
	{
		TArray<FMOCaveSurfacePoint> Points;
		FMOWorldGen::FindCaveSurfaces(*P, Seed, FBox2D(E.Center - FVector2D(7000.0, 7000.0), E.Center + FVector2D(7000.0, 7000.0)), 100.0f, Points);
		for (const FMOCaveSurfacePoint& Point : Points)
		{
			if ((double)Point.GroundCm - Point.Location.Z > 800.0 || FMOWorldGen::CaveBowlShare(*P, Point.Location.X, Point.Location.Y, Seed) > 1.0f)
			{
				continue;   // under the ground (the passage), or the country round the bowl
			}
			const float Slope = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp((float)Point.AirNormal.Z, -1.0f, 1.0f)));
			++InBowls;
			TooSteep += Slope > 44.8f ? 1 : 0;
			Steepest = FMath::Max(Steepest, Slope);
		}
	}
	const float SteepShare = TooSteep / (float)FMath::Max(InBowls, 1);
	AddInfo(FString::Printf(TEXT("%d surface points in %d shallow caves' bowls: %.1f %% steeper than a pawn walks, the steepest %.0f deg"), InBowls, Shallow.Num(), SteepShare * 100.0f, Steepest));
	TestTrue(TEXT("the bowls' surfaces were measured"), InBowls > 1000);
	TestTrue(TEXT("nearly all of a bowl walks"), SteepShare < 0.08f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_CaveBowlRocks,
	"MOFramework.WorldGen.CaveBowlRocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_CaveBowlRocks::RunTest(const FString& Parameters)
{
	using namespace MOCaveBowlTest;
	const FMOWorldGenParamsRef P = UMOWorldGenSettings::BuildParams();
	const int32 Seed = TestSeed();
	const float Voxel = P->Tuning.VoxelSizeCm;
	const auto Field = [&P, Seed, Voxel](double X, double Y, double Z) { return FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Z, FMOWorldGen::SampleHeightCm(*P, X, Y, Seed, Voxel), Seed, Voxel); };

	int32 Checked = 0, Counts[3] = {}, TooFarIn[3] = {}, InTheWay = 0, Floating = 0, Buried = 0, Near = 0, Far = 0;
	double ShareSum[3] = {};
	for (int64 Cy = -CellSpan; Cy < CellSpan && Checked < 40; ++Cy)
	{
		for (int64 Cx = -CellSpan; Cx < CellSpan && Checked < 40; ++Cx)
		{
			FMOCaveEntranceHollow E;
			if (!FMOWorldGen::GetCaveEntranceInCell(*P, Cx, Cy, Seed, E) || E.Kind == EMOCaveEntranceKind::Overhang)
			{
				continue;
			}
			++Checked;
			const FBox2D Area(E.Center - FVector2D(13000.0, 13000.0), E.Center + FVector2D(13000.0, 13000.0));
			TArray<FMOCaveSurfacePoint> Rocks;
			FMOWorldGen::FindCaveBowlRocks(*P, Seed, Area, Rocks);

			// A cut through the area gives the same rocks: each belongs to one PCG cell, and a cell's rocks don't depend on the others.
			const double Cut = E.Center.X + 317.0;
			TArray<FMOCaveSurfacePoint> Left, Right;
			FMOWorldGen::FindCaveBowlRocks(*P, Seed, FBox2D(Area.Min, FVector2D(Cut, Area.Max.Y)), Left);
			FMOWorldGen::FindCaveBowlRocks(*P, Seed, FBox2D(FVector2D(Cut, Area.Min.Y), Area.Max), Right);
			if (!TestEqual(TEXT("two PCG cells' rocks add up to the same rocks"), Left.Num() + Right.Num(), Rocks.Num()))
			{
				return false;
			}
			const FVector2D Way0 = E.Center;
			const float WayTo = E.RadiusCm * 1.3f;
			const float WayRadius = 500.0f;
			for (const FMOCaveSurfacePoint& Rock : Rocks)
			{
				// Only this entrance's: a neighbour's rocks can reach into the area.
				if (FVector2D::Distance(FVector2D(Rock.Location), E.Center) > 12000.0)
				{
					continue;
				}
				const int32 Kind = (int32)Rock.Kind - (int32)EMOCaveSurfaceKind::BowlRockLarge;
				if (!TestTrue(TEXT("a rim rock's kind"), Kind >= 0 && Kind < 3))
				{
					return false;
				}
				const float Share = FMOWorldGen::CaveBowlShare(*P, Rock.Location.X, Rock.Location.Y, Seed);
				++Counts[Kind];
				ShareSum[Kind] += FMath::Min(Share, 5.0f);
				const float Least[3] = { 0.82f, 0.72f, 0.55f };
				TooFarIn[Kind] += Share < Least[Kind] - 0.001f ? 1 : 0;
				const FVector2D Plan(Rock.Location);
				const double Along = FMath::Clamp(FVector2D::DotProduct(Plan - Way0, E.Uphill), 0.0, (double)WayTo);
				InTheWay += FVector2D::Distance(Plan, Way0 + E.Uphill * Along) < WayRadius ? 1 : 0;
				Near += Share <= 1.5f ? 1 : 0;
				Far += Share > 1.5f ? 1 : 0;
				// On the ground as carved: open air a little over it. Large and medium rocks rest on the lowest ground under them, so only "not hanging over air" holds for those.
				const float Ground = FMOWorldGen::SampleHeightCm(*P, Rock.Location.X, Rock.Location.Y, Seed, Voxel);
				Floating += Rock.Location.Z > (double)Ground + 5.0 || (Rock.Location.Z < (double)Ground - 30.0 && Field(Rock.Location.X, Rock.Location.Y, Rock.Location.Z - 30.0) < -40.0f) ? 1 : 0;
				Buried += Rock.Kind == EMOCaveSurfaceKind::BowlRockSmall && Rock.Location.Z < (double)Ground - 30.0 && Field(Rock.Location.X, Rock.Location.Y, Rock.Location.Z + 30.0) > 40.0f ? 1 : 0;
			}
		}
	}
	AddInfo(FString::Printf(TEXT("%d entrances: %d large, %d medium, %d small rocks; mean bowl share %.2f / %.2f / %.2f (1 = the rim); %d within 1.5 x a bowl's radius, %d further out"),
		Checked, Counts[0], Counts[1], Counts[2], ShareSum[0] / FMath::Max(Counts[0], 1), ShareSum[1] / FMath::Max(Counts[1], 1), ShareSum[2] / FMath::Max(Counts[2], 1), Near, Far));
	TestTrue(TEXT("entrances were checked"), Checked >= 20);
	TestTrue(TEXT("every kind of rock stands"), Counts[0] > 0 && Counts[1] > 0 && Counts[2] > 0);
	TestTrue(TEXT("more medium rocks than large, more small than medium: a cascade"), Counts[0] < Counts[1] && Counts[1] < Counts[2]);
	TestEqual(TEXT("no large rock inside its limit of the rim"), TooFarIn[0], 0);
	TestEqual(TEXT("no medium rock inside its limit"), TooFarIn[1], 0);
	TestEqual(TEXT("no small rock inside its limit: the middle stays the gravel's"), TooFarIn[2], 0);
	TestEqual(TEXT("no rock stands in the way in"), InTheWay, 0);
	TestEqual(TEXT("no rock hangs in the air"), Floating, 0);
	TestEqual(TEXT("no small rock lies under the rock"), Buried, 0);
	TestTrue(TEXT("they thin out with distance"), Near > Far);
	return true;
}

// Wes, Nov 6: "the overhangs should be "native" to their biome ... the native biome vegetation and such should go right up to the overhang, it's suppose to give the existing biome some nice variation not be its own little poi type thing
// ... grass and shit on top of the overhang, dirt/rocks/vines underneath". Read back from the generator: no bowl, only the dome carved, on any rock, plants allowed to its lip and over its roof.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_OverhangsNative,
	"MOFramework.WorldGen.OverhangsNative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_OverhangsNative::RunTest(const FString& Parameters)
{
	using namespace MOCaveBowlTest;
	const FMOWorldGenParamsRef P = UMOWorldGenSettings::BuildParams();
	const int32 Seed = TestSeed();
	const float Voxel = P->Tuning.VoxelSizeCm;
	const auto Field = [&P, Seed, Voxel](double X, double Y, double Z, float G) { return FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, Z, G, Seed, Voxel); };

	int32 Overhangs = 0, Cells = 0, Shelters = 0, HasBowls = 0, Checked = 0, Oversized = 0, ChainSizes[6] = {};
	int32 Rocks = 0, NotKarst = 0, KeptOut = 0, OnRoof = 0, RoofSpots = 0, NearLip = 0, LipSpots = 0, Carved = 0, Footprint = 0, Under = 0, UnderSpots = 0;
	for (int64 Cy = -CellSpan; Cy < CellSpan; ++Cy)
	{
		for (int64 Cx = -CellSpan; Cx < CellSpan; ++Cx)
		{
			++Cells;
			FMOCaveEntranceHollow E;
			if (!FMOWorldGen::GetCaveEntranceInCell(*P, Cx, Cy, Seed, E) || E.Kind != EMOCaveEntranceKind::Overhang)
			{
				continue;
			}
			++Overhangs;
			Shelters += E.Shelters.Num();
			++ChainSizes[FMath::Min(E.Shelters.Num(), 5)];
			HasBowls += E.Bowls.Num() > 0 ? 1 : 0;
			NotKarst += FMOWorldGen::SampleBedrock(*P, E.Center.X, E.Center.Y, Seed) != EMOBedrock::Limestone ? 1 : 0;
			if (Checked >= 60)
			{
				continue;
			}
			++Checked;
			const FBox2D Area(E.Center - FVector2D(9000.0, 9000.0), E.Center + FVector2D(9000.0, 9000.0));
			TArray<FMOCaveSurfacePoint> AroundIt;
			FMOWorldGen::FindCaveBowlRocks(*P, Seed, Area, AroundIt);
			for (const FMOCaveSurfacePoint& Rock : AroundIt)
			{
				Rocks += FVector2D::Distance(FVector2D(Rock.Location), E.Center) < 2500.0 ? 1 : 0;   // a neighbour's reach can't come this near
			}
			for (const FMOCaveEntranceHollow::FShelter& S : E.Shelters)
			{
				const FVector2D Across(-S.Uphill.Y, S.Uphill.X);
				// Plants: nothing keeps them out, on the roof over the back half or on the slope beside it.
				for (const float Share : { 0.6f, 0.85f })
				{
					const FVector2D Spot = S.Middle + S.Uphill * (double)(Share * S.HalfDepthCm);
					const float G = FMOWorldGen::SampleHeightCm(*P, Spot.X, Spot.Y, Seed, Voxel);
					++RoofSpots;
					OnRoof += FMOWorldGen::SampleOpeningDistanceUnderCm(*P, Spot.X, Spot.Y, G, Seed) >= 0.0f ? 1 : 0;
					KeptOut += FMOWorldGen::DistanceToSinkholeRimZone(*P, Spot.X, Spot.Y, Seed) <= 0.0f ? 1 : 0;
				}
				// Just past its plan to either side, and in front of its mouth.
				for (const FVector2D Spot : { S.Middle + Across * (double)(S.HalfWidthCm * 1.4f), S.Middle - Across * (double)(S.HalfWidthCm * 1.4f), S.Middle - S.Uphill * (double)(S.HalfDepthCm * 1.4f) })
				{
					const float G = FMOWorldGen::SampleHeightCm(*P, Spot.X, Spot.Y, Seed, Voxel);
					++LipSpots;
					NearLip += FMOWorldGen::SampleOpeningDistanceUnderCm(*P, Spot.X, Spot.Y, G, Seed) >= 0.0f ? 1 : 0;
					KeptOut += FMOWorldGen::DistanceToSinkholeRimZone(*P, Spot.X, Spot.Y, Seed) <= 0.0f ? 1 : 0;
				}
				// Under the roof reads as under it.
				++UnderSpots;
				Under += FMOWorldGen::IsUnderOverhang(*P, S.Middle.X, S.Middle.Y, Seed) ? 1 : 0;
			}
			// How much ground it opens: columns carved 60 cm under the surface, against its domes' plans. A bowl would be several times this.
			float PlanCm2 = 0.0f;
			for (const FMOCaveEntranceHollow::FShelter& S : E.Shelters)
			{
				PlanCm2 += UE_PI * S.HalfDepthCm * S.HalfWidthCm;
			}
			int32 CarvedHere = 0;
			for (int32 Iy = -45; Iy <= 45; ++Iy)
			{
				for (int32 Ix = -45; Ix <= 45; ++Ix)
				{
					const double X = E.Center.X + Ix * 200.0, Y = E.Center.Y + Iy * 200.0;
					const float G = FMOWorldGen::SampleHeightCm(*P, X, Y, Seed, Voxel);
					CarvedHere += Field(X, Y, G - 60.0f, G) < 0.0f && FMOWorldGen::IsUnderOverhang(*P, X, Y, Seed) ? 1 : 0;
				}
			}
			Carved += CarvedHere * 40000;
			Footprint += (int32)PlanCm2;
			Oversized += CarvedHere * 40000.0f > PlanCm2 ? 1 : 0;
		}
	}
	AddInfo(FString::Printf(TEXT("%d overhangs in %d cells (%.1f km2 each: %d of them not on limestone); %.1f shelters each: 1 x%d, 2 x%d, 3 x%d, 4 x%d, 5 x%d"), Overhangs, Cells,
		Cells * FMath::Square(P->Tuning.CaveEntranceCellCm / 100000.0f) / FMath::Max(Overhangs, 1), NotKarst, Shelters / (float)FMath::Max(Overhangs, 1), ChainSizes[1], ChainSizes[2], ChainSizes[3], ChainSizes[4], ChainSizes[5]));
	AddInfo(FString::Printf(TEXT("of %d checked: plants allowed on %d of %d roof spots, %d of %d spots round the lip; ground opened %.0f %% of the domes' plans"), Checked, OnRoof, RoofSpots, NearLip, LipSpots, 100.0f * Carved / (float)FMath::Max(Footprint, 1)));
	TestTrue(TEXT("overhangs stand"), Overhangs > 20);
	TestEqual(TEXT("an overhang has no bowl"), HasBowls, 0);
	TestEqual(TEXT("no rim rocks round an overhang"), Rocks, 0);
	TestTrue(TEXT("native to any rock: some are not on limestone"), NotKarst > 0);
	TestTrue(TEXT("some run to 2-5 shelters along the slope"), ChainSizes[2] + ChainSizes[3] + ChainSizes[4] + ChainSizes[5] > 0);
	TestEqual(TEXT("no keep-out on or round a shelter: the biome's plants come right up"), KeptOut, 0);
	TestTrue(TEXT("nearly all the roof takes plants (a thin lip doesn't)"), OnRoof * 100 >= RoofSpots * 90);
	TestTrue(TEXT("the ground round a shelter is whole"), NearLip * 100 >= LipSpots * 90);
	TestEqual(TEXT("a shelter's middle reads as under the overhang"), Under, UnderSpots);
	TestEqual(TEXT("it opens no more ground than its domes' plans"), Oversized, 0);
	return true;
}

#if WITH_EDITOR
namespace MOClosedMeshTest
{
	/** Edges used by one triangle only on the mesh's source, positions welded to a millimetre (a UV seam is not a hole). -1: no source to measure. */
	int32 BoundaryEdges(const UStaticMesh* Mesh, int32& OutTriangles)
	{
		const FMeshDescription* Description = Mesh ? Mesh->GetMeshDescription(0) : nullptr;
		OutTriangles = Description ? Description->Triangles().Num() : 0;
		if (!Description)
		{
			return -1;
		}
		const FStaticMeshConstAttributes Attributes(*Description);
		const TVertexAttributesConstRef<FVector3f> Positions = Attributes.GetVertexPositions();
		const auto Weld = [&Positions](FVertexID Vertex)
		{
			const FVector3f V = Positions[Vertex];
			return FIntVector(FMath::RoundToInt32(V.X * 10.0f), FMath::RoundToInt32(V.Y * 10.0f), FMath::RoundToInt32(V.Z * 10.0f));
		};
		TMap<TPair<FIntVector, FIntVector>, int32> Edges;
		Edges.Reserve(OutTriangles * 2);
		for (const FTriangleID Triangle : Description->Triangles().GetElementIDs())
		{
			const TArrayView<const FVertexID> Corners = Description->GetTriangleVertices(Triangle);
			const FIntVector Welded[3] = { Weld(Corners[0]), Weld(Corners[1]), Weld(Corners[2]) };
			for (int32 K = 0; K < 3; ++K)
			{
				const FIntVector& A = Welded[K];
				const FIntVector& Other = Welded[(K + 1) % 3];
				if (A == Other)
				{
					continue;
				}
				const bool bOrdered = A.X != Other.X ? A.X < Other.X : A.Y != Other.Y ? A.Y < Other.Y : A.Z < Other.Z;
				++Edges.FindOrAdd(bOrdered ? TPair<FIntVector, FIntVector>(A, Other) : TPair<FIntVector, FIntVector>(Other, A));
			}
		}
		int32 Boundary = 0;
		for (const TPair<TPair<FIntVector, FIntVector>, int32>& Edge : Edges)
		{
			Boundary += Edge.Value == 1 ? 1 : 0;
		}
		return Boundary;
	}

	/** A few stray edges from welding at a millimetre are not an open scan's rim, which runs to hundreds. */
	bool IsClosed(int32 Boundary, int32 Triangles)
	{
		return Boundary >= 0 && Boundary <= FMath::Max(8, Triangles / 20000);
	}

	/** A leaf card or a bark sheet is open by nature: not what this is about (the census's rule). */
	bool IsFoliage(const UStaticMesh* Mesh)
	{
		for (const FStaticMaterial& Slot : Mesh->GetStaticMaterials())
		{
			const FString Name = Slot.MaterialInterface ? Slot.MaterialInterface->GetPathName().ToLower() : FString();
			const UMaterial* Base = Slot.MaterialInterface ? Slot.MaterialInterface->GetMaterial() : nullptr;
			const FString BaseName = Base ? Base->GetPathName().ToLower() : FString();
			for (const TCHAR* Word : { TEXT("foliage"), TEXT("leaf"), TEXT("grass"), TEXT("fern"), TEXT("vine"), TEXT("bark") })
			{
				if (Name.Contains(Word) || BaseName.Contains(Word))
				{
					return true;
				}
			}
		}
		return false;
	}
}

// The biomes' rocks, covers and debris (the census's tags), once Tools/close_rocks.py swap has pointed them at closed copies.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOPCG_BiomeRocksAreClosed,
	"MOFramework.PCG.BiomeRocksAreClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOPCG_BiomeRocksAreClosed::RunTest(const FString& Parameters)
{
	UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/MOFramework/Data/DT_Biomes.DT_Biomes"));
	if (!TestNotNull(TEXT("DT_Biomes"), Table))
	{
		return false;
	}
	const TSet<FName> Tags = { FName(TEXT("MOResource_Rock")), FName(TEXT("MOBiomeCover_Rock")), FName(TEXT("MOBiomeCover_Debris")) };
	TMap<UStaticMesh*, FString> Meshes;
	Table->ForeachRow<FMOBiomeDefinitionRow>(TEXT("BiomeRocksAreClosed"), [&](const FName& Name, const FMOBiomeDefinitionRow& Row)
	{
		for (const TArray<FMOBiomeSpeciesEntry>* List : { &Row.Species, &Row.WaterEdgeSpecies })
		{
			for (const FMOBiomeSpeciesEntry& Entry : *List)
			{
				if (Tags.Contains(Entry.HISMTag))
				{
					if (UStaticMesh* Mesh = Entry.Mesh.LoadSynchronous())
					{
						Meshes.Add(Mesh, Name.ToString());
					}
				}
			}
		}
	});
	TestTrue(TEXT("the biomes place rocks"), Meshes.Num() > 5);
	int32 Measured = 0, Open = 0;
	for (const TPair<UStaticMesh*, FString>& Pair : Meshes)
	{
		if (MOClosedMeshTest::IsFoliage(Pair.Key))
		{
			continue;
		}
		int32 Triangles = 0;
		const int32 Boundary = MOClosedMeshTest::BoundaryEdges(Pair.Key, Triangles);
		++Measured;
		const bool bClosed = MOClosedMeshTest::IsClosed(Boundary, Triangles);
		Open += bClosed ? 0 : 1;
		TestTrue(FString::Printf(TEXT("%s (biome %s) is closed: %d boundary edges of %d triangles. Close it with Tools/close_rocks.py, then swap"), *Pair.Key->GetPathName(), *Pair.Value, Boundary, Triangles), bClosed);
	}
	AddInfo(FString::Printf(TEXT("%d rock meshes measured, %d open"), Measured, Open));
	return true;
}

// Wes, Nov 6: "you're somehow back to using shitty open geometry assets, fix that". A rock or a patch of ground in the cave table must be closed: seen from behind or past a raised edge an open scan is a hole in the world. Plants are open by nature (the vines).
// Boundary edges of the source mesh, positions welded to a millimetre (a UV seam is not a hole). A mesh that fails goes through Tools/close_rocks.py first, and only then into the table.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOPCG_CaveTableIsClosed,
	"MOFramework.PCG.CaveTableIsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOPCG_CaveTableIsClosed::RunTest(const FString& Parameters)
{
	UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/MOFramework/Data/DT_CaveScatter.DT_CaveScatter"));
	if (!TestNotNull(TEXT("DT_CaveScatter"), Table))
	{
		return false;
	}
	TMap<UStaticMesh*, FString> Meshes;
	Table->ForeachRow<FMOCaveScatterRow>(TEXT("CaveTableIsClosed"), [&Meshes](const FName& Name, const FMOCaveScatterRow& Row)
	{
		// A row that places nothing, and the plants, aren't what this is about.
		if (Row.PerSquareMetre <= 0.0f || Row.bHangs || Row.Surface == EMOCaveSurfaceKind::ShelterWall)
		{
			return;
		}
		if (UStaticMesh* Mesh = Row.Mesh.LoadSynchronous())
		{
			Meshes.Add(Mesh, Name.ToString());
		}
	});
	TestTrue(TEXT("the table places rocks"), Meshes.Num() > 5);
	int32 Measured = 0;
	for (const TPair<UStaticMesh*, FString>& Pair : Meshes)
	{
		int32 Triangles = 0;
		const int32 Boundary = MOClosedMeshTest::BoundaryEdges(Pair.Key, Triangles);
		if (!TestTrue(FString::Printf(TEXT("%s: has a source mesh to measure"), *Pair.Value), Boundary >= 0))
		{
			continue;
		}
		++Measured;
		AddInfo(FString::Printf(TEXT("%s (%s): %d triangles, %d boundary edges"), *Pair.Key->GetName(), *Pair.Value, Triangles, Boundary));
		TestTrue(FString::Printf(TEXT("%s (row %s) is closed: %d boundary edges. Close it with Tools/close_rocks.py before it goes in the table"), *Pair.Key->GetName(), *Pair.Value, Boundary), MOClosedMeshTest::IsClosed(Boundary, Triangles));
	}
	AddInfo(FString::Printf(TEXT("%d meshes measured"), Measured));
	return true;
}
#endif // WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOPCG_CaveBowlDressing,
	"MOFramework.PCG.CaveBowlDressing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOPCG_CaveBowlDressing::RunTest(const FString& Parameters)
{
	FMOCaveScatterRow Plain;
	TestEqual(TEXT("a row that says nothing keeps its density in a bowl: the middle"), Plain.BowlWeight(0.0f), 1.0f);
	TestEqual(TEXT("a row that says nothing keeps its density in a bowl: the rim"), Plain.BowlWeight(1.0f), 1.0f);
	FMOCaveScatterRow Pit;
	Pit.BowlMiddleWeight = 2.0f;
	Pit.BowlRimWeight = 0.0f;
	TestEqual(TEXT("the middle's weight out to a quarter of the way"), Pit.BowlWeight(0.2f), 2.0f);
	TestEqual(TEXT("halfway between"), Pit.BowlWeight(0.5f), 1.0f);
	TestEqual(TEXT("the rim's weight from three quarters out"), Pit.BowlWeight(0.8f), 0.0f);

	UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/MOFramework/Data/DT_CaveScatter.DT_CaveScatter"));
	if (!TestNotNull(TEXT("DT_CaveScatter"), Table))
	{
		return false;
	}
	int32 Gravel = 0, GravelOffTheRim = 0;
	double Width[3] = {};
	int32 Rows[3] = {};
	int32 ShelterRows[3] = {};
	int32 Hanging = 0;
	Table->ForeachRow<FMOCaveScatterRow>(TEXT("CaveBowlDressing"), [&](const FName& Name, const FMOCaveScatterRow& Row)
	{
		if (Row.HISMTag == FName(TEXT("MOCave_Floor")) && Row.Surface == EMOCaveSurfaceKind::Floor)
		{
			++Gravel;
			GravelOffTheRim += Row.BowlRimWeight == 0.0f && Row.BowlMiddleWeight >= 1.0f ? 1 : 0;
		}
		if (Row.Surface == EMOCaveSurfaceKind::ShelterFloor || Row.Surface == EMOCaveSurfaceKind::ShelterCeiling || Row.Surface == EMOCaveSurfaceKind::ShelterWall)
		{
			ShelterRows[Row.Surface == EMOCaveSurfaceKind::ShelterFloor ? 0 : Row.Surface == EMOCaveSurfaceKind::ShelterCeiling ? 1 : 2] += Row.Mesh.LoadSynchronous() ? 1 : 0;
			Hanging += Row.Surface == EMOCaveSurfaceKind::ShelterCeiling && Row.bHangs ? 1 : 0;
		}
		else if (Row.Surface >= EMOCaveSurfaceKind::BowlRockLarge)
		{
			const int32 Kind = (int32)Row.Surface - (int32)EMOCaveSurfaceKind::BowlRockLarge;
			if (const UStaticMesh* Mesh = Row.Mesh.LoadSynchronous())
			{
				const FVector Extent = Mesh->GetBounds().BoxExtent;
				++Rows[Kind];
				Width[Kind] += FMath::Max(Extent.X, Extent.Y) * 2.0 * 0.5 * (Row.ScaleMin + Row.ScaleMax);
				AddInfo(FString::Printf(TEXT("%s: kind %d, %.0f cm across on average"), *Name.ToString(), Kind, FMath::Max(Extent.X, Extent.Y) * (Row.ScaleMin + Row.ScaleMax)));
			}
		}
	});
	TestTrue(TEXT("the table has gravel"), Gravel > 0);
	TestEqual(TEXT("all the gravel keeps off a bowl's rim"), GravelOffTheRim, Gravel);
	TestTrue(TEXT("rows for every size of rim rock"), Rows[0] > 0 && Rows[1] > 0 && Rows[2] > 0);
	TestTrue(TEXT("under an overhang: rows for the floor, the ceiling and the walls"), ShelterRows[0] > 0 && ShelterRows[1] > 0 && ShelterRows[2] > 0);
	TestEqual(TEXT("under an overhang: what's on the ceiling hangs from it"), Hanging, ShelterRows[1]);
	if (Rows[0] > 0 && Rows[1] > 0 && Rows[2] > 0)
	{
		TestTrue(TEXT("large rocks are wider than medium ones, and those than small ones"), Width[0] / Rows[0] > Width[1] / Rows[1] && Width[1] / Rows[1] > Width[2] / Rows[2]);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
