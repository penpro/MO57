#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOMedicalTypes.h"
#include "MOVitalsComponent.generated.h"

class UMOAnatomyComponent;
class UMOMetabolismComponent;
class UMOMentalStateComponent;

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOOnVitalSignChanged, FName, VitalName, float, OldValue, float, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnBloodLossStageChanged, EMOBloodLossStage, OldStage, EMOBloodLossStage, NewStage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnCardiacArrest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnRespiratoryFailure);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnVitalsChanged);

// ============================================================================
// SAVE DATA
// ============================================================================

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOVitalsSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	FMOVitalSigns Vitals;

	UPROPERTY()
	FMOExertionState Exertion;
};

// ============================================================================
// COMPONENT
// ============================================================================

/**
 * Component managing vital signs and physiological state.
 * Tracks blood volume, heart rate, blood pressure, SpO2, temperature, glucose.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOVitalsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOVitalsComponent();

	// ============================================================================
	// REPLICATED STATE
	// ============================================================================

	/** Current vital signs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="MO|Vitals")
	FMOVitalSigns Vitals;

	/** Current exertion and stress state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="MO|Vitals")
	FMOExertionState Exertion;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Blood regeneration rate in mL/day (natural recovery). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Config", meta=(ClampMin="0"))
	float BloodRegenerationRate = 500.0f;

	/** Time scale multiplier (1.0 = real time). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Config", meta=(ClampMin="0.01"))
	float TimeScaleMultiplier = 1.0f;

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Fired when a vital sign changes significantly. */
	UPROPERTY(BlueprintAssignable, Category="MO|Vitals|Events")
	FMOOnVitalSignChanged OnVitalSignChanged;

	/** Fired when blood loss stage changes. */
	UPROPERTY(BlueprintAssignable, Category="MO|Vitals|Events")
	FMOOnBloodLossStageChanged OnBloodLossStageChanged;

	/** Fired when heart stops (cardiac arrest). */
	UPROPERTY(BlueprintAssignable, Category="MO|Vitals|Events")
	FMOOnCardiacArrest OnCardiacArrest;

	/** Fired when breathing stops (respiratory failure). */
	UPROPERTY(BlueprintAssignable, Category="MO|Vitals|Events")
	FMOOnRespiratoryFailure OnRespiratoryFailure;

	/** Fired when any vital sign changes (for UI updates). */
	UPROPERTY(BlueprintAssignable, Category="MO|Vitals|Events")
	FMOOnVitalsChanged OnVitalsChanged;

	// ============================================================================
	// BLOOD API
	// ============================================================================

	/**
	 * Apply blood loss (from wounds).
	 * @param AmountML Amount of blood lost in mL.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Blood")
	void ApplyBloodLoss(float AmountML);

	/**
	 * Apply blood transfusion.
	 * @param AmountML Amount of blood to add in mL.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Blood")
	void ApplyBloodTransfusion(float AmountML);

	/**
	 * Get current blood loss stage.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Blood")
	EMOBloodLossStage GetBloodLossStage() const;

	/**
	 * Get blood volume as percentage of max.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Blood")
	float GetBloodVolumePercent() const;

	// ============================================================================
	// EXERTION API
	// ============================================================================

	/**
	 * Set current exertion level (from movement/actions).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Exertion")
	void SetExertionLevel(float NewExertion);

	/**
	 * Add stress (from pain, fear, combat).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Exertion")
	void AddStress(float Amount);

	/**
	 * Set pain level (usually called by anatomy component).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Exertion")
	void SetPainLevel(float NewPain);

	/**
	 * Add fatigue (long-term exhaustion).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Exertion")
	void AddFatigue(float Amount);

	// ============================================================================
	// TEMPERATURE API
	// ============================================================================

	/**
	 * Apply environmental temperature effect.
	 * @param AmbientTemp Ambient temperature in Celsius.
	 * @param InsulationFactor How well protected from environment (0-1, 1=fully insulated).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Temperature")
	void ApplyEnvironmentalTemperature(float AmbientTemp, float InsulationFactor);

	// ============================================================================
	// GLUCOSE API
	// ============================================================================

	/**
	 * Apply glucose from food digestion.
	 * @param Amount Amount of glucose to add in mg/dL equivalent.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Glucose")
	void ApplyGlucose(float Amount);

	/**
	 * Consume glucose from activity.
	 * @param Amount Amount of glucose consumed.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Glucose")
	void ConsumeGlucose(float Amount);

	// ============================================================================
	// QUERY API
	// ============================================================================

	/**
	 * Get current vital signs (const reference for reading).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Query")
	const FMOVitalSigns& GetVitalSigns() const { return Vitals; }

	/**
	 * Check if in shock (hypovolemic/traumatic).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Query")
	bool IsInShock() const;

	/**
	 * Check if in critical condition.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Query")
	bool IsCritical() const;

	/**
	 * Get formatted blood pressure string.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Query")
	FString GetBloodPressureString() const;

	// ============================================================================
	// PERSISTENCE
	// ============================================================================

	/**
	 * Build save data from current state.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Save")
	void BuildSaveData(FMOVitalsSaveData& OutSaveData) const;

	/**
	 * Apply save data to restore state (authority only).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Save")
	bool ApplySaveDataAuthority(const FMOVitalsSaveData& InSaveData);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// ============================================================================
	// INTERNAL STATE
	// ============================================================================

	/** Timer for periodic vital sign calculations. */
	FTimerHandle TickTimerHandle;

	/** Tick interval in seconds. */
	float TickInterval = 0.5f;

	/** Previous blood loss stage for change detection. */
	EMOBloodLossStage PreviousBloodLossStage = EMOBloodLossStage::None;

	/** Cached reference to anatomy component. */
	UPROPERTY(Transient)
	TObjectPtr<UMOAnatomyComponent> CachedAnatomyComp;

	/** Cached reference to metabolism component. */
	UPROPERTY(Transient)
	TObjectPtr<UMOMetabolismComponent> CachedMetabolismComp;

	/** Cached reference to mental state component. */
	UPROPERTY(Transient)
	TObjectPtr<UMOMentalStateComponent> CachedMentalComp;

	// ============================================================================
	// INTERNAL METHODS
	// ============================================================================

	/** Periodic tick to update vitals. */
	void TickVitals();

	/** Calculate heart rate based on all factors. */
	void CalculateHeartRate();

	/** Calculate blood pressure based on all factors. */
	void CalculateBloodPressure();

	/** Calculate respiratory rate. */
	void CalculateRespiratoryRate();

	/** Calculate oxygen saturation. */
	void CalculateOxygenSaturation();

	/** Regenerate blood over time. */
	void RegenerateBlood(float DeltaTime);

	/** Process exertion recovery. */
	void ProcessExertionRecovery(float DeltaTime);

	/** Check for critical conditions. */
	void CheckCriticalConditions();

	/** Broadcast vital sign change if significant. */
	void CheckAndBroadcastChange(FName VitalName, float OldValue, float NewValue, float Threshold = 1.0f);
};
