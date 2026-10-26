// FMOWorldGen: the generator in WORLD space (MOWorldGen.h). Every function adds FMOWorldGenParams::Origin to the positions it takes and takes
// it off the positions it returns, so a world whose start was moved to (0,0) is the same land, shifted -- bit for bit
// (MOFramework.WorldGen.OriginEquivalence). One double addition per coordinate: IEEE-exact, the same on every machine (RULES 1 in the header).

#include "MOWorldGen.h"

namespace
{
	FORCEINLINE double Fx(const FMOWorldGenParams& P, double X) { return X + P.Origin.X; }   // world -> field
	FORCEINLINE double Fy(const FMOWorldGenParams& P, double Y) { return Y + P.Origin.Y; }

	FORCEINLINE void ToWorld(const FMOWorldGenParams& P, FVector2D& V) { V.X -= P.Origin.X; V.Y -= P.Origin.Y; }   // field -> world
	FORCEINLINE void ToWorld(const FMOWorldGenParams& P, FVector& V) { V.X -= P.Origin.X; V.Y -= P.Origin.Y; }

	FBox2D ToField(const FMOWorldGenParams& P, const FBox2D& Area)
	{
		FBox2D Out = Area;
		Out.Min += P.Origin;
		Out.Max += P.Origin;
		return Out;
	}

	void PointsToWorld(const FMOWorldGenParams& P, TArray<FMOCaveSurfacePoint>& Points, int32 FirstNew)
	{
		for (int32 I = FirstNew; I < Points.Num(); ++I)
		{
			ToWorld(P, Points[I].Location);
		}
	}
}

void FMOWorldGen::WorldToCell(const FMOWorldGenParams& Params, double X, double Y, double CellCm, int64& OutCellX, int64& OutCellY)
{
	OutCellX = (int64)FMath::FloorToDouble(Fx(Params, X) / CellCm);
	OutCellY = (int64)FMath::FloorToDouble(Fy(Params, Y) / CellCm);
}

// ---- Caves ----

float FMOWorldGen::SampleCaveDistanceCm(const FMOWorldGenParams& Params, double X, double Y, double Z, int32 Seed, float StepCm)
{
	return FMOWorldGenDomain::SampleCaveDistanceCm(Params, Fx(Params, X), Fy(Params, Y), Z, Seed, StepCm);
}

float FMOWorldGen::SampleCaveDistanceAtGround(const FMOWorldGenParams& Params, double X, double Y, double Z, float GroundCm, int32 Seed, float StepCm)
{
	return FMOWorldGenDomain::SampleCaveDistanceAtGround(Params, Fx(Params, X), Fy(Params, Y), Z, GroundCm, Seed, StepCm);
}

void FMOWorldGen::FindCaveSurfaces(const FMOWorldGenParams& Params, int32 Seed, const FBox2D& Area, float SpacingCm, TArray<FMOCaveSurfacePoint>& Out)
{
	const int32 First = Out.Num();
	FMOWorldGenDomain::FindCaveSurfaces(Params, Seed, ToField(Params, Area), SpacingCm, Out);
	PointsToWorld(Params, Out, First);
}

bool FMOWorldGen::FindNearestCaveMouth(const FMOWorldGenParams& Params, int32 Seed, const FVector2D& From, double MaxRadiusCm, FVector& OutMouth, float& OutSlopeDeg)
{
	const bool bFound = FMOWorldGenDomain::FindNearestCaveMouth(Params, Seed, From + Params.Origin, MaxRadiusCm, OutMouth, OutSlopeDeg);
	if (bFound)
	{
		ToWorld(Params, OutMouth);
	}
	return bFound;
}

void FMOWorldGen::FindOutcrops(const FMOWorldGenParams& Params, int32 Seed, const FBox2D& Area, float SpacingCm, float MinSlopeDeg, TArray<FMOCaveSurfacePoint>& Out)
{
	const int32 First = Out.Num();
	FMOWorldGenDomain::FindOutcrops(Params, Seed, ToField(Params, Area), SpacingCm, MinSlopeDeg, Out);
	PointsToWorld(Params, Out, First);
}

bool FMOWorldGen::GetSinkholeInCell(const FMOWorldGenParams& Params, int64 CellX, int64 CellY, int32 Seed, FMOSinkhole& Out)
{
	const bool bFound = FMOWorldGenDomain::GetSinkholeInCell(Params, CellX, CellY, Seed, Out);
	if (bFound)
	{
		ToWorld(Params, Out.Center);
	}
	return bFound;
}

bool FMOWorldGen::FindSinkholeAt(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, FMOSinkhole& Out)
{
	const bool bFound = FMOWorldGenDomain::FindSinkholeAt(Params, Fx(Params, X), Fy(Params, Y), Seed, Out);
	if (bFound)
	{
		ToWorld(Params, Out.Center);
	}
	return bFound;
}

float FMOWorldGen::DistanceToSinkholeRimZone(const FMOWorldGenParams& Params, double X, double Y, int32 Seed)
{
	return FMOWorldGenDomain::DistanceToSinkholeRimZone(Params, Fx(Params, X), Fy(Params, Y), Seed);
}

float FMOWorldGen::DistanceToCaveEntranceHollow(const FMOWorldGenParams& Params, double X, double Y, int32 Seed)
{
	return FMOWorldGenDomain::DistanceToCaveEntranceHollow(Params, Fx(Params, X), Fy(Params, Y), Seed);
}

// ---- Rock ----

EMOBedrock FMOWorldGen::SampleBedrock(const FMOWorldGenParams& Params, double X, double Y, int32 Seed)
{
	return FMOWorldGenDomain::SampleBedrock(Params, Fx(Params, X), Fy(Params, Y), Seed);
}

FMOOreSample FMOWorldGen::SampleOreIn(const FMOWorldGenParams& Params, EMOBedrock Bedrock, double X, double Y, double Z, int32 Seed)
{
	return FMOWorldGenDomain::SampleOreIn(Params, Bedrock, Fx(Params, X), Fy(Params, Y), Z, Seed);
}

FMOOreSample FMOWorldGen::SampleOre(const FMOWorldGenParams& Params, double X, double Y, double Z, int32 Seed)
{
	return FMOWorldGenDomain::SampleOre(Params, Fx(Params, X), Fy(Params, Y), Z, Seed);
}

float FMOWorldGen::SoilDepthCm(const FMOWorldGenParams& Params, double X, double Y, int32 Seed)
{
	return FMOWorldGenDomain::SoilDepthCm(Params, Fx(Params, X), Fy(Params, Y), Seed);
}

FMODigSample FMOWorldGen::SampleDig(const FMOWorldGenParams& Params, double X, double Y, double Z, int32 Seed, float MinOreGrade)
{
	return FMOWorldGenDomain::SampleDig(Params, Fx(Params, X), Fy(Params, Y), Z, Seed, MinOreGrade);
}

// ---- Ground and climate ----

FMOClimateSample FMOWorldGen::SampleClimate(const FMOWorldGenParams& Params, double X, double Y, int32 Seed)
{
	return FMOWorldGenDomain::SampleClimate(Params, Fx(Params, X), Fy(Params, Y), Seed);
}

float FMOWorldGen::SampleHeightCm(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float StepCm, FMOClimateSample* OutClimate)
{
	return FMOWorldGenDomain::SampleHeightCm(Params, Fx(Params, X), Fy(Params, Y), Seed, StepCm, OutClimate);
}

float FMOWorldGen::SampleBaseHeightCm(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float StepCm, FMOClimateSample* OutClimate)
{
	return FMOWorldGenDomain::SampleBaseHeightCm(Params, Fx(Params, X), Fy(Params, Y), Seed, StepCm, OutClimate);
}

FMOTerrainSample FMOWorldGen::SampleColumn(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float StepCm)
{
	return FMOWorldGenDomain::SampleColumn(Params, Fx(Params, X), Fy(Params, Y), Seed, StepCm);
}

int32 FMOWorldGen::SampleSurfaceBiomeWeights(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float HeightCm, float Moisture, float Temperature, FBiomeWeight* Out, int32 MaxOut)
{
	return FMOWorldGenDomain::SampleSurfaceBiomeWeights(Params, Fx(Params, X), Fy(Params, Y), Seed, HeightCm, Moisture, Temperature, Out, MaxOut);
}

int32 FMOWorldGen::PickSurfaceBiome(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float HeightCm, float SlopeDeg, float Moisture, float Temperature)
{
	return FMOWorldGenDomain::PickSurfaceBiome(Params, Fx(Params, X), Fy(Params, Y), Seed, HeightCm, SlopeDeg, Moisture, Temperature);
}

// ---- Water ----

float FMOWorldGen::ApplyWater(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float BaseHeightCm, float StepCm)
{
	return FMOWorldGenDomain::ApplyWater(Params, Fx(Params, X), Fy(Params, Y), Seed, BaseHeightCm, StepCm);
}

bool FMOWorldGen::GetLakeInCell(const FMOWorldGenParams& Params, int32 Tier, int64 CellX, int64 CellY, int32 Seed, FLake& Out)
{
	const bool bFound = FMOWorldGenDomain::GetLakeInCell(Params, Tier, CellX, CellY, Seed, Out);
	if (bFound)
	{
		ToWorld(Params, Out.Center);
	}
	return bFound;
}

void FMOWorldGen::GetUplandLakesNear(const FMOWorldGenParams& Params, double X, double Y, double RadiusCm, int32 Seed, TArray<FLake>& Out)
{
	const int32 First = Out.Num();
	FMOWorldGenDomain::GetUplandLakesNear(Params, Fx(Params, X), Fy(Params, Y), RadiusCm, Seed, Out);
	for (int32 I = First; I < Out.Num(); ++I)
	{
		ToWorld(Params, Out[I].Center);
	}
}

bool FMOWorldGen::IsInUplandLakeZone(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float ZoneRadii)
{
	return FMOWorldGenDomain::IsInUplandLakeZone(Params, Fx(Params, X), Fy(Params, Y), Seed, ZoneRadii);
}

float FMOWorldGen::UplandLakeShoreDistanceCm(const FMOWorldGenParams& Params, double X, double Y, int32 Seed)
{
	return FMOWorldGenDomain::UplandLakeShoreDistanceCm(Params, Fx(Params, X), Fy(Params, Y), Seed);
}

bool FMOWorldGen::GetWaterSurfaceCm(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float& OutSurfaceCm)
{
	return FMOWorldGenDomain::GetWaterSurfaceCm(Params, Fx(Params, X), Fy(Params, Y), Seed, OutSurfaceCm);
}

EMOWaterBodyKind FMOWorldGen::ClassifyWaterBody(const FMOWorldGenParams& Params, double X, double Y, int32 Seed)
{
	return FMOWorldGenDomain::ClassifyWaterBody(Params, Fx(Params, X), Fy(Params, Y), Seed);
}

float FMOWorldGen::UplandLakeBedWeight(const FMOWorldGenParams& Params, double X, double Y, int32 Seed)
{
	return FMOWorldGenDomain::UplandLakeBedWeight(Params, Fx(Params, X), Fy(Params, Y), Seed);
}

// ---- Spawn ----

bool FMOWorldGen::FindCoastalSpawn(const FMOWorldGenParams& Params, int32 Seed, FMOCoastalSpawn& Out, const FMOCoastalSearch& Search)
{
	const bool bFound = FMOWorldGenDomain::FindCoastalSpawn(Params, Seed, Out, Search);
	if (bFound)
	{
		ToWorld(Params, Out.Location);
	}
	return bFound;
}

bool FMOWorldGen::FindWaterFacingYaw(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float MaxDistanceCm, float& OutYawDegrees)
{
	return FMOWorldGenDomain::FindWaterFacingYaw(Params, Fx(Params, X), Fy(Params, Y), Seed, MaxDistanceCm, OutYawDegrees);
}

int32 FMOWorldGen::MeasureLandmassCells(const FMOWorldGenParams& Params, int32 Seed, double X, double Y, float CellCm, int32 MaxCells)
{
	return FMOWorldGenDomain::MeasureLandmassCells(Params, Seed, Fx(Params, X), Fy(Params, Y), CellCm, MaxCells);
}
