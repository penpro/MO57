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

bool FMOTraversal::IsRockTag(FName Tag)
{
	static const FName RockTags[] = { TEXT("MOLandmark"), TEXT("MOResource_Rock"), TEXT("MOBiomeCover_Rock") };
	for (const FName& Rock : RockTags)
	{
		if (Tag == Rock)
		{
			return true;
		}
	}
	return false;
}

int32 FMOTraversal::PickTop(TConstArrayView<FTopSample> Samples, bool& bOutAboveReach)
{
	TArray<int32> Ranked;
	RankTops(Samples, Ranked, bOutAboveReach);
	return Ranked.Num() > 0 ? Ranked[0] : INDEX_NONE;
}

void FMOTraversal::RankTops(TConstArrayView<FTopSample> Samples, TArray<int32>& OutBestFirst, bool& bOutAboveReach)
{
	OutBestFirst.Reset();
	bOutAboveReach = false;
	bool bSurfaceSeen = false;
	for (int32 i = 0; i < Samples.Num(); ++i)
	{
		const FTopSample& Sample = Samples[i];
		if (Sample.Kind == FTopSample::EKind::AboveReach)
		{
			bOutAboveReach = OutBestFirst.Num() == 0;   // nothing to stand on before it: the obstacle is taller than the reach
			break;                                       // it rises out of reach here: nothing past it can be got to
		}
		if (Sample.Kind == FTopSample::EKind::Miss)
		{
			if (bSurfaceSeen)
			{
				break;   // past the obstacle's far side
			}
			continue;
		}
		bSurfaceSeen = true;
		if (Sample.bWalkable)
		{
			OutBestFirst.Add(i);
		}
	}
	if (OutBestFirst.Num() < 2)
	{
		return;
	}
	// First the nearest within 2 cm of the highest (a block's top is its edge), then the rest from the highest down.
	float Highest = -UE_BIG_NUMBER;
	for (const int32 i : OutBestFirst)
	{
		Highest = FMath::Max(Highest, Samples[i].Z);
	}
	const int32 First = *OutBestFirst.FindByPredicate([&](int32 i) { return Samples[i].Z >= Highest - 2.0f; });   // OutBestFirst is near to far
	OutBestFirst.Remove(First);
	OutBestFirst.Sort([&Samples](int32 A, int32 B) { return Samples[A].Z > Samples[B].Z || (Samples[A].Z == Samples[B].Z && A < B); });
	OutBestFirst.Insert(First, 0);
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

float FMOTraversal::FaceAngleDeg(const FVector& FaceNormal)
{
	const FVector N = FaceNormal.GetSafeNormal();
	return N.IsNearlyZero() ? 90.0f : static_cast<float>(FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(N.Z, -1.0, 1.0))));
}

FMOTraversal::FClimbAbility FMOTraversal::ClimbAbility(int32 ClimbingLevel, float MuscleShare)
{
	const float Share = MuscleShare > 0.0f ? MuscleShare : ReferenceMuscleShare;
	const float Trained = 1.0f - FMath::Exp(-static_cast<float>(FMath::Max(0, ClimbingLevel)) / 10.0f);
	FClimbAbility Ability;
	Ability.CostScale = FMath::Clamp(ReferenceMuscleShare / Share, 0.6f, 1.8f) * (1.0f - 0.5f * Trained);
	Ability.SpeedScale = FMath::Clamp(1.0f + (Share - ReferenceMuscleShare), 0.85f, 1.15f) * (1.0f + 0.35f * Trained);
	return Ability;
}
