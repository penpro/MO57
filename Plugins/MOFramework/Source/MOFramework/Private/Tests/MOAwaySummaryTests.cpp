// Taking a pawn back tells the player what it did meanwhile (UMOCharacterHistoryComponent::BuildAwaySummary, N-091, Oct 20 2026): the pawn's history
// entries after the moment a player left it, the newest few, the rest counted. Run: python Tools/ue.py auto --filter "MOFramework.Colony.AwaySummary"

#include "Misc/AutomationTest.h"
#include "MOCharacterHistoryComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAwaySummary_WhatHappenedSince, "MOFramework.Colony.AwaySummary.WhatHappenedSince",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAwaySummary_WhatHappenedSince::RunTest(const FString& Parameters)
{
	const FDateTime Left(2026, 6, 1, 12, 0, 0);
	auto At = [&](int32 Minutes, const TCHAR* What)
	{
		FMOCharacterHistoryEntry E;
		E.Timestamp = Left + FTimespan::FromMinutes(Minutes);
		E.Description = FText::FromString(What);
		return E;
	};
	TArray<FMOCharacterHistoryEntry> Entries = { At(-30, TEXT("Moved into the lean-to")), At(10, TEXT("Finished: Gather Sticks")),
		At(40, TEXT("Became friends with Ada")), At(70, TEXT("Finished: Gather Stones")), At(95, TEXT("Gave up: Chop Tree (no axe)")),
		At(120, TEXT("Finished: Gather Bark")) };
	FText Out;

	TestFalse(TEXT("no player has left this pawn: nothing to tell"), UMOCharacterHistoryComponent::BuildAwaySummary(Entries, FDateTime::MinValue(), Left, 4, Out));
	TestFalse(TEXT("nothing recorded since the player left: no notice"),
		UMOCharacterHistoryComponent::BuildAwaySummary({ At(-30, TEXT("Moved into the lean-to")) }, Left, Left + FTimespan::FromHours(1), 4, Out));

	TestTrue(TEXT("five things happened in 2 h 10 min"), UMOCharacterHistoryComponent::BuildAwaySummary(Entries, Left, Left + FTimespan::FromMinutes(130), 4, Out));
	const FString S = Out.ToString();
	TArray<FString> Lines;
	S.ParseIntoArrayLines(Lines);
	TestEqual(TEXT("a title, the newest four, and a count of the rest"), Lines.Num(), 6);
	if (Lines.Num() == 6)
	{
		TestEqual(TEXT("the title says how long"), Lines[0], FString(TEXT("While you were away (2 h 10 min):")));
		TestEqual(TEXT("oldest of the four shown first"), Lines[1], FString(TEXT("- Became friends with Ada")));
		TestEqual(TEXT("the newest last"), Lines[4], FString(TEXT("- Finished: Gather Bark")));
		TestEqual(TEXT("the one that did not fit is counted"), Lines[5], FString(TEXT("(and 1 earlier)")));
	}
	TestFalse(TEXT("what happened before the player left is not news"), S.Contains(TEXT("lean-to")));

	UMOCharacterHistoryComponent::BuildAwaySummary(Entries, Left, Left + FTimespan::FromSeconds(20), 4, Out);
	TestTrue(TEXT("a short absence still reads in minutes (at least 1)"), Out.ToString().StartsWith(TEXT("While you were away (1 min):")));
	UMOCharacterHistoryComponent::BuildAwaySummary(Entries, Left, Left + FTimespan::FromHours(27), 4, Out);
	TestTrue(TEXT("a long one in days"), Out.ToString().StartsWith(TEXT("While you were away (1 d 3 h):")));
	return true;
}

#endif
