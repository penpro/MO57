#include "MOPCGWaterMask.h"
#include "MOWorldGenSettings.h"
#include "MOWorldSeedSubsystem.h"
#include "GameFramework/Actor.h"

FMOPCGWaterMask FMOPCGWaterMask::ForActor(const AActor* Actor, float InBankRadii)
{
	const UMOWorldSeedSubsystem* Seeds = Actor ? UMOWorldSeedSubsystem::Get(Actor) : nullptr;
	if (!Seeds || !Seeds->HasActiveSeed())
	{
		return FMOPCGWaterMask();
	}
	// The same refresh the spawners do: picks up DT / tuning edits made since the last generation and publishes them.
	return FMOPCGWaterMask(UMOWorldGenSettings::RefreshPublishedParams(), Seeds->GetActiveTerrainSeed(), InBankRadii);
}

bool FMOPCGWaterMask::IsInCave(double X, double Y, double Z) const
{
	if (!bActive || !Params.IsSet())
	{
		return false;
	}
	const FMOWorldGenParams& P = *Params.GetValue();
	if (!P.Tuning.bEnableCaves)
	{
		return false;
	}
	if (FMOWorldGen::DistanceToSinkholeRimZone(P, X, Y, TerrainSeed) <= 0.0f)
	{
		return true;
	}
	return Z < (double)FMOWorldGen::SampleHeightCm(P, X, Y, TerrainSeed, P.Tuning.VoxelSizeCm) - 200.0;
}

bool FMOPCGWaterMask::IsInWater(double X, double Y, double GroundZ) const
{
	if (!bActive || !Params.IsSet())
	{
		return false;
	}
	const FMOWorldGenParams& P = *Params.GetValue();
	if (!P.Tuning.bEnableWater)
	{
		return false;
	}
	// The sea, the rivers, the ponds and the sea-level lakes are all terrain below the sea plane.
	if (GroundZ < (double)P.Tuning.SeaLevelCm + 15.0)
	{
		return true;
	}
	return P.Tuning.bEnableUplandLakes && FMOWorldGen::IsInUplandLakeZone(P, X, Y, TerrainSeed, BankRadii);
}

float FMOPCGWaterMask::ShoreFactor(double X, double Y, double GroundZ, double MaxHeightCm, double MaxDistCm) const
{
	if (!bActive || !Params.IsSet() || MaxHeightCm <= 0.0 || IsInWater(X, Y, GroundZ))
	{
		return 0.0f;
	}
	const FMOWorldGenParams& P = *Params.GetValue();
	const FMOWorldGenTuning& T = P.Tuning;
	if (!T.bEnableWater)
	{
		return 0.0f;
	}
	// An upland lake close by: its own surface is the water level, its ellipse measures the distance.
	if (T.bEnableUplandLakes)
	{
		const double Cell = (double)T.UplandLakeCellCm;
		FMOWorldGen::FLake L;
		if (FMOWorldGen::GetLakeInCell(P, 2, (int64)FMath::FloorToDouble(X / Cell), (int64)FMath::FloorToDouble(Y / Cell), TerrainSeed, L))
		{
			const double Dx = X - L.Center.X, Dy = Y - L.Center.Y;
			const double Lx = ((double)L.CosAngle * Dx + (double)L.SinAngle * Dy) / (double)L.RadiusCm;
			const double Ly = (-(double)L.SinAngle * Dx + (double)L.CosAngle * Dy) / ((double)L.RadiusCm * (double)L.Aspect);
			const double Q = FMath::Sqrt(Lx * Lx + Ly * Ly);
			const double Height = GroundZ - (double)L.SurfaceCm;
			if (Height >= 0.0 && Height <= MaxHeightCm && (Q - 1.0) * (double)L.RadiusCm * (double)L.Aspect <= MaxDistCm)
			{
				return (float)(1.0 - Height / MaxHeightCm);
			}
		}
	}
	// The sea, a river or a pond: ground within the band above the sea plane AND water (ground under the plane) within MaxDistCm in some direction.
	const double Height = GroundZ - (double)T.SeaLevelCm;
	if (Height < 0.0 || Height > MaxHeightCm)
	{
		return 0.0f;
	}
	for (int32 K = 0; K < 8; ++K)
	{
		const double A = (double)K * UE_PI / 4.0;
		if (FMOWorldGen::SampleHeightCm(P, X + MaxDistCm * FMath::Cos(A), Y + MaxDistCm * FMath::Sin(A), TerrainSeed, T.VoxelSizeCm) < T.SeaLevelCm + 15.0f)
		{
			return (float)(1.0 - Height / MaxHeightCm);
		}
	}
	return 0.0f;
}

bool FMOPCGWaterMask::MayHaveShore(double MinX, double MinY, double MaxX, double MaxY, double MarginCm) const
{
	if (!bActive || !Params.IsSet())
	{
		return false;
	}
	const FMOWorldGenParams& P = *Params.GetValue();
	const FMOWorldGenTuning& T = P.Tuning;
	if (!T.bEnableWater)
	{
		return false;
	}
	MinX -= MarginCm; MinY -= MarginCm; MaxX += MarginCm; MaxY += MarginCm;
	if (T.bEnableUplandLakes)
	{
		// Lakes whose centre is within the box's circumscribed circle plus the biggest lake radius there is (a few hundred metres).
		TArray<FMOWorldGen::FLake> Lakes;
		const double Cx = 0.5 * (MinX + MaxX), Cy = 0.5 * (MinY + MaxY);
		FMOWorldGen::GetUplandLakesNear(P, Cx, Cy, 0.5 * FMath::Sqrt(FMath::Square(MaxX - MinX) + FMath::Square(MaxY - MinY)) + 20000.0, TerrainSeed, Lakes);
		if (Lakes.Num() > 0)
		{
			return true;
		}
	}
	const double Step = 1000.0;
	for (double Y = MinY; Y <= MaxY + Step * 0.5; Y += Step)
	{
		for (double X = MinX; X <= MaxX + Step * 0.5; X += Step)
		{
			if (FMOWorldGen::SampleHeightCm(P, X, Y, TerrainSeed, T.VoxelSizeCm) < T.SeaLevelCm + 15.0f)
			{
				return true;
			}
		}
	}
	return false;
}

int32 FMOPCGWaterMask::BiomeAt(double X, double Y, double GroundZ, float SlopeDeg) const
{
	if (!bActive || !Params.IsSet())
	{
		return INDEX_NONE;
	}
	const FMOWorldGenParams& P = *Params.GetValue();
	const FMOClimateSample C = FMOWorldGen::SampleClimate(P, X, Y, TerrainSeed);
	return FMOWorldGen::ResolveBiome(P, (float)GroundZ, SlopeDeg, C.Moisture, FMOWorldGen::TemperatureAtHeight(P, C.TemperatureSeaLevel, (float)GroundZ));
}

int32 FMOPCGWaterMask::FindBiome(FName BiomeId) const
{
	return (bActive && Params.IsSet()) ? Params.GetValue()->FindBiomeIndex(BiomeId) : INDEX_NONE;
}
