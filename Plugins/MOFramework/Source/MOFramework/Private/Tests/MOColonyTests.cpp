/**
 * Headless tests for the V1 settlement-loop math (pipeline V1 gate c).
 * Run: python Tools/ue.py auto "MOFramework.Colony."
 *
 * ComputeVillagerMood is a pure function over REAL-sim inputs; these tests
 * pin its contract so upkeep tuning can't silently invert it.
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "MOColonyManagerSubsystem.h"
#include "MOColonyTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MOColonyTestData
{
	FMOVillagerMoodInputs Content()
	{
		FMOVillagerMoodInputs In;
		In.bHasHome = true;
		return In;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Mood_ContentBaseline,
	"MOFramework.Colony.Mood.ContentBaseline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Mood_ContentBaseline::RunTest(const FString& Parameters)
{
	const float Mood = UMOColonyManagerSubsystem::ComputeVillagerMood(MOColonyTestData::Content());
	TestTrue(TEXT("fed, dry, housed, calm villager is content (>0.7)"), Mood > 0.7f);
	TestTrue(TEXT("mood clamped to <=1"), Mood <= 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Mood_StarvationBites,
	"MOFramework.Colony.Mood.StarvationBites",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Mood_StarvationBites::RunTest(const FString& Parameters)
{
	FMOVillagerMoodInputs In = MOColonyTestData::Content();
	const float Fed = UMOColonyManagerSubsystem::ComputeVillagerMood(In);
	In.bStarving = true;
	const float Starving = UMOColonyManagerSubsystem::ComputeVillagerMood(In);
	TestTrue(TEXT("starvation drops mood by >=0.25"), Fed - Starving >= 0.25f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Mood_HomelessnessDecays,
	"MOFramework.Colony.Mood.HomelessnessDecays",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Mood_HomelessnessDecays::RunTest(const FString& Parameters)
{
	FMOVillagerMoodInputs Housed = MOColonyTestData::Content();
	FMOVillagerMoodInputs Fresh = Housed;   Fresh.bHasHome = false;   Fresh.UnhousedHours = 0.0f;
	FMOVillagerMoodInputs DayOut = Housed;  DayOut.bHasHome = false;  DayOut.UnhousedHours = 24.0f;

	const float MoodHoused = UMOColonyManagerSubsystem::ComputeVillagerMood(Housed);
	const float MoodFresh = UMOColonyManagerSubsystem::ComputeVillagerMood(Fresh);
	const float MoodDayOut = UMOColonyManagerSubsystem::ComputeVillagerMood(DayOut);

	TestTrue(TEXT("housed beats freshly-unhoused"), MoodHoused > MoodFresh);
	TestTrue(TEXT("a day unhoused decays further"), MoodFresh > MoodDayOut);
	TestTrue(TEXT("a day unhoused costs >=0.15 vs housed"), MoodHoused - MoodDayOut >= 0.15f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Mood_VolatileCrashesHarder,
	"MOFramework.Colony.Mood.VolatileCrashesHarder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Mood_VolatileCrashesHarder::RunTest(const FString& Parameters)
{
	FMOVillagerMoodInputs Stable = MOColonyTestData::Content();
	Stable.bStarving = true;
	Stable.MoodVarianceModifier = 0.5f;      // rock-steady personality
	FMOVillagerMoodInputs Volatile = Stable;
	Volatile.MoodVarianceModifier = 1.5f;    // swings hard

	const float MoodStable = UMOColonyManagerSubsystem::ComputeVillagerMood(Stable);
	const float MoodVolatile = UMOColonyManagerSubsystem::ComputeVillagerMood(Volatile);
	TestTrue(TEXT("same pressure hits volatile harder"), MoodStable > MoodVolatile);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Mood_ClampedAtRockBottom,
	"MOFramework.Colony.Mood.ClampedAtRockBottom",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Mood_ClampedAtRockBottom::RunTest(const FString& Parameters)
{
	FMOVillagerMoodInputs In;
	In.bStarving = true;
	In.bDehydrated = true;
	In.Wetness = 1.0f;
	In.Shock = 100.0f;
	In.TraumaticStress = 100.0f;
	In.MoraleFatigue = 100.0f;
	In.bHasHome = false;
	In.UnhousedHours = 200.0f;
	In.MoodVarianceModifier = 2.0f;
	const float Mood = UMOColonyManagerSubsystem::ComputeVillagerMood(In);
	TestEqual(TEXT("worst case clamps to 0"), Mood, 0.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
