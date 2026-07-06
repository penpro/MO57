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
#include "MOSkillsComponent.h"

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


// ============================================================================
// V2.1 quota decision math
// ============================================================================

namespace MOColonyTestData
{
	FMOColonyQuota Quota(const TCHAR* Item, const TCHAR* Recipe, int32 Target, int32 Prio = 0)
	{
		FMOColonyQuota Q;
		Q.OutputItemId = Item;
		Q.RecipeId = Recipe;
		Q.TargetCount = Target;
		Q.Priority = Prio;
		return Q;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Quota_FillsBelowTarget,
	"MOFramework.Colony.Quota.FillsBelowTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Quota_FillsBelowTarget::RunTest(const FString& Parameters)
{
	TArray<FMOColonyQuota> Quotas = { MOColonyTestData::Quota(TEXT("Flake"), TEXT("Knap"), 8) };
	TMap<FName, int32> Stock;
	Stock.Add(TEXT("Flake"), 3);
	const TArray<FName> Work = UMOColonyManagerSubsystem::DecideQuotaWork(Quotas, Stock, {}, 2);
	TestEqual(TEXT("one assignment"), Work.Num(), 1);
	TestTrue(TEXT("assigns the quota recipe"), Work.Num() == 1 && Work[0] == FName(TEXT("Knap")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Quota_MetQuotaIdles,
	"MOFramework.Colony.Quota.MetQuotaIdles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Quota_MetQuotaIdles::RunTest(const FString& Parameters)
{
	TArray<FMOColonyQuota> Quotas = { MOColonyTestData::Quota(TEXT("Flake"), TEXT("Knap"), 8) };
	TMap<FName, int32> Stock;
	Stock.Add(TEXT("Flake"), 8);
	const TArray<FName> Work = UMOColonyManagerSubsystem::DecideQuotaWork(Quotas, Stock, {}, 3);
	TestEqual(TEXT("no busywork when quota met"), Work.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Quota_PriorityWinsScarceHands,
	"MOFramework.Colony.Quota.PriorityWinsScarceHands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Quota_PriorityWinsScarceHands::RunTest(const FString& Parameters)
{
	TArray<FMOColonyQuota> Quotas = {
		MOColonyTestData::Quota(TEXT("Rope"), TEXT("TwistRope"), 4, /*Prio=*/1),
		MOColonyTestData::Quota(TEXT("Flake"), TEXT("Knap"), 8, /*Prio=*/5),
	};
	TMap<FName, int32> Stock;   // both empty
	const TArray<FName> Work = UMOColonyManagerSubsystem::DecideQuotaWork(Quotas, Stock, {}, 1);
	TestEqual(TEXT("one idle hand, one job"), Work.Num(), 1);
	TestTrue(TEXT("higher priority quota first"), Work.Num() == 1 && Work[0] == FName(TEXT("Knap")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Quota_InFlightNotDoubled,
	"MOFramework.Colony.Quota.InFlightNotDoubled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Quota_InFlightNotDoubled::RunTest(const FString& Parameters)
{
	TArray<FMOColonyQuota> Quotas = { MOColonyTestData::Quota(TEXT("Flake"), TEXT("Knap"), 8) };
	TMap<FName, int32> Stock;
	TSet<FName> InFlight;
	InFlight.Add(TEXT("Knap"));
	const TArray<FName> Work = UMOColonyManagerSubsystem::DecideQuotaWork(Quotas, Stock, InFlight, 3);
	TestEqual(TEXT("in-flight order not double-assigned"), Work.Num(), 0);
	return true;
}


// ============================================================================
// V2.2 decay + teaching math
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Decay_GraceWindowProtects,
	"MOFramework.Colony.Decay.GraceWindowProtects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Decay_GraceWindowProtects::RunTest(const FString& Parameters)
{
	// Used 10h ago, grace 24h: no decay yet.
	TestEqual(TEXT("inside grace = zero decay"),
		UMOSkillsComponent::ComputeSkillDecayXP(2.0f, 10.0f, 24.0f, 1.5f), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Decay_RustPastGrace,
	"MOFramework.Colony.Decay.RustPastGrace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Decay_RustPastGrace::RunTest(const FString& Parameters)
{
	// Unused 30h (6h past grace), 2h elapsed this pass: full 2h decays.
	TestEqual(TEXT("past grace decays per elapsed hour"),
		UMOSkillsComponent::ComputeSkillDecayXP(2.0f, 30.0f, 24.0f, 1.5f), 3.0f);
	// Crossing the boundary mid-pass: unused 25h, pass 4h -> only 1h decays.
	TestEqual(TEXT("boundary crossing decays only the past-grace slice"),
		UMOSkillsComponent::ComputeSkillDecayXP(4.0f, 25.0f, 24.0f, 1.5f), 1.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Teach_DoubleSpeed,
	"MOFramework.Colony.Teach.DoubleSpeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Teach_DoubleSpeed::RunTest(const FString& Parameters)
{
	// 3 game-hours at base 20 XP/h -> taught = 120 (2x the 60 of doing).
	TestEqual(TEXT("teaching is exactly 2x direct action"),
		UMOColonyManagerSubsystem::ComputeTeachXP(3.0f, 20.0f), 120.0f);
	TestEqual(TEXT("no time, no XP"),
		UMOColonyManagerSubsystem::ComputeTeachXP(0.0f, 20.0f), 0.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
