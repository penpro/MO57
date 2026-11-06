#include "Misc/AutomationTest.h"
#include "MOMetabolismComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

// A day without vitamin C stopped wounds healing (N-310, the June audit's L9): every store fell a flat 100 % a day (Principle 11: no game-y decay rates).
// Expected: a week on no vitamin C heals as well as a full store, and deficiency still comes, at about 60 days; failing before: 0 % and no healing at 24 hours.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMetabolismNutrientStoresLast, "MOFramework.Medical.Metabolism.NutrientStoresLast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMetabolismNutrientStoresLast::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();
	const float HealingWhenFull = Metabolism->GetWoundHealingMultiplier();

	// One game day, a second at a time as the tick does: a year's store must move too, though one second's use is below a float's resolution at 100 %.
	for (int32 Second = 0; Second < 86400; ++Second)
	{
		Metabolism->ProcessNutrientDecay(1.0f);
	}
	TestEqual(TEXT("a day takes a sixtieth of the way down from vitamin C"), Metabolism->Nutrients.VitaminC, 100.0f - 70.0f / 60.0f, 0.05f);
	TestEqual(TEXT("and a 365th from vitamin A: a year's store is not stuck"), Metabolism->Nutrients.VitaminA, 100.0f - 70.0f / 365.0f, 0.01f);
	TestEqual(TEXT("potassium goes fastest"), Metabolism->Nutrients.Potassium, 100.0f - 70.0f / 14.0f, 0.05f);

	// A week from full.
	Metabolism->ProcessNutrientDecay(6.0f * 86400.0f);
	TestEqual(TEXT("a week without fruit heals as well as a full store"), Metabolism->GetWoundHealingMultiplier(), HealingWhenFull, 0.001f);
	TestFalse(TEXT("and is no deficiency"), Metabolism->Nutrients.HasVitaminCDeficiency());

	// CONTROL: the store does run out, at the line and when it should, and healing falls with it.
	Metabolism->ProcessNutrientDecay(52.0f * 86400.0f);
	TestFalse(TEXT("CONTROL: day 59: not yet"), Metabolism->Nutrients.HasVitaminCDeficiency());
	Metabolism->ProcessNutrientDecay(2.0f * 86400.0f);
	TestTrue(TEXT("CONTROL: day 61: scurvy"), Metabolism->Nutrients.HasVitaminCDeficiency());
	TestTrue(TEXT("CONTROL: and wounds heal slower"), Metabolism->GetWoundHealingMultiplier() < HealingWhenFull);
	return true;
}

#endif
