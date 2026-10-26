// The world origin (FMOWorldGenParams::Origin, Wes Oct 26: a new world is shifted so its start is the world's (0,0)). The world-space generator
// (FMOWorldGen) must be the SAME land, moved: with origin O, every query at P equals, bit for bit, the unmoved world's answer at P + O, and every
// position it returns is the unmoved one less O. Headless: real DT_Biomes params, no world.

#include "Misc/AutomationTest.h"
#include "MOWorldGen.h"
#include "MOWorldGenSettings.h"
#include "MOWorldSeedSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** This project's params (DT_Biomes, the default shape) with the given origin, built the way the game builds them. Leaves the active origin as it was. */
	FMOWorldGenParamsRef BuildWithOrigin(const FVector2D& Origin)
	{
		const FVector2D Previous = UMOWorldGenSettings::GetActiveWorldOrigin();
		UMOWorldGenSettings::SetActiveWorldOrigin(Origin);
		const FMOWorldGenParamsRef P = UMOWorldGenSettings::BuildParams();
		UMOWorldGenSettings::SetActiveWorldOrigin(Previous);
		return P;
	}

	bool SameBits(float A, float B) { return FMemory::Memcmp(&A, &B, sizeof(float)) == 0; }
	bool SameBits(double A, double B) { return FMemory::Memcmp(&A, &B, sizeof(double)) == 0; }

	/** Counts disagreements per function; reports the first of each. */
	struct FOriginChecker
	{
		FAutomationTestBase& Test;
		TMap<FString, int32> Mismatches;
		int32 Checks = 0;

		explicit FOriginChecker(FAutomationTestBase& InTest) : Test(InTest) {}

		void Check(const TCHAR* What, bool bSame, const FVector2D& Origin, const FVector2D& World)
		{
			++Checks;
			if (!bSame)
			{
				int32& N = Mismatches.FindOrAdd(What);
				if (N++ == 0)
				{
					Test.AddError(FString::Printf(TEXT("%s differs with origin (%.2f, %.2f) at world (%.2f, %.2f)"), What, Origin.X, Origin.Y, World.X, World.Y));
				}
			}
		}

		/** A position the moved world returns must be the unmoved one less the origin (the same subtraction the facade does). */
		void CheckMoved(const TCHAR* What, const FVector2D& Moved, FVector2D Unmoved, const FVector2D& Origin, const FVector2D& World)
		{
			Unmoved.X -= Origin.X;
			Unmoved.Y -= Origin.Y;
			Check(What, SameBits(Moved.X, Unmoved.X) && SameBits(Moved.Y, Unmoved.Y), Origin, World);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_OriginEquivalence,
	"MOFramework.WorldGen.OriginEquivalence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_OriginEquivalence::RunTest(const FString& Parameters)
{
	const int32 Seed = UMOWorldSeedSubsystem::TerrainSeedFromWorldSeed(12345);
	const FMOWorldGenParamsRef Unmoved = BuildWithOrigin(FVector2D::ZeroVector);
	const FMOWorldGenParams& P0 = *Unmoved;
	const double Half = (double)P0.Tuning.WorldHalfSizeCm;

	// Whole metres as the game picks them, both signs, and a fractional one: the equivalence must not depend on the origin being round.
	const FVector2D Origins[] = { FVector2D(3712300.0, -2456700.0), FVector2D(-5123400.0, 4876500.0), FVector2D(1234.5, -987.25) };

	FOriginChecker C(*this);
	for (const FVector2D& O : Origins)
	{
		const FMOWorldGenParamsRef Moved = BuildWithOrigin(O);
		const FMOWorldGenParams& PO = *Moved;

		// The island, where it now lies, and the handshake sees the move.
		TestTrue(TEXT("the island's centre is minus the origin"), PO.GetWorldCentre() == -O);
		TestTrue(TEXT("a point just inside the moved border is inside"), PO.IsInsideWorld(-O.X + Half - 100.0, -O.Y - Half + 100.0));
		TestFalse(TEXT("a point just past the moved border is outside"), PO.IsInsideWorld(-O.X + Half + 100.0, -O.Y));
		TestNotEqual(TEXT("the params hash includes a non-zero origin (host and joiner must agree on it)"), PO.ComputeHash(), P0.ComputeHash());

		// A 7 x 7 grid over the island (in the moved world's coordinates); F is where each point lies in the generator's field.
		for (int32 Iy = 0; Iy < 7; ++Iy)
		{
			for (int32 Ix = 0; Ix < 7; ++Ix)
			{
				const FVector2D W(-O.X + (Ix - 3) * Half / 3.6, -O.Y + (Iy - 3) * Half / 3.6);
				const double Fx = W.X + O.X, Fy = W.Y + O.Y;

				const float H = FMOWorldGen::SampleHeightCm(PO, W.X, W.Y, Seed);
				C.Check(TEXT("SampleHeightCm"), SameBits(H, FMOWorldGen::SampleHeightCm(P0, Fx, Fy, Seed)), O, W);
				const float Base = FMOWorldGen::SampleBaseHeightCm(PO, W.X, W.Y, Seed);
				C.Check(TEXT("SampleBaseHeightCm"), SameBits(Base, FMOWorldGen::SampleBaseHeightCm(P0, Fx, Fy, Seed)), O, W);

				const FMOClimateSample Ca = FMOWorldGen::SampleClimate(PO, W.X, W.Y, Seed), Cb = FMOWorldGen::SampleClimate(P0, Fx, Fy, Seed);
				C.Check(TEXT("SampleClimate"), SameBits(Ca.Continentalness, Cb.Continentalness) && SameBits(Ca.Erosion, Cb.Erosion) && SameBits(Ca.PeaksValleys, Cb.PeaksValleys)
					&& SameBits(Ca.Moisture, Cb.Moisture) && SameBits(Ca.TemperatureSeaLevel, Cb.TemperatureSeaLevel) && SameBits(Ca.EdgeLand, Cb.EdgeLand)
					&& SameBits(Ca.MoistureRegion, Cb.MoistureRegion) && SameBits(Ca.TemperatureRegion, Cb.TemperatureRegion), O, W);

				const FMOTerrainSample Ta = FMOWorldGen::SampleColumn(PO, W.X, W.Y, Seed), Tb = FMOWorldGen::SampleColumn(P0, Fx, Fy, Seed);
				C.Check(TEXT("SampleColumn"), SameBits(Ta.HeightCm, Tb.HeightCm) && SameBits(Ta.Moisture, Tb.Moisture) && SameBits(Ta.Temperature, Tb.Temperature)
					&& Ta.BiomeIndex == Tb.BiomeIndex && SameBits(Ta.Continentalness, Tb.Continentalness) && SameBits(Ta.Erosion, Tb.Erosion), O, W);

				const double Z = H - 3000.0;
				C.Check(TEXT("SampleCaveDistanceCm"), SameBits(FMOWorldGen::SampleCaveDistanceCm(PO, W.X, W.Y, Z, Seed), FMOWorldGen::SampleCaveDistanceCm(P0, Fx, Fy, Z, Seed)), O, W);
				C.Check(TEXT("SampleCaveDistanceAtGround"), SameBits(FMOWorldGen::SampleCaveDistanceAtGround(PO, W.X, W.Y, Z, H, Seed),
					FMOWorldGen::SampleCaveDistanceAtGround(P0, Fx, Fy, Z, H, Seed)), O, W);
				C.Check(TEXT("DistanceToSinkholeRimZone"), SameBits(FMOWorldGen::DistanceToSinkholeRimZone(PO, W.X, W.Y, Seed), FMOWorldGen::DistanceToSinkholeRimZone(P0, Fx, Fy, Seed)), O, W);
				C.Check(TEXT("DistanceToCaveEntranceHollow"), SameBits(FMOWorldGen::DistanceToCaveEntranceHollow(PO, W.X, W.Y, Seed),
					FMOWorldGen::DistanceToCaveEntranceHollow(P0, Fx, Fy, Seed)), O, W);

				C.Check(TEXT("SampleBedrock"), FMOWorldGen::SampleBedrock(PO, W.X, W.Y, Seed) == FMOWorldGen::SampleBedrock(P0, Fx, Fy, Seed), O, W);
				const FMOOreSample Oa = FMOWorldGen::SampleOre(PO, W.X, W.Y, Z, Seed), Ob = FMOWorldGen::SampleOre(P0, Fx, Fy, Z, Seed);
				C.Check(TEXT("SampleOre"), Oa.Kind == Ob.Kind && SameBits(Oa.Grade, Ob.Grade), O, W);
				C.Check(TEXT("SoilDepthCm"), SameBits(FMOWorldGen::SoilDepthCm(PO, W.X, W.Y, Seed), FMOWorldGen::SoilDepthCm(P0, Fx, Fy, Seed)), O, W);
				const FMODigSample Da = FMOWorldGen::SampleDig(PO, W.X, W.Y, Z, Seed), Db = FMOWorldGen::SampleDig(P0, Fx, Fy, Z, Seed);
				C.Check(TEXT("SampleDig"), Da.Material == Db.Material && Da.Bedrock == Db.Bedrock && Da.Ore == Db.Ore && SameBits(Da.Grade, Db.Grade), O, W);

				C.Check(TEXT("ApplyWater"), SameBits(FMOWorldGen::ApplyWater(PO, W.X, W.Y, Seed, Base, 100.0f), FMOWorldGen::ApplyWater(P0, Fx, Fy, Seed, Base, 100.0f)), O, W);
				C.Check(TEXT("IsInUplandLakeZone"), FMOWorldGen::IsInUplandLakeZone(PO, W.X, W.Y, Seed) == FMOWorldGen::IsInUplandLakeZone(P0, Fx, Fy, Seed), O, W);
				C.Check(TEXT("UplandLakeShoreDistanceCm"), SameBits(FMOWorldGen::UplandLakeShoreDistanceCm(PO, W.X, W.Y, Seed), FMOWorldGen::UplandLakeShoreDistanceCm(P0, Fx, Fy, Seed)), O, W);
				float Sa = 0.0f, Sb = 0.0f;
				const bool bSa = FMOWorldGen::GetWaterSurfaceCm(PO, W.X, W.Y, Seed, Sa), bSb = FMOWorldGen::GetWaterSurfaceCm(P0, Fx, Fy, Seed, Sb);
				C.Check(TEXT("GetWaterSurfaceCm"), bSa == bSb && SameBits(Sa, Sb), O, W);
				C.Check(TEXT("ClassifyWaterBody"), FMOWorldGen::ClassifyWaterBody(PO, W.X, W.Y, Seed) == FMOWorldGen::ClassifyWaterBody(P0, Fx, Fy, Seed), O, W);
				C.Check(TEXT("UplandLakeBedWeight"), SameBits(FMOWorldGen::UplandLakeBedWeight(PO, W.X, W.Y, Seed), FMOWorldGen::UplandLakeBedWeight(P0, Fx, Fy, Seed)), O, W);

				C.Check(TEXT("PickSurfaceBiome"), FMOWorldGen::PickSurfaceBiome(PO, W.X, W.Y, Seed, H, 5.0f, Ta.Moisture, Ta.Temperature)
					== FMOWorldGen::PickSurfaceBiome(P0, Fx, Fy, Seed, H, 5.0f, Ta.Moisture, Ta.Temperature), O, W);
				FMOWorldGen::FBiomeWeight Wa[8], Wb[8];
				const int32 Na = FMOWorldGen::SampleSurfaceBiomeWeights(PO, W.X, W.Y, Seed, H, Ta.Moisture, Ta.Temperature, Wa, 8);
				const int32 Nb = FMOWorldGen::SampleSurfaceBiomeWeights(P0, Fx, Fy, Seed, H, Ta.Moisture, Ta.Temperature, Wb, 8);
				bool bWeights = Na == Nb;
				for (int32 I = 0; bWeights && I < Na; ++I)
				{
					bWeights = Wa[I].Biome == Wb[I].Biome && SameBits(Wa[I].Weight, Wb[I].Weight);
				}
				C.Check(TEXT("SampleSurfaceBiomeWeights"), bWeights, O, W);

				float Ya = 0.0f, Yb = 0.0f;
				const bool bYa = FMOWorldGen::FindWaterFacingYaw(PO, W.X, W.Y, Seed, 30000.0f, Ya), bYb = FMOWorldGen::FindWaterFacingYaw(P0, Fx, Fy, Seed, 30000.0f, Yb);
				C.Check(TEXT("FindWaterFacingYaw"), bYa == bYb && SameBits(Ya, Yb), O, W);

				// Positions it hands back.
				FMOSinkhole Ka, Kb;
				const bool bKa = FMOWorldGen::FindSinkholeAt(PO, W.X, W.Y, Seed, Ka), bKb = FMOWorldGen::FindSinkholeAt(P0, Fx, Fy, Seed, Kb);
				C.Check(TEXT("FindSinkholeAt (found)"), bKa == bKb && SameBits(Ka.RadiusCm, Kb.RadiusCm) && SameBits(Ka.GroundCm, Kb.GroundCm), O, W);
				if (bKa && bKb)
				{
					C.CheckMoved(TEXT("FindSinkholeAt (centre)"), Ka.Center, Kb.Center, O, W);
				}

				int64 Cxa = 0, Cya = 0, Cxb = 0, Cyb = 0;
				const double SinkCell = (double)P0.Tuning.SinkholeCellCm;
				FMOWorldGen::WorldToCell(PO, W.X, W.Y, SinkCell, Cxa, Cya);
				FMOWorldGen::WorldToCell(P0, Fx, Fy, SinkCell, Cxb, Cyb);
				C.Check(TEXT("WorldToCell"), Cxa == Cxb && Cya == Cyb, O, W);
				const bool bCa = FMOWorldGen::GetSinkholeInCell(PO, Cxa, Cya, Seed, Ka), bCb = FMOWorldGen::GetSinkholeInCell(P0, Cxb, Cyb, Seed, Kb);
				C.Check(TEXT("GetSinkholeInCell (found)"), bCa == bCb, O, W);
				if (bCa && bCb)
				{
					C.CheckMoved(TEXT("GetSinkholeInCell (centre)"), Ka.Center, Kb.Center, O, W);
				}

				int64 Lxa = 0, Lya = 0, Lxb = 0, Lyb = 0;
				FMOWorldGen::WorldToCell(PO, W.X, W.Y, (double)P0.Tuning.UplandLakeCellCm, Lxa, Lya);
				FMOWorldGen::WorldToCell(P0, Fx, Fy, (double)P0.Tuning.UplandLakeCellCm, Lxb, Lyb);
				FMOWorldGen::FLake La, Lb;
				const bool bLa = FMOWorldGen::GetLakeInCell(PO, 2, Lxa, Lya, Seed, La), bLb = FMOWorldGen::GetLakeInCell(P0, 2, Lxb, Lyb, Seed, Lb);
				C.Check(TEXT("GetLakeInCell (found)"), bLa == bLb && La.CellX == Lb.CellX && La.CellY == Lb.CellY && SameBits(La.RadiusCm, Lb.RadiusCm) && SameBits(La.SurfaceCm, Lb.SurfaceCm), O, W);
				if (bLa && bLb)
				{
					C.CheckMoved(TEXT("GetLakeInCell (centre)"), La.Center, Lb.Center, O, W);
				}

				TArray<FMOWorldGen::FLake> Near, NearField;
				FMOWorldGen::GetUplandLakesNear(PO, W.X, W.Y, 300000.0, Seed, Near);
				FMOWorldGen::GetUplandLakesNear(P0, Fx, Fy, 300000.0, Seed, NearField);
				C.Check(TEXT("GetUplandLakesNear (count)"), Near.Num() == NearField.Num(), O, W);
				for (int32 I = 0; I < FMath::Min(Near.Num(), NearField.Num()); ++I)
				{
					C.CheckMoved(TEXT("GetUplandLakesNear (centre)"), Near[I].Center, NearField[I].Center, O, W);
				}
			}
		}

		// Searches over an area: the nearest cave mouth to the island's centre, the cave surfaces and outcrops around it, the landmass there.
		{
			const FVector2D W = -O;
			const FVector2D F = W + O;
			FVector Ma, Mb;
			float SlopeA = 0.0f, SlopeB = 0.0f;
			const bool bMa = FMOWorldGen::FindNearestCaveMouth(PO, Seed, W, 2500000.0, Ma, SlopeA);
			const bool bMb = FMOWorldGen::FindNearestCaveMouth(P0, Seed, F, 2500000.0, Mb, SlopeB);
			C.Check(TEXT("FindNearestCaveMouth (found)"), bMa == bMb && SameBits(SlopeA, SlopeB) && SameBits(Ma.Z, Mb.Z), O, W);
			TestTrue(TEXT("this seed has a cave mouth within 25 km of the island's centre (else the cave checks below prove nothing)"), bMb);
			if (bMa && bMb)
			{
				C.CheckMoved(TEXT("FindNearestCaveMouth (mouth)"), FVector2D(Ma), FVector2D(Mb), O, W);

				const FVector2D MouthWorld(Ma.X, Ma.Y);
				const FBox2D AreaWorld(MouthWorld - FVector2D(20000.0), MouthWorld + FVector2D(20000.0));
				FBox2D AreaField = AreaWorld;
				AreaField.Min += O;
				AreaField.Max += O;
				TArray<FMOCaveSurfacePoint> Sa, Sb;
				FMOWorldGen::FindCaveSurfaces(PO, Seed, AreaWorld, 400.0f, Sa);
				FMOWorldGen::FindCaveSurfaces(P0, Seed, AreaField, 400.0f, Sb);
				C.Check(TEXT("FindCaveSurfaces (count)"), Sa.Num() == Sb.Num(), O, MouthWorld);
				TestTrue(TEXT("there are cave surfaces around the mouth (else the check proves nothing)"), Sb.Num() > 0);
				for (int32 I = 0; I < FMath::Min(Sa.Num(), Sb.Num()); ++I)
				{
					C.CheckMoved(TEXT("FindCaveSurfaces (point)"), FVector2D(Sa[I].Location), FVector2D(Sb[I].Location), O, MouthWorld);
					C.Check(TEXT("FindCaveSurfaces (point z, kind)"), SameBits(Sa[I].Location.Z, Sb[I].Location.Z) && Sa[I].Kind == Sb[I].Kind, O, MouthWorld);
				}

				TArray<FMOCaveSurfacePoint> Ra, Rb;
				FMOWorldGen::FindOutcrops(PO, Seed, AreaWorld, 400.0f, 35.0f, Ra);
				FMOWorldGen::FindOutcrops(P0, Seed, AreaField, 400.0f, 35.0f, Rb);
				C.Check(TEXT("FindOutcrops (count)"), Ra.Num() == Rb.Num(), O, MouthWorld);
				for (int32 I = 0; I < FMath::Min(Ra.Num(), Rb.Num()); ++I)
				{
					C.CheckMoved(TEXT("FindOutcrops (point)"), FVector2D(Ra[I].Location), FVector2D(Rb[I].Location), O, MouthWorld);
				}
			}

			C.Check(TEXT("MeasureLandmassCells"), FMOWorldGen::MeasureLandmassCells(PO, Seed, W.X, W.Y, 50000.0f, 256) == FMOWorldGen::MeasureLandmassCells(P0, Seed, F.X, F.Y, 50000.0f, 256), O, W);
		}

		// The new game's coastal start: the same shore, moved.
		{
			FMOCoastalSpawn Ca, Cb;
			const bool bCa = FMOWorldGen::FindCoastalSpawn(PO, Seed, Ca), bCb = FMOWorldGen::FindCoastalSpawn(P0, Seed, Cb);
			C.Check(TEXT("FindCoastalSpawn (found)"), bCa == bCb && SameBits(Ca.GroundCm, Cb.GroundCm) && SameBits(Ca.YawDegrees, Cb.YawDegrees)
				&& SameBits(Ca.OpenWaterCm, Cb.OpenWaterCm) && SameBits(Ca.Score, Cb.Score), O, Ca.Location);
			TestTrue(TEXT("this seed has a coastal start"), bCb);
			if (bCa && bCb)
			{
				C.CheckMoved(TEXT("FindCoastalSpawn (location)"), Ca.Location, Cb.Location, O, Ca.Location);
			}
		}
	}

	AddInfo(FString::Printf(TEXT("%d checks over %d origins; %d functions disagreed"), C.Checks, UE_ARRAY_COUNT(Origins), C.Mismatches.Num()));
	return true;
}

/** Choosing the origin the way a new game does: the coastal start moved to (0,0) is a coastal start at (0,0), the same shore. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_OriginAtCoastalStart,
	"MOFramework.WorldGen.OriginAtCoastalStart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_OriginAtCoastalStart::RunTest(const FString& Parameters)
{
	const int32 Seed = UMOWorldSeedSubsystem::TerrainSeedFromWorldSeed(12345);
	const FMOWorldGenParamsRef Unmoved = BuildWithOrigin(FVector2D::ZeroVector);
	FMOCoastalSpawn Field;
	if (!TestTrue(TEXT("this seed has a coastal start"), FMOWorldGen::FindCoastalSpawn(*Unmoved, Seed, Field)))
	{
		return false;
	}
	AddInfo(FString::Printf(TEXT("the unmoved coastal start: (%.0f, %.0f), %.2f km from the island's centre"), Field.Location.X, Field.Location.Y, Field.Location.Size() / 100000.0));
	TestTrue(TEXT("the unmoved start is off the origin (else the move proves nothing)"), Field.Location.Size() > 10000.0);

	// Whole metres, as AMOGameMode::ChooseNewWorldOrigin picks it.
	const FVector2D Origin(FMath::RoundToDouble(Field.Location.X / 100.0) * 100.0, FMath::RoundToDouble(Field.Location.Y / 100.0) * 100.0);
	const FMOWorldGenParamsRef Moved = BuildWithOrigin(Origin);
	FMOCoastalSpawn Start;
	TestTrue(TEXT("the moved world has the coastal start"), FMOWorldGen::FindCoastalSpawn(*Moved, Seed, Start));
	TestTrue(FString::Printf(TEXT("the start is within half a metre of (0,0) (%.1f, %.1f)"), Start.Location.X, Start.Location.Y),
		FMath::Abs(Start.Location.X) <= 50.0 && FMath::Abs(Start.Location.Y) <= 50.0);
	TestEqual(TEXT("the same ground"), Start.GroundCm, Field.GroundCm);
	TestEqual(TEXT("the same view across the water"), Start.YawDegrees, Field.YawDegrees);
	TestEqual(TEXT("the ground at the start is the ground at the unmoved start"), FMOWorldGen::SampleHeightCm(*Moved, Start.Location.X, Start.Location.Y, Seed),
		FMOWorldGen::SampleHeightCm(*Unmoved, Field.Location.X, Field.Location.Y, Seed));
	TestTrue(TEXT("the start is inside the moved world"), Moved->IsInsideWorld(Start.Location.X, Start.Location.Y, 50000.0));
	return true;
}

#endif
