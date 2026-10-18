// Gatherable ground cover (UMOCoverForageSettings): a mesh name finds its entry -- every variant of a grass, the hazel rod before the generic seedling --
// the tall grass at the water's edge is reed, and trees (resources, not cover) find nothing. Run: python Tools/ue.py auto --filter "MOFramework.Cover"

#include "Misc/AutomationTest.h"
#include "MOCoverForageSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCoverForage_Lookup, "MOFramework.Cover.Lookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOCoverForage_Lookup::RunTest(const FString& Parameters)
{
	auto Gives = [](const FMOCoverForage* Cover, bool bShore, FName Item)
	{
		return Cover && Cover->YieldsFor(bShore).ContainsByPredicate([Item](const FMODigDropEntry& E) { return E.ItemId == Item; });
	};

	const FMOCoverForage* Grass = UMOCoverForageSettings::FindForMeshName(TEXT("SM_vlkhcbxia_VarC"));
	TestTrue(TEXT("a wild grass tussock gives dry grass"), Gives(Grass, false, TEXT("DriedGrass01")));
	TestTrue(TEXT("... and at the water's edge, reeds"), Gives(Grass, true, TEXT("Reeds01")));
	TestFalse(TEXT("... not reeds away from water"), Gives(Grass, false, TEXT("Reeds01")));
	TestTrue(TEXT("the other grass mesh too"), Gives(UMOCoverForageSettings::FindForMeshName(TEXT("SM_smdrailr_VarF")), true, TEXT("Reeds01")));
	TestEqual(TEXT("... as the SAME entry (two entries listed \"Gather dry grass\" twice in the forage menu)"),
		UMOCoverForageSettings::FindIndexForMeshName(TEXT("SM_smdrailr_VarF")), UMOCoverForageSettings::FindIndexForMeshName(TEXT("SM_vlkhcbxia_VarC")));

	const FMOCoverForage* Fern = UMOCoverForageSettings::FindForMeshName(TEXT("SM_Fern_03"));
	TestTrue(TEXT("bracken gives fibre"), Gives(Fern, false, TEXT("PlantFiber01")));
	TestTrue(TEXT("no shore variant: the same at the water"), Gives(Fern, true, TEXT("PlantFiber01")));

	TestTrue(TEXT("a hazel sapling gives a straight rod (before the generic seedling entry)"),
		Gives(UMOCoverForageSettings::FindForMeshName(TEXT("SM_CommonHazel_Sapling_01_PP")), false, TEXT("StraightBranch01")));
	TestTrue(TEXT("an alder seedling gives a green stick"), Gives(UMOCoverForageSettings::FindForMeshName(TEXT("SM_BlackAlder_Seedling_02_PP")), false, TEXT("Stick01")));
	TestTrue(TEXT("a stone scatter gives stones"), Gives(UMOCoverForageSettings::FindForMeshName(TEXT("SM_SmallStonesPack_11")), false, TEXT("Stone01")));

	const FMOCoverForage* Log = UMOCoverForageSettings::FindForMeshName(TEXT("wcjgbc1_tier_2"));
	TestTrue(TEXT("an old log gives dry deadwood"), Gives(Log, false, TEXT("Branch01")));
	TestTrue(TEXT("... and is broken up (taken)"), Log && Log->bRemovesInstance);
	const FMOCoverForage* Snag = UMOCoverForageSettings::FindForMeshName(TEXT("SM_DeadTree_02"));
	TestTrue(TEXT("a dead tree gives branches and stays standing"), Gives(Snag, false, TEXT("Branch01")) && !Snag->bRemovesInstance);
	const FMOCoverForage* Sandstone = UMOCoverForageSettings::FindForMeshName(TEXT("SM_SandstoneRockyGround_01"));
	TestTrue(TEXT("sandstone ground gives sandstone"), Gives(Sandstone, false, TEXT("Sandstone01")));
	FText Reason;
	TestFalse(TEXT("... but not to bare hands"), Sandstone && Sandstone->CanBeWorkedBy(nullptr, &Reason));
	TestFalse(TEXT("... and says what is missing"), Reason.IsEmpty());
	TestTrue(TEXT("bracken needs no tool"), Fern && Fern->CanBeWorkedBy(nullptr));

	TestNull(TEXT("a tree is a resource, not cover"), UMOCoverForageSettings::FindForMeshName(TEXT("SM_BlackAlder_Forest_01_PP")));
	TestNull(TEXT("nothing for no name"), UMOCoverForageSettings::FindForMeshName(FString()));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
