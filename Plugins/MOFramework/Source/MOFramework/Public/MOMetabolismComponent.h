#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOMedicalTypes.h"
#include "MOItemDefinitionRow.h"
#include "MOMetabolismComponent.generated.h"

class UMOVitalsComponent;
class UMOAnatomyComponent;

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnNutrientLevelChanged, FName, NutrientName, float, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnBodyCompositionChanged, FName, MetricName, float, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnFoodDigested, FName, FoodItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnStarvationBegins);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnDehydrationBegins);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnDeficiencyDetected, FName, NutrientName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnMetabolismChanged);

// ============================================================================
// SAVE DATA
// ============================================================================

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMODigestingFoodSaveEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid DigestId;

	UPROPERTY()
	FName FoodItemId;

	UPROPERTY()
	float RemainingCalories = 0.0f;

	UPROPERTY()
	float RemainingProtein = 0.0f;

	UPROPERTY()
	float RemainingCarbs = 0.0f;

	UPROPERTY()
	float RemainingFat = 0.0f;

	UPROPERTY()
	float RemainingWater = 0.0f;

	UPROPERTY()
	float RemainingFiber = 0.0f;

	UPROPERTY()
	float DigestTime = 0.0f;

	UPROPERTY()
	float TotalDigestDuration = 0.0f;
};

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOMetabolismSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	FMOBodyComposition BodyComposition;

	UPROPERTY()
	FMONutrientLevels Nutrients;

	UPROPERTY()
	TArray<FMODigestingFoodSaveEntry> DigestingFood;

	UPROPERTY()
	float TotalCaloriesConsumedToday = 0.0f;

	UPROPERTY()
	float TotalCaloriesBurnedToday = 0.0f;
};

// ============================================================================
// COMPONENT
// ============================================================================

/**
 * Component managing metabolism, nutrition, and body composition.
 * Handles food digestion, calorie expenditure, fitness training, and nutrient tracking.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOMetabolismComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOMetabolismComponent();

	// ============================================================================
	// REPLICATED STATE
	// ============================================================================

	/** Body composition metrics. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="MO|Metabolism")
	FMOBodyComposition BodyComposition;

	/** Current nutrient levels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="MO|Metabolism")
	FMONutrientLevels Nutrients;

	/** Food currently being digested. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="MO|Metabolism")
	FMODigestingFoodList DigestingFood;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Time scale multiplier (1.0 = real time). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Config", meta=(ClampMin="0.01"))
	float TimeScaleMultiplier = 1.0f;

	/** Daily water requirement in mL (normal ~2500mL/day). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Config")
	float DailyWaterRequirement = 2500.0f;

	/** Daily calorie requirement (calculated from BMR + activity). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Config")
	float BaseActivityMultiplier = 1.4f;

	/** Rate of fitness decay per day when not training (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Metabolism|Config", meta=(ClampMin="0", ClampMax="1"))
	float FitnessDecayRate = 0.01f;

	// ============================================================================
	// TRACKING
	// ============================================================================

	/** Calories consumed today. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Metabolism|Tracking")
	float TotalCaloriesConsumedToday = 0.0f;

	/** Calories burned today. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Metabolism|Tracking")
	float TotalCaloriesBurnedToday = 0.0f;

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Fired when a nutrient level changes significantly. */
	UPROPERTY(BlueprintAssignable, Category="MO|Metabolism|Events")
	FMOOnNutrientLevelChanged OnNutrientLevelChanged;

	/** Fired when body composition changes. */
	UPROPERTY(BlueprintAssignable, Category="MO|Metabolism|Events")
	FMOOnBodyCompositionChanged OnBodyCompositionChanged;

	/** Fired when food item finishes digesting. */
	UPROPERTY(BlueprintAssignable, Category="MO|Metabolism|Events")
	FMOOnFoodDigested OnFoodDigested;

	/** Fired when starvation state begins. */
	UPROPERTY(BlueprintAssignable, Category="MO|Metabolism|Events")
	FMOOnStarvationBegins OnStarvationBegins;

	/** Fired when dehydration state begins. */
	UPROPERTY(BlueprintAssignable, Category="MO|Metabolism|Events")
	FMOOnDehydrationBegins OnDehydrationBegins;

	/** Fired when a nutrient deficiency is detected. */
	UPROPERTY(BlueprintAssignable, Category="MO|Metabolism|Events")
	FMOOnDeficiencyDetected OnDeficiencyDetected;

	/** Fired when any metabolism value changes (for UI updates). */
	UPROPERTY(BlueprintAssignable, Category="MO|Metabolism|Events")
	FMOOnMetabolismChanged OnMetabolismChanged;

	// ============================================================================
	// FOOD API
	// ============================================================================

	/**
	 * Consume food, adding it to the digestion queue.
	 * @param Nutrition Nutritional content of the food.
	 * @param ItemId Reference ID for the food item.
	 * @return True if food was successfully consumed.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Metabolism|Food")
	bool ConsumeFood(const FMOItemNutrition& Nutrition, FName ItemId);

	/**
	 * Drink water directly (bypasses digestion).
	 * @param AmountML Amount of water in mL.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Metabolism|Food")
	void DrinkWater(float AmountML);

	/**
	 * Get number of items currently being digested.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Metabolism|Food")
	int32 GetDigestingFoodCount() const;

	// ============================================================================
	// CALORIE API
	// ============================================================================

	/**
	 * Apply calorie burn from activity.
	 * @param Calories Amount of calories burned.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Metabolism|Calories")
	void ApplyCalorieBurn(float Calories);

	/**
	 * Get current Basal Metabolic Rate in kcal/day.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Metabolism|Calories")
	float GetCurrentBMR() const;

	/**
	 * Get total daily energy expenditure (BMR * activity multiplier).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Metabolism|Calories")
	float GetTDEE() const;

	/**
	 * Get calorie balance for today (consumed - burned).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Metabolism|Calories")
	float GetCalorieBalance() const;

	// ============================================================================
	// TRAINING API
	// ============================================================================

	/**
	 * Apply strength training stimulus.
	 * @param Intensity Training intensity (0-1).
	 * @param Duration Duration in seconds.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Metabolism|Training")
	void ApplyStrengthTraining(float Intensity, float Duration);

	/**
	 * Apply cardiovascular training stimulus.
	 * @param Intensity Training intensity (0-1).
	 * @param Duration Duration in seconds.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Metabolism|Training")
	void ApplyCardioTraining(float Intensity, float Duration);

	// ============================================================================
	// QUERY API
	// ============================================================================

	/**
	 * Get current nutrient levels (const reference for reading).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Metabolism|Query")
	const FMONutrientLevels& GetNutrientLevels() const { return Nutrients; }

	/**
	 * Get current body composition (const reference for reading).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Metabolism|Query")
	const FMOBodyComposition& GetBodyComposition() const { return BodyComposition; }

	/**
	 * Get current stamina as 0-1.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Metabolism|Query")
	float GetCurrentStamina() const;

	/**
	 * Get daily calorie balance (consumed - burned).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Metabolism|Query")
	float GetDailyCalorieBalance() const;

	/**
	 * Get estimated days of fat reserves at current burn rate.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Metabolism|Query")
	float GetDaysOfFatReserves() const;

	/**
	 * Get estimated days until dehydration death at current rate.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Metabolism|Query")
	float GetDaysUntilDehydration() const;

	/**
	 * Check if currently starving (glycogen depleted, using fat/muscle).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Metabolism|Query")
	bool IsStarving() const;

	/**
	 * Check if currently dehydrated.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Metabolism|Query")
	bool IsDehydrated() const;

	/**
	 * Check if severely malnourished.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Metabolism|Query")
	bool IsMalnourished() const;

	/**
	 * Get wound healing multiplier based on nutrition.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Metabolism|Query")
	float GetWoundHealingMultiplier() const;

	// ============================================================================
	// PERSISTENCE
	// ============================================================================

	/**
	 * Build save data from current state.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Metabolism|Save")
	void BuildSaveData(FMOMetabolismSaveData& OutSaveData) const;

	/**
	 * Apply save data to restore state (authority only).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Metabolism|Save")
	bool ApplySaveDataAuthority(const FMOMetabolismSaveData& InSaveData);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// ============================================================================
	// INTERNAL STATE
	// ============================================================================

	/** Timer for periodic metabolism processing. */
	FTimerHandle TickTimerHandle;

	/** Tick interval in seconds. */
	float TickInterval = 1.0f;

	/** Cached reference to vitals component. */
	UPROPERTY(Transient)
	TObjectPtr<UMOVitalsComponent> CachedVitalsComp;

	/** Cached reference to anatomy component. */
	UPROPERTY(Transient)
	TObjectPtr<UMOAnatomyComponent> CachedAnatomyComp;

	/** Track previous hydration for event detection. */
	bool bWasDehydrated = false;

	/** Track previous starvation for event detection. */
	bool bWasStarving = false;

	// ============================================================================
	// INTERNAL METHODS
	// ============================================================================

	/** Periodic tick to process metabolism. */
	void TickMetabolism();

	/** Process digestion for all food items. */
	void ProcessDigestion(float DeltaTime);

	/** Process a single digesting food item. */
	void ProcessDigestingFood(FMODigestingFood& Food, float DeltaTime);

	/** Apply basal calorie consumption. */
	void ProcessBasalMetabolism(float DeltaTime);

	/** Process hydration loss. */
	void ProcessHydration(float DeltaTime);

	/** Process vitamin/mineral decay over time. */
	void ProcessNutrientDecay(float DeltaTime);

	/** Process fitness decay if not training. */
	void ProcessFitnessDecay(float DeltaTime);

	/** Apply training adaptations. */
	void ProcessTrainingAdaptations(float DeltaTime);

	/** Update body weight based on calorie balance. */
	void UpdateBodyWeight(float DeltaTime);

	/** Check for nutrient deficiencies and fire events. */
	void CheckDeficiencies();

	/** Add nutrients from absorbed food. */
	void AbsorbNutrients(float Carbs, float Protein, float Fat, float Water,
						 float VitA, float VitB, float VitC, float VitD,
						 float Iron, float Calcium, float Potassium, float Sodium);

	/** Convert carbs to glycogen or blood glucose. */
	void ProcessCarbAbsorption(float CarbGrams);

	/** Convert protein for muscle maintenance/building. */
	void ProcessProteinAbsorption(float ProteinGrams);

	/** Store fat or convert to energy. */
	void ProcessFatAbsorption(float FatGrams);
};
