// Drinking physiology and the shape of an illness course (FMOWaterPhysiology): the numbers Docs/Water_And_Illness.md argues for, pinned. Headless.

#include "Misc/AutomationTest.h"
#include "MOWaterPhysiology.h"
#include "MOWaterSourceSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWaterPhysiology_Draught,
	"MOFramework.Water.Physiology",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWaterPhysiology_Draught::RunTest(const FString& Parameters)
{
	// Fresh water is all body water; sea water costs about half a litre per litre (1.5 L of urine to clear its salt) and loads ~7x a day's sodium.
	TestEqual(TEXT("fresh water: every ml stays"), FMOWaterPhysiology::NetBodyWaterMl(250.0f, 0.0f), 250.0f);
	TestEqual(TEXT("sea water: a net loss of half the volume"), FMOWaterPhysiology::NetBodyWaterMl(1000.0f, 35.0f), -500.0f, 0.01f);
	TestTrue(TEXT("brackish water (5 g/L) still hydrates"), FMOWaterPhysiology::NetBodyWaterMl(1000.0f, 5.0f) > 0.0f);
	TestEqual(TEXT("break-even salinity ~23 g/L"), FMOWaterPhysiology::NetBodyWaterMl(1000.0f, 35.0f / 1.5f), 0.0f, 0.5f);
	TestEqual(TEXT("a litre of sea water = 720 % of a day's sodium"), FMOWaterPhysiology::SodiumPercentOfDailyNeed(1000.0f, 35.0f), 720.0f, 0.5f);
	TestEqual(TEXT("fresh water carries no sodium"), FMOWaterPhysiology::SodiumPercentOfDailyNeed(1000.0f, 0.0f), 0.0f);

	// Illness chance: independent litres; small volumes scale linearly, zero risk is zero.
	TestEqual(TEXT("one litre at 1 %/L = 1 %"), FMOWaterPhysiology::IllnessChance(1000.0f, 0.01f), 0.01f, 1e-5f);
	TestEqual(TEXT("ten litres at 1 %/L = 9.6 %"), FMOWaterPhysiology::IllnessChance(10000.0f, 0.01f), 1.0f - FMath::Pow(0.99f, 10.0f), 1e-4f);
	TestEqual(TEXT("a 250 ml draught at 1 %/L ~ 0.25 %"), FMOWaterPhysiology::IllnessChance(250.0f, 0.01f), 0.0025f, 1e-4f);
	TestEqual(TEXT("boiled (0 %/L) never"), FMOWaterPhysiology::IllnessChance(5000.0f, 0.0f), 0.0f);

	// The calibration claim: a typical trip (~10 L untreated) at the river / lake rates lands in the 5-10 % ill-per-trip band the trip studies support.
	const FMOWaterSourceProfile River = UMOWaterSourceSettings::GetProfile(EMOWaterBodyKind::River);
	const FMOWaterSourceProfile Lake = UMOWaterSourceSettings::GetProfile(EMOWaterBodyKind::Lake);
	const FMOWaterSourceProfile Sea = UMOWaterSourceSettings::GetProfile(EMOWaterBodyKind::Sea);
	const float RiverTrip = FMOWaterPhysiology::IllnessChance(10000.0f, River.IllnessChancePerLitre);
	const float LakeTrip = FMOWaterPhysiology::IllnessChance(10000.0f, Lake.IllnessChancePerLitre);
	AddInfo(FString::Printf(TEXT("10 L untreated: river %.1f %%, lake %.1f %%"), RiverTrip * 100.0f, LakeTrip * 100.0f));
	TestTrue(TEXT("a trip's worth of river water: 3-10 % ill"), RiverTrip > 0.03f && RiverTrip < 0.10f);
	TestTrue(TEXT("a trip's worth of lake water: 5-15 % ill"), LakeTrip > 0.05f && LakeTrip < 0.15f);
	TestTrue(TEXT("flowing water is safer than still"), River.IllnessChancePerLitre < Lake.IllnessChancePerLitre);
	TestTrue(TEXT("the sea dehydrates"), FMOWaterPhysiology::NetBodyWaterMl(250.0f, Sea.SalinityGPerL) < 0.0f);
	TestTrue(TEXT("rivers and lakes hydrate"), FMOWaterPhysiology::NetBodyWaterMl(250.0f, River.SalinityGPerL) > 0.0f && FMOWaterPhysiology::NetBodyWaterMl(250.0f, Lake.SalinityGPerL) > 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWaterPhysiology_Course,
	"MOFramework.Medical.IllnessCourse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWaterPhysiology_Course::RunTest(const FString& Parameters)
{
	// A self-limiting illness: nothing before it starts, a rise to the peak over the first third, a slow mend to nothing at the end, gone after.
	const float Course = 3600.0f * 12.0f, Peak = 60.0f;
	TestEqual(TEXT("before the course"), FMOWaterPhysiology::CourseSeverity(0.0f, Course, Peak), 0.0f);
	TestEqual(TEXT("the peak at a third"), FMOWaterPhysiology::CourseSeverity(Course / 3.0f, Course, Peak), Peak, 0.01f);
	TestEqual(TEXT("half way down at two thirds"), FMOWaterPhysiology::CourseSeverity(Course * 2.0f / 3.0f, Course, Peak), Peak * 0.5f, 0.01f);
	TestEqual(TEXT("over at the end"), FMOWaterPhysiology::CourseSeverity(Course, Course, Peak), 0.0f);
	TestEqual(TEXT("over after the end"), FMOWaterPhysiology::CourseSeverity(Course * 2.0f, Course, Peak), 0.0f);
	float Max = 0.0f, Last = 0.0f;
	bool bRiseThenFall = true, bFalling = false;
	for (float T = 0.0f; T <= Course; T += Course / 200.0f)
	{
		const float S = FMOWaterPhysiology::CourseSeverity(T, Course, Peak);
		Max = FMath::Max(Max, S);
		if (S < Last - 1e-4f)
		{
			bFalling = true;
		}
		else if (bFalling && S > Last + 1e-4f)
		{
			bRiseThenFall = false;
		}
		Last = S;
	}
	TestTrue(TEXT("one rise, one fall"), bRiseThenFall);
	TestTrue(TEXT("never above the peak"), Max <= Peak + 0.01f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
