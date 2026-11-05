// The thermal moodle does not flicker (UMOVitalsComponent::ClassifyThermalComfort). The load moves with every gust; on a line with no margin the bucket, and the
// moodle with it, flipped at every change (the June audit's M16). Expected: one change over a load wandering across 0.5; failing before: one per step.
// Run: python Tools/ue.py auto --filter "MOFramework.Medical.Vitals.ThermalComfort"

#include "Misc/AutomationTest.h"
#include "MOVitalsComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOVitals_ThermalComfortMargin,
	"MOFramework.Medical.Vitals.ThermalComfortMargin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOVitals_ThermalComfortMargin::RunTest(const FString& Parameters)
{
	const FMOThermalComfortThresholds T;
	const auto Classify = [&T](float Core, float Load, EMOThermalComfort Current) { return UMOVitalsComponent::ClassifyThermalComfort(Core, Load, T, Current); };

	// Entering takes the line, as before.
	TestEqual(TEXT("37 C, resting: comfortable"), Classify(37.0f, 0.0f, EMOThermalComfort::Comfortable), EMOThermalComfort::Comfortable);
	TestEqual(TEXT("under ColdBelow: cold"), Classify(T.ColdBelow - 0.01f, 0.0f, EMOThermalComfort::Comfortable), EMOThermalComfort::Cold);
	TestEqual(TEXT("under VeryColdBelow: very cold"), Classify(T.VeryColdBelow - 0.01f, 0.0f, EMOThermalComfort::Cold), EMOThermalComfort::VeryCold);
	TestEqual(TEXT("at HotAbove: hot"), Classify(T.HotAbove, 0.0f, EMOThermalComfort::Comfortable), EMOThermalComfort::Hot);
	TestEqual(TEXT("at VeryHotAbove: very hot"), Classify(T.VeryHotAbove, 0.0f, EMOThermalComfort::Hot), EMOThermalComfort::VeryHot);
	TestEqual(TEXT("shivering hard: cold with the core holding"), Classify(37.0f, 0.5f, EMOThermalComfort::Comfortable), EMOThermalComfort::Cold);
	TestEqual(TEXT("sweating hard: hot with the core holding"), Classify(37.0f, -0.6f, EMOThermalComfort::Comfortable), EMOThermalComfort::Hot);

	// A load wandering across the line changes the bucket once, and a clear fall changes it back.
	EMOThermalComfort Bucket = EMOThermalComfort::Comfortable;
	int32 Changes = 0;
	for (const float Load : { 0.49f, 0.51f, 0.49f, 0.51f, 0.48f, 0.52f, 0.47f })
	{
		const EMOThermalComfort Next = Classify(37.0f, Load, Bucket);
		Changes += Next != Bucket ? 1 : 0;
		Bucket = Next;
	}
	TestEqual(TEXT("one change over a load wandering across 0.5"), Changes, 1);
	TestEqual(TEXT("still cold"), Bucket, EMOThermalComfort::Cold);
	TestEqual(TEXT("the load well down: comfortable again"), Classify(37.0f, 0.39f, EMOThermalComfort::Cold), EMOThermalComfort::Comfortable);

	// The same for the core on a line, each way.
	TestEqual(TEXT("cold stays cold just over the line"), Classify(T.ColdBelow + 0.05f, 0.0f, EMOThermalComfort::Cold), EMOThermalComfort::Cold);
	TestEqual(TEXT("cold ends a margin over it"), Classify(T.ColdBelow + 0.11f, 0.0f, EMOThermalComfort::Cold), EMOThermalComfort::Comfortable);
	TestEqual(TEXT("very cold stays very cold just over its line"), Classify(T.VeryColdBelow + 0.05f, 0.0f, EMOThermalComfort::VeryCold), EMOThermalComfort::VeryCold);
	TestEqual(TEXT("hot stays hot just under the line"), Classify(T.HotAbove - 0.05f, 0.0f, EMOThermalComfort::Hot), EMOThermalComfort::Hot);
	TestEqual(TEXT("hot ends a margin under it"), Classify(T.HotAbove - 0.11f, 0.0f, EMOThermalComfort::Hot), EMOThermalComfort::Comfortable);
	TestEqual(TEXT("comfortable does not borrow the margin"), Classify(T.HotAbove - 0.05f, 0.0f, EMOThermalComfort::Comfortable), EMOThermalComfort::Comfortable);
	return true;
}

#endif
