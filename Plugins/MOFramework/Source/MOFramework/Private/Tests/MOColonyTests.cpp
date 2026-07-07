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
#include "MOCharacterHistoryComponent.h"
#include "MOWeatherIntegrationSubsystem.h"
#include "MOMetabolismComponent.h"
#include "MOEquipmentComponent.h"
#include "MOItemDefinitionRow.h"

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

// ============================================================================
// V2.3 relationship math
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Rel_SharedTimeGrows,
	"MOFramework.Colony.Relationships.SharedTimeGrows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Rel_SharedTimeGrows::RunTest(const FString& Parameters)
{
	// Strangers -> one shared game-hour moves the bond by the grow rate.
	const float After1h = UMOCharacterHistoryComponent::ComputeStrengthDelta(0.0f, 1.0f, 0.0f);
	TestEqual(TEXT("one shared hour = grow rate from zero"), After1h, 0.02f, 0.0001f);
	// Growth is asymptotic: the second stretch closes less absolute distance.
	const float After10h = UMOCharacterHistoryComponent::ComputeStrengthDelta(0.0f, 10.0f, 0.0f);
	const float After20h = UMOCharacterHistoryComponent::ComputeStrengthDelta(0.0f, 20.0f, 0.0f);
	TestTrue(TEXT("10h grows a real bond"), After10h > 0.15f);
	TestTrue(TEXT("second 10h adds less than the first (asymptote)"),
		(After20h - After10h) < After10h);
	// Never exceeds 1 even after absurd shared time.
	TestTrue(TEXT("clamped at 1"),
		UMOCharacterHistoryComponent::ComputeStrengthDelta(0.9f, 10000.0f, 0.0f) <= 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Rel_ApartDrifts,
	"MOFramework.Colony.Relationships.ApartDrifts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Rel_ApartDrifts::RunTest(const FString& Parameters)
{
	// Apart time fades a positive bond toward zero...
	const float Faded = UMOCharacterHistoryComponent::ComputeStrengthDelta(0.5f, 0.0f, 100.0f);
	TestEqual(TEXT("100h apart fades by drift rate"), Faded, 0.4f, 0.0001f);
	// ...but absence alone never flips friend to enemy (floors at 0).
	TestEqual(TEXT("fade floors at zero"),
		UMOCharacterHistoryComponent::ComputeStrengthDelta(0.1f, 0.0f, 100000.0f), 0.0f);
	// Hostility also cools toward zero with distance (ceilings at 0).
	TestEqual(TEXT("grudges cool toward zero, not past it"),
		UMOCharacterHistoryComponent::ComputeStrengthDelta(-0.1f, 0.0f, 100000.0f), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Rel_GrowthBeatsDrift,
	"MOFramework.Colony.Relationships.GrowthBeatsDrift",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Rel_GrowthBeatsDrift::RunTest(const FString& Parameters)
{
	// A villager sharing 8h/day and apart 16h/day must still NET GAIN —
	// otherwise no friendship could ever form in normal village life.
	float Strength = 0.0f;
	for (int32 Day = 0; Day < 30; ++Day)
	{
		Strength = UMOCharacterHistoryComponent::ComputeStrengthDelta(Strength, 8.0f, 0.0f);
		Strength = UMOCharacterHistoryComponent::ComputeStrengthDelta(Strength, 0.0f, 16.0f);
	}
	TestTrue(TEXT("30 days of village life forms a friend-grade bond"), Strength > 0.35f);
	return true;
}

// ============================================================================
// V2.4 seasons math
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Season_MonthMapping,
	"MOFramework.Colony.Seasons.MonthMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Season_MonthMapping::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("March = Spring"), UMOWeatherIntegrationSubsystem::SeasonIndexFromMonth(3), 0);
	TestEqual(TEXT("July = Summer"), UMOWeatherIntegrationSubsystem::SeasonIndexFromMonth(7), 1);
	TestEqual(TEXT("October = Autumn"), UMOWeatherIntegrationSubsystem::SeasonIndexFromMonth(10), 2);
	TestEqual(TEXT("January = Winter"), UMOWeatherIntegrationSubsystem::SeasonIndexFromMonth(1), 3);
	TestEqual(TEXT("December = Winter"), UMOWeatherIntegrationSubsystem::SeasonIndexFromMonth(12), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Season_BaselineClimate,
	"MOFramework.Colony.Seasons.BaselineClimate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Season_BaselineClimate::RunTest(const FString& Parameters)
{
	// Midsummer noon vs midwinter night — the poles of the climate model.
	const float SummerDay = UMOWeatherIntegrationSubsystem::ComputeSeasonalBaselineCelsius(202, 15.0f);
	const float WinterNight = UMOWeatherIntegrationSubsystem::ComputeSeasonalBaselineCelsius(20, 3.0f);
	TestTrue(TEXT("midsummer afternoon is warm (>20C)"), SummerDay > 20.0f);
	TestTrue(TEXT("midwinter night is below freezing"), WinterNight < 0.0f);
	// Diurnal: any given day, 15:00 beats 03:00.
	const float WinterDay = UMOWeatherIntegrationSubsystem::ComputeSeasonalBaselineCelsius(20, 15.0f);
	TestTrue(TEXT("day warmer than night in the same season"), WinterDay > WinterNight);
	// Annual mean holds: equinox mid-morning sits near the configured mean.
	const float Spring = UMOWeatherIntegrationSubsystem::ComputeSeasonalBaselineCelsius(111, 9.0f);
	TestTrue(TEXT("spring equinox near annual mean (11C +/- 6)"), FMath::Abs(Spring - 11.0f) < 6.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Season_ColdThermogenesis,
	"MOFramework.Colony.Seasons.ColdThermogenesis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Season_ColdThermogenesis::RunTest(const FString& Parameters)
{
	// Comfortable core temp: no extra burn.
	TestEqual(TEXT("neutral at 37.0C"), UMOMetabolismComponent::ComputeColdThermogenesisMultiplier(37.0f), 1.0f);
	TestEqual(TEXT("neutral at exactly 36.5C"), UMOMetabolismComponent::ComputeColdThermogenesisMultiplier(36.5f), 1.0f);
	// Mid-ramp: 35.25C is halfway to 34.0 -> halfway to 2.5x.
	TestEqual(TEXT("half-ramp at 35.25C"), UMOMetabolismComponent::ComputeColdThermogenesisMultiplier(35.25f), 1.75f, 0.001f);
	// Full shiver at 34.0C, clamped below.
	TestEqual(TEXT("max at 34.0C"), UMOMetabolismComponent::ComputeColdThermogenesisMultiplier(34.0f), 2.5f, 0.001f);
	TestEqual(TEXT("clamped below 34.0C"), UMOMetabolismComponent::ComputeColdThermogenesisMultiplier(30.0f), 2.5f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Season_ForageWindow,
	"MOFramework.Colony.Seasons.ForageWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Season_ForageWindow::RunTest(const FString& Parameters)
{
	FMOItemDefinitionRow Item;
	// Empty window = year-round.
	for (int32 SeasonIdx = 0; SeasonIdx < 4; ++SeasonIdx)
	{
		TestTrue(TEXT("empty window is year-round"), Item.IsInForageSeason(SeasonIdx));
	}
	// Berry window: summer + autumn only.
	Item.ForageSeasons = TEXT("Summer,Autumn");
	TestFalse(TEXT("berries not in spring"), Item.IsInForageSeason(0));
	TestTrue(TEXT("berries in summer"), Item.IsInForageSeason(1));
	TestTrue(TEXT("berries in autumn"), Item.IsInForageSeason(2));
	TestFalse(TEXT("berries not in winter"), Item.IsInForageSeason(3));
	return true;
}

// ============================================================================
// V2.5 family math
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Family_RomanceIsMutual,
	"MOFramework.Colony.Family.RomanceIsMutual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Family_RomanceIsMutual::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("mutual strong bond forms romance"),
		UMOColonyManagerSubsystem::ShouldBecomeRomantic(0.7f, 0.65f, 0.6f));
	TestFalse(TEXT("one-sided fondness is not a couple"),
		UMOColonyManagerSubsystem::ShouldBecomeRomantic(0.9f, 0.3f, 0.6f));
	TestFalse(TEXT("threshold is inclusive-exact both ways"),
		UMOColonyManagerSubsystem::ShouldBecomeRomantic(0.59f, 0.9f, 0.6f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Family_GestationClock,
	"MOFramework.Colony.Family.GestationClock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Family_GestationClock::RunTest(const FString& Parameters)
{
	// 6480 game-hours gestation: halfway at 3240h, due at 6480h.
	const double H = 3600.0;
	TestEqual(TEXT("conception = 0 progress"),
		UMOColonyManagerSubsystem::ComputePregnancyProgress(1000.0, 1000.0, 6480.0f), 0.0f);
	TestEqual(TEXT("halfway"),
		UMOColonyManagerSubsystem::ComputePregnancyProgress(0.0, 3240.0 * H, 6480.0f), 0.5f, 0.001f);
	TestTrue(TEXT("due at term"),
		UMOColonyManagerSubsystem::ComputePregnancyProgress(0.0, 6480.0 * H, 6480.0f) >= 1.0f);
	TestEqual(TEXT("degenerate gestation guards to 0"),
		UMOColonyManagerSubsystem::ComputePregnancyProgress(0.0, 100.0, 0.0f), 0.0f);
	return true;
}

// ============================================================================
// Codex review: clothing insulation math (H12 family)
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColony_Insulation_WarmthSum,
	"MOFramework.Colony.Insulation.WarmthSum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMOColony_Insulation_WarmthSum::RunTest(const FString& Parameters)
{
	// A full winter kit sums per-piece warmth, capped at the wearable ceiling.
	TestEqual(TEXT("no clothes = no bonus"),
		UMOEquipmentComponent::ComputeInsulationFromWarmth({}), 0.0f);
	TestEqual(TEXT("kit sums (0.15+0.1+0.05)"),
		UMOEquipmentComponent::ComputeInsulationFromWarmth({0.15f, 0.1f, 0.05f}), 0.3f, 0.001f);
	TestEqual(TEXT("stacked furs cap at the ceiling (0.4)"),
		UMOEquipmentComponent::ComputeInsulationFromWarmth({0.3f, 0.3f, 0.3f}), 0.4f, 0.001f);
	TestEqual(TEXT("negative warmth ignored, never chills"),
		UMOEquipmentComponent::ComputeInsulationFromWarmth({-0.5f, 0.2f}), 0.2f, 0.001f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
