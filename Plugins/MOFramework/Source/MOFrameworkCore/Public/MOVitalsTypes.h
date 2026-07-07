/**
 * =============================================================================
 * MOVitalsTypes.h - Medical Vital Signs Data Structures
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Defines the vital signs data structures used by the medical system. These
 * represent realistic physiological measurements that drive the medical
 * simulation.
 *
 * MEDICAL REALISM REFERENCE:
 * All values use real-world medical units and normal ranges:
 * - Blood Volume: 4500-5500 mL (adult normal)
 * - Heart Rate: 60-100 BPM (resting normal)
 * - Blood Pressure: 120/80 mmHg (normal)
 * - SpO2: 95-100% (normal oxygen saturation)
 * - Body Temperature: 36.5-37.5°C (normal)
 * - Blood Glucose: 70-110 mg/dL (fasting normal)
 *
 * VITAL CASCADE:
 * Blood loss → Low BP → High HR → Low SpO2 → Consciousness drop
 * Dehydration → High HR → High Temp → Performance penalties
 * Low glucose → Confusion → Unconsciousness
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-01] BLOOD VOLUME ZERO: If BloodVolume reaches 0, character should die.
 *   Check GetBloodLossPercent() > 0.4 for Class III hemorrhage.
 *
 * [2024-02] TEMPERATURE UNITS: Body temperature is in CELSIUS, not Fahrenheit.
 *   37°C = 98.6°F. Don't mix units in calculations.
 *
 * [2024-02] HEART RATE BASELINE: BaseHeartRate is per-character (fitness level).
 *   HeartRate is the current dynamic value. Don't confuse them.
 *
 * [2024-02] HEMORRHAGE CLASSES:
 *   Class I: <15% blood loss - minimal symptoms
 *   Class II: 15-30% - HR up, anxiety
 *   Class III: 30-40% - HR very high, confusion, cold/clammy
 *   Class IV: >40% - lethal without intervention
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOVitalsComponent.h - Uses these types, manages vital simulation
 * - MOAnatomyComponent.h - Wounds affect blood volume
 * - MOMentalStateComponent.h - Consciousness based on vitals
 * - MOMetabolismComponent.h - Affects blood glucose
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOVitalsTypes.generated.h"

/**
 * Thermal comfort bucket — perceived "how cold/hot the player feels".
 * Derived from FMOVitalSigns::BodyTemperature (not ambient): drops as the
 * core cools, rises as the body overheats. Each bucket maps to a HUD icon.
 *
 * Medical ranges (UMOVitalsComponent::ThermalThresholds defaults to these):
 *   VeryCold     core < 35.0 °C   severe hypothermia
 *   Cold         35.0 – 36.5      mild hypothermia / shivering
 *   Comfortable  36.5 – 37.5      normal
 *   Hot          37.5 – 39.0      mild hyperthermia / sweating
 *   VeryHot      core ≥ 39.0      severe heat stress
 *
 * Five-value, numeric ordering preserved so a HUD widget can use the value
 * directly as an array index.
 */
UENUM(BlueprintType)
enum class EMOThermalComfort : uint8
{
	VeryCold    = 0  UMETA(DisplayName = "Very Cold"),
	Cold        = 1  UMETA(DisplayName = "Cold"),
	Comfortable = 2  UMETA(DisplayName = "Comfortable"),
	Hot         = 3  UMETA(DisplayName = "Hot"),
	VeryHot     = 4  UMETA(DisplayName = "Very Hot"),
};

/**
 * Four-step wetness bucket. WetnessLevel (0.0–1.0) maps to one of these
 * via FMOWetnessThresholds. HUD moodle re-renders on bucket change only,
 * not on every continuous level update.
 */
UENUM(BlueprintType)
enum class EMOWetnessState : uint8
{
	Dry    = 0  UMETA(DisplayName = "Dry"),
	Damp   = 1  UMETA(DisplayName = "Damp"),
	Wet    = 2  UMETA(DisplayName = "Wet"),
	Soaked = 3  UMETA(DisplayName = "Soaked"),
};

/**
 * Threshold curve for mapping continuous WetnessLevel (0–1) → bucket.
 * Designers can tune to control how quickly the moodle escalates without
 * touching the underlying accumulator logic.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORKCORE_API FMOWetnessThresholds
{
	GENERATED_BODY()

	/** WetnessLevel ≥ this and < WetAbove = Damp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Wetness",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float DampAbove = 0.15f;

	/** WetnessLevel ≥ this and < SoakedAbove = Wet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Wetness",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float WetAbove = 0.45f;

	/** WetnessLevel ≥ this = Soaked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Wetness",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float SoakedAbove = 0.80f;
};

/**
 * Threshold curve for mapping body-core temperature → thermal comfort bucket.
 * Tunable per UMOVitalsComponent so designers can experiment without touching
 * code, but the defaults are medically grounded (see EMOThermalComfort).
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORKCORE_API FMOThermalComfortThresholds
{
	GENERATED_BODY()

	/** Core temp below this = VeryCold (severe hypothermia). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Thermal",
		meta=(ClampMin="20.0", ClampMax="40.0"))
	float VeryColdBelow = 35.0f;

	/** Core temp below this (and ≥ VeryColdBelow) = Cold. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Thermal",
		meta=(ClampMin="20.0", ClampMax="40.0"))
	float ColdBelow = 36.5f;

	/** Core temp above this (and < HotAbove) = Hot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Thermal",
		meta=(ClampMin="35.0", ClampMax="45.0"))
	float HotAbove = 37.5f;

	/** Core temp above this = VeryHot (severe heat stress). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Thermal",
		meta=(ClampMin="35.0", ClampMax="45.0"))
	float VeryHotAbove = 39.0f;
};

/**
 * Complete vital signs reading.
 * See file header for normal ranges and medical context.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORKCORE_API FMOVitalSigns
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
struct MOFRAMEWORKCORE_API FMOExertionState
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
