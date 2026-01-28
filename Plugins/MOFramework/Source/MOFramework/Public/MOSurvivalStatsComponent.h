#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOItemDefinitionRow.h"

#include "MOSurvivalStatsComponent.generated.h"

class UMOInventoryComponent;

/**
 * A single survival stat with current/max values and rates.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOSurvivalStat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Survival")
	float Current = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Survival")
	float Max = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Survival")
	float RegenRate = 0.0f;  // per second (positive = regenerate)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Survival")
	float DecayRate = 0.0f;  // per second (positive = decay)

	float GetPercent() const { return Max > 0.0f ? Current / Max : 0.0f; }
	bool IsDepleted() const { return Current <= 0.0f; }
	bool IsCritical(float Threshold = 0.25f) const { return GetPercent() <= Threshold; }
};

/**
 * Tracks accumulated nutrition levels from consumed food.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMONutritionStatus
{
	GENERATED_BODY()

	// Macronutrient accumulation
	UPROPERTY(BlueprintReadOnly, Category="MO|Survival|Nutrition")
	float Calories = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="MO|Survival|Nutrition")
	float Hydration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="MO|Survival|Nutrition")
	float Protein = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="MO|Survival|Nutrition")
	float Carbohydrates = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="MO|Survival|Nutrition")
	float Fat = 0.0f;

	// Vitamin levels (0-100 representing % of daily needs)
	UPROPERTY(BlueprintReadOnly, Category="MO|Survival|Nutrition|Vitamins")
	float VitaminA = 50.0f;

	UPROPERTY(BlueprintReadOnly, Category="MO|Survival|Nutrition|Vitamins")
	float VitaminB = 50.0f;

	UPROPERTY(BlueprintReadOnly, Category="MO|Survival|Nutrition|Vitamins")
	float VitaminC = 50.0f;

	UPROPERTY(BlueprintReadOnly, Category="MO|Survival|Nutrition|Vitamins")
	float VitaminD = 50.0f;

	// Mineral levels (0-100 representing % of daily needs)
	UPROPERTY(BlueprintReadOnly, Category="MO|Survival|Nutrition|Minerals")
	float Iron = 50.0f;

	UPROPERTY(BlueprintReadOnly, Category="MO|Survival|Nutrition|Minerals")
	float Calcium = 50.0f;

	UPROPERTY(BlueprintReadOnly, Category="MO|Survival|Nutrition|Minerals")
	float Potassium = 50.0f;

	UPROPERTY(BlueprintReadOnly, Category="MO|Survival|Nutrition|Minerals")
	float Sodium = 50.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOOnSurvivalStatChanged, FName, StatName, float, OldValue, float, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnSurvivalStatDepleted, FName, StatName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnSurvivalStatCritical, FName, StatName, float, CurrentPercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnNutritionApplied, const FMOItemNutrition&, Nutrition);

/**
 * Component that manages survival stats like health, hunger, thirst, etc.
 * Tracks nutrition from consumed food and applies effects over time.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOSurvivalStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOSurvivalStatsComponent();

	// Stats
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="MO|Survival|Stats")
	FMOSurvivalStat Health;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="MO|Survival|Stats")
	FMOSurvivalStat Stamina;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="MO|Survival|Stats")
	FMOSurvivalStat Hunger;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="MO|Survival|Stats")
	FMOSurvivalStat Thirst;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="MO|Survival|Stats")
	FMOSurvivalStat Temperature;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="MO|Survival|Stats")
	FMOSurvivalStat Energy;

	// Nutrition tracking
	UPROPERTY(BlueprintReadOnly, Replicated, Category="MO|Survival|Nutrition")
	FMONutritionStatus NutritionStatus;

	// Delegates
	UPROPERTY(BlueprintAssignable, Category="MO|Survival|Events")
	FMOOnSurvivalStatChanged OnStatChanged;

	UPROPERTY(BlueprintAssignable, Category="MO|Survival|Events")
	FMOOnSurvivalStatDepleted OnStatDepleted;

	UPROPERTY(BlueprintAssignable, Category="MO|Survival|Events")
	FMOOnSurvivalStatCritical OnStatCritical;

	UPROPERTY(BlueprintAssignable, Category="MO|Survival|Events")
	FMOOnNutritionApplied OnNutritionApplied;

	// Configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Survival|Config")
	float TickInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Survival|Config")
	float CriticalThreshold = 0.25f;

	/** How quickly nutrition levels decay per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Survival|Config")
	float NutritionDecayRate = 0.1f;

	// Functions
	UFUNCTION(BlueprintCallable, Category="MO|Survival")
	void ApplyNutrition(const FMOItemNutrition& Nutrition);

	UFUNCTION(BlueprintCallable, Category="MO|Survival")
	bool ConsumeItem(UMOInventoryComponent* InventoryComponent, const FGuid& ItemGuid);

	UFUNCTION(BlueprintCallable, Category="MO|Survival")
	void ModifyStat(FName StatName, float Delta);

	UFUNCTION(BlueprintCallable, Category="MO|Survival")
	void SetStat(FName StatName, float Value);

	UFUNCTION(BlueprintPure, Category="MO|Survival")
	float GetStatCurrent(FName StatName) const;

	UFUNCTION(BlueprintPure, Category="MO|Survival")
	float GetStatPercent(FName StatName) const;

	UFUNCTION(BlueprintPure, Category="MO|Survival")
	bool IsStatDepleted(FName StatName) const;

	UFUNCTION(BlueprintPure, Category="MO|Survival")
	bool IsStatCritical(FName StatName) const;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void TickStats();
	void ProcessStatTick(FMOSurvivalStat& Stat, FName StatName, float DeltaTime);
	void DecayNutrition(float DeltaTime);
	FMOSurvivalStat* GetStatByName(FName StatName);
	const FMOSurvivalStat* GetStatByName(FName StatName) const;

	FTimerHandle TickTimerHandle;
};
