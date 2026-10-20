// The inventory's Details tab says an item's weight as a person reads it (UMOItemInfoPanel::FormatWeight): it read "Weight: Weight: 1", the panel's label
// plus the code's own and no unit (N-180, Oct 20 2026). Run: python Tools/ue.py auto --filter "MOFramework.UI.ItemWeight"

#include "Misc/AutomationTest.h"
#include "MOItemInfoPanel.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOItemInfo_WeightReadsAsAWeight, "MOFramework.UI.ItemWeight.ReadsAsAWeight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOItemInfo_WeightReadsAsAWeight::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("a kilogram"), UMOItemInfoPanel::FormatWeight(1.0f).ToString(), FString(TEXT("1 kg")));
	TestEqual(TEXT("to a tenth"), UMOItemInfoPanel::FormatWeight(2.54f).ToString(), FString(TEXT("2.5 kg")));
	TestEqual(TEXT("under a kilogram, grams"), UMOItemInfoPanel::FormatWeight(0.1f).ToString(), FString(TEXT("100 g")));
	TestEqual(TEXT("a pinch of tinder is still a gram"), UMOItemInfoPanel::FormatWeight(0.0002f).ToString(), FString(TEXT("1 g")));
	TestFalse(TEXT("no label in the value (the panel has one)"), UMOItemInfoPanel::FormatWeight(1.0f).ToString().Contains(TEXT("Weight")));
	return true;
}

#endif
