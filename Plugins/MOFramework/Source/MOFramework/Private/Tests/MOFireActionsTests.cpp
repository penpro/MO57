// Lighting a fire is a real, chancy attempt (FMOFireActions::LightChance): practice helps, a better tool (a bow drill) beats an improvised hand drill, rain and wet
// hands hurt, nothing is certain.

#include "Misc/AutomationTest.h"
#include "MOFireActions.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOFireActions_LightChance,
	"MOFramework.Fire.LightChance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOFireActions_LightChance::RunTest(const FString& Parameters)
{
	const float Improvised = FMOFireActions::ImprovisedDrillEffectiveness;
	constexpr float BowDrill = 0.7f;   // BowDrill01's Firestarter effectiveness
	const float Novice = FMOFireActions::LightChance(Improvised, 0, false, 0.0f);
	TestEqual(TEXT("a novice's improvised hand drill in the dry: 20 %"), Novice, 0.2f, 1e-5f);
	TestTrue(TEXT("practice helps"), FMOFireActions::LightChance(Improvised, 5, false, 0.0f) > Novice);
	TestTrue(TEXT("a bow drill beats an improvised drill"), FMOFireActions::LightChance(BowDrill, 0, false, 0.0f) > Novice);
	TestEqual(TEXT("rain cuts it to a third"), FMOFireActions::LightChance(Improvised, 0, true, 0.0f), Novice * 0.3f, 1e-5f);
	TestEqual(TEXT("wet hands cut it"), FMOFireActions::LightChance(Improvised, 0, false, 0.8f), Novice * 0.6f, 1e-5f);
	TestEqual(TEXT("a little damp does not"), FMOFireActions::LightChance(Improvised, 0, false, 0.3f), Novice, 1e-5f);
	TestEqual(TEXT("never certain: capped at 95 %"), FMOFireActions::LightChance(BowDrill, 50, false, 0.0f), 0.95f, 1e-5f);
	TestTrue(TEXT("a master in the rain still struggles"), FMOFireActions::LightChance(BowDrill, 10, true, 0.9f) < 0.3f);
	TestEqual(TEXT("no tool, no chance"), FMOFireActions::LightChance(0.0f, 0, false, 0.0f), 0.0f, 1e-5f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
