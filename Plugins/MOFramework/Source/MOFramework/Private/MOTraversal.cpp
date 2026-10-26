#include "MOTraversal.h"

EMOTraversal FMOTraversal::Classify(float HeightCm, bool bRoomOnTop, bool bRoomBehind)
{
	if (HeightCm < HopMinCm || HeightCm > PullUpMaxCm)
	{
		return EMOTraversal::None;
	}
	if (HeightCm <= HopMaxCm)
	{
		// A thin obstacle with ground just behind it is vaulted; one with room on top is stepped onto.
		return bRoomBehind ? EMOTraversal::Vault : (bRoomOnTop ? EMOTraversal::HopUp : EMOTraversal::None);
	}
	return bRoomOnTop ? EMOTraversal::PullUp : EMOTraversal::None;
}

float FMOTraversal::DurationSeconds(EMOTraversal Kind, float HeightCm)
{
	switch (Kind)
	{
	case EMOTraversal::Vault:
		return 0.55f;
	case EMOTraversal::HopUp:
		return 0.6f + 0.2f * FMath::Clamp((HeightCm - HopMinCm) / (HopMaxCm - HopMinCm), 0.0f, 1.0f);
	case EMOTraversal::PullUp:
		return 1.1f + 0.5f * FMath::Clamp((HeightCm - HopMaxCm) / (PullUpMaxCm - HopMaxCm), 0.0f, 1.0f);
	default:
		return 0.0f;
	}
}

FVector FMOTraversal::Evaluate(const FPath& Path, float Fraction)
{
	const float Lengths[3] = {
		static_cast<float>(FVector::Dist(Path.Points[0], Path.Points[1])),
		static_cast<float>(FVector::Dist(Path.Points[1], Path.Points[2])),
		static_cast<float>(FVector::Dist(Path.Points[2], Path.Points[3])) };
	const float Total = Lengths[0] + Lengths[1] + Lengths[2];
	if (Total <= UE_KINDA_SMALL_NUMBER)
	{
		return Path.Points[3];
	}
	float Along = FMath::Clamp(Fraction, 0.0f, 1.0f) * Total;
	for (int32 Leg = 0; Leg < 3; ++Leg)
	{
		if (Along <= Lengths[Leg] || Leg == 2)
		{
			const float T = Lengths[Leg] > UE_KINDA_SMALL_NUMBER ? FMath::Clamp(Along / Lengths[Leg], 0.0f, 1.0f) : 1.0f;
			return FMath::Lerp(Path.Points[Leg], Path.Points[Leg + 1], T);
		}
		Along -= Lengths[Leg];
	}
	return Path.Points[3];
}

EMOClimbSurface FMOTraversal::ClassifyClimbSurface(bool bTreeTrunkTag, bool bRockTag, bool bClimbableTag, bool bVoxelTerrain, bool bBuilt,
	const FString& MeshPath, float NormalZ)
{
	if (bBuilt)
	{
		return EMOClimbSurface::None;
	}
	if (bTreeTrunkTag)
	{
		return EMOClimbSurface::Tree;
	}
	if (bRockTag || bClimbableTag)
	{
		return EMOClimbSurface::Rock;
	}
	const float SteepNormalZ = FMath::Cos(FMath::DegreesToRadians(MinCliffSlopeDeg));
	if (bVoxelTerrain)
	{
		return NormalZ < SteepNormalZ ? EMOClimbSurface::Rock : EMOClimbSurface::None;
	}
	static const TCHAR* RockWords[] = { TEXT("Rock"), TEXT("Cliff"), TEXT("Boulder"), TEXT("Crag"), TEXT("Stone"), TEXT("Shelf") };
	for (const TCHAR* Word : RockWords)
	{
		if (MeshPath.Contains(Word, ESearchCase::IgnoreCase))
		{
			return EMOClimbSurface::Rock;
		}
	}
	static const TCHAR* TreeWords[] = { TEXT("Tree"), TEXT("Trunk") };
	for (const TCHAR* Word : TreeWords)
	{
		if (MeshPath.Contains(Word, ESearchCase::IgnoreCase))
		{
			return EMOClimbSurface::Tree;
		}
	}
	return EMOClimbSurface::None;
}

float FMOTraversal::ClimbSpeedCm(EMOClimbSurface Surface)
{
	switch (Surface)
	{
	case EMOClimbSurface::Rock:
		return 30.0f;
	case EMOClimbSurface::Tree:
		return 22.0f;
	default:
		return 0.0f;
	}
}
