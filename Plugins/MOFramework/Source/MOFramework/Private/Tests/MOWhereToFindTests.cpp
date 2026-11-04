// Where to find a missing ingredient (N-255; Wes, Nov 1: "the crude chopper takes stone, idk where to get stone"): the item-source
// analysis keeps every reachable way to get each item, in words a player reads, for the crafting panels.

#include "Misc/AutomationTest.h"
#include "MOItemSourceAnalysis.h"

namespace
{
	bool AnyWaySays(const TArray<FMOItemSourceHint>* Ways, const TCHAR* Words)
	{
		return Ways && Ways->ContainsByPredicate([Words](const FMOItemSourceHint& Way) { return Way.Label.ToString().Contains(Words); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWhereToFind_Words,
	"MOFramework.Crafting.WhereToFindWords",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWhereToFind_Words::RunTest(const FString& Parameters)
{
	const FMOItemSourceReport Report = FMOItemSourceAnalysis::Analyze(nullptr);

	// Wes's case: stone -- loose stones lie about (ground cover "Pick up loose stones"); a building stone is broken off a boulder.
	TestTrue(TEXT("Stone01 has a way to get it"), Report.Hints.Contains(FName(TEXT("Stone01"))));
	TestTrue(TEXT("a building stone: break it off a boulder"), AnyWaySays(Report.Hints.Find(FName(TEXT("FieldStone01"))), TEXT("Break off a building stone")));
	TestTrue(TEXT("moss: strip it off a mossy rock"), AnyWaySays(Report.Hints.Find(FName(TEXT("Moss01"))), TEXT("Strip moss")));

	// Every obtainable item a recipe can need has words for it, and no word is the analysis's own shorthand.
	int32 Missing = 0;
	int32 Debug = 0;
	FString Examples;
	for (const TPair<FName, FString>& Item : Report.SourceOf)
	{
		const TArray<FMOItemSourceHint>* Ways = Report.Hints.Find(Item.Key);
		if (!Ways || Ways->Num() == 0)
		{
			if (!Item.Value.StartsWith(TEXT("recipe ")))   // a building's output says nothing (it is not "made")
			{
				++Missing;
				Examples += Item.Key.ToString() + TEXT(" ");
			}
			continue;
		}
		for (const FMOItemSourceHint& Way : *Ways)
		{
			const FString S = Way.Label.ToString();
			if (S.IsEmpty() || S.StartsWith(TEXT("resource ")) || S.StartsWith(TEXT("recipe ")) || S.Contains(TEXT("MOBiomeCover_")) || S.Contains(TEXT("PCG_")))
			{
				++Debug;
				Examples += S + TEXT(" | ");
			}
		}
	}
	TestEqual(FString::Printf(TEXT("every obtainable item has a way in words (%s)"), *Examples), Missing, 0);
	TestEqual(FString::Printf(TEXT("no way is shorthand (%s)"), *Examples), Debug, 0);
	// CONTROL (can fail): an item nothing gives has no words.
	TestFalse(TEXT("CONTROL: a made-up item has no way"), Report.Hints.Contains(FName(TEXT("NoSuchItem_Nov3"))));
	return true;
}
