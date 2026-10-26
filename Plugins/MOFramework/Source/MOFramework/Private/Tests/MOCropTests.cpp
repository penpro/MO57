// Crops (Docs/Farming_And_Husbandry.md §2): where a sown plot is after so many days (FMOCropGrowth), and the world's Growing pace.
// Run: python Tools/ue.py auto --filter "MOFramework.Crop"

#include "Misc/AutomationTest.h"
#include "MOCropTypes.h"
#include "MOWorldCreationTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCrop_StagesByDays, "MOFramework.Crop.StagesByDays",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOCrop_StagesByDays::RunTest(const FString& Parameters)
{
	FMOCropDefinitionRow Barley;   // sprouts in 7 days, ripe at 95, stands 14, gone 30 days later
	Barley.DaysToSprout = 7.0f;
	Barley.DaysToRipe = 95.0f;
	Barley.DaysRipe = 14.0f;
	Barley.DaysOverripeToLost = 30.0f;
	TestEqual(TEXT("nothing sown: fallow"), FMOCropGrowth::StageAt(Barley, -1.0), EMOCropStage::Fallow);
	TestEqual(TEXT("day 0: sown"), FMOCropGrowth::StageAt(Barley, 0.0), EMOCropStage::Sown);
	TestEqual(TEXT("day 6: still under the ground"), FMOCropGrowth::StageAt(Barley, 6.0), EMOCropStage::Sown);
	TestEqual(TEXT("day 8: sprouted"), FMOCropGrowth::StageAt(Barley, 8.0), EMOCropStage::Sprouted);
	TestEqual(TEXT("day 50: growing"), FMOCropGrowth::StageAt(Barley, 50.0), EMOCropStage::Growing);
	TestEqual(TEXT("day 95: ripe"), FMOCropGrowth::StageAt(Barley, 95.0), EMOCropStage::Ripe);
	TestEqual(TEXT("day 108: still ripe"), FMOCropGrowth::StageAt(Barley, 108.0), EMOCropStage::Ripe);
	TestEqual(TEXT("day 110: overripe"), FMOCropGrowth::StageAt(Barley, 110.0), EMOCropStage::Overripe);
	TestEqual(TEXT("NaN days: fallow"), FMOCropGrowth::StageAt(Barley, NAN), EMOCropStage::Fallow);

	TestEqual(TEXT("half-way: half grown"), FMOCropGrowth::GrowthFraction(Barley, 47.5), 0.5f);
	TestEqual(TEXT("past ripe: full grown"), FMOCropGrowth::GrowthFraction(Barley, 200.0), 1.0f);
	TestEqual(TEXT("not ripe: nothing to take"), FMOCropGrowth::HarvestableFraction(Barley, 90.0), 0.0f);
	TestEqual(TEXT("ripe: all of it"), FMOCropGrowth::HarvestableFraction(Barley, 100.0), 1.0f);
	TestTrue(TEXT("15 days into shattering: half of it"), FMath::IsNearlyEqual(FMOCropGrowth::HarvestableFraction(Barley, 95.0 + 14.0 + 15.0), 0.5f, 0.001f));
	TestEqual(TEXT("past the overripe days: nothing left"), FMOCropGrowth::HarvestableFraction(Barley, 200.0), 0.0f);

	FMOCropDefinitionRow Odd;   // a broken row must not divide by zero
	Odd.DaysToSprout = 10.0f;
	Odd.DaysToRipe = 0.0f;
	Odd.DaysRipe = 0.0f;
	Odd.DaysOverripeToLost = 0.0f;
	TestEqual(TEXT("ripe at 0 days: overripe at once, not a crash"), FMOCropGrowth::StageAt(Odd, 0.0), EMOCropStage::Overripe);
	TestTrue(TEXT("finite fractions"), FMath::IsFinite(FMOCropGrowth::GrowthFraction(Odd, 1.0)) && FMath::IsFinite(FMOCropGrowth::HarvestableFraction(Odd, 1.0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCrop_GrowingPace, "MOFramework.Crop.GrowingPace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOCrop_GrowingPace::RunTest(const FString& Parameters)
{
	FMOWorldPace Pace;
	TestEqual(TEXT("a new world grows crops in real seasons"), Pace.For(EMOWorldPace::Growing), 1.0f);
	Pace.Growing = 2.0f;
	TestEqual(TEXT("the knob doubles the season"), Pace.For(EMOWorldPace::Growing), 2.0f);
	TestTrue(TEXT("Growing is a world knob (the New Game panel lists it)"), FMOWorldCreation::FindKnob(TEXT("Growing")) != nullptr);
	FMOWorldPace Other;
	TestTrue(TEXT("paces that differ only in Growing are not equal (co-op / save compare)"), Pace != Other);
	return true;
}

#endif
