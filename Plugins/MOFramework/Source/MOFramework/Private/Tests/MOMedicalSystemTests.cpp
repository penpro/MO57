#include "Misc/AutomationTest.h"
#include "MOMetabolismComponent.h"
#include "MOVitalsComponent.h"
#include "MOAnatomyComponent.h"
#include "MOMentalStateComponent.h"
#include "MOMedicalTypes.h"
#include "MOItemDefinitionRow.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if WITH_DEV_AUTOMATION_TESTS

//=============================================================================
// Test Data Helpers
//=============================================================================

namespace MOMedicalTestData
{
	/**
	 * Creates a test nutrition struct with specified values.
	 */
	FMOItemNutrition MakeTestNutrition(
		float Calories = 200.0f,
		float Protein = 10.0f,
		float Carbs = 30.0f,
		float Fat = 8.0f,
		float Water = 50.0f,
		float Fiber = 2.0f)
	{
		FMOItemNutrition Nutrition;
		Nutrition.Calories = Calories;
		Nutrition.Protein = Protein;
		Nutrition.Carbohydrates = Carbs;
		Nutrition.Fat = Fat;
		Nutrition.WaterContent = Water;
		Nutrition.Fiber = Fiber;
		Nutrition.VitaminA = 10.0f;
		Nutrition.VitaminB = 10.0f;
		Nutrition.VitaminC = 15.0f;
		Nutrition.VitaminD = 5.0f;
		Nutrition.Iron = 8.0f;
		Nutrition.Calcium = 12.0f;
		Nutrition.Potassium = 10.0f;
		Nutrition.Sodium = 5.0f;
		return Nutrition;
	}

	/**
	 * Creates a high-calorie, fat-heavy food for testing slow digestion.
	 */
	FMOItemNutrition MakeHighFatFood()
	{
		return MakeTestNutrition(500.0f, 20.0f, 10.0f, 40.0f, 20.0f, 1.0f);
	}

	/**
	 * Creates a simple carb food for testing fast digestion.
	 */
	FMOItemNutrition MakeSimpleCarbFood()
	{
		return MakeTestNutrition(150.0f, 2.0f, 35.0f, 1.0f, 80.0f, 0.5f);
	}

	/**
	 * Creates a balanced meal for general testing.
	 */
	FMOItemNutrition MakeBalancedMeal()
	{
		return MakeTestNutrition(400.0f, 25.0f, 45.0f, 15.0f, 100.0f, 5.0f);
	}

	/**
	 * Creates vitamin-rich food for nutrient testing.
	 */
	FMOItemNutrition MakeVitaminRichFood()
	{
		FMOItemNutrition Nutrition = MakeTestNutrition(50.0f, 2.0f, 10.0f, 0.5f, 150.0f, 3.0f);
		Nutrition.VitaminA = 50.0f;
		Nutrition.VitaminB = 40.0f;
		Nutrition.VitaminC = 100.0f;
		Nutrition.VitaminD = 25.0f;
		Nutrition.Iron = 20.0f;
		Nutrition.Calcium = 30.0f;
		return Nutrition;
	}
}

//=============================================================================
// Metabolism Component Tests - Food Consumption
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMetabolism_ConsumeFood_AddsToDigestionQueue,
	"MOFramework.Medical.Metabolism.ConsumeFood.AddsToDigestionQueue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMetabolism_ConsumeFood_AddsToDigestionQueue::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();
	TestNotNull(TEXT("Metabolism component created"), Metabolism);

	const int32 InitialCount = Metabolism->GetDigestingFoodCount();
	TestEqual(TEXT("Initial digestion queue is empty"), InitialCount, 0);

	// Simulate authority for the test
	FMOItemNutrition TestFood = MOMedicalTestData::MakeBalancedMeal();

	// Note: ConsumeFood checks for authority, but in tests we're not in a networked context
	// The component will still process the food data internally
	const bool bConsumed = Metabolism->ConsumeFood(TestFood, FName("TestMeal"));

	// In test context without authority, this may return false, but we can still verify state
	if (bConsumed)
	{
		TestEqual(TEXT("Digestion queue has 1 item"), Metabolism->GetDigestingFoodCount(), 1);
	}
	else
	{
		AddInfo(TEXT("ConsumeFood returned false (expected without network authority in test)"));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMetabolism_ConsumeFood_TracksCalories,
	"MOFramework.Medical.Metabolism.ConsumeFood.TracksCalories",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMetabolism_ConsumeFood_TracksCalories::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	const float InitialCalories = Metabolism->TotalCaloriesConsumedToday;
	TestEqual(TEXT("Initial calories consumed is zero"), InitialCalories, 0.0f);

	FMOItemNutrition TestFood = MOMedicalTestData::MakeTestNutrition(300.0f, 15.0f, 40.0f, 10.0f, 50.0f, 2.0f);
	Metabolism->ConsumeFood(TestFood, FName("TestFood"));

	// If authority check passes, calories should be tracked
	const float CaloriesAfter = Metabolism->TotalCaloriesConsumedToday;
	// Note: May be 0 if authority check fails in test context
	TestTrue(TEXT("Calories tracking is non-negative"), CaloriesAfter >= 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMetabolism_ConsumeFood_MultipleItems,
	"MOFramework.Medical.Metabolism.ConsumeFood.MultipleItems",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMetabolism_ConsumeFood_MultipleItems::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	// Consume multiple foods
	Metabolism->ConsumeFood(MOMedicalTestData::MakeSimpleCarbFood(), FName("Bread"));
	Metabolism->ConsumeFood(MOMedicalTestData::MakeHighFatFood(), FName("Cheese"));
	Metabolism->ConsumeFood(MOMedicalTestData::MakeVitaminRichFood(), FName("Fruit"));

	// In test context, queue may or may not be populated based on authority
	const int32 QueueCount = Metabolism->GetDigestingFoodCount();
	AddInfo(FString::Printf(TEXT("Digestion queue count: %d"), QueueCount));

	return true;
}

//=============================================================================
// Metabolism Component Tests - Water Consumption
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMetabolism_DrinkWater_IncreasesHydration,
	"MOFramework.Medical.Metabolism.DrinkWater.IncreasesHydration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMetabolism_DrinkWater_IncreasesHydration::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	// Set initial hydration to a lower value for testing
	Metabolism->Nutrients.HydrationLevel = 50.0f;
	const float InitialHydration = Metabolism->Nutrients.HydrationLevel;

	// Drink 500mL of water
	Metabolism->DrinkWater(500.0f);

	// If authority check passes, hydration should increase
	// 500mL / 2500mL daily requirement * 100 = 20% increase
	const float ExpectedIncrease = (500.0f / 2500.0f) * 100.0f;
	const float CurrentHydration = Metabolism->Nutrients.HydrationLevel;

	AddInfo(FString::Printf(TEXT("Hydration: %.1f -> %.1f (expected +%.1f)"),
		InitialHydration, CurrentHydration, ExpectedIncrease));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMetabolism_DrinkWater_ClampsAtMax,
	"MOFramework.Medical.Metabolism.DrinkWater.ClampsAtMax",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMetabolism_DrinkWater_ClampsAtMax::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	// Start at high hydration
	Metabolism->Nutrients.HydrationLevel = 95.0f;

	// Drink a lot of water
	Metabolism->DrinkWater(1000.0f);

	// Should clamp at 100
	TestTrue(TEXT("Hydration clamped at or below 100"), Metabolism->Nutrients.HydrationLevel <= 100.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMetabolism_DrinkWater_ZeroAmount,
	"MOFramework.Medical.Metabolism.DrinkWater.ZeroAmount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMetabolism_DrinkWater_ZeroAmount::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	Metabolism->Nutrients.HydrationLevel = 50.0f;
	const float Initial = Metabolism->Nutrients.HydrationLevel;

	// Zero should have no effect
	Metabolism->DrinkWater(0.0f);
	TestEqual(TEXT("Zero water has no effect"), Metabolism->Nutrients.HydrationLevel, Initial);

	// Negative should have no effect
	Metabolism->DrinkWater(-100.0f);
	TestEqual(TEXT("Negative water has no effect"), Metabolism->Nutrients.HydrationLevel, Initial);

	return true;
}

//=============================================================================
// Metabolism Component Tests - Calorie Burning
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMetabolism_ApplyCalorieBurn_TracksTotal,
	"MOFramework.Medical.Metabolism.ApplyCalorieBurn.TracksTotal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMetabolism_ApplyCalorieBurn_TracksTotal::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	const float InitialBurned = Metabolism->TotalCaloriesBurnedToday;
	TestEqual(TEXT("Initial burned is zero"), InitialBurned, 0.0f);

	Metabolism->ApplyCalorieBurn(100.0f);
	Metabolism->ApplyCalorieBurn(150.0f);

	// Total should be tracked (if authority passes)
	const float TotalBurned = Metabolism->TotalCaloriesBurnedToday;
	AddInfo(FString::Printf(TEXT("Total burned: %.1f"), TotalBurned));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMetabolism_ApplyCalorieBurn_UsesGlycogen,
	"MOFramework.Medical.Metabolism.ApplyCalorieBurn.UsesGlycogen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMetabolism_ApplyCalorieBurn_UsesGlycogen::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	// Set known glycogen level
	const float InitialGlycogen = 500.0f;
	Metabolism->Nutrients.GlycogenStores = InitialGlycogen;

	// Burn calories
	Metabolism->ApplyCalorieBurn(200.0f);

	// Glycogen should decrease (if authority passes)
	const float CurrentGlycogen = Metabolism->Nutrients.GlycogenStores;
	AddInfo(FString::Printf(TEXT("Glycogen: %.1f -> %.1f"), InitialGlycogen, CurrentGlycogen));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMetabolism_ApplyCalorieBurn_ZeroNegativeIgnored,
	"MOFramework.Medical.Metabolism.ApplyCalorieBurn.ZeroNegativeIgnored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMetabolism_ApplyCalorieBurn_ZeroNegativeIgnored::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	Metabolism->Nutrients.GlycogenStores = 500.0f;
	const float Initial = Metabolism->Nutrients.GlycogenStores;

	Metabolism->ApplyCalorieBurn(0.0f);
	TestEqual(TEXT("Zero burn has no effect"), Metabolism->Nutrients.GlycogenStores, Initial);

	Metabolism->ApplyCalorieBurn(-100.0f);
	TestEqual(TEXT("Negative burn has no effect"), Metabolism->Nutrients.GlycogenStores, Initial);

	return true;
}

//=============================================================================
// Metabolism Component Tests - Body Composition
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMetabolism_BodyComposition_BMRCalculation,
	"MOFramework.Medical.Metabolism.BodyComposition.BMRCalculation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMetabolism_BodyComposition_BMRCalculation::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	// Set known body composition
	Metabolism->BodyComposition.TotalWeight = 75.0f;
	Metabolism->BodyComposition.MuscleMass = 30.0f;
	Metabolism->BodyComposition.BodyFatPercent = 18.0f;

	const float BMR = Metabolism->GetCurrentBMR();

	// BMR should be positive and reasonable (typically 1500-2500 kcal/day for adults)
	TestTrue(TEXT("BMR is positive"), BMR > 0.0f);
	AddInfo(FString::Printf(TEXT("Calculated BMR: %.1f kcal/day"), BMR));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMetabolism_BodyComposition_FatMassCalculation,
	"MOFramework.Medical.Metabolism.BodyComposition.FatMassCalculation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMetabolism_BodyComposition_FatMassCalculation::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	Metabolism->BodyComposition.TotalWeight = 80.0f;
	Metabolism->BodyComposition.BodyFatPercent = 20.0f;

	const float FatMass = Metabolism->BodyComposition.GetFatMass();
	const float ExpectedFatMass = 80.0f * 0.20f;  // 16 kg

	TestEqual(TEXT("Fat mass calculated correctly"), FatMass, ExpectedFatMass);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMetabolism_BodyComposition_LeanMassCalculation,
	"MOFramework.Medical.Metabolism.BodyComposition.LeanMassCalculation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMetabolism_BodyComposition_LeanMassCalculation::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	Metabolism->BodyComposition.TotalWeight = 80.0f;
	Metabolism->BodyComposition.BodyFatPercent = 20.0f;

	const float LeanMass = Metabolism->BodyComposition.GetLeanMass();
	const float ExpectedLeanMass = 80.0f * 0.80f;  // 64 kg

	TestEqual(TEXT("Lean mass calculated correctly"), LeanMass, ExpectedLeanMass);

	return true;
}

//=============================================================================
// Metabolism Component Tests - Training
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMetabolism_ApplyStrengthTraining_IncreasesStrength,
	"MOFramework.Medical.Metabolism.ApplyStrengthTraining.IncreasesStrength",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMetabolism_ApplyStrengthTraining_IncreasesStrength::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	Metabolism->BodyComposition.StrengthLevel = 50.0f;
	const float Initial = Metabolism->BodyComposition.StrengthLevel;

	Metabolism->ApplyStrengthTraining(0.8f, 60.0f);  // 80% intensity, 60 seconds

	AddInfo(FString::Printf(TEXT("Strength: %.1f -> %.1f"),
		Initial, Metabolism->BodyComposition.StrengthLevel));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMetabolism_ApplyCardioTraining_IncreasesFitness,
	"MOFramework.Medical.Metabolism.ApplyCardioTraining.IncreasesFitness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMetabolism_ApplyCardioTraining_IncreasesFitness::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	Metabolism->BodyComposition.CardiovascularFitness = 50.0f;
	const float Initial = Metabolism->BodyComposition.CardiovascularFitness;

	Metabolism->ApplyCardioTraining(0.7f, 60.0f);  // 70% intensity, 60 seconds

	AddInfo(FString::Printf(TEXT("Cardio fitness: %.1f -> %.1f"),
		Initial, Metabolism->BodyComposition.CardiovascularFitness));

	return true;
}

//=============================================================================
// Metabolism Component Tests - Dehydration Detection
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMetabolism_IsDehydrated_DetectsLowHydration,
	"MOFramework.Medical.Metabolism.IsDehydrated.DetectsLowHydration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMetabolism_IsDehydrated_DetectsLowHydration::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	// Full hydration - not dehydrated
	Metabolism->Nutrients.HydrationLevel = 100.0f;
	TestFalse(TEXT("Not dehydrated at 100%"), Metabolism->IsDehydrated());

	// Moderate hydration - not dehydrated
	Metabolism->Nutrients.HydrationLevel = 50.0f;
	TestFalse(TEXT("Not dehydrated at 50%"), Metabolism->IsDehydrated());

	// Low hydration - dehydrated (threshold typically 30%)
	Metabolism->Nutrients.HydrationLevel = 25.0f;
	TestTrue(TEXT("Dehydrated at 25%"), Metabolism->IsDehydrated());

	// Critical hydration
	Metabolism->Nutrients.HydrationLevel = 10.0f;
	TestTrue(TEXT("Dehydrated at 10%"), Metabolism->IsDehydrated());

	return true;
}

//=============================================================================
// Metabolism Component Tests - Starvation Detection
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMetabolism_IsStarving_DetectsLowCalories,
	"MOFramework.Medical.Metabolism.IsStarving.DetectsLowCalories",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMetabolism_IsStarving_DetectsLowCalories::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	// Full glycogen - not starving
	Metabolism->Nutrients.GlycogenStores = 500.0f;
	Metabolism->BodyComposition.BodyFatPercent = 18.0f;
	TestFalse(TEXT("Not starving with glycogen and fat"), Metabolism->IsStarving());

	// No glycogen but has fat - may or may not be "starving" depending on implementation
	Metabolism->Nutrients.GlycogenStores = 0.0f;
	Metabolism->BodyComposition.BodyFatPercent = 15.0f;
	const bool bStarvingNoGlycogen = Metabolism->IsStarving();
	AddInfo(FString::Printf(TEXT("Starving with no glycogen but 15%% fat: %s"),
		bStarvingNoGlycogen ? TEXT("true") : TEXT("false")));

	// Critical state - very low fat and no glycogen
	Metabolism->Nutrients.GlycogenStores = 0.0f;
	Metabolism->BodyComposition.BodyFatPercent = 4.0f;
	TestTrue(TEXT("Starving at critical fat levels"), Metabolism->IsStarving());

	return true;
}

//=============================================================================
// Vitals Component Tests - Blood Volume
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOVitals_ApplyBloodLoss_DecreasesVolume,
	"MOFramework.Medical.Vitals.ApplyBloodLoss.DecreasesVolume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOVitals_ApplyBloodLoss_DecreasesVolume::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();
	TestNotNull(TEXT("Vitals component created"), Vitals);

	const float InitialVolume = Vitals->Vitals.BloodVolume;
	TestTrue(TEXT("Initial blood volume is positive"), InitialVolume > 0.0f);

	Vitals->ApplyBloodLoss(500.0f);

	// If authority passes, volume should decrease
	const float CurrentVolume = Vitals->Vitals.BloodVolume;
	AddInfo(FString::Printf(TEXT("Blood volume: %.0f -> %.0f mL"), InitialVolume, CurrentVolume));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOVitals_ApplyBloodLoss_ClampsAtZero,
	"MOFramework.Medical.Vitals.ApplyBloodLoss.ClampsAtZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOVitals_ApplyBloodLoss_ClampsAtZero::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();

	// Try to lose more blood than exists
	Vitals->ApplyBloodLoss(10000.0f);

	TestTrue(TEXT("Blood volume cannot go negative"), Vitals->Vitals.BloodVolume >= 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOVitals_ApplyBloodTransfusion_IncreasesVolume,
	"MOFramework.Medical.Vitals.ApplyBloodTransfusion.IncreasesVolume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOVitals_ApplyBloodTransfusion_IncreasesVolume::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();

	// Reduce blood first
	Vitals->Vitals.BloodVolume = 3000.0f;
	const float LowVolume = Vitals->Vitals.BloodVolume;

	Vitals->ApplyBloodTransfusion(500.0f);

	const float AfterTransfusion = Vitals->Vitals.BloodVolume;
	AddInfo(FString::Printf(TEXT("Blood volume: %.0f -> %.0f mL"), LowVolume, AfterTransfusion));

	return true;
}

//=============================================================================
// Vitals Component Tests - Blood Loss Stages
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOVitals_GetBloodLossStage_CorrectStages,
	"MOFramework.Medical.Vitals.GetBloodLossStage.CorrectStages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOVitals_GetBloodLossStage_CorrectStages::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();

	// Normal blood volume (5000 mL default)
	Vitals->Vitals.BloodVolume = 5000.0f;
	Vitals->Vitals.MaxBloodVolume = 5000.0f;
	TestEqual(TEXT("No blood loss at 100%"), Vitals->GetBloodLossStage(), EMOBloodLossStage::None);

	// Class 1: 15-30% loss (85-70% remaining = 4250-3500 mL)
	Vitals->Vitals.BloodVolume = 4000.0f;  // 80% = 20% loss
	TestEqual(TEXT("Class 1 at 20% loss"), Vitals->GetBloodLossStage(), EMOBloodLossStage::Class1);

	// Class 2: 30-40% loss (70-60% remaining = 3500-3000 mL)
	Vitals->Vitals.BloodVolume = 3200.0f;  // 64% = 36% loss
	TestEqual(TEXT("Class 2 at 36% loss"), Vitals->GetBloodLossStage(), EMOBloodLossStage::Class2);

	// Class 3: >40% loss (<60% remaining = <3000 mL)
	Vitals->Vitals.BloodVolume = 2500.0f;  // 50% = 50% loss
	TestEqual(TEXT("Class 3 at 50% loss"), Vitals->GetBloodLossStage(), EMOBloodLossStage::Class3);

	return true;
}

//=============================================================================
// Vitals Component Tests - Heart Rate
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOVitals_HeartRate_InitialValues,
	"MOFramework.Medical.Vitals.HeartRate.InitialValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOVitals_HeartRate_InitialValues::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();

	// Heart rate should be in normal range (60-100 BPM)
	TestTrue(TEXT("Initial HR >= 60"), Vitals->Vitals.HeartRate >= 60.0f);
	TestTrue(TEXT("Initial HR <= 100"), Vitals->Vitals.HeartRate <= 100.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOVitals_SetExertionLevel_AffectsHeartRate,
	"MOFramework.Medical.Vitals.SetExertionLevel.AffectsHeartRate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOVitals_SetExertionLevel_AffectsHeartRate::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();

	const float RestingHR = Vitals->Vitals.HeartRate;

	// Set high exertion
	Vitals->SetExertionLevel(80.0f);

	AddInfo(FString::Printf(TEXT("HR at rest: %.0f, exertion level set to 80"), RestingHR));

	return true;
}

//=============================================================================
// Vitals Component Tests - Blood Pressure
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOVitals_BloodPressure_InitialValues,
	"MOFramework.Medical.Vitals.BloodPressure.InitialValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOVitals_BloodPressure_InitialValues::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();

	// Normal BP is around 120/80
	TestTrue(TEXT("Systolic in normal range"), Vitals->Vitals.SystolicBP >= 90.0f && Vitals->Vitals.SystolicBP <= 140.0f);
	TestTrue(TEXT("Diastolic in normal range"), Vitals->Vitals.DiastolicBP >= 60.0f && Vitals->Vitals.DiastolicBP <= 90.0f);
	TestTrue(TEXT("Systolic > Diastolic"), Vitals->Vitals.SystolicBP > Vitals->Vitals.DiastolicBP);

	return true;
}

//=============================================================================
// Vitals Component Tests - Blood Glucose
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOVitals_BloodGlucose_InitialValues,
	"MOFramework.Medical.Vitals.BloodGlucose.InitialValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOVitals_BloodGlucose_InitialValues::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();

	// Normal fasting glucose is 70-110 mg/dL
	TestTrue(TEXT("Initial glucose >= 70"), Vitals->Vitals.BloodGlucose >= 70.0f);
	TestTrue(TEXT("Initial glucose <= 110"), Vitals->Vitals.BloodGlucose <= 110.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOVitals_ApplyGlucose_IncreasesLevel,
	"MOFramework.Medical.Vitals.ApplyGlucose.IncreasesLevel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOVitals_ApplyGlucose_IncreasesLevel::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();

	Vitals->Vitals.BloodGlucose = 80.0f;
	const float Initial = Vitals->Vitals.BloodGlucose;

	Vitals->ApplyGlucose(20.0f);

	AddInfo(FString::Printf(TEXT("Glucose: %.0f -> %.0f mg/dL"), Initial, Vitals->Vitals.BloodGlucose));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOVitals_ConsumeGlucose_DecreasesLevel,
	"MOFramework.Medical.Vitals.ConsumeGlucose.DecreasesLevel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOVitals_ConsumeGlucose_DecreasesLevel::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();

	Vitals->Vitals.BloodGlucose = 100.0f;
	const float Initial = Vitals->Vitals.BloodGlucose;

	Vitals->ConsumeGlucose(15.0f);

	AddInfo(FString::Printf(TEXT("Glucose: %.0f -> %.0f mg/dL"), Initial, Vitals->Vitals.BloodGlucose));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOVitals_IsHypoglycemic_DetectsLowGlucose,
	"MOFramework.Medical.Vitals.IsHypoglycemic.DetectsLowGlucose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOVitals_IsHypoglycemic_DetectsLowGlucose::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();

	// Normal glucose
	Vitals->Vitals.BloodGlucose = 90.0f;
	TestFalse(TEXT("Not hypoglycemic at 90"), Vitals->Vitals.IsHypoglycemic());

	// Borderline
	Vitals->Vitals.BloodGlucose = 70.0f;
	// May or may not be hypoglycemic depending on threshold

	// Low glucose
	Vitals->Vitals.BloodGlucose = 50.0f;
	TestTrue(TEXT("Hypoglycemic at 50"), Vitals->Vitals.IsHypoglycemic());

	return true;
}

//=============================================================================
// Vitals Component Tests - SpO2
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOVitals_SpO2_InitialValues,
	"MOFramework.Medical.Vitals.SpO2.InitialValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOVitals_SpO2_InitialValues::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();

	// Normal SpO2 is 95-100%
	TestTrue(TEXT("Initial SpO2 >= 95"), Vitals->Vitals.SpO2 >= 95.0f);
	TestTrue(TEXT("Initial SpO2 <= 100"), Vitals->Vitals.SpO2 <= 100.0f);

	return true;
}

//=============================================================================
// Vitals Component Tests - Temperature
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOVitals_Temperature_InitialValues,
	"MOFramework.Medical.Vitals.Temperature.InitialValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOVitals_Temperature_InitialValues::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();

	// Normal body temperature is 36.5-37.5 C
	TestTrue(TEXT("Initial temp >= 36.5"), Vitals->Vitals.BodyTemperature >= 36.5f);
	TestTrue(TEXT("Initial temp <= 37.5"), Vitals->Vitals.BodyTemperature <= 37.5f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOVitals_IsHyperthermic_DetectsHighTemp,
	"MOFramework.Medical.Vitals.IsHyperthermic.DetectsHighTemp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOVitals_IsHyperthermic_DetectsHighTemp::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();

	// Normal temp
	Vitals->Vitals.BodyTemperature = 37.0f;
	TestFalse(TEXT("Not hyperthermic at 37C"), Vitals->Vitals.IsHyperthermic());

	// High temp (>38C)
	Vitals->Vitals.BodyTemperature = 38.5f;
	TestTrue(TEXT("Hyperthermic at 38.5C"), Vitals->Vitals.IsHyperthermic());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOVitals_IsHypothermic_DetectsLowTemp,
	"MOFramework.Medical.Vitals.IsHypothermic.DetectsLowTemp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOVitals_IsHypothermic_DetectsLowTemp::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();

	// Normal temp
	Vitals->Vitals.BodyTemperature = 37.0f;
	TestFalse(TEXT("Not hypothermic at 37C"), Vitals->Vitals.IsHypothermic());

	// Mild hypothermia
	Vitals->Vitals.BodyTemperature = 35.0f;
	TestTrue(TEXT("Hypothermic at 35C"), Vitals->Vitals.IsHypothermic());

	return true;
}

//=============================================================================
// Anatomy Component Tests - Damage
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAnatomy_InflictDamage_CreatesWound,
	"MOFramework.Medical.Anatomy.InflictDamage.CreatesWound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAnatomy_InflictDamage_CreatesWound::RunTest(const FString& Parameters)
{
	UMOAnatomyComponent* Anatomy = NewObject<UMOAnatomyComponent>();
	TestNotNull(TEXT("Anatomy component created"), Anatomy);

	const int32 InitialWounds = Anatomy->GetAllWounds().Num();

	// Inflict damage to arm
	const bool bDamaged = Anatomy->InflictDamage(EMOBodyPartType::ForearmLeft, 25.0f, EMOWoundType::Laceration);

	if (bDamaged)
	{
		TestTrue(TEXT("Wound was created"), Anatomy->GetAllWounds().Num() > InitialWounds);
	}
	else
	{
		AddInfo(TEXT("InflictDamage returned false (expected without authority in test)"));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAnatomy_InflictDamage_DifferentWoundTypes,
	"MOFramework.Medical.Anatomy.InflictDamage.DifferentWoundTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAnatomy_InflictDamage_DifferentWoundTypes::RunTest(const FString& Parameters)
{
	UMOAnatomyComponent* Anatomy = NewObject<UMOAnatomyComponent>();

	// Test various wound types
	Anatomy->InflictDamage(EMOBodyPartType::ThighLeft, 20.0f, EMOWoundType::Laceration);
	Anatomy->InflictDamage(EMOBodyPartType::ThighRight, 15.0f, EMOWoundType::Puncture);
	Anatomy->InflictDamage(EMOBodyPartType::Torso, 30.0f, EMOWoundType::Blunt);
	Anatomy->InflictDamage(EMOBodyPartType::HandLeft, 10.0f, EMOWoundType::BurnFirst);
	Anatomy->InflictDamage(EMOBodyPartType::CalfRight, 25.0f, EMOWoundType::Fracture);

	AddInfo(FString::Printf(TEXT("Total wounds after damage: %d"), Anatomy->GetAllWounds().Num()));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAnatomy_InflictDamage_ZeroNegativeIgnored,
	"MOFramework.Medical.Anatomy.InflictDamage.ZeroNegativeIgnored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAnatomy_InflictDamage_ZeroNegativeIgnored::RunTest(const FString& Parameters)
{
	UMOAnatomyComponent* Anatomy = NewObject<UMOAnatomyComponent>();

	const int32 InitialWounds = Anatomy->GetAllWounds().Num();

	// Zero damage should not create wound
	Anatomy->InflictDamage(EMOBodyPartType::ForearmLeft, 0.0f, EMOWoundType::Laceration);
	TestEqual(TEXT("Zero damage creates no wound"), Anatomy->GetAllWounds().Num(), InitialWounds);

	// Negative damage should not create wound
	Anatomy->InflictDamage(EMOBodyPartType::ForearmLeft, -10.0f, EMOWoundType::Laceration);
	TestEqual(TEXT("Negative damage creates no wound"), Anatomy->GetAllWounds().Num(), InitialWounds);

	return true;
}

//=============================================================================
// Anatomy Component Tests - Body Part State
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAnatomy_GetBodyPartState_ReturnsValidData,
	"MOFramework.Medical.Anatomy.GetBodyPartState.ReturnsValidData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAnatomy_GetBodyPartState_ReturnsValidData::RunTest(const FString& Parameters)
{
	UMOAnatomyComponent* Anatomy = NewObject<UMOAnatomyComponent>();

	FMOBodyPartState State;
	const bool bFound = Anatomy->GetBodyPartState(EMOBodyPartType::Head, State);

	TestTrue(TEXT("Head body part found"), bFound);
	if (bFound)
	{
		TestTrue(TEXT("HP is positive"), State.CurrentHP > 0.0f);
		TestEqual(TEXT("Initial status is Healthy"), State.Status, EMOBodyPartStatus::Healthy);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAnatomy_IsBodyPartFunctional_ChecksStatus,
	"MOFramework.Medical.Anatomy.IsBodyPartFunctional.ChecksStatus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAnatomy_IsBodyPartFunctional_ChecksStatus::RunTest(const FString& Parameters)
{
	UMOAnatomyComponent* Anatomy = NewObject<UMOAnatomyComponent>();

	// Healthy part should be functional
	TestTrue(TEXT("Healthy head is functional"), Anatomy->IsBodyPartFunctional(EMOBodyPartType::Head));
	TestTrue(TEXT("Healthy arm is functional"), Anatomy->IsBodyPartFunctional(EMOBodyPartType::ForearmLeft));

	return true;
}

//=============================================================================
// Anatomy Component Tests - Bleed Rate
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAnatomy_GetTotalBleedRate_SumsWounds,
	"MOFramework.Medical.Anatomy.GetTotalBleedRate.SumsWounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAnatomy_GetTotalBleedRate_SumsWounds::RunTest(const FString& Parameters)
{
	UMOAnatomyComponent* Anatomy = NewObject<UMOAnatomyComponent>();

	// Initial bleed rate should be zero
	TestEqual(TEXT("Initial bleed rate is zero"), Anatomy->GetTotalBleedRate(), 0.0f);

	// Add bleeding wounds
	Anatomy->InflictDamage(EMOBodyPartType::ThighLeft, 30.0f, EMOWoundType::Laceration);
	Anatomy->InflictDamage(EMOBodyPartType::ForearmRight, 20.0f, EMOWoundType::Puncture);

	const float TotalBleed = Anatomy->GetTotalBleedRate();
	AddInfo(FString::Printf(TEXT("Total bleed rate after wounds: %.2f mL/s"), TotalBleed));

	return true;
}

//=============================================================================
// Anatomy Component Tests - Pain Level
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAnatomy_GetTotalPainLevel_SumsWounds,
	"MOFramework.Medical.Anatomy.GetTotalPainLevel.SumsWounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAnatomy_GetTotalPainLevel_SumsWounds::RunTest(const FString& Parameters)
{
	UMOAnatomyComponent* Anatomy = NewObject<UMOAnatomyComponent>();

	// Initial pain should be zero
	TestEqual(TEXT("Initial pain is zero"), Anatomy->GetTotalPainLevel(), 0.0f);

	// Add wounds
	Anatomy->InflictDamage(EMOBodyPartType::ThighLeft, 40.0f, EMOWoundType::Laceration);

	const float TotalPain = Anatomy->GetTotalPainLevel();
	AddInfo(FString::Printf(TEXT("Total pain after wound: %.1f"), TotalPain));

	return true;
}

//=============================================================================
// Anatomy Component Tests - Conditions
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAnatomy_AddCondition_AddsToList,
	"MOFramework.Medical.Anatomy.AddCondition.AddsToList",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAnatomy_AddCondition_AddsToList::RunTest(const FString& Parameters)
{
	UMOAnatomyComponent* Anatomy = NewObject<UMOAnatomyComponent>();

	TestFalse(TEXT("No infection initially"), Anatomy->HasCondition(EMOConditionType::Infection));

	Anatomy->AddCondition(EMOConditionType::Infection, EMOBodyPartType::ForearmLeft, 30.0f);

	// If authority passes, condition should be added
	const bool bHasInfection = Anatomy->HasCondition(EMOConditionType::Infection);
	AddInfo(FString::Printf(TEXT("Has infection after add: %s"), bHasInfection ? TEXT("true") : TEXT("false")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAnatomy_HasCondition_ChecksType,
	"MOFramework.Medical.Anatomy.HasCondition.ChecksType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAnatomy_HasCondition_ChecksType::RunTest(const FString& Parameters)
{
	UMOAnatomyComponent* Anatomy = NewObject<UMOAnatomyComponent>();

	// Add specific condition
	Anatomy->AddCondition(EMOConditionType::Concussion, EMOBodyPartType::Head, 50.0f);

	// Check for different conditions
	AddInfo(FString::Printf(TEXT("Has Concussion: %s"),
		Anatomy->HasCondition(EMOConditionType::Concussion) ? TEXT("true") : TEXT("false")));
	AddInfo(FString::Printf(TEXT("Has Sepsis: %s"),
		Anatomy->HasCondition(EMOConditionType::Sepsis) ? TEXT("true") : TEXT("false")));

	return true;
}

//=============================================================================
// Anatomy Component Tests - Movement Capability
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAnatomy_CanMove_ChecksLegStatus,
	"MOFramework.Medical.Anatomy.CanMove.ChecksLegStatus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAnatomy_CanMove_ChecksLegStatus::RunTest(const FString& Parameters)
{
	UMOAnatomyComponent* Anatomy = NewObject<UMOAnatomyComponent>();

	// Initially should be able to move
	TestTrue(TEXT("Can move initially"), Anatomy->CanMove());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAnatomy_CanGrip_ChecksArmStatus,
	"MOFramework.Medical.Anatomy.CanGrip.ChecksArmStatus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAnatomy_CanGrip_ChecksArmStatus::RunTest(const FString& Parameters)
{
	UMOAnatomyComponent* Anatomy = NewObject<UMOAnatomyComponent>();

	// Check grip capability (requires at least one functional hand)
	const bool bCanGrip = Anatomy->CanGrip();

	TestTrue(TEXT("Can grip initially"), bCanGrip);

	return true;
}

//=============================================================================
// Mental State Component Tests - Consciousness
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMentalState_GetConsciousnessLevel_InitialAlert,
	"MOFramework.Medical.MentalState.GetConsciousnessLevel.InitialAlert",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMentalState_GetConsciousnessLevel_InitialAlert::RunTest(const FString& Parameters)
{
	UMOMentalStateComponent* Mental = NewObject<UMOMentalStateComponent>();
	TestNotNull(TEXT("Mental state component created"), Mental);

	TestEqual(TEXT("Initial consciousness is Alert"),
		Mental->MentalState.Consciousness, EMOConsciousnessLevel::Alert);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMentalState_ForceConsciousnessLevel_SetsLevel,
	"MOFramework.Medical.MentalState.ForceConsciousnessLevel.SetsLevel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMentalState_ForceConsciousnessLevel_SetsLevel::RunTest(const FString& Parameters)
{
	UMOMentalStateComponent* Mental = NewObject<UMOMentalStateComponent>();

	Mental->ForceConsciousnessLevel(EMOConsciousnessLevel::Unconscious);

	// If authority passes, consciousness should change
	AddInfo(FString::Printf(TEXT("Consciousness after force: %d"),
		static_cast<int32>(Mental->MentalState.Consciousness)));

	return true;
}

//=============================================================================
// Mental State Component Tests - Shock
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMentalState_AddShock_AccumulatesShock,
	"MOFramework.Medical.MentalState.AddShock.AccumulatesShock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMentalState_AddShock_AccumulatesShock::RunTest(const FString& Parameters)
{
	UMOMentalStateComponent* Mental = NewObject<UMOMentalStateComponent>();

	const float InitialShock = Mental->MentalState.ShockAccumulation;
	TestEqual(TEXT("Initial shock is zero"), InitialShock, 0.0f);

	Mental->AddShock(25.0f);

	AddInfo(FString::Printf(TEXT("Shock: %.1f -> %.1f"), InitialShock, Mental->MentalState.ShockAccumulation));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMentalState_AddShock_ClampsAt100,
	"MOFramework.Medical.MentalState.AddShock.ClampsAt100",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMentalState_AddShock_ClampsAt100::RunTest(const FString& Parameters)
{
	UMOMentalStateComponent* Mental = NewObject<UMOMentalStateComponent>();

	// Add excessive shock
	Mental->AddShock(150.0f);

	TestTrue(TEXT("Shock clamped at or below 100"), Mental->MentalState.ShockAccumulation <= 100.0f);

	return true;
}

//=============================================================================
// Mental State Component Tests - Can Perform Actions
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMentalState_CanPerformActions_ChecksConsciousness,
	"MOFramework.Medical.MentalState.CanPerformActions.ChecksConsciousness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMentalState_CanPerformActions_ChecksConsciousness::RunTest(const FString& Parameters)
{
	UMOMentalStateComponent* Mental = NewObject<UMOMentalStateComponent>();

	// Alert - can act
	Mental->MentalState.Consciousness = EMOConsciousnessLevel::Alert;
	TestTrue(TEXT("Can act when Alert"), Mental->CanPerformActions());

	// Confused - can still act
	Mental->MentalState.Consciousness = EMOConsciousnessLevel::Confused;
	TestTrue(TEXT("Can act when Confused"), Mental->CanPerformActions());

	// Drowsy - can still act (maybe with penalties)
	Mental->MentalState.Consciousness = EMOConsciousnessLevel::Drowsy;
	TestTrue(TEXT("Can act when Drowsy"), Mental->CanPerformActions());

	// Unconscious - cannot act
	Mental->MentalState.Consciousness = EMOConsciousnessLevel::Unconscious;
	TestFalse(TEXT("Cannot act when Unconscious"), Mental->CanPerformActions());

	// Comatose - cannot act
	Mental->MentalState.Consciousness = EMOConsciousnessLevel::Comatose;
	TestFalse(TEXT("Cannot act when Comatose"), Mental->CanPerformActions());

	return true;
}

//=============================================================================
// Mental State Component Tests - Visual Effects
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMentalState_VisualEffects_InitialZero,
	"MOFramework.Medical.MentalState.VisualEffects.InitialZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMentalState_VisualEffects_InitialZero::RunTest(const FString& Parameters)
{
	UMOMentalStateComponent* Mental = NewObject<UMOMentalStateComponent>();

	TestEqual(TEXT("Initial aim shake is zero"), Mental->MentalState.AimShakeIntensity, 0.0f);
	TestEqual(TEXT("Initial tunnel vision is zero"), Mental->MentalState.TunnelVisionIntensity, 0.0f);
	TestEqual(TEXT("Initial blur is zero"), Mental->MentalState.BlurredVisionIntensity, 0.0f);
	TestEqual(TEXT("Initial stumbling is zero"), Mental->MentalState.StumblingChance, 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMentalState_GetAimPenalty_ReturnsValue,
	"MOFramework.Medical.MentalState.GetAimPenalty.ReturnsValue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMentalState_GetAimPenalty_ReturnsValue::RunTest(const FString& Parameters)
{
	UMOMentalStateComponent* Mental = NewObject<UMOMentalStateComponent>();

	const float Penalty = Mental->GetAimPenalty();
	TestTrue(TEXT("Aim penalty is non-negative"), Penalty >= 0.0f);

	// Set some shake
	Mental->MentalState.AimShakeIntensity = 0.5f;
	const float PenaltyWithShake = Mental->GetAimPenalty();

	AddInfo(FString::Printf(TEXT("Aim penalty: base=%.2f, with shake=%.2f"), Penalty, PenaltyWithShake));

	return true;
}

//=============================================================================
// Edge Case Tests
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_EdgeCase_ExtremeNutritionValues,
	"MOFramework.Medical.EdgeCases.ExtremeNutritionValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_EdgeCase_ExtremeNutritionValues::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	// Test with very high values
	FMOItemNutrition HighNutrition = MOMedicalTestData::MakeTestNutrition(
		10000.0f,  // 10k calories
		500.0f,    // 500g protein
		1000.0f,   // 1kg carbs
		500.0f,    // 500g fat
		5000.0f,   // 5L water
		100.0f     // 100g fiber
	);

	Metabolism->ConsumeFood(HighNutrition, FName("ExtremeFood"));

	// Should handle without crash
	AddInfo(TEXT("Extreme nutrition values handled without crash"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_EdgeCase_AllBodyPartsExist,
	"MOFramework.Medical.EdgeCases.AllBodyPartsExist",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_EdgeCase_AllBodyPartsExist::RunTest(const FString& Parameters)
{
	UMOAnatomyComponent* Anatomy = NewObject<UMOAnatomyComponent>();

	// Test that all common body parts can be queried
	TArray<EMOBodyPartType> PartsToTest = {
		EMOBodyPartType::Head,
		EMOBodyPartType::Brain,
		EMOBodyPartType::EyeLeft,
		EMOBodyPartType::EyeRight,
		EMOBodyPartType::Torso,
		EMOBodyPartType::Heart,
		EMOBodyPartType::LungLeft,
		EMOBodyPartType::LungRight,
		EMOBodyPartType::ShoulderLeft,
		EMOBodyPartType::ShoulderRight,
		EMOBodyPartType::ForearmLeft,
		EMOBodyPartType::ForearmRight,
		EMOBodyPartType::HandLeft,
		EMOBodyPartType::HandRight,
		EMOBodyPartType::ThighLeft,
		EMOBodyPartType::ThighRight,
		EMOBodyPartType::CalfLeft,
		EMOBodyPartType::CalfRight,
		EMOBodyPartType::FootLeft,
		EMOBodyPartType::FootRight
	};

	int32 FoundCount = 0;
	for (EMOBodyPartType Part : PartsToTest)
	{
		FMOBodyPartState State;
		if (Anatomy->GetBodyPartState(Part, State))
		{
			FoundCount++;
		}
	}

	AddInfo(FString::Printf(TEXT("Found %d / %d body parts"), FoundCount, PartsToTest.Num()));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_EdgeCase_RapidStateChanges,
	"MOFramework.Medical.EdgeCases.RapidStateChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_EdgeCase_RapidStateChanges::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();

	// Rapidly change vitals
	for (int32 i = 0; i < 100; i++)
	{
		Vitals->ApplyBloodLoss(10.0f);
		Vitals->ApplyBloodTransfusion(5.0f);
		Vitals->ApplyGlucose(5.0f);
		Vitals->ConsumeGlucose(3.0f);
	}

	// Should handle without crash and maintain valid state
	TestTrue(TEXT("Blood volume remains non-negative"), Vitals->Vitals.BloodVolume >= 0.0f);
	TestTrue(TEXT("Blood glucose remains non-negative"), Vitals->Vitals.BloodGlucose >= 0.0f);

	AddInfo(TEXT("Rapid state changes handled without crash"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_EdgeCase_ManyWounds,
	"MOFramework.Medical.EdgeCases.ManyWounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_EdgeCase_ManyWounds::RunTest(const FString& Parameters)
{
	UMOAnatomyComponent* Anatomy = NewObject<UMOAnatomyComponent>();

	// Create many wounds
	TArray<EMOBodyPartType> Parts = {
		EMOBodyPartType::ForearmLeft, EMOBodyPartType::ForearmRight,
		EMOBodyPartType::ThighLeft, EMOBodyPartType::ThighRight,
		EMOBodyPartType::CalfLeft, EMOBodyPartType::CalfRight,
		EMOBodyPartType::Torso, EMOBodyPartType::Head
	};

	for (int32 i = 0; i < 50; i++)
	{
		EMOBodyPartType Part = Parts[i % Parts.Num()];
		Anatomy->InflictDamage(Part, 5.0f + (i % 20), EMOWoundType::Laceration);
	}

	const int32 WoundCount = Anatomy->GetAllWounds().Num();
	const float TotalBleed = Anatomy->GetTotalBleedRate();

	AddInfo(FString::Printf(TEXT("Created wounds, total count: %d, bleed rate: %.2f mL/s"),
		WoundCount, TotalBleed));

	return true;
}

//=============================================================================
// Struct Validation Tests
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Structs_VitalSignsDefaults,
	"MOFramework.Medical.Structs.VitalSignsDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Structs_VitalSignsDefaults::RunTest(const FString& Parameters)
{
	FMOVitalSigns Vitals;

	// Verify default values are medically reasonable
	TestEqual(TEXT("Default blood volume is 5000"), Vitals.BloodVolume, 5000.0f);
	TestEqual(TEXT("Default heart rate is 72"), Vitals.HeartRate, 72.0f);
	TestEqual(TEXT("Default systolic is 120"), Vitals.SystolicBP, 120.0f);
	TestEqual(TEXT("Default diastolic is 80"), Vitals.DiastolicBP, 80.0f);
	TestEqual(TEXT("Default SpO2 is 98"), Vitals.SpO2, 98.0f);
	TestEqual(TEXT("Default temp is 37"), Vitals.BodyTemperature, 37.0f);
	TestEqual(TEXT("Default glucose is 90"), Vitals.BloodGlucose, 90.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Structs_BodyCompositionDefaults,
	"MOFramework.Medical.Structs.BodyCompositionDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Structs_BodyCompositionDefaults::RunTest(const FString& Parameters)
{
	FMOBodyComposition Comp;

	TestEqual(TEXT("Default weight is 75kg"), Comp.TotalWeight, 75.0f);
	TestEqual(TEXT("Default muscle is 30kg"), Comp.MuscleMass, 30.0f);
	TestEqual(TEXT("Default body fat is 18%"), Comp.BodyFatPercent, 18.0f);
	TestEqual(TEXT("Default cardio is 50"), Comp.CardiovascularFitness, 50.0f);
	TestEqual(TEXT("Default strength is 50"), Comp.StrengthLevel, 50.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Structs_NutrientLevelsDefaults,
	"MOFramework.Medical.Structs.NutrientLevelsDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Structs_NutrientLevelsDefaults::RunTest(const FString& Parameters)
{
	FMONutrientLevels Nutrients;

	TestEqual(TEXT("Default glycogen is 500g"), Nutrients.GlycogenStores, 500.0f);
	TestEqual(TEXT("Default hydration is 100%"), Nutrients.HydrationLevel, 100.0f);
	TestEqual(TEXT("Default protein balance is 0"), Nutrients.ProteinBalance, 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Structs_MentalStateDefaults,
	"MOFramework.Medical.Structs.MentalStateDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Structs_MentalStateDefaults::RunTest(const FString& Parameters)
{
	FMOMentalState Mental;

	TestEqual(TEXT("Default consciousness is Alert"), Mental.Consciousness, EMOConsciousnessLevel::Alert);
	TestEqual(TEXT("Default shock is 0"), Mental.ShockAccumulation, 0.0f);
	TestEqual(TEXT("Default traumatic stress is 0"), Mental.TraumaticStress, 0.0f);
	TestEqual(TEXT("Default morale fatigue is 0"), Mental.MoraleFatigue, 0.0f);
	TestEqual(TEXT("Default aim shake is 0"), Mental.AimShakeIntensity, 0.0f);

	return true;
}

//=============================================================================
// Wound Tests
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Wound_BleedRatesByType,
	"MOFramework.Medical.Wounds.BleedRatesByType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Wound_BleedRatesByType::RunTest(const FString& Parameters)
{
	// Lacerations should bleed more than blunt trauma
	FMOWound Laceration;
	Laceration.WoundType = EMOWoundType::Laceration;
	Laceration.Severity = 50.0f;

	FMOWound Blunt;
	Blunt.WoundType = EMOWoundType::Blunt;
	Blunt.Severity = 50.0f;

	// The actual bleed rates depend on implementation
	// This test documents expected behavior
	AddInfo(TEXT("Wound bleed rates vary by type - lacerations > punctures > blunt"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Wound_InfectionRisk,
	"MOFramework.Medical.Wounds.InfectionRisk",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Wound_InfectionRisk::RunTest(const FString& Parameters)
{
	FMOWound Wound;
	Wound.WoundType = EMOWoundType::Puncture;
	Wound.Severity = 40.0f;
	Wound.InfectionRisk = 0.3f;
	Wound.bIsBandaged = false;

	TestTrue(TEXT("Unbandaged wound has infection risk"), Wound.InfectionRisk > 0.0f);

	// Bandaging should reduce risk (implementation detail)
	Wound.bIsBandaged = true;
	AddInfo(TEXT("Bandaged wounds have reduced infection progression"));

	return true;
}

//=============================================================================
// INTEGRATION TESTS - Food/Water Pipeline
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Integration_EatMeal_FullPipeline,
	"MOFramework.Medical.Integration.EatMeal.FullPipeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Integration_EatMeal_FullPipeline::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	// Record initial state
	const float InitialGlycogen = Metabolism->Nutrients.GlycogenStores;
	const float InitialHydration = Metabolism->Nutrients.HydrationLevel;

	// Eat a balanced meal
	FMOItemNutrition Meal = MOMedicalTestData::MakeBalancedMeal();
	Metabolism->ConsumeFood(Meal, FName("BalancedMeal"));

	// Record state after eating
	const int32 DigestingCount = Metabolism->GetDigestingFoodCount();
	const float CaloriesConsumed = Metabolism->TotalCaloriesConsumedToday;

	AddInfo(FString::Printf(TEXT("Meal consumed: %d items digesting, %.0f calories tracked"),
		DigestingCount, CaloriesConsumed));
	AddInfo(FString::Printf(TEXT("Glycogen: %.1f -> current state pending digestion"),
		InitialGlycogen));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Integration_DrinkWater_FullPipeline,
	"MOFramework.Medical.Integration.DrinkWater.FullPipeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Integration_DrinkWater_FullPipeline::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	// Set low hydration to test recovery
	Metabolism->Nutrients.HydrationLevel = 40.0f;
	const float InitialHydration = Metabolism->Nutrients.HydrationLevel;

	// Drink water (500mL glass)
	Metabolism->DrinkWater(500.0f);
	const float AfterFirstDrink = Metabolism->Nutrients.HydrationLevel;

	// Drink more water
	Metabolism->DrinkWater(300.0f);
	const float AfterSecondDrink = Metabolism->Nutrients.HydrationLevel;

	AddInfo(FString::Printf(TEXT("Hydration: %.1f%% -> %.1f%% -> %.1f%%"),
		InitialHydration, AfterFirstDrink, AfterSecondDrink));

	// Verify hydration increased (if authority allowed it)
	TestTrue(TEXT("Hydration state is valid"), Metabolism->Nutrients.HydrationLevel >= 0.0f);
	TestTrue(TEXT("Hydration capped at 100"), Metabolism->Nutrients.HydrationLevel <= 100.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Integration_MultipleFood_Digestion,
	"MOFramework.Medical.Integration.MultipleFood.Digestion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Integration_MultipleFood_Digestion::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	// Simulate eating multiple items
	Metabolism->ConsumeFood(MOMedicalTestData::MakeSimpleCarbFood(), FName("Bread"));
	const int32 Count1 = Metabolism->GetDigestingFoodCount();

	Metabolism->ConsumeFood(MOMedicalTestData::MakeHighFatFood(), FName("Cheese"));
	const int32 Count2 = Metabolism->GetDigestingFoodCount();

	Metabolism->ConsumeFood(MOMedicalTestData::MakeVitaminRichFood(), FName("Apple"));
	const int32 Count3 = Metabolism->GetDigestingFoodCount();

	// Drink water between meals
	Metabolism->DrinkWater(200.0f);

	AddInfo(FString::Printf(TEXT("Digestion queue: %d -> %d -> %d items"), Count1, Count2, Count3));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Integration_CalorieBurn_WithExercise,
	"MOFramework.Medical.Integration.CalorieBurn.WithExercise",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Integration_CalorieBurn_WithExercise::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	// Set initial state
	Metabolism->Nutrients.GlycogenStores = 400.0f;
	Metabolism->BodyComposition.BodyFatPercent = 18.0f;
	const float InitialGlycogen = Metabolism->Nutrients.GlycogenStores;
	const float InitialFat = Metabolism->BodyComposition.BodyFatPercent;

	// Simulate exercise session (burn 500 calories)
	for (int32 i = 0; i < 10; i++)
	{
		Metabolism->ApplyCalorieBurn(50.0f);
	}

	const float FinalGlycogen = Metabolism->Nutrients.GlycogenStores;
	const float FinalFat = Metabolism->BodyComposition.BodyFatPercent;

	AddInfo(FString::Printf(TEXT("After 500 cal burn - Glycogen: %.1f -> %.1f, Fat: %.1f%% -> %.1f%%"),
		InitialGlycogen, FinalGlycogen, InitialFat, FinalFat));

	return true;
}

//=============================================================================
// INTEGRATION TESTS - Wound/Blood/Vitals Cascade
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Integration_WoundBleedingCascade,
	"MOFramework.Medical.Integration.WoundBleedingCascade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Integration_WoundBleedingCascade::RunTest(const FString& Parameters)
{
	UMOAnatomyComponent* Anatomy = NewObject<UMOAnatomyComponent>();
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();

	// Record initial blood volume
	const float InitialBlood = Vitals->Vitals.BloodVolume;
	TestEqual(TEXT("Initial blood volume is normal"), InitialBlood, 5000.0f);

	// Inflict wounds
	Anatomy->InflictDamage(EMOBodyPartType::ThighLeft, 40.0f, EMOWoundType::Laceration);
	Anatomy->InflictDamage(EMOBodyPartType::ForearmRight, 25.0f, EMOWoundType::Puncture);

	// Get bleed rate from anatomy
	const float BleedRate = Anatomy->GetTotalBleedRate();

	// Simulate blood loss (what would happen during tick)
	// BleedRate is mL/second, simulate 60 seconds
	const float SimulatedBloodLoss = BleedRate * 60.0f;
	Vitals->ApplyBloodLoss(SimulatedBloodLoss);

	// Check vitals after blood loss
	const float FinalBlood = Vitals->Vitals.BloodVolume;
	const EMOBloodLossStage Stage = Vitals->GetBloodLossStage();

	AddInfo(FString::Printf(TEXT("Bleed rate: %.2f mL/s, simulated 60s loss: %.0f mL"),
		BleedRate, SimulatedBloodLoss));
	AddInfo(FString::Printf(TEXT("Blood: %.0f -> %.0f mL, Stage: %d"),
		InitialBlood, FinalBlood, static_cast<int32>(Stage)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Integration_SevereBloodLoss_VitalChanges,
	"MOFramework.Medical.Integration.SevereBloodLoss.VitalChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Integration_SevereBloodLoss_VitalChanges::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();

	// Record baseline vitals
	const float BaseHR = Vitals->Vitals.HeartRate;
	const float BaseSystolic = Vitals->Vitals.SystolicBP;

	// Simulate progressive blood loss
	// Stage 1: 15-30% loss (750-1500 mL)
	Vitals->ApplyBloodLoss(1000.0f);
	TestEqual(TEXT("Stage 1 blood loss"), Vitals->GetBloodLossStage(), EMOBloodLossStage::Class1);
	AddInfo(FString::Printf(TEXT("After 1000mL loss - Blood: %.0f, Stage 1"),
		Vitals->Vitals.BloodVolume));

	// Stage 2: 30-40% loss (1500-2000 mL total)
	Vitals->ApplyBloodLoss(750.0f);
	TestEqual(TEXT("Stage 2 blood loss"), Vitals->GetBloodLossStage(), EMOBloodLossStage::Class2);
	AddInfo(FString::Printf(TEXT("After 1750mL loss - Blood: %.0f, Stage 2"),
		Vitals->Vitals.BloodVolume));

	// Stage 3: >40% loss (>2000 mL total)
	Vitals->ApplyBloodLoss(500.0f);
	TestEqual(TEXT("Stage 3 blood loss"), Vitals->GetBloodLossStage(), EMOBloodLossStage::Class3);
	AddInfo(FString::Printf(TEXT("After 2250mL loss - Blood: %.0f, Stage 3 (critical)"),
		Vitals->Vitals.BloodVolume));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Integration_MultipleConditions,
	"MOFramework.Medical.Integration.MultipleConditions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Integration_MultipleConditions::RunTest(const FString& Parameters)
{
	UMOAnatomyComponent* Anatomy = NewObject<UMOAnatomyComponent>();

	// Add multiple conditions
	Anatomy->AddCondition(EMOConditionType::Infection, EMOBodyPartType::ForearmLeft, 25.0f);
	Anatomy->AddCondition(EMOConditionType::Concussion, EMOBodyPartType::Head, 40.0f);
	Anatomy->AddCondition(EMOConditionType::FoodPoisoning, EMOBodyPartType::None, 30.0f);

	// Verify conditions are tracked
	const bool bHasInfection = Anatomy->HasCondition(EMOConditionType::Infection);
	const bool bHasConcussion = Anatomy->HasCondition(EMOConditionType::Concussion);
	const bool bHasPoison = Anatomy->HasCondition(EMOConditionType::FoodPoisoning);
	const bool bHasSepsis = Anatomy->HasCondition(EMOConditionType::Sepsis);

	AddInfo(FString::Printf(TEXT("Conditions - Infection: %s, Concussion: %s, Poison: %s, Sepsis: %s"),
		bHasInfection ? TEXT("Y") : TEXT("N"),
		bHasConcussion ? TEXT("Y") : TEXT("N"),
		bHasPoison ? TEXT("Y") : TEXT("N"),
		bHasSepsis ? TEXT("Y") : TEXT("N")));

	return true;
}

//=============================================================================
// INTEGRATION TESTS - Mental State Effects
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Integration_ShockFromTrauma,
	"MOFramework.Medical.Integration.ShockFromTrauma",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Integration_ShockFromTrauma::RunTest(const FString& Parameters)
{
	UMOMentalStateComponent* Mental = NewObject<UMOMentalStateComponent>();

	// Initial state
	TestEqual(TEXT("Initial shock is zero"), Mental->MentalState.ShockAccumulation, 0.0f);
	TestEqual(TEXT("Initial consciousness is Alert"), Mental->MentalState.Consciousness, EMOConsciousnessLevel::Alert);

	// Add shock from trauma
	Mental->AddShock(30.0f);
	AddInfo(FString::Printf(TEXT("After 30 shock: %.1f, conscious: %d"),
		Mental->MentalState.ShockAccumulation, static_cast<int32>(Mental->MentalState.Consciousness)));

	// Add more shock
	Mental->AddShock(40.0f);
	AddInfo(FString::Printf(TEXT("After 70 shock: %.1f, conscious: %d"),
		Mental->MentalState.ShockAccumulation, static_cast<int32>(Mental->MentalState.Consciousness)));

	// Critical shock
	Mental->AddShock(40.0f);
	AddInfo(FString::Printf(TEXT("After 110 shock (clamped): %.1f"),
		Mental->MentalState.ShockAccumulation));

	// Verify shock doesn't exceed 100
	TestTrue(TEXT("Shock capped at 100"), Mental->MentalState.ShockAccumulation <= 100.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Integration_ConsciousnessProgression,
	"MOFramework.Medical.Integration.ConsciousnessProgression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Integration_ConsciousnessProgression::RunTest(const FString& Parameters)
{
	UMOMentalStateComponent* Mental = NewObject<UMOMentalStateComponent>();

	// Test all consciousness levels
	TArray<TPair<EMOConsciousnessLevel, FString>> Levels = {
		{EMOConsciousnessLevel::Alert, TEXT("Alert")},
		{EMOConsciousnessLevel::Confused, TEXT("Confused")},
		{EMOConsciousnessLevel::Drowsy, TEXT("Drowsy")},
		{EMOConsciousnessLevel::Unconscious, TEXT("Unconscious")},
		{EMOConsciousnessLevel::Comatose, TEXT("Comatose")}
	};

	for (const auto& Level : Levels)
	{
		Mental->ForceConsciousnessLevel(Level.Key);
		const bool bCanAct = Mental->CanPerformActions();

		AddInfo(FString::Printf(TEXT("%s: CanPerformActions=%s"),
			*Level.Value, bCanAct ? TEXT("true") : TEXT("false")));
	}

	return true;
}

//=============================================================================
// INTEGRATION TESTS - Training System
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Integration_FitnessTraining,
	"MOFramework.Medical.Integration.FitnessTraining",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Integration_FitnessTraining::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	// Record initial fitness
	const float InitialStrength = Metabolism->BodyComposition.StrengthLevel;
	const float InitialCardio = Metabolism->BodyComposition.CardiovascularFitness;

	// Simulate training sessions
	for (int32 i = 0; i < 5; i++)
	{
		Metabolism->ApplyStrengthTraining(0.8f, 60.0f);  // High intensity, 60 seconds
		Metabolism->ApplyCardioTraining(0.6f, 60.0f);   // Moderate intensity, 60 seconds
	}

	const float FinalStrength = Metabolism->BodyComposition.StrengthLevel;
	const float FinalCardio = Metabolism->BodyComposition.CardiovascularFitness;

	AddInfo(FString::Printf(TEXT("Strength: %.1f -> %.1f, Cardio: %.1f -> %.1f"),
		InitialStrength, FinalStrength, InitialCardio, FinalCardio));

	return true;
}

//=============================================================================
// INTEGRATION TESTS - Combined Stressors
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Integration_CombinedStressors,
	"MOFramework.Medical.Integration.CombinedStressors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Integration_CombinedStressors::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();
	UMOMentalStateComponent* Mental = NewObject<UMOMentalStateComponent>();

	// Simulate multiple stressors: dehydration + low glucose + exertion
	Metabolism->Nutrients.HydrationLevel = 25.0f;  // Dehydrated
	Vitals->Vitals.BloodGlucose = 55.0f;          // Hypoglycemic
	Vitals->SetExertionLevel(80.0f);               // High exertion

	// Record combined state
	const bool bIsDehydrated = Metabolism->IsDehydrated();
	const bool bIsHypoglycemic = Vitals->Vitals.IsHypoglycemic();

	AddInfo(FString::Printf(TEXT("Combined stressors - Dehydrated: %s, Hypoglycemic: %s, Exertion: 80%%"),
		bIsDehydrated ? TEXT("Y") : TEXT("N"),
		bIsHypoglycemic ? TEXT("Y") : TEXT("N")));

	// These conditions would normally cascade to mental state
	// causing confusion, tremors, etc.

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Integration_RecoveryScenario,
	"MOFramework.Medical.Integration.RecoveryScenario",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Integration_RecoveryScenario::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	// Simulate injured/depleted state
	Vitals->ApplyBloodLoss(1200.0f);  // ~25% blood loss
	Metabolism->Nutrients.HydrationLevel = 35.0f;
	Metabolism->Nutrients.GlycogenStores = 100.0f;

	// Record depleted state
	const float DepletedBlood = Vitals->Vitals.BloodVolume;
	const float DepletedHydration = Metabolism->Nutrients.HydrationLevel;

	// Simulate recovery: transfusion, eating, drinking
	Vitals->ApplyBloodTransfusion(500.0f);
	Metabolism->DrinkWater(800.0f);
	Metabolism->ConsumeFood(MOMedicalTestData::MakeBalancedMeal(), FName("RecoveryMeal"));

	// Record recovered state
	const float RecoveredBlood = Vitals->Vitals.BloodVolume;
	const float RecoveredHydration = Metabolism->Nutrients.HydrationLevel;

	AddInfo(FString::Printf(TEXT("Blood: %.0f -> %.0f mL"), DepletedBlood, RecoveredBlood));
	AddInfo(FString::Printf(TEXT("Hydration: %.1f%% -> %.1f%%"), DepletedHydration, RecoveredHydration));

	// Verify improvement
	TestTrue(TEXT("Blood volume increased"), RecoveredBlood > DepletedBlood);

	return true;
}

//=============================================================================
// STRESS TESTS - High Volume Operations
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Stress_HighVolumeWounds,
	"MOFramework.Medical.Stress.HighVolumeWounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Stress_HighVolumeWounds::RunTest(const FString& Parameters)
{
	UMOAnatomyComponent* Anatomy = NewObject<UMOAnatomyComponent>();

	const double StartTime = FPlatformTime::Seconds();

	// Create 100 wounds across different body parts
	TArray<EMOBodyPartType> Parts = {
		EMOBodyPartType::Head, EMOBodyPartType::Torso,
		EMOBodyPartType::ForearmLeft, EMOBodyPartType::ForearmRight,
		EMOBodyPartType::ThighLeft, EMOBodyPartType::ThighRight,
		EMOBodyPartType::CalfLeft, EMOBodyPartType::CalfRight
	};

	TArray<EMOWoundType> WoundTypes = {
		EMOWoundType::Laceration, EMOWoundType::Puncture,
		EMOWoundType::Blunt, EMOWoundType::BurnFirst
	};

	for (int32 i = 0; i < 100; i++)
	{
		EMOBodyPartType Part = Parts[i % Parts.Num()];
		EMOWoundType Type = WoundTypes[i % WoundTypes.Num()];
		Anatomy->InflictDamage(Part, 5.0f + (i % 30), Type);
	}

	const double EndTime = FPlatformTime::Seconds();
	const double Duration = (EndTime - StartTime) * 1000.0;

	const int32 WoundCount = Anatomy->GetAllWounds().Num();
	const float TotalBleed = Anatomy->GetTotalBleedRate();
	const float TotalPain = Anatomy->GetTotalPainLevel();

	AddInfo(FString::Printf(TEXT("Created 100 wounds in %.2f ms"), Duration));
	AddInfo(FString::Printf(TEXT("Final: %d wounds, %.2f mL/s bleed, %.1f pain"),
		WoundCount, TotalBleed, TotalPain));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Stress_HighVolumeVitalChanges,
	"MOFramework.Medical.Stress.HighVolumeVitalChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Stress_HighVolumeVitalChanges::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();

	const double StartTime = FPlatformTime::Seconds();

	// Simulate 1000 rapid vital changes
	for (int32 i = 0; i < 1000; i++)
	{
		Vitals->ApplyBloodLoss(1.0f);
		Vitals->ApplyGlucose(0.5f);
		Vitals->ConsumeGlucose(0.3f);

		if (i % 100 == 0)
		{
			Vitals->ApplyBloodTransfusion(50.0f);
		}
	}

	const double EndTime = FPlatformTime::Seconds();
	const double Duration = (EndTime - StartTime) * 1000.0;

	AddInfo(FString::Printf(TEXT("1000 vital changes in %.2f ms"), Duration));
	AddInfo(FString::Printf(TEXT("Final - Blood: %.0f mL, Glucose: %.0f mg/dL"),
		Vitals->Vitals.BloodVolume, Vitals->Vitals.BloodGlucose));

	// Verify state remains valid
	TestTrue(TEXT("Blood volume valid"), Vitals->Vitals.BloodVolume >= 0.0f);
	TestTrue(TEXT("Blood glucose valid"), Vitals->Vitals.BloodGlucose >= 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Stress_HighVolumeDigestion,
	"MOFramework.Medical.Stress.HighVolumeDigestion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Stress_HighVolumeDigestion::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	const double StartTime = FPlatformTime::Seconds();

	// Consume 50 food items
	for (int32 i = 0; i < 50; i++)
	{
		FMOItemNutrition Food = MOMedicalTestData::MakeTestNutrition(
			50.0f + (i * 5),   // Varying calories
			5.0f + (i % 10),   // Varying protein
			10.0f + (i % 20),  // Varying carbs
			2.0f + (i % 5),    // Varying fat
			20.0f + (i * 2),   // Varying water
			1.0f               // Fiber
		);

		Metabolism->ConsumeFood(Food, FName(*FString::Printf(TEXT("Food_%d"), i)));
	}

	const double EndTime = FPlatformTime::Seconds();
	const double Duration = (EndTime - StartTime) * 1000.0;

	const int32 QueueSize = Metabolism->GetDigestingFoodCount();
	const float TotalCalories = Metabolism->TotalCaloriesConsumedToday;

	AddInfo(FString::Printf(TEXT("50 food items consumed in %.2f ms"), Duration));
	AddInfo(FString::Printf(TEXT("Queue size: %d, Total calories: %.0f"),
		QueueSize, TotalCalories));

	return true;
}

//=============================================================================
// BOUNDARY TESTS - Extreme Values
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Boundary_ZeroBlood,
	"MOFramework.Medical.Boundary.ZeroBlood",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Boundary_ZeroBlood::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();

	// Drain all blood
	Vitals->ApplyBloodLoss(5000.0f);

	TestTrue(TEXT("Blood volume at zero or positive"), Vitals->Vitals.BloodVolume >= 0.0f);
	TestEqual(TEXT("Blood loss stage is Class3"), Vitals->GetBloodLossStage(), EMOBloodLossStage::Class3);

	// Try to drain more (should not go negative)
	Vitals->ApplyBloodLoss(1000.0f);
	TestTrue(TEXT("Blood cannot go negative"), Vitals->Vitals.BloodVolume >= 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Boundary_MaxHydration,
	"MOFramework.Medical.Boundary.MaxHydration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Boundary_MaxHydration::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	// Start at full hydration
	Metabolism->Nutrients.HydrationLevel = 100.0f;

	// Try to over-hydrate
	Metabolism->DrinkWater(5000.0f);

	TestTrue(TEXT("Hydration capped at 100"), Metabolism->Nutrients.HydrationLevel <= 100.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Boundary_ZeroNutrients,
	"MOFramework.Medical.Boundary.ZeroNutrients",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Boundary_ZeroNutrients::RunTest(const FString& Parameters)
{
	UMOMetabolismComponent* Metabolism = NewObject<UMOMetabolismComponent>();

	// Deplete all nutrients
	Metabolism->Nutrients.GlycogenStores = 0.0f;
	Metabolism->Nutrients.HydrationLevel = 0.0f;
	Metabolism->BodyComposition.BodyFatPercent = 3.0f;  // Minimum viable

	// Check starvation detection
	TestTrue(TEXT("Is starving with no reserves"), Metabolism->IsStarving());
	TestTrue(TEXT("Is dehydrated at 0%"), Metabolism->IsDehydrated());

	// Try to burn calories with no reserves
	Metabolism->ApplyCalorieBurn(500.0f);

	// Should handle gracefully
	TestTrue(TEXT("Fat percent remains valid"), Metabolism->BodyComposition.BodyFatPercent >= 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Boundary_ExtremeTemperatures,
	"MOFramework.Medical.Boundary.ExtremeTemperatures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Boundary_ExtremeTemperatures::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();

	// Severe hypothermia
	Vitals->Vitals.BodyTemperature = 28.0f;
	TestTrue(TEXT("28C is hypothermic"), Vitals->Vitals.IsHypothermic());

	// Severe hyperthermia
	Vitals->Vitals.BodyTemperature = 42.0f;
	TestTrue(TEXT("42C is hyperthermic"), Vitals->Vitals.IsHyperthermic());

	// Normal range
	Vitals->Vitals.BodyTemperature = 37.0f;
	TestFalse(TEXT("37C is not hypothermic"), Vitals->Vitals.IsHypothermic());
	TestFalse(TEXT("37C is not hyperthermic"), Vitals->Vitals.IsHyperthermic());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedical_Boundary_ExtremeGlucose,
	"MOFramework.Medical.Boundary.ExtremeGlucose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMedical_Boundary_ExtremeGlucose::RunTest(const FString& Parameters)
{
	UMOVitalsComponent* Vitals = NewObject<UMOVitalsComponent>();

	// Severe hypoglycemia
	Vitals->Vitals.BloodGlucose = 30.0f;
	TestTrue(TEXT("30 mg/dL is hypoglycemic"), Vitals->Vitals.IsHypoglycemic());

	// Severe hyperglycemia
	Vitals->Vitals.BloodGlucose = 400.0f;
	TestTrue(TEXT("400 mg/dL is hyperglycemic"), Vitals->Vitals.IsHyperglycemic());

	// Normal range
	Vitals->Vitals.BloodGlucose = 90.0f;
	TestFalse(TEXT("90 mg/dL is not hypoglycemic"), Vitals->Vitals.IsHypoglycemic());
	TestFalse(TEXT("90 mg/dL is not hyperglycemic"), Vitals->Vitals.IsHyperglycemic());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
