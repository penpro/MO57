#include "MOCoverForageSettings.h"
#include "Engine/StaticMesh.h"

#define LOCTEXT_NAMESPACE "MOCoverForage"

namespace
{
	FMODigDropEntry Yield(const TCHAR* ItemId, int32 Min, int32 Max, float Chance = 1.0f, int32 MinLevel = 0)
	{
		FMODigDropEntry E;
		E.ItemId = FName(ItemId);
		E.MinQuantity = Min;
		E.MaxQuantity = Max;
		E.DropChance = Chance;
		E.MinForagingLevel = MinLevel;
		return E;
	}

	FMOCoverForage Cover(const TCHAR* Fragment, const FText& Label, TArray<FMODigDropEntry> Yields, float Seconds)
	{
		FMOCoverForage C;
		C.MeshNameContains = Fragment;
		C.Label = Label;
		C.Yields = MoveTemp(Yields);
		C.Seconds = Seconds;
		return C;
	}
}

UMOCoverForageSettings::UMOCoverForageSettings()
{
	// Grass tussocks: the dead, dry stems at the base (tinder, bedding, cordage) and some fibre. The same grass meshes stand in for reeds at the water's edge
	// (real reed meshes are still to come), where a clump cut at the base is a bundle of reed stems (baskets, mats, fish traps).
	for (const TCHAR* Grass : { TEXT("vlkhcbxia"), TEXT("smdrailr") })
	{
		FMOCoverForage C = Cover(Grass, LOCTEXT("GatherGrass", "Gather dry grass"), { Yield(TEXT("DriedGrass01"), 1, 2), Yield(TEXT("PlantFiber01"), 1, 1, 0.5f) }, 5.0f);
		C.ShoreLabel = LOCTEXT("CutReeds", "Cut reeds");
		C.ShoreYields = { Yield(TEXT("Reeds01"), 2, 4) };
		Entries.Add(C);
	}
	// Bracken: fronds for bedding and thatch, the stems' fibre.
	Entries.Add(Cover(TEXT("SM_Fern_"), LOCTEXT("CutBracken", "Cut bracken"), { Yield(TEXT("PlantFiber01"), 1, 2) }, 4.0f));
	// A scatter of small stones: a few loose ones worth picking up, now and then a flint pebble among them.
	Entries.Add(Cover(TEXT("SmallStonesPack"), LOCTEXT("PickStones", "Pick up loose stones"),
		{ Yield(TEXT("Stone01"), 1, 2), Yield(TEXT("FlintNodule01"), 1, 1, 0.05f, 2) }, 4.0f));
	// A hazel shoot at the water's edge: a straight rod.
	Entries.Add(Cover(TEXT("CommonHazel_Sapling"), LOCTEXT("CutHazelRod", "Cut a hazel rod"),
		{ Yield(TEXT("StraightBranch01"), 1, 1), Yield(TEXT("Stick01"), 1, 1, 0.5f) }, 5.0f));
	// A tree seedling, pulled up: a green stick.
	Entries.Add(Cover(TEXT("Seedling"), LOCTEXT("PullSeedling", "Pull up the seedling"), { Yield(TEXT("Stick01"), 1, 1) }, 4.0f));
}

const FMOCoverForage* UMOCoverForageSettings::FindForMeshName(const FString& MeshName)
{
	if (MeshName.IsEmpty())
	{
		return nullptr;
	}
	for (const FMOCoverForage& Entry : GetDefault<UMOCoverForageSettings>()->Entries)
	{
		if (!Entry.MeshNameContains.IsEmpty() && MeshName.Contains(Entry.MeshNameContains))
		{
			return &Entry;
		}
	}
	return nullptr;
}

const FMOCoverForage* UMOCoverForageSettings::FindForMesh(const UStaticMesh* Mesh)
{
	return Mesh ? FindForMeshName(Mesh->GetName()) : nullptr;
}

#undef LOCTEXT_NAMESPACE
