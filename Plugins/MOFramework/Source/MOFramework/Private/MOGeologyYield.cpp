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

void FMOGeologyYield::ForDig(const FMOWorldGenParams& Params, int32 Seed, const FVector& Center, float RadiusCm, float VolumeM3, float ItemsPerCubicMeter, bool bSlab,
	FName SoilItemId, TArray<FMOItemYield>& Out)
{
	const int32 Total = FMath::FloorToInt(FMath::Max(0.0f, VolumeM3) * FMath::Max(0.0f, ItemsPerCubicMeter));
	if (Total <= 0)
	{
		return;
	}
	const float R = FMath::Max(RadiusCm, 10.0f);
	// The dug region's sample points: a 5 x 5 grid over the disc (a slab as deep as the volume over the disc, sampled at its middle), or a 5 x 5 x 5 grid over
	// the sphere (a pick into a wall: the points in the air in front of it -- above the ground, inside a cave -- are skipped below).
	TArray<FVector, TInlineAllocator<128>> Points;
	if (bSlab)
	{
		const float Depth = (float)(VolumeM3 * 1.0e6 / (UE_PI * (double)R * (double)R));
		for (int32 I = -2; I <= 2; ++I)
		{
			for (int32 J = -2; J <= 2; ++J)
			{
				const FVector2D O(I * R / 2.5f, J * R / 2.5f);
				if (O.SizeSquared() <= R * R)
				{
					Points.Add(Center + FVector(O.X, O.Y, -0.5f * Depth));
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
	// Tally by item, then share the total by largest remainder (counts add up exactly; a kind seen at one point of 25 still yields its share).
	struct FTally { FName Item; bool bFill; int32 Points; };
	TArray<FTally, TInlineAllocator<8>> Tally;
	const float Voxel = Params.Tuning.VoxelSizeCm;
	int32 Solid = 0;
	for (const FVector& P : Points)
	{
		// Air holds nothing: above the unworked ground, or inside a cave (the generator's ground knows neither).
		const float Ground = FMOWorldGen::SampleHeightCm(Params, P.X, P.Y, Seed, Voxel);
		if (P.Z > Ground || FMOWorldGen::SampleCaveDistanceAtGround(Params, P.X, P.Y, P.Z, Ground, Seed, 50.0f) < 0.0f)
		{
			continue;
		}
		++Solid;
		bool bFill = true;
		const FName Item = ItemFor(FMOWorldGen::SampleDig(Params, P.X, P.Y, P.Z, Seed), SoilItemId, bFill);
		FTally* Found = Tally.FindByPredicate([&Item](const FTally& T) { return T.Item == Item; });
		if (Found) { ++Found->Points; } else { Tally.Add({ Item, bFill, 1 }); }
	}
	const int32 N = Solid;
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
