#include "MOGeologyYield.h"
#include "MOFramework.h"
#include "MOInventoryComponent.h"
#include "MOWorldItemFactory.h"
#include "MOWorldSeedSubsystem.h"
#include "Engine/World.h"

FName FMOGeologyYield::ItemFor(const FMODigSample& Sample, FName SoilItemId, bool& bOutFill)
{
	bOutFill = true;
	switch (Sample.Material)
	{
	case EMODigMaterial::Soil:
		return SoilItemId.IsNone() ? FName(TEXT("Dirt01")) : SoilItemId;
	case EMODigMaterial::Rock:
		switch (Sample.Bedrock)
		{
		case EMOBedrock::Limestone: return FName(TEXT("Limestone01"));
		case EMOBedrock::CoalMeasures: return FName(TEXT("Sandstone01"));   // the beds between the seams
		case EMOBedrock::Granite: return FName(TEXT("Granite01"));
		case EMOBedrock::Basalt: return FName(TEXT("Basalt01"));
		}
		return FName(TEXT("Stone01"));
	case EMODigMaterial::Ore:
		bOutFill = false;
		switch (Sample.Ore)
		{
		case EMOOreKind::Coal: return FName(TEXT("Coal01"));
		case EMOOreKind::Hematite: return FName(TEXT("IronOre01"));
		case EMOOreKind::Copper: return FName(TEXT("CopperOre01"));
		case EMOOreKind::Tin: return FName(TEXT("TinOre01"));
		case EMOOreKind::Flint: return FName(TEXT("FlintNodule01"));
		case EMOOreKind::Galena: return FName(TEXT("GalenaOre01"));
		case EMOOreKind::None: break;
		}
		break;
	}
	bOutFill = true;
	return FName(TEXT("Stone01"));
}

void FMOGeologyYield::SampleDugRegion(const FMOWorldGenParams& Params, int32 Seed, const FVector& Center, float RadiusCm, float VolumeM3, bool bSlab,
	TArray<FMODigSample>& OutSolid)
{
	const float R = FMath::Max(RadiusCm, 10.0f);
	// The dug region's sample points: a 5 x 5 grid over the disc (a slab as deep as the volume over the disc, sampled at its middle), or a 5 x 5 x 5 grid over
	// the sphere (a pick into a wall: the points in the air in front of it -- above the ground, inside a cave -- are skipped below).
	TArray<FVector, TInlineAllocator<128>> Points;
	const float Voxel = Params.Tuning.VoxelSizeCm;
	if (bSlab)
	{
		// A height dig lowers every column of the disc by the same amount, so the cut follows the ground's shape: each point sits half the depth under its own
		// column's surface. The surface there is the generator's ground shifted by what the centre has already been worked (a pit, a raised mound) -- on a
		// slope a point at the centre's height would be deep under the uphill side and count rock that a shallow scrape never reaches.
		const float Depth = (float)(FMath::Max(0.0f, VolumeM3) * 1.0e6 / (UE_PI * (double)R * (double)R));
		const float Worked = (float)Center.Z - FMOWorldGen::SampleHeightCm(Params, Center.X, Center.Y, Seed, Voxel);
		for (int32 I = -2; I <= 2; ++I)
		{
			for (int32 J = -2; J <= 2; ++J)
			{
				const FVector2D O(I * R / 2.5f, J * R / 2.5f);
				if (O.SizeSquared() <= R * R)
				{
					const double X = Center.X + O.X, Y = Center.Y + O.Y;
					Points.Add(FVector(X, Y, FMOWorldGen::SampleHeightCm(Params, X, Y, Seed, Voxel) + Worked - 0.5f * Depth));
				}
			}
		}
	}
	else
	{
		for (int32 I = -2; I <= 2; ++I)
		{
			for (int32 J = -2; J <= 2; ++J)
			{
				for (int32 K = -2; K <= 2; ++K)
				{
					const FVector O(I * R / 2.5f, J * R / 2.5f, K * R / 2.5f);
					if (O.SizeSquared() <= R * R)
					{
						Points.Add(Center + O);
					}
				}
			}
		}
	}
	for (const FVector& P : Points)
	{
		// Air holds nothing: above the unworked ground, or inside a cave (the generator's ground knows neither).
		const float Ground = FMOWorldGen::SampleHeightCm(Params, P.X, P.Y, Seed, Voxel);
		if (P.Z > Ground || FMOWorldGen::SampleCaveDistanceAtGround(Params, P.X, P.Y, P.Z, Ground, Seed, 50.0f) < 0.0f)
		{
			continue;
		}
		OutSolid.Add(FMOWorldGen::SampleDig(Params, P.X, P.Y, P.Z, Seed));
	}
}

void FMOGeologyYield::ForDig(const FMOWorldGenParams& Params, int32 Seed, const FVector& Center, float RadiusCm, float VolumeM3, float ItemsPerCubicMeter, bool bSlab,
	FName SoilItemId, TArray<FMOItemYield>& Out)
{
	const int32 Total = FMath::FloorToInt(FMath::Max(0.0f, VolumeM3) * FMath::Max(0.0f, ItemsPerCubicMeter));
	if (Total <= 0)
	{
		return;
	}
	TArray<FMODigSample> Solid;
	SampleDugRegion(Params, Seed, Center, RadiusCm, VolumeM3, bSlab, Solid);
	// Tally by item, then share the total by largest remainder (counts add up exactly; a kind seen at one point of 25 still yields its share).
	struct FTally { FName Item; bool bFill; int32 Points; };
	TArray<FTally, TInlineAllocator<8>> Tally;
	for (const FMODigSample& Sample : Solid)
	{
		bool bFill = true;
		const FName Item = ItemFor(Sample, SoilItemId, bFill);
		FTally* Found = Tally.FindByPredicate([&Item](const FTally& T) { return T.Item == Item; });
		if (Found) { ++Found->Points; } else { Tally.Add({ Item, bFill, 1 }); }
	}
	const int32 N = Solid.Num();
	if (N == 0)
	{
		bool bFill = true;
		Out.Add({ ItemFor(FMODigSample(), SoilItemId, bFill), Total, true });   // every point in the air: the worked ground it really cut was earth
		return;
	}
	int32 Given = 0;
	TArray<TPair<double, int32>, TInlineAllocator<8>> Remainders;
	for (int32 T = 0; T < Tally.Num(); ++T)
	{
		const double Share = (double)Total * Tally[T].Points / N;
		const int32 Whole = FMath::FloorToInt(Share);
		Out.Add({ Tally[T].Item, Whole, Tally[T].bFill });
		Given += Whole;
		Remainders.Emplace(Share - Whole, Out.Num() - 1);
	}
	Remainders.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key > B.Key; });
	for (int32 I = 0; Given < Total && I < Remainders.Num(); ++I, ++Given)
	{
		++Out[Remainders[I].Value].Count;
	}
	Out.RemoveAll([](const FMOItemYield& Y) { return Y.Count <= 0; });
}

float FMOGeologyYield::HardnessFor(const FMODigSample& Sample)
{
	auto Rock = [](EMOBedrock Bedrock)
	{
		switch (Bedrock)
		{
		case EMOBedrock::Limestone: return 5.0f;
		case EMOBedrock::CoalMeasures: return 3.0f;
		case EMOBedrock::Granite: return 40.0f;
		case EMOBedrock::Basalt: return 40.0f;
		}
		return 5.0f;
	};
	switch (Sample.Material)
	{
	case EMODigMaterial::Soil:
		return 1.0f;
	case EMODigMaterial::Rock:
		return Rock(Sample.Bedrock);
	case EMODigMaterial::Ore:
		switch (Sample.Ore)
		{
		case EMOOreKind::Coal: return 1.5f;       // soft (Mohs 1-2.5), breaks along its bedding
		case EMOOreKind::Galena: return 3.0f;     // soft (2.5): the vein gives faster than the limestone round it
		case EMOOreKind::Hematite: return 8.0f;   // massive hematite is hard (5.5-6.5)
		case EMOOreKind::Flint: return 10.0f;     // chert (7): very hard, worked by breaking it out
		case EMOOreKind::Copper: return 15.0f;    // malachite and native copper are soft, but the vein sits in basalt
		case EMOOreKind::Tin: return 40.0f;       // cassiterite (6-7) in quartz veins: as hard as the granite
		case EMOOreKind::None: break;
		}
		return Rock(Sample.Bedrock);
	}
	return 1.0f;
}

float FMOGeologyYield::DigTimeMultiplier(const FMOWorldGenParams& Params, int32 Seed, const FVector& Center, float RadiusCm, float VolumeM3, bool bSlab)
{
	TArray<FMODigSample> Solid;
	SampleDugRegion(Params, Seed, Center, RadiusCm, VolumeM3, bSlab, Solid);
	if (Solid.Num() == 0)
	{
		return 1.0f;
	}
	// Time adds over the volume: the mean of the points' multipliers (each point stands for an equal share of the dug volume).
	double Sum = 0.0;
	for (const FMODigSample& Sample : Solid)
	{
		Sum += HardnessFor(Sample);
	}
	return (float)(Sum / Solid.Num());
}

float FMOGeologyYield::DigTimeMultiplierInWorld(const UObject* WorldContext, const FVector& Center, float RadiusCm, float VolumeM3, bool bSlab)
{
	const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(WorldContext);
	if (!Seeds || !Seeds->HasActiveSeed())
	{
		return 1.0f;
	}
	return DigTimeMultiplier(*FMOWorldGenParamsProvider::Get(), Seeds->GetActiveTerrainSeed(), Center, RadiusCm, VolumeM3, bSlab);
}

bool FMOGeologyYield::ForDigInWorld(const UObject* WorldContext, const FVector& Center, float RadiusCm, float VolumeM3, float ItemsPerCubicMeter, bool bSlab, FName SoilItemId,
	TArray<FMOItemYield>& Out)
{
	const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(WorldContext);
	if (!Seeds || !Seeds->HasActiveSeed())
	{
		return false;
	}
	ForDig(*FMOWorldGenParamsProvider::Get(), Seeds->GetActiveTerrainSeed(), Center, RadiusCm, VolumeM3, ItemsPerCubicMeter, bSlab, SoilItemId, Out);
	return true;
}

int32 FMOGeologyYield::GiveOrDrop(UMOInventoryComponent* Inventory, const TArray<FMOItemYield>& Yields, const FVector& DropLocation, const UObject* WorldContext)
{
	UWorld* World = Inventory ? Inventory->GetWorld() : (WorldContext ? WorldContext->GetWorld() : nullptr);
	int32 Dropped = 0;
	for (const FMOItemYield& Y : Yields)
	{
		// The most of this kind that fits (CanAddItemByDefinitionId is monotonic in the quantity): AddItemByGuid alone does not refuse a full inventory.
		int32 Fit = 0;
		if (Inventory)
		{
			int32 Lo = 0, Hi = Y.Count;
			while (Lo < Hi)
			{
				const int32 Mid = (Lo + Hi + 1) / 2;
				if (Inventory->CanAddItemByDefinitionId(Y.ItemId, Mid)) { Lo = Mid; } else { Hi = Mid - 1; }
			}
			Fit = Lo;
			if (Fit > 0)
			{
				Inventory->AddItemByGuid(FGuid::NewGuid(), Y.ItemId, Fit);
			}
		}
		const int32 Rest = Y.Count - Fit;
		if (Rest > 0)
		{
			UMOWorldItemFactory* Factory = World ? World->GetSubsystem<UMOWorldItemFactory>() : nullptr;
			if (Factory && Factory->SpawnDroppedItem(Y.ItemId, Rest, FGuid::NewGuid(), DropLocation + FVector(0.0, 0.0, 40.0), FRotator::ZeroRotator, Inventory ? Inventory->GetOwner() : nullptr))
			{
				Dropped += Rest;
			}
			else
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOGeologyYield] %d %s did not fit and could not be dropped"), Rest, *Y.ItemId.ToString());
			}
		}
	}
	return Dropped;
}

FString FMOGeologyYield::Describe(const TArray<FMOItemYield>& Yields)
{
	FString S;
	for (const FMOItemYield& Y : Yields)
	{
		S += FString::Printf(TEXT("%s%d %s"), S.IsEmpty() ? TEXT("") : TEXT(", "), Y.Count, *Y.ItemId.ToString());
	}
	return S.IsEmpty() ? FString(TEXT("nothing")) : S;
}
