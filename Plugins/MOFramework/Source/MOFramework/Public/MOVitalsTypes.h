#pragma once

#include "CoreMinimal.h"
#include "MOVitalsTypes.generated.h"

/**
 * Complete vital signs reading.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOVitalSigns
{
	GENERATED_BODY()

	// ---- Blood System ----

	/** Current blood volume in mL (adult normal: 4500-5500). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Blood")
	float BloodVolume = 5000.0f;

	/** Maximum blood volume in mL. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Blood")
	float MaxBloodVolume = 5000.0f;

	// ---- Cardiovascular ----

	/** Heart rate in BPM (resting normal: 60-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Cardiovascular")
	float HeartRate = 72.0f;

	/** Individual baseline heart rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Cardiovascular")
	float BaseHeartRate = 72.0f;

	/** Systolic blood pressure in mmHg (normal: ~120). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Cardiovascular")
	float SystolicBP = 120.0f;

	/** Diastolic blood pressure in mmHg (normal: ~80). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Cardiovascular")
	float DiastolicBP = 80.0f;

	// ---- Respiratory ----

	/** Respiratory rate in breaths/min (normal: 12-20). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Respiratory")
	float RespiratoryRate = 16.0f;

	/** Blood oxygen saturation percentage (normal: 95-100%). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Respiratory")
	float SpO2 = 98.0f;

	// ---- Metabolic ----

	/** Core body temperature in Celsius (normal: 36.5-37.5). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Metabolic")
	float BodyTemperature = 37.0f;

	/** Blood glucose in mg/dL (fasting normal: 70-110). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Metabolic")
	float BloodGlucose = 90.0f;

	// ---- Derived Values ----

	/** Get blood loss as percentage (0-1). */
	float GetBloodLossPercent() const
	{
		return MaxBloodVolume > 0.f ? 1.f - FMath::Clamp(BloodVolume / MaxBloodVolume, 0.f, 1.f) : 1.f;
	}

	/** Get Mean Arterial Pressure (MAP). */
	float GetMeanArterialPressure() const
	{
		return DiastolicBP + (SystolicBP - DiastolicBP) / 3.f;
	}

	/** Check for hypotension (low BP). */
	bool IsHypotensive() const { return SystolicBP < 90.0f; }

	/** Check for tachycardia (fast heart rate). */
	bool IsTachycardic() const { return HeartRate > 100.0f; }

	/** Check for bradycardia (slow heart rate). */
	bool IsBradycardic() const { return HeartRate < 60.0f; }

	/** Check for hypoxia (low oxygen). */
	bool IsHypoxic() const { return SpO2 < 90.0f; }

	/** Check for hypoglycemia (low blood sugar). */
	bool IsHypoglycemic() const { return BloodGlucose < 70.0f; }

	/** Check for hyperglycemia (high blood sugar). */
	bool IsHyperglycemic() const { return BloodGlucose > 140.0f; }

	/** Check for hypothermia. */
	bool IsHypothermic() const { return BodyTemperature < 35.0f; }

	/** Check for hyperthermia/fever. */
	bool IsHyperthermic() const { return BodyTemperature > 38.0f; }
};

/**
 * Exertion and stress state.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOExertionState
{
	GENERATED_BODY()

	/** Current exertion level (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Exertion", meta=(ClampMin="0", ClampMax="100"))
	float CurrentExertion = 0.0f;

	/** Stress level from psychological factors (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Exertion", meta=(ClampMin="0", ClampMax="100"))
	float StressLevel = 0.0f;

	/** Aggregate pain level from all wounds (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Exertion", meta=(ClampMin="0", ClampMax="100"))
	float PainLevel = 0.0f;

	/** Long-term fatigue (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Exertion", meta=(ClampMin="0", ClampMax="100"))
	float Fatigue = 0.0f;

	/** Get multiplier for how exertion affects heart rate. */
	float GetExertionMultiplier() const
	{
		return 1.0f + (CurrentExertion / 100.0f) * 1.5f;
	}
};
