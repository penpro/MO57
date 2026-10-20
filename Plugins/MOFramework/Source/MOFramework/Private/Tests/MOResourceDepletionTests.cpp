// Standing resource nodes (UMOResourceDepletionSubsystem): every yield is finite -- an item missing from InitialCountByItem takes
// DefaultYieldRange (it was unlimited: one dead tuft gave tinder forever) -- and a node counts only what it yields, so it does read as fully
// depleted and stamps its respawn (a tree rolled cobble counts it never gave and never did). Run: python Tools/ue.py auto --filter "MOFramework.Resource"

#include "Misc/AutomationTest.h"
#include "MOResourceDepletionSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOResourceDepletion_EveryYieldIsFinite, "MOFramework.Resource.EveryYieldIsFinite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOResourceDepletion_EveryYieldIsFinite::RunTest(const FString& Parameters)
{
	UMOResourceDepletionSubsystem* Depletion = NewObject<UMOResourceDepletionSubsystem>(GetTransientPackage());
	Depletion->InitialCountByItem.Reset();
	Depletion->InitialCountByItem.Add(TEXT("Stick01"), FMOResourceYieldRange(1, 1));
	Depletion->InitialCountByItem.Add(TEXT("RiverCobble01"), FMOResourceYieldRange(2, 2));
	Depletion->DefaultYieldRange = FMOResourceYieldRange(3, 3);

	// A dead shrub's tinder is not in the map: it holds DefaultYieldRange, then nothing.
	const FString Shrub(TEXT("ISM|0_0_0"));
	TestTrue(TEXT("a fresh node can yield"), Depletion->CanYield(Shrub, TEXT("Tinder01")));
	int32 Taken = 0;
	for (int32 i = 0; i < 10; ++i)
	{
		Taken += Depletion->ConsumeYield(Shrub, TEXT("Tinder01")) ? 1 : 0;
	}
	TestEqual(TEXT("an unlisted yield gives DefaultYieldRange units, not forever"), Taken, 3);
	TestFalse(TEXT("... then the node has none"), Depletion->CanYield(Shrub, TEXT("Tinder01")));

	// A tree gives sticks only: once they are gone it is fully depleted (the cobble count it never yields must not hold it open).
	const FString Tree(TEXT("ISM|100_0_0"));
	TestTrue(TEXT("the tree's one stick"), Depletion->ConsumeYield(Tree, TEXT("Stick01")));
	TestFalse(TEXT("... and no second"), Depletion->ConsumeYield(Tree, TEXT("Stick01")));
	FMOResourceDepletionSaveData Save;
	Depletion->BuildSaveData(Save);
	const FMOResourceNodeDepletionSaveEntry* TreeEntry = Save.Entries.FindByPredicate([&Tree](const FMOResourceNodeDepletionSaveEntry& E) { return E.NodeKey == Tree; });
	TestNotNull(TEXT("the tree is tracked"), TreeEntry);
	if (TreeEntry)
	{
		TestFalse(TEXT("the tree tracks only what it gave (no cobble count)"), TreeEntry->RemainingByItem.Contains(TEXT("RiverCobble01")));
		TestTrue(TEXT("the tree reads fully depleted: its respawn is stamped"), TreeEntry->FullyDepletedAt > FDateTime::MinValue());
	}
	return true;
}

#endif
