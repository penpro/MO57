#include "MOCoverForageSettings.h"
#include "Engine/StaticMesh.h"
#include "MOInventoryComponent.h"

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
	{
		FMOCoverForage C = Cover(TEXT("vlkhcbxia"), LOCTEXT("GatherGrass", "Gather dry grass"), { Yield(TEXT("DriedGrass01"), 1, 2), Yield(TEXT("PlantFiber01"), 1, 1, 0.5f) }, 5.0f);
		C.MoreMeshNamesContaining = { TEXT("smdrailr") };   // the other grass: the same gathering, one menu entry
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
	// An old fallen log: seasoned dead wood to burn and to drill, and the rotten "punk" wood inside that takes an ember. Broken up and carried off.
	Entries.Add(Cover(TEXT("wcjgbc1"), LOCTEXT("BreakLog", "Break up the old log"),
		{ Yield(TEXT("Branch01"), 2, 3), Yield(TEXT("Firewood01"), 1, 2), Yield(TEXT("Tinder01"), 1, 1, 0.4f) }, 12.0f));
	// A standing dead tree: dry branches snapped off its lower trunk. The tree stays.
	{
		FMOCoverForage Snag = Cover(TEXT("SM_DeadTree"), LOCTEXT("SnapDeadBranches", "Snap off dead branches"), { Yield(TEXT("Branch01"), 1, 2) }, 8.0f);
		Snag.bRemovesInstance = false;
		Entries.Add(Snag);
	}
	// Sandstone ground and outcrops: a block broken off with a hammerstone (a grinding stone, a whetstone). The rock stays.
	for (const TCHAR* Sandstone : { TEXT("SandstoneRockyGround"), TEXT("SandstoneCliff") })
	{
		FMOCoverForage Rock = Cover(Sandstone, LOCTEXT("BreakSandstone", "Break off sandstone"), { Yield(TEXT("Sandstone01"), 1, 1) }, 20.0f);
		Rock.bRemovesInstance = false;
		Rock.RequiredTool = EMOToolType::Hammer;
		Rock.MinToolQuality = 0.3f;
		Entries.Add(Rock);
	}
}

bool FMOCoverForage::CanBeWorkedBy(const UMOInventoryComponent* Inventory, FText* OutReason) const
{
	if (RequiredTool == EMOToolType::None)
	{
		return true;
	}
	const float Best = Inventory ? Inventory->GetBestToolEffectiveness(RequiredTool) : 0.0f;
	if (Best > 0.0f && Best >= MinToolQuality)
	{
		return true;
	}
	if (OutReason)
	{
		*OutReason = FText::Format(LOCTEXT("NeedsTool", "Needs a {0}."), UEnum::GetDisplayValueAsText(RequiredTool).ToLower());
	}
	return false;
}

int32 UMOCoverForageSettings::FindIndexForMeshName(const FString& MeshName)
{
	if (MeshName.IsEmpty())
	{
		return INDEX_NONE;
	}
	const TArray<FMOCoverForage>& All = GetDefault<UMOCoverForageSettings>()->Entries;
	for (int32 Index = 0; Index < All.Num(); ++Index)
	{
		if (All[Index].MatchesMeshName(MeshName))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

const FMOCoverForage* UMOCoverForageSettings::GetEntry(int32 Index)
{
	const TArray<FMOCoverForage>& All = GetDefault<UMOCoverForageSettings>()->Entries;
	return All.IsValidIndex(Index) ? &All[Index] : nullptr;
}

const FMOCoverForage* UMOCoverForageSettings::FindForMeshName(const FString& MeshName)
{
	return GetEntry(FindIndexForMeshName(MeshName));
}

const FMOCoverForage* UMOCoverForageSettings::FindForMesh(const UStaticMesh* Mesh)
{
	return Mesh ? FindForMeshName(Mesh->GetName()) : nullptr;
}

#undef LOCTEXT_NAMESPACE
