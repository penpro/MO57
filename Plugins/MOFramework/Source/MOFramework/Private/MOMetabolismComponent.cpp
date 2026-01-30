#include "MOMetabolismComponent.h"
#include "MOVitalsComponent.h"
#include "MOAnatomyComponent.h"
#include "MOItemDefinitionRow.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UMOMetabolismComponent::UMOMetabolismComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	DigestingFood.SetOwner(this);
}

void UMOMetabolismComponent::BeginPlay()
{
	Super::BeginPlay();

	// Cache sibling components
	if (AActor* Owner = GetOwner())
	{
		CachedVitalsComp = Owner->FindComponentByClass<UMOVitalsComponent>();
		CachedAnatomyComp = Owner->FindComponentByClass<UMOAnatomyComponent>();
	}

	// Start tick timer on authority
	if (GetOwnerRole() == ROLE_Authority)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				TickTimerHandle,
				this,
				&UMOMetabolismComponent::TickMetabolism,
				TickInterval,
				true
			);
		}
	}
}

void UMOMetabolismComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UMOMetabolismComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UMOMetabolismComponent, BodyComposition, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMOMetabolismComponent, Nutrients, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMOMetabolismComponent, DigestingFood, COND_OwnerOnly);
}

// ============================================================================
// FOOD API
// ============================================================================

bool UMOMetabolismComponent::ConsumeFood(const FMOItemNutrition& Nutrition, FName ItemId)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	// Create digesting food entry
	FMODigestingFood NewFood;
	NewFood.FoodItemId = ItemId;
	NewFood.RemainingCalories = Nutrition.Calories;
	NewFood.RemainingProtein = Nutrition.Protein;
	NewFood.RemainingCarbs = Nutrition.Carbohydrates;
	NewFood.RemainingFat = Nutrition.Fat;
	NewFood.RemainingWater = Nutrition.WaterContent;
	NewFood.RemainingFiber = Nutrition.Fiber;
	NewFood.RemainingVitaminA = Nutrition.VitaminA;
	NewFood.RemainingVitaminB = Nutrition.VitaminB;
	NewFood.RemainingVitaminC = Nutrition.VitaminC;
	NewFood.RemainingVitaminD = Nutrition.VitaminD;
	NewFood.RemainingIron = Nutrition.Iron;
	NewFood.RemainingCalcite = Nutrition.Calcium;
	NewFood.RemainingPotassium = Nutrition.Potassium;
	NewFood.RemainingSodium = Nutrition.Sodium;
	NewFood.DigestTime = 0.0f;

	// Calculate digestion time based on composition
	// Fat-heavy foods take longer, simple carbs are faster
	// Default 1 hour base digestion time
	float BaseDuration = 3600.0f;

	// Adjust for composition
	float FatRatio = (Nutrition.Calories > 0.0f) ? (Nutrition.Fat * 9.0f / Nutrition.Calories) : 0.0f;
	float FiberMod = 1.0f + (Nutrition.Fiber * 0.1f);  // Fiber slows digestion

	NewFood.TotalDigestDuration = BaseDuration * (1.0f + FatRatio * 0.5f) * FiberMod;

	// Add to digestion queue
	DigestingFood.AddFood(NewFood);

	// Track daily consumption
	TotalCaloriesConsumedToday += Nutrition.Calories;

	return true;
}

void UMOMetabolismComponent::DrinkWater(float AmountML)
{
	if (GetOwnerRole() != ROLE_Authority || AmountML <= 0.0f)
	{
		return;
	}

	// Direct hydration (water absorbs quickly)
	// Normal daily need is ~2500mL, so each mL is ~0.04% of daily need
	float HydrationGain = (AmountML / DailyWaterRequirement) * 100.0f;

	float OldHydration = Nutrients.HydrationLevel;
	Nutrients.HydrationLevel = FMath::Clamp(Nutrients.HydrationLevel + HydrationGain, 0.0f, 100.0f);

	if (FMath::Abs(Nutrients.HydrationLevel - OldHydration) >= 5.0f)
	{
		OnNutrientLevelChanged.Broadcast(FName("Hydration"), Nutrients.HydrationLevel);
	}
}

int32 UMOMetabolismComponent::GetDigestingFoodCount() const
{
	return DigestingFood.Items.Num();
}

// ============================================================================
// CALORIE API
// ============================================================================

void UMOMetabolismComponent::ApplyCalorieBurn(float Calories)
{
	if (GetOwnerRole() != ROLE_Authority || Calories <= 0.0f)
	{
		return;
	}

	TotalCaloriesBurnedToday += Calories;

	// First, use blood glucose
	if (CachedVitalsComp)
	{
		// 1 calorie of carbs = 4 kcal/gram, blood glucose is in mg/dL
		// Simplified: just reduce glucose proportionally
		float GlucoseToUse = Calories * 0.1f;  // Rough conversion
		CachedVitalsComp->ConsumeGlucose(GlucoseToUse);
	}

	// Then glycogen stores
	// 1 gram glycogen = ~4 kcal
	float GlycogenCalories = Calories * 0.5f;  // 50% comes from glycogen during activity
	float GlycogenGrams = GlycogenCalories / 4.0f;

	if (Nutrients.GlycogenStores > 0.0f)
	{
		float UsedGlycogen = FMath::Min(Nutrients.GlycogenStores, GlycogenGrams);
		Nutrients.GlycogenStores -= UsedGlycogen;

		// Remaining calories must come from fat/protein
		float RemainingCalories = Calories - (UsedGlycogen * 4.0f);

		if (RemainingCalories > 0.0f)
		{
			// Burn fat (1 gram = ~9 kcal)
			float FatGrams = RemainingCalories / 9.0f;
			float FatKg = FatGrams / 1000.0f;

			// Reduce body fat
			float OldFatMass = BodyComposition.GetFatMass();
			float NewFatMass = FMath::Max(OldFatMass - FatKg, BodyComposition.TotalWeight * 0.03f);  // Min 3% body fat
			BodyComposition.BodyFatPercent = (NewFatMass / BodyComposition.TotalWeight) * 100.0f;
		}
	}
	else
	{
		// No glycogen - burning fat and potentially muscle (starvation mode)
		float FatCalories = Calories * 0.7f;
		float ProteinCalories = Calories * 0.3f;

		// Burn fat
		float FatGrams = FatCalories / 9.0f;
		float FatKg = FatGrams / 1000.0f;
		float OldFatMass = BodyComposition.GetFatMass();
		float NewFatMass = FMath::Max(OldFatMass - FatKg, BodyComposition.TotalWeight * 0.03f);
		BodyComposition.BodyFatPercent = (NewFatMass / BodyComposition.TotalWeight) * 100.0f;

		// Catabolize muscle (negative protein balance)
		float ProteinGrams = ProteinCalories / 4.0f;
		Nutrients.ProteinBalance -= ProteinGrams;
	}
}

float UMOMetabolismComponent::GetCurrentBMR() const
{
	return BodyComposition.GetBMR();
}

float UMOMetabolismComponent::GetTDEE() const
{
	return GetCurrentBMR() * BaseActivityMultiplier;
}

float UMOMetabolismComponent::GetCalorieBalance() const
{
	return TotalCaloriesConsumedToday - TotalCaloriesBurnedToday;
}

float UMOMetabolismComponent::GetCurrentStamina() const
{
	// Stamina based on glycogen stores and hydration
	// Full glycogen (500g) + full hydration = 100% stamina
	float GlycogenFactor = FMath::Clamp(Nutrients.GlycogenStores / 500.0f, 0.0f, 1.0f);
	float HydrationFactor = FMath::Clamp(Nutrients.HydrationLevel / 100.0f, 0.0f, 1.0f);

	// Weighted average - hydration is more immediately impactful
	return (GlycogenFactor * 0.4f) + (HydrationFactor * 0.6f);
}

float UMOMetabolismComponent::GetDailyCalorieBalance() const
{
	return GetCalorieBalance();
}

// ============================================================================
// TRAINING API
// ============================================================================

void UMOMetabolismComponent::ApplyStrengthTraining(float Intensity, float Duration)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	Intensity = FMath::Clamp(Intensity, 0.0f, 1.0f);

	// Training stimulus accumulates
	// Need ~45 minutes of moderate intensity for meaningful adaptation
	float Stimulus = Intensity * (Duration / 60.0f);  // Normalized to minutes

	BodyComposition.StrengthTrainingAccum += Stimulus;

	// Burn calories (strength training: ~5-8 kcal/min depending on intensity)
	float CaloriesPerMinute = 5.0f + (Intensity * 3.0f);
	float CaloriesBurned = CaloriesPerMinute * (Duration / 60.0f);
	ApplyCalorieBurn(CaloriesBurned);

	// Use protein for muscle repair
	Nutrients.ProteinBalance -= Intensity * (Duration / 3600.0f) * 5.0f;  // Protein demand
}

void UMOMetabolismComponent::ApplyCardioTraining(float Intensity, float Duration)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	Intensity = FMath::Clamp(Intensity, 0.0f, 1.0f);

	// Cardio training stimulus
	float Stimulus = Intensity * (Duration / 60.0f);

	BodyComposition.CardioTrainingAccum += Stimulus;

	// Burn calories (cardio: ~8-15 kcal/min depending on intensity)
	float CaloriesPerMinute = 8.0f + (Intensity * 7.0f);
	float CaloriesBurned = CaloriesPerMinute * (Duration / 60.0f);
	ApplyCalorieBurn(CaloriesBurned);

	// Increase exertion in vitals
	if (CachedVitalsComp)
	{
		CachedVitalsComp->SetExertionLevel(Intensity * 100.0f);
	}
}

// ============================================================================
// QUERY API
// ============================================================================

float UMOMetabolismComponent::GetDaysOfFatReserves() const
{
	float FatMass = BodyComposition.GetFatMass();
	float MinFatMass = BodyComposition.TotalWeight * 0.03f;  // Essential fat
	float UsableFat = FMath::Max(0.0f, FatMass - MinFatMass);

	// Fat provides ~9 kcal/gram = 9000 kcal/kg
	float AvailableCalories = UsableFat * 9000.0f;

	float DailyNeed = GetTDEE();
	if (DailyNeed <= 0.0f)
	{
		return 999.0f;
	}

	return AvailableCalories / DailyNeed;
}

float UMOMetabolismComponent::GetDaysUntilDehydration() const
{
	// Death occurs at ~0% hydration
	// Normal loss is ~2.5L/day = 100% over course of day
	// With no intake, death in ~3 days

	float CurrentHydration = Nutrients.HydrationLevel;

	// Daily water loss as percentage
	float DailyLossPercent = 100.0f / 3.0f;  // ~33% per day

	if (DailyLossPercent <= 0.0f)
	{
		return 999.0f;
	}

	return CurrentHydration / DailyLossPercent;
}

bool UMOMetabolismComponent::IsStarving() const
{
	// Starving when glycogen is depleted
	return Nutrients.GlycogenStores <= 10.0f;
}

bool UMOMetabolismComponent::IsDehydrated() const
{
	return Nutrients.HydrationLevel < 70.0f;
}

bool UMOMetabolismComponent::IsMalnourished() const
{
	// Check for multiple deficiencies
	int32 DeficiencyCount = 0;

	if (Nutrients.VitaminA < 30.0f) DeficiencyCount++;
	if (Nutrients.VitaminB < 30.0f) DeficiencyCount++;
	if (Nutrients.VitaminC < 30.0f) DeficiencyCount++;
	if (Nutrients.VitaminD < 30.0f) DeficiencyCount++;
	if (Nutrients.Iron < 30.0f) DeficiencyCount++;
	if (Nutrients.Calcium < 30.0f) DeficiencyCount++;
	if (Nutrients.ProteinBalance < -50.0f) DeficiencyCount++;

	return DeficiencyCount >= 3 || BodyComposition.BodyFatPercent < 5.0f;
}

float UMOMetabolismComponent::GetWoundHealingMultiplier() const
{
	float Multiplier = 1.0f;

	// Protein is essential for wound healing
	if (Nutrients.ProteinBalance < 0.0f)
	{
		Multiplier *= FMath::Max(0.3f, 1.0f + (Nutrients.ProteinBalance / 100.0f));
	}

	// Vitamin C is critical for collagen synthesis
	if (Nutrients.VitaminC < 50.0f)
	{
		Multiplier *= (Nutrients.VitaminC / 50.0f);
	}

	// Zinc (simplified as part of minerals)
	// Iron for oxygen delivery
	if (Nutrients.Iron < 50.0f)
	{
		Multiplier *= FMath::Max(0.5f, Nutrients.Iron / 50.0f);
	}

	// Dehydration slows healing
	if (IsDehydrated())
	{
		Multiplier *= (Nutrients.HydrationLevel / 100.0f);
	}

	// Starvation severely impairs healing
	if (IsStarving())
	{
		Multiplier *= 0.3f;
	}

	return FMath::Max(0.1f, Multiplier);
}

// ============================================================================
// PERSISTENCE
// ============================================================================

void UMOMetabolismComponent::BuildSaveData(FMOMetabolismSaveData& OutSaveData) const
{
	OutSaveData.BodyComposition = BodyComposition;
	OutSaveData.Nutrients = Nutrients;
	OutSaveData.TotalCaloriesConsumedToday = TotalCaloriesConsumedToday;
	OutSaveData.TotalCaloriesBurnedToday = TotalCaloriesBurnedToday;

	// Save digesting food
	OutSaveData.DigestingFood.Reset();
	for (const FMODigestingFood& Food : DigestingFood.Items)
	{
		FMODigestingFoodSaveEntry Entry;
		Entry.DigestId = Food.DigestId;
		Entry.FoodItemId = Food.FoodItemId;
		Entry.RemainingCalories = Food.RemainingCalories;
		Entry.RemainingProtein = Food.RemainingProtein;
		Entry.RemainingCarbs = Food.RemainingCarbs;
		Entry.RemainingFat = Food.RemainingFat;
		Entry.RemainingWater = Food.RemainingWater;
		Entry.RemainingFiber = Food.RemainingFiber;
		Entry.DigestTime = Food.DigestTime;
		Entry.TotalDigestDuration = Food.TotalDigestDuration;
		OutSaveData.DigestingFood.Add(Entry);
	}
}

bool UMOMetabolismComponent::ApplySaveDataAuthority(const FMOMetabolismSaveData& InSaveData)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	BodyComposition = InSaveData.BodyComposition;
	Nutrients = InSaveData.Nutrients;
	TotalCaloriesConsumedToday = InSaveData.TotalCaloriesConsumedToday;
	TotalCaloriesBurnedToday = InSaveData.TotalCaloriesBurnedToday;

	// Restore digesting food
	DigestingFood.Items.Reset();
	for (const FMODigestingFoodSaveEntry& Entry : InSaveData.DigestingFood)
	{
		FMODigestingFood Food;
		Food.DigestId = Entry.DigestId;
		Food.FoodItemId = Entry.FoodItemId;
		Food.RemainingCalories = Entry.RemainingCalories;
		Food.RemainingProtein = Entry.RemainingProtein;
		Food.RemainingCarbs = Entry.RemainingCarbs;
		Food.RemainingFat = Entry.RemainingFat;
		Food.RemainingWater = Entry.RemainingWater;
		Food.RemainingFiber = Entry.RemainingFiber;
		Food.DigestTime = Entry.DigestTime;
		Food.TotalDigestDuration = Entry.TotalDigestDuration;
		DigestingFood.Items.Add(Food);
	}

	// Update state tracking
	bWasDehydrated = IsDehydrated();
	bWasStarving = IsStarving();

	return true;
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

void UMOMetabolismComponent::TickMetabolism()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	float ScaledDeltaTime = TickInterval * TimeScaleMultiplier;

	// Process all metabolism functions
	ProcessDigestion(ScaledDeltaTime);
	ProcessBasalMetabolism(ScaledDeltaTime);
	ProcessHydration(ScaledDeltaTime);
	ProcessNutrientDecay(ScaledDeltaTime);
	ProcessFitnessDecay(ScaledDeltaTime);
	ProcessTrainingAdaptations(ScaledDeltaTime);
	UpdateBodyWeight(ScaledDeltaTime);

	// Check for state changes
	bool bIsDehydrated = IsDehydrated();
	bool bIsStarving = IsStarving();

	if (bIsDehydrated && !bWasDehydrated)
	{
		OnDehydrationBegins.Broadcast();
	}
	bWasDehydrated = bIsDehydrated;

	if (bIsStarving && !bWasStarving)
	{
		OnStarvationBegins.Broadcast();
	}
	bWasStarving = bIsStarving;

	// Check deficiencies
	CheckDeficiencies();

	// Broadcast general metabolism changed for UI updates
	OnMetabolismChanged.Broadcast();
}

void UMOMetabolismComponent::ProcessDigestion(float DeltaTime)
{
	TArray<FGuid> CompletedItems;

	for (FMODigestingFood& Food : DigestingFood.Items)
	{
		ProcessDigestingFood(Food, DeltaTime);

		if (Food.IsDigestionComplete())
		{
			CompletedItems.Add(Food.DigestId);
			OnFoodDigested.Broadcast(Food.FoodItemId);
		}
	}

	// Remove completed items
	for (const FGuid& Id : CompletedItems)
	{
		DigestingFood.Items.RemoveAll([&Id](const FMODigestingFood& Food)
		{
			return Food.DigestId == Id;
		});
	}

	if (CompletedItems.Num() > 0)
	{
		DigestingFood.MarkArrayDirty();
	}
}

void UMOMetabolismComponent::ProcessDigestingFood(FMODigestingFood& Food, float DeltaTime)
{
	Food.DigestTime += DeltaTime;

	if (Food.TotalDigestDuration <= 0.0f)
	{
		return;
	}

	float ProgressRatio = DeltaTime / Food.TotalDigestDuration;

	// Carbs absorb first (fast)
	float CarbAbsorb = 0.0f;
	if (Food.DigestTime < Food.TotalDigestDuration * 0.3f)
	{
		// Carb absorption phase (first 30% of digestion)
		CarbAbsorb = Food.RemainingCarbs * FMath::Min(1.0f, ProgressRatio * 3.0f);
		Food.RemainingCarbs = FMath::Max(0.0f, Food.RemainingCarbs - CarbAbsorb);
	}

	// Protein absorbs mid-range
	float ProteinAbsorb = 0.0f;
	if (Food.DigestTime >= Food.TotalDigestDuration * 0.1f &&
		Food.DigestTime < Food.TotalDigestDuration * 0.7f)
	{
		ProteinAbsorb = Food.RemainingProtein * FMath::Min(1.0f, ProgressRatio * 1.7f);
		Food.RemainingProtein = FMath::Max(0.0f, Food.RemainingProtein - ProteinAbsorb);
	}

	// Fat absorbs slowest (throughout)
	float FatAbsorb = Food.RemainingFat * ProgressRatio;
	Food.RemainingFat = FMath::Max(0.0f, Food.RemainingFat - FatAbsorb);

	// Water absorbs quickly
	float WaterAbsorb = Food.RemainingWater * FMath::Min(1.0f, ProgressRatio * 2.0f);
	Food.RemainingWater = FMath::Max(0.0f, Food.RemainingWater - WaterAbsorb);

	// Vitamins and minerals absorb with food
	float VitAAbsorb = Food.RemainingVitaminA * ProgressRatio;
	float VitBAbsorb = Food.RemainingVitaminB * ProgressRatio;
	float VitCAbsorb = Food.RemainingVitaminC * ProgressRatio;
	float VitDAbsorb = Food.RemainingVitaminD * ProgressRatio;
	float IronAbsorb = Food.RemainingIron * ProgressRatio;
	float CalciumAbsorb = Food.RemainingCalcite * ProgressRatio;
	float PotassiumAbsorb = Food.RemainingPotassium * ProgressRatio;
	float SodiumAbsorb = Food.RemainingSodium * ProgressRatio;

	Food.RemainingVitaminA = FMath::Max(0.0f, Food.RemainingVitaminA - VitAAbsorb);
	Food.RemainingVitaminB = FMath::Max(0.0f, Food.RemainingVitaminB - VitBAbsorb);
	Food.RemainingVitaminC = FMath::Max(0.0f, Food.RemainingVitaminC - VitCAbsorb);
	Food.RemainingVitaminD = FMath::Max(0.0f, Food.RemainingVitaminD - VitDAbsorb);
	Food.RemainingIron = FMath::Max(0.0f, Food.RemainingIron - IronAbsorb);
	Food.RemainingCalcite = FMath::Max(0.0f, Food.RemainingCalcite - CalciumAbsorb);
	Food.RemainingPotassium = FMath::Max(0.0f, Food.RemainingPotassium - PotassiumAbsorb);
	Food.RemainingSodium = FMath::Max(0.0f, Food.RemainingSodium - SodiumAbsorb);

	// Apply absorbed nutrients
	AbsorbNutrients(CarbAbsorb, ProteinAbsorb, FatAbsorb, WaterAbsorb,
					VitAAbsorb, VitBAbsorb, VitCAbsorb, VitDAbsorb,
					IronAbsorb, CalciumAbsorb, PotassiumAbsorb, SodiumAbsorb);
}

void UMOMetabolismComponent::ProcessBasalMetabolism(float DeltaTime)
{
	// BMR is in kcal/day, convert to kcal/second
	float BMRPerSecond = GetCurrentBMR() / 86400.0f;
	float CaloriesBurned = BMRPerSecond * DeltaTime;

	// Apply without tracking (basal is separate from activity)
	TotalCaloriesBurnedToday += CaloriesBurned;

	// Consume glycogen for basal metabolism
	// 1 gram glycogen = ~4 kcal
	float GlycogenNeeded = CaloriesBurned / 4.0f;

	if (Nutrients.GlycogenStores > 0.0f)
	{
		float UsedGlycogen = FMath::Min(Nutrients.GlycogenStores, GlycogenNeeded);
		Nutrients.GlycogenStores -= UsedGlycogen;

		// If glycogen insufficient, use fat
		float RemainingCalories = (GlycogenNeeded - UsedGlycogen) * 4.0f;
		if (RemainingCalories > 0.0f)
		{
			float FatKg = (RemainingCalories / 9.0f) / 1000.0f;
			float OldFatMass = BodyComposition.GetFatMass();
			float NewFatMass = FMath::Max(OldFatMass - FatKg, BodyComposition.TotalWeight * 0.03f);
			BodyComposition.BodyFatPercent = (NewFatMass / BodyComposition.TotalWeight) * 100.0f;
		}
	}
	else
	{
		// Starvation: burn fat and muscle
		float FatKg = (CaloriesBurned * 0.7f / 9.0f) / 1000.0f;
		float ProteinGrams = (CaloriesBurned * 0.3f) / 4.0f;

		float OldFatMass = BodyComposition.GetFatMass();
		float NewFatMass = FMath::Max(OldFatMass - FatKg, BodyComposition.TotalWeight * 0.03f);
		BodyComposition.BodyFatPercent = (NewFatMass / BodyComposition.TotalWeight) * 100.0f;

		Nutrients.ProteinBalance -= ProteinGrams;
	}
}

void UMOMetabolismComponent::ProcessHydration(float DeltaTime)
{
	// Daily water loss ~2500mL = 100% hydration per day
	// Loss per second = 100 / 86400 = ~0.00116% per second
	float HydrationLossPerSecond = 100.0f / 86400.0f;

	// Activity and temperature increase water loss
	// Simplified: handled by exertion in vitals

	float OldHydration = Nutrients.HydrationLevel;
	Nutrients.HydrationLevel = FMath::Max(0.0f, Nutrients.HydrationLevel - HydrationLossPerSecond * DeltaTime);

	// Update vitals with dehydration effects
	if (CachedVitalsComp)
	{
		// Severe dehydration affects blood volume (plasma)
		if (Nutrients.HydrationLevel < 50.0f)
		{
			// Blood plasma decreases with dehydration
			// Not direct blood loss, but reduced volume
			float DehydrationFactor = (50.0f - Nutrients.HydrationLevel) / 50.0f;
			// Already handled by vitals reading metabolism state
		}
	}

	if (FMath::Abs(Nutrients.HydrationLevel - OldHydration) >= 5.0f)
	{
		OnNutrientLevelChanged.Broadcast(FName("Hydration"), Nutrients.HydrationLevel);
	}
}

void UMOMetabolismComponent::ProcessNutrientDecay(float DeltaTime)
{
	// Vitamins and minerals deplete over time (daily requirements)
	// Lose ~100% per day = ~0.00116% per second
	float DecayPerSecond = 100.0f / 86400.0f;
	float Decay = DecayPerSecond * DeltaTime;

	Nutrients.VitaminA = FMath::Max(0.0f, Nutrients.VitaminA - Decay);
	Nutrients.VitaminB = FMath::Max(0.0f, Nutrients.VitaminB - Decay);
	Nutrients.VitaminC = FMath::Max(0.0f, Nutrients.VitaminC - Decay);
	Nutrients.VitaminD = FMath::Max(0.0f, Nutrients.VitaminD - Decay * 0.5f);  // D stored longer
	Nutrients.Iron = FMath::Max(0.0f, Nutrients.Iron - Decay * 0.5f);  // Iron stored in body
	Nutrients.Calcium = FMath::Max(0.0f, Nutrients.Calcium - Decay);
	Nutrients.Potassium = FMath::Max(0.0f, Nutrients.Potassium - Decay);
	Nutrients.Sodium = FMath::Max(0.0f, Nutrients.Sodium - Decay);
}

void UMOMetabolismComponent::ProcessFitnessDecay(float DeltaTime)
{
	// Fitness decays if not training
	// FitnessDecayRate is per day, convert to per second
	float DecayPerSecond = FitnessDecayRate / 86400.0f;
	float Decay = DecayPerSecond * DeltaTime;

	// Only decay if not training recently
	if (BodyComposition.StrengthTrainingAccum < 1.0f)
	{
		BodyComposition.StrengthLevel = FMath::Max(10.0f, BodyComposition.StrengthLevel - Decay * 100.0f);
	}

	if (BodyComposition.CardioTrainingAccum < 1.0f)
	{
		BodyComposition.CardiovascularFitness = FMath::Max(10.0f, BodyComposition.CardiovascularFitness - Decay * 100.0f);
	}
}

void UMOMetabolismComponent::ProcessTrainingAdaptations(float DeltaTime)
{
	// Training adaptations happen over time (recovery/growth)
	// Need adequate protein and rest

	float ProteinFactor = (Nutrients.ProteinBalance >= 0.0f) ? 1.0f : 0.5f;

	// Strength adaptation
	if (BodyComposition.StrengthTrainingAccum > 0.0f)
	{
		// ~45 minutes of training = 45 units of stimulus
		// Adaptation rate: gain ~1 point per day of good training
		float AdaptationRate = 1.0f / 86400.0f;  // Per second
		float Adaptation = BodyComposition.StrengthTrainingAccum * AdaptationRate * ProteinFactor * DeltaTime;

		BodyComposition.StrengthLevel = FMath::Min(100.0f, BodyComposition.StrengthLevel + Adaptation);

		// Decay training accumulator
		BodyComposition.StrengthTrainingAccum = FMath::Max(0.0f, BodyComposition.StrengthTrainingAccum - DeltaTime / 86400.0f * 50.0f);
	}

	// Cardio adaptation
	if (BodyComposition.CardioTrainingAccum > 0.0f)
	{
		float AdaptationRate = 1.0f / 86400.0f;
		float Adaptation = BodyComposition.CardioTrainingAccum * AdaptationRate * DeltaTime;

		BodyComposition.CardiovascularFitness = FMath::Min(100.0f, BodyComposition.CardiovascularFitness + Adaptation);

		BodyComposition.CardioTrainingAccum = FMath::Max(0.0f, BodyComposition.CardioTrainingAccum - DeltaTime / 86400.0f * 50.0f);
	}

	// Muscle mass changes with protein balance and training
	if (Nutrients.ProteinBalance < -20.0f)
	{
		// Losing muscle
		float MuscleDecay = (-Nutrients.ProteinBalance / 100.0f) * 0.01f * DeltaTime / 86400.0f;  // kg per day
		BodyComposition.MuscleMass = FMath::Max(15.0f, BodyComposition.MuscleMass - MuscleDecay);
	}
	else if (Nutrients.ProteinBalance > 10.0f && BodyComposition.StrengthTrainingAccum > 10.0f)
	{
		// Building muscle (requires surplus protein AND training)
		float MuscleGain = 0.01f * DeltaTime / 86400.0f;  // ~0.01 kg per day max
		BodyComposition.MuscleMass = FMath::Min(50.0f, BodyComposition.MuscleMass + MuscleGain);
	}
}

void UMOMetabolismComponent::UpdateBodyWeight(float DeltaTime)
{
	// Weight = Muscle + Fat + Bone + Other (water, organs, etc.)
	float FatMass = BodyComposition.GetFatMass();
	float MuscleMass = BodyComposition.MuscleMass;
	float BoneMass = BodyComposition.BoneMass;
	float OtherMass = 15.0f;  // Rough estimate for organs, blood, etc.

	float OldWeight = BodyComposition.TotalWeight;
	BodyComposition.TotalWeight = FatMass + MuscleMass + BoneMass + OtherMass;

	// Recalculate body fat percentage based on new weight
	if (BodyComposition.TotalWeight > 0.0f)
	{
		BodyComposition.BodyFatPercent = (FatMass / BodyComposition.TotalWeight) * 100.0f;
	}

	if (FMath::Abs(BodyComposition.TotalWeight - OldWeight) >= 0.5f)
	{
		OnBodyCompositionChanged.Broadcast(FName("TotalWeight"), BodyComposition.TotalWeight);
	}
}

void UMOMetabolismComponent::CheckDeficiencies()
{
	static TMap<FName, float> LastReportedDeficiencies;

	auto CheckAndReport = [this](FName Name, float Value, float Threshold)
	{
		if (Value < Threshold)
		{
			float* LastValue = LastReportedDeficiencies.Find(Name);
			if (!LastValue || FMath::Abs(*LastValue - Value) >= 10.0f)
			{
				OnDeficiencyDetected.Broadcast(Name);
				LastReportedDeficiencies.Add(Name, Value);
			}
		}
	};

	CheckAndReport(FName("VitaminA"), Nutrients.VitaminA, 30.0f);
	CheckAndReport(FName("VitaminB"), Nutrients.VitaminB, 30.0f);
	CheckAndReport(FName("VitaminC"), Nutrients.VitaminC, 30.0f);
	CheckAndReport(FName("VitaminD"), Nutrients.VitaminD, 30.0f);
	CheckAndReport(FName("Iron"), Nutrients.Iron, 30.0f);
	CheckAndReport(FName("Calcium"), Nutrients.Calcium, 30.0f);
}

void UMOMetabolismComponent::AbsorbNutrients(float Carbs, float Protein, float Fat, float Water,
											 float VitA, float VitB, float VitC, float VitD,
											 float Iron, float Calcium, float Potassium, float Sodium)
{
	// Process macronutrients
	ProcessCarbAbsorption(Carbs);
	ProcessProteinAbsorption(Protein);
	ProcessFatAbsorption(Fat);

	// Water directly increases hydration
	if (Water > 0.0f)
	{
		float HydrationGain = (Water / DailyWaterRequirement) * 100.0f;
		Nutrients.HydrationLevel = FMath::Clamp(Nutrients.HydrationLevel + HydrationGain, 0.0f, 100.0f);
	}

	// Vitamins and minerals add to stores (capped at 200%)
	Nutrients.VitaminA = FMath::Clamp(Nutrients.VitaminA + VitA, 0.0f, 200.0f);
	Nutrients.VitaminB = FMath::Clamp(Nutrients.VitaminB + VitB, 0.0f, 200.0f);
	Nutrients.VitaminC = FMath::Clamp(Nutrients.VitaminC + VitC, 0.0f, 200.0f);
	Nutrients.VitaminD = FMath::Clamp(Nutrients.VitaminD + VitD, 0.0f, 200.0f);
	Nutrients.Iron = FMath::Clamp(Nutrients.Iron + Iron, 0.0f, 200.0f);
	Nutrients.Calcium = FMath::Clamp(Nutrients.Calcium + Calcium, 0.0f, 200.0f);
	Nutrients.Potassium = FMath::Clamp(Nutrients.Potassium + Potassium, 0.0f, 200.0f);
	Nutrients.Sodium = FMath::Clamp(Nutrients.Sodium + Sodium, 0.0f, 200.0f);
}

void UMOMetabolismComponent::ProcessCarbAbsorption(float CarbGrams)
{
	if (CarbGrams <= 0.0f)
	{
		return;
	}

	// Carbs convert to blood glucose and glycogen
	// 1 gram carbs = ~4 kcal

	// First, replenish glycogen stores
	float GlycogenDeficit = Nutrients.MaxGlycogen - Nutrients.GlycogenStores;
	float GlycogenToStore = FMath::Min(CarbGrams, GlycogenDeficit);
	Nutrients.GlycogenStores += GlycogenToStore;

	// Remaining carbs go to blood glucose
	float RemainingCarbs = CarbGrams - GlycogenToStore;
	if (RemainingCarbs > 0.0f && CachedVitalsComp)
	{
		// Convert to blood glucose (simplified)
		// 1 gram glucose raises blood glucose by ~3-4 mg/dL for average person
		float GlucoseIncrease = RemainingCarbs * 3.0f;
		CachedVitalsComp->ApplyGlucose(GlucoseIncrease);
	}
}

void UMOMetabolismComponent::ProcessProteinAbsorption(float ProteinGrams)
{
	if (ProteinGrams <= 0.0f)
	{
		return;
	}

	// Protein goes toward muscle maintenance/building
	// Daily requirement is roughly 0.8-1.0 g/kg body weight
	float DailyRequirement = BodyComposition.TotalWeight * 0.9f;  // grams/day

	// Add to protein balance
	Nutrients.ProteinBalance += ProteinGrams;

	// Cap protein balance (excess is converted to energy or excreted)
	if (Nutrients.ProteinBalance > 50.0f)
	{
		float ExcessProtein = Nutrients.ProteinBalance - 50.0f;
		Nutrients.ProteinBalance = 50.0f;

		// Excess protein can be converted to glucose (gluconeogenesis)
		// But this is less efficient
	}
}

void UMOMetabolismComponent::ProcessFatAbsorption(float FatGrams)
{
	if (FatGrams <= 0.0f)
	{
		return;
	}

	// Fat gets stored as body fat
	// 1 gram fat = ~9 kcal

	// If in calorie deficit, fat is used for energy
	// If in surplus, fat is stored

	// Simplified: add directly to fat stores
	float FatKg = FatGrams / 1000.0f;

	float CurrentFatMass = BodyComposition.GetFatMass();
	float NewFatMass = CurrentFatMass + FatKg;
	float NewFatPercent = (NewFatMass / BodyComposition.TotalWeight) * 100.0f;

	// Cap body fat at reasonable maximum
	BodyComposition.BodyFatPercent = FMath::Clamp(NewFatPercent, 3.0f, 50.0f);
}
