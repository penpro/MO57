/**
 * =============================================================================
 * MOMetabolismTypes.h - Nutrition & Body Composition Types
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Defines types for the metabolism simulation: body composition, nutrition
 * tracking, digestion, and energy balance. Drives hunger/thirst systems
 * and long-term fitness progression.
 *
 * KEY TYPES:
 * - FMOBodyComposition: Weight, muscle, fat, fitness levels
 * - FMONutritionState: Macro/micronutrient status
 * - FMODigestionEntry: Food in digestive tract
 *
 * ENERGY FLOW:
 * Food → Stomach → Digestion → Nutrients → Blood Glucose → Cells
 *                                       ↓
 *                              Fat/Glycogen (storage)
 *
 * BODY COMPOSITION:
 * - TotalWeight = Muscle + Fat + Bone + Water + Organs
 * - MuscleMass: Increases with strength training, decreases with starvation
 * - BodyFatPercent: Increases with calorie surplus, decreases with deficit
 * - CardiovascularFitness: Increases with cardio, affects stamina recovery
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] WEIGHT UNITS: All weights are in KILOGRAMS, not pounds.
 *   Average adult male: 70-80 kg. Don't use imperial units.
 *
 * [2024-02] FAT MINIMUM: BodyFatPercent has ClampMin=3. Below 3% is lethal
 *   in real life. Don't allow it to go lower.
 *
 * [2024-02] TRAINING ACCUMULATORS: StrengthTrainingAccum/CardioTrainingAccum
 *   are transient authority-only values. They reset on save/load. This is
 *   intentional - only completed training sessions persist.
 *
 * [2024-02] CALORIE BALANCE: Positive balance = weight gain, negative = loss.
 *   ~7700 kcal = 1 kg of body fat. Use this for weight change calculations.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOMetabolismComponent.h - Uses these types for simulation
 * - MOVitalsComponent.h - Blood glucose affects vitals
 * - MOActivityTypes.h - Activity level affects calorie burn
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "MOMetabolismTypes.generated.h"

// Forward declarations

/**
 * Body composition metrics.
 * See file header for energy flow and body composition details.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORKCORE_API FMOBodyComposition
{
	GENERATED_BODY()

	/** Total body weight in kg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Weight")
	float TotalWeight = 75.0f;

	/** Lean muscle mass in kg (trainable). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Composition")
	float MuscleMass = 30.0f;

	/** Body fat percentage (diet-dependent). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Composition", meta=(ClampMin="3", ClampMax="50"))
	float BodyFatPercent = 18.0f;

	/** Bone mass in kg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Composition")
	float BoneMass = 3.5f;

	/** Cardiovascular fitness (0-100, trainable). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Fitness", meta=(ClampMin="0", ClampMax="100"))
	float CardiovascularFitness = 50.0f;

	/** Strength level (0-100, trainable). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Fitness", meta=(ClampMin="0", ClampMax="100"))
	float StrengthLevel = 50.0f;

	// ---- Training Accumulators (authority only) ----

	UPROPERTY()
	float StrengthTrainingAccum = 0.0f;

	UPROPERTY()
	float CardioTrainingAccum = 0.0f;

	// ---- Derived Calculations ----

	/** Get Basal Metabolic Rate in kcal/day. */
	float GetBMR() const
	{
		// Simplified: ~24 kcal per kg of lean mass per day
		float LeanMass = TotalWeight * (1.0f - BodyFatPercent / 100.0f);
		return LeanMass * 24.0f;
	}

	/** Get fat mass in kg. */
	float GetFatMass() const { return TotalWeight * (BodyFatPercent / 100.0f); }

	/** Get lean mass in kg. */
	float GetLeanMass() const { return TotalWeight - GetFatMass(); }

	/** Get cold resistance multiplier from body fat. */
	float GetColdResistance() const
	{
		// 5% body fat = 0.5x resistance, 30% = 1.5x
		return FMath::GetMappedRangeValueClamped(FVector2D(5.f, 30.f), FVector2D(0.5f, 1.5f), BodyFatPercent);
	}

	/** Get starvation survival time multiplier from fat reserves. */
	float GetStarvationSurvivalMultiplier() const
	{
		return FMath::Max(0.5f, BodyFatPercent / 15.0f);
	}
};

/**
 * Nutrient storage levels.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORKCORE_API FMONutrientLevels
{
	GENERATED_BODY()

	// ---- Energy Stores ----

	/** Glycogen stores in grams (liver + muscle, max ~500g = ~2000 kcal). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Energy")
	float GlycogenStores = 500.0f;

	/** Maximum glycogen storage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Energy")
	float MaxGlycogen = 500.0f;

	// ---- Hydration ----

	/** Hydration level (0-100%). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Hydration", meta=(ClampMin="0", ClampMax="100"))
	float HydrationLevel = 100.0f;

	// ---- Protein Balance ----

	/** Protein balance - negative = muscle catabolism. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Protein")
	float ProteinBalance = 0.0f;

	// ---- Vitamins (% of daily needs, 0-200) ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Vitamins", meta=(ClampMin="0", ClampMax="200"))
	float VitaminA = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Vitamins", meta=(ClampMin="0", ClampMax="200"))
	float VitaminB = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Vitamins", meta=(ClampMin="0", ClampMax="200"))
	float VitaminC = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Vitamins", meta=(ClampMin="0", ClampMax="200"))
	float VitaminD = 100.0f;

	// ---- Minerals (% of daily needs, 0-200) ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Minerals", meta=(ClampMin="0", ClampMax="200"))
	float Iron = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Minerals", meta=(ClampMin="0", ClampMax="200"))
	float Calcium = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Minerals", meta=(ClampMin="0", ClampMax="200"))
	float Potassium = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Minerals", meta=(ClampMin="0", ClampMax="200"))
	float Sodium = 100.0f;

	// ---- Deficiency Checks ----

	bool HasVitaminCDeficiency() const { return VitaminC < 30.0f; }  // Scurvy risk
	bool HasIronDeficiency() const { return Iron < 30.0f; }          // Anemia
	bool HasCalciumDeficiency() const { return Calcium < 30.0f; }    // Bone weakness
	bool HasSevereDehydration() const { return HydrationLevel < 70.0f; }
};

/**
 * Food item currently being digested.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORKCORE_API FMODigestingFood : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** Unique identifier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Digestion", meta=(IgnoreForMemberInitializationTest))
	FGuid DigestId;

	/** Reference to item definition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Digestion")
	FName FoodItemId;

	// ---- Remaining Macronutrients ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Digestion")
	float RemainingCalories = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Digestion")
	float RemainingProtein = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Digestion")
	float RemainingCarbs = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Digestion")
	float RemainingFat = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Digestion")
	float RemainingWater = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Digestion")
	float RemainingFiber = 0.0f;

	// ---- Remaining Vitamins ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Digestion")
	float RemainingVitaminA = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Digestion")
	float RemainingVitaminB = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Digestion")
	float RemainingVitaminC = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Digestion")
	float RemainingVitaminD = 0.0f;

	// ---- Remaining Minerals ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Digestion")
	float RemainingIron = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Digestion")
	float RemainingCalcite = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Digestion")
	float RemainingPotassium = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Digestion")
	float RemainingSodium = 0.0f;

	// ---- Digestion Timing ----

	/** Time spent digesting (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Digestion")
	float DigestTime = 0.0f;

	/** Total time needed for full digestion (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Digestion")
	float TotalDigestDuration = 3600.0f;

	FMODigestingFood()
	{
		DigestId = FGuid::NewGuid();
	}

	// ---- Absorption Rates (per second) ----
	// Carbs absorb fastest, fats slowest

	float GetCarbAbsorptionRate() const
	{
		float CarbDigestTime = TotalDigestDuration * 0.3f;  // 30% of total time
		return CarbDigestTime > 0.f ? RemainingCarbs / CarbDigestTime : 0.f;
	}

	float GetProteinAbsorptionRate() const
	{
		float ProteinDigestTime = TotalDigestDuration * 0.6f;  // 60% of total time
		return ProteinDigestTime > 0.f ? RemainingProtein / ProteinDigestTime : 0.f;
	}

	float GetFatAbsorptionRate() const
	{
		// Fats take full duration
		return TotalDigestDuration > 0.f ? RemainingFat / TotalDigestDuration : 0.f;
	}

	/** Check if digestion is complete. */
	bool IsDigestionComplete() const
	{
		return DigestTime >= TotalDigestDuration ||
			(RemainingCalories <= 0.f && RemainingProtein <= 0.f &&
			 RemainingCarbs <= 0.f && RemainingFat <= 0.f && RemainingWater <= 0.f);
	}
};

/**
 * FastArray container for digesting food.
 */

USTRUCT(BlueprintType)
struct MOFRAMEWORKCORE_API FMODigestingFoodList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMODigestingFood> Items;

	// NOTE: no owner back-pointer — the replication callbacks are stubs and
	// nothing ever used it (removed in the C1 decoupling).

	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FMODigestingFood, FMODigestingFoodList>(Items, DeltaParams, *this);
	}

	void AddFood(const FMODigestingFood& NewFood);
	void RemoveCompletedItems();
};

template<>
struct TStructOpsTypeTraits<FMODigestingFoodList> : public TStructOpsTypeTraitsBase2<FMODigestingFoodList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};
