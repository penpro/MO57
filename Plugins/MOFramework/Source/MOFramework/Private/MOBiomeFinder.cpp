#include "MOBiomeFinder.h"

FMOBiomeFinder::FMOBiomeFinder(const FMOWorldGenParamsRef& InParams, int32 InTerrainSeed)
	: Params(InParams), TerrainSeed(InTerrainSeed), Water(InParams, InTerrainSeed, 1.3f)
{
}

int32 FMOBiomeFinder::BiomeAt(double X, double Y, float& OutGround) const
{
	float SlopeDeg = 0.0f;
	return BiomeAt(X, Y, OutGround, SlopeDeg);
}

int32 FMOBiomeFinder::BiomeAt(double X, double Y, float& OutGround, float& OutSlopeDeg) const
{
	const FMOWorldGenParams& P = *Params;
	const float Step = P.Tuning.VoxelSizeCm;
	const FMOTerrainSample S = FMOWorldGen::SampleColumn(P, X, Y, TerrainSeed, Step);
	OutGround = S.HeightCm;
	const float Dx = FMOWorldGen::SampleHeightCm(P, X + 200.0, Y, TerrainSeed, Step) - FMOWorldGen::SampleHeightCm(P, X - 200.0, Y, TerrainSeed, Step);
	const float Dy = FMOWorldGen::SampleHeightCm(P, X, Y + 200.0, TerrainSeed, Step) - FMOWorldGen::SampleHeightCm(P, X, Y - 200.0, TerrainSeed, Step);
	OutSlopeDeg = FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(Dx * Dx + Dy * Dy) / 400.0f));
	return FMOWorldGen::ResolveBiome(P, S.HeightCm, OutSlopeDeg, S.Moisture, S.Temperature);
}

bool FMOBiomeFinder::IsDeepInside(double X, double Y, int32 BiomeIndex, bool bRough, float& OutGround) const
{
	if (BiomeAt(X, Y, OutGround) != BiomeIndex || OutGround <= Params->Tuning.SeaLevelCm + 80.0f || Water.IsInWater(X, Y, OutGround))
	{
		return false;
	}
	for (int32 K = 0; K < 8; ++K)
	{
		const double Ax = X + 3500.0 * FMath::Cos(K * UE_PI / 4.0), Ay = Y + 3500.0 * FMath::Sin(K * UE_PI / 4.0);
		float RingGround = 0.0f;
		if (BiomeAt(Ax, Ay, RingGround) != BiomeIndex || Water.IsInWater(Ax, Ay, RingGround) || (!bRough && FMath::Abs(RingGround - OutGround) >= 350.0f))
		{
			return false;
		}
	}
	return true;
}

bool FMOBiomeFinder::IsSlopeBanded(int32 BiomeIndex) const
{
	return Params->Biomes.IsValidIndex(BiomeIndex) && Params->Biomes[BiomeIndex].SlopeMinDeg > 0.0f;
}

bool FMOBiomeFinder::FindNearest(const FVector2D& From, int32 BiomeIndex, double MaxRadiusCm, bool bRough, FVector& OutSpot, int32* OutSamples, double CoarseStepCm) const
{
	if (!Params->Biomes.IsValidIndex(BiomeIndex))
	{
		return false;
	}
	const double Limit = Params->Tuning.WorldHalfSizeCm - 50000.0;   // 500 m inside the world border
	const double Step = FMath::Max(CoarseStepCm, 10000.0);
	const int32 Rings = FMath::CeilToInt(MaxRadiusCm / Step);
	const bool bSlopeBanded = IsSlopeBanded(BiomeIndex);
	TSet<FIntPoint> Refined;   // 1 km cells already searched finely
	int32 Samples = 0;
	TArray<FVector2D> Ring;
	for (int32 R = 0; R <= Rings; ++R)
	{
		// The square ring of Chebyshev radius R, nearest points first (rings are only roughly round; within one the order is by real distance).
		Ring.Reset();
		for (int32 I = -R; I <= R; ++I)
		{
			for (int32 J = -R; J <= R; ++J)
			{
				if (FMath::Max(FMath::Abs(I), FMath::Abs(J)) == R)
				{
					const FVector2D P(From.X + I * Step, From.Y + J * Step);
					if (FMath::Abs(P.X) <= Limit && FMath::Abs(P.Y) <= Limit)
					{
						Ring.Add(P);
					}
				}
			}
		}
		Ring.Sort([&From](const FVector2D& A, const FVector2D& B) { return FVector2D::DistSquared(A, From) < FVector2D::DistSquared(B, From); });
		for (const FVector2D& C : Ring)
		{
			++Samples;
			// Cheap test first: the column's biome at slope 0 (a slope-banded biome needs the real slope).
			float Ground = 0.0f;
			const int32 Coarse = bSlopeBanded ? BiomeAt(C.X, C.Y, Ground) : FMOWorldGen::SampleColumn(*Params, C.X, C.Y, TerrainSeed, Params->Tuning.VoxelSizeCm).BiomeIndex;
			if (Coarse != BiomeIndex)
			{
				continue;
			}
			const FIntPoint Cell(FMath::FloorToInt(C.X / 100000.0), FMath::FloorToInt(C.Y / 100000.0));
			if (Refined.Contains(Cell))
			{
				continue;
			}
			Refined.Add(Cell);
			// A 100 m grid over +-600 m around the hit, nearest to From first: the first spot deep inside wins.
			TArray<FVector2D> Fine;
			for (int32 I = -6; I <= 6; ++I)
			{
				for (int32 J = -6; J <= 6; ++J)
				{
					Fine.Add(FVector2D(C.X + I * 10000.0, C.Y + J * 10000.0));
				}
			}
			Fine.Sort([&From](const FVector2D& A, const FVector2D& B) { return FVector2D::DistSquared(A, From) < FVector2D::DistSquared(B, From); });
			for (const FVector2D& F : Fine)
			{
				if (FMath::Abs(F.X) <= Limit && FMath::Abs(F.Y) <= Limit && IsDeepInside(F.X, F.Y, BiomeIndex, bRough, Ground))
				{
					OutSpot = FVector(F.X, F.Y, Ground);
					if (OutSamples)
					{
						*OutSamples = Samples;
					}
					return true;
				}
			}
		}
	}
	if (OutSamples)
	{
		*OutSamples = Samples;
	}
	return false;
}
