#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "MOMedicalTypes.generated.h"

// Forward declarations
class UMOAnatomyComponent;
class UMOMetabolismComponent;

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * Activity level for physical exertion tracking.
 * Used to drive calorie expenditure, stamina drain, and fatigue accumulation.
 *
 * =============================================================================
 * INTEGRATION GUIDE
 * =============================================================================
 *
 * The activity system connects movement, crafting, building, and combat to the
 * medical/metabolism systems. External systems call SetActivityLevel() to report
 * what the player is doing, and the vitals component handles the downstream effects.
 *
 * INTEGRATION POINTS:
 *
 * 1. MOVEMENT SYSTEM (Required Integration)
 *    - In your Character or CharacterMovementComponent:
 *    - Call VitalsComp->SetActivityLevel() when movement mode changes
 *    - Example integration in Character::Tick():
 *      if (VitalsComp)
 *      {
 *          float Speed = GetVelocity().Size();
 *          if (Speed > SprintThreshold)
 *              VitalsComp->SetActivityLevel(EMOActivityLevel::Sprinting);
 *          else if (Speed > JogThreshold)
 *              VitalsComp->SetActivityLevel(EMOActivityLevel::Jogging);
 *          else if (Speed > WalkThreshold)
 *              VitalsComp->SetActivityLevel(EMOActivityLevel::Walking);
 *          else
 *              VitalsComp->SetActivityLevel(EMOActivityLevel::Idle);
 *      }
 *
 * 2. CRAFTING SYSTEM (Optional Integration)
 *    - When player is at crafting station, set appropriate work level
 *    - Map recipe ActionId to activity level:
 *      - "Weave", "Carve" -> LightWork
 *      - "Hammer", "Saw", "Cook" -> MediumWork
 *      - "Forge", "Smelt" -> HeavyWork
 *    - When leaving station or craft completes, restore previous activity
 *
 * 3. BUILDING SYSTEM (Optional Integration)
 *    - When player is actively building (interacting with ghost):
 *      - Map build action to activity: "Dig" -> HeavyWork, "Assemble" -> MediumWork
 *    - When stepping away or build completes, restore Idle
 *
 * 4. COMBAT SYSTEM (Required Integration)
 *    - On entering combat: VitalsComp->SetActivityLevel(EMOActivityLevel::Combat)
 *    - On exiting combat: Restore to movement-based activity
 *    - Combat activity has high stamina drain for balance
 *
 * 5. UI INTEGRATION
 *    - Bind to OnStaminaChanged for stamina bar updates
 *    - Bind to OnStaminaDepleted to show exhaustion indicator
 *    - Use GetStaminaPercent() for bar fill amount
 *    - Use CanStartActivity() to gray out sprint button when stamina low
 *
 * STAMINA FLOW:
 *    Walking/Idle -> Stamina recovers
 *    Jogging -> Moderate drain
 *    Sprinting/Combat -> Fast drain
 *    Depleted -> Forced downgrade to lower activity
 *
 * CALORIE FLOW:
 *    Activity -> Calorie burn (via MOMetabolismComponent)
 *    Calorie burn -> Uses glycogen/fat stores
 *    Low stores -> Reduced stamina recovery
 *
 * =============================================================================
 */
UENUM(BlueprintType)
enum class EMOActivityLevel : uint8
{
	/** Sleeping, lying down. Minimal energy expenditure. */
	Resting,

	/** Standing still or sitting. BMR only. */
	Idle,

	/** Walking at comfortable pace. ~3-4 km/h. */
	Walking,

	/** Jogging at moderate pace. ~8-10 km/h. */
	Jogging,

	/** Full sprint. ~15-20+ km/h. High stamina drain. */
	Sprinting,

	/** Light crafting: weaving, carving, simple assembly. */
	LightWork,

	/** Medium crafting: hammering, sawing, cooking. */
	MediumWork,

	/** Heavy crafting: forging, chopping, mining. */
	HeavyWork,

	/** Active combat: fighting, dodging. Maximum exertion. */
	Combat,

	MAX UMETA(Hidden)
};

/**
 * Configuration for a specific activity level.
 * Defines energy costs, stamina drain rates, and other activity-specific parameters.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOActivityConfig
{
	GENERATED_BODY()

	/** Multiplier applied to BMR for calorie expenditure (1.0 = BMR only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity")
	float CalorieMultiplier = 1.0f;

	/** Exertion level this activity generates (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity", meta=(ClampMin="0", ClampMax="100"))
	float ExertionLevel = 0.0f;

	/** Stamina drain per second (0-100 scale). 0 = no drain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity", meta=(ClampMin="0"))
	float StaminaDrainPerSecond = 0.0f;

	/** Fatigue accumulation per hour of sustained activity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity", meta=(ClampMin="0"))
	float FatiguePerHour = 0.0f;

	/** Whether this activity counts as cardio training. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity")
	bool bIsCardioTraining = false;

	/** Whether this activity counts as strength training. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity")
	bool bIsStrengthTraining = false;

	/** Training intensity if this is a training activity (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity", meta=(ClampMin="0", ClampMax="1"))
	float TrainingIntensity = 0.0f;

	/** Whether stamina is required to maintain this activity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity")
	bool bRequiresStamina = false;

	/** Minimum stamina required to start this activity (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity", meta=(ClampMin="0", ClampMax="1"))
	float MinimumStaminaToStart = 0.0f;

	/** Get default config for an activity level. */
	static FMOActivityConfig GetDefaultConfig(EMOActivityLevel Level);
};

/**
 * Current activity state tracking.
 * Replicated for UI and network sync.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOActivityState
{
	GENERATED_BODY()

	/** Current activity level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity")
	EMOActivityLevel CurrentActivity = EMOActivityLevel::Idle;

	/** Time spent in current activity (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity")
	float TimeInCurrentActivity = 0.0f;

	/** Total time active today (seconds, excludes Resting/Idle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity")
	float TotalActiveTimeToday = 0.0f;

	/** Current stamina (0-100). Drains with intense activity, recovers at rest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity", meta=(ClampMin="0", ClampMax="100"))
	float CurrentStamina = 100.0f;

	/** Maximum stamina capacity (affected by fitness). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity")
	float MaxStamina = 100.0f;

	/** Get stamina as percentage (0-1). */
	float GetStaminaPercent() const { return MaxStamina > 0.0f ? CurrentStamina / MaxStamina : 0.0f; }

	/** Check if stamina is depleted. */
	bool IsStaminaDepleted() const { return CurrentStamina <= 1.0f; }

	/** Check if in an active state (not resting or idle). */
	bool IsActive() const { return CurrentActivity > EMOActivityLevel::Idle; }
};

/**
 * Hierarchical body part identification.
 * ~55 distinct body parts including individual fingers and toes.
 */
UENUM(BlueprintType)
enum class EMOBodyPartType : uint8
{
	None = 0,

	// Head Region
	Head,
	Brain,				// VITAL - instant death
	EyeLeft,
	EyeRight,
	EarLeft,
	EarRight,
	Jaw,

	// Torso / Organs
	Torso,
	Heart,				// VITAL - instant death
	LungLeft,			// VITAL - death in ~3 minutes
	LungRight,			// VITAL - death in ~3 minutes
	Liver,
	Stomach,
	Intestines,			// "Gut" - death in hours (sepsis)
	KidneyLeft,
	KidneyRight,

	// Spine
	SpineCervical,		// Neck
	SpineThoracic,		// Upper back
	SpineLumbar,		// Lower back

	// Left Arm
	ShoulderLeft,
	UpperArmLeft,
	ElbowLeft,
	ForearmLeft,
	WristLeft,
	HandLeft,
	ThumbLeft,
	IndexFingerLeft,
	MiddleFingerLeft,
	RingFingerLeft,
	PinkyFingerLeft,

	// Right Arm
	ShoulderRight,
	UpperArmRight,
	ElbowRight,
	ForearmRight,
	WristRight,
	HandRight,
	ThumbRight,
	IndexFingerRight,
	MiddleFingerRight,
	RingFingerRight,
	PinkyFingerRight,

	// Left Leg
	HipLeft,
	ThighLeft,
	KneeLeft,
	CalfLeft,
	AnkleLeft,
	FootLeft,
	BigToeLeft,
	SecondToeLeft,
	ThirdToeLeft,
	FourthToeLeft,
	PinkyToeLeft,

	// Right Leg
	HipRight,
	ThighRight,
	KneeRight,
	CalfRight,
	AnkleRight,
	FootRight,
	BigToeRight,
	SecondToeRight,
	ThirdToeRight,
	FourthToeRight,
	PinkyToeRight,

	MAX UMETA(Hidden)
};

/**
 * Status of a body part.
 */
UENUM(BlueprintType)
enum class EMOBodyPartStatus : uint8
{
	Healthy,		// HP at or near max
	Injured,		// HP < Max but > 0
	Destroyed,		// HP = 0, still attached
	Missing			// Amputated/severed
};

/**
 * Types of wounds that can be inflicted.
 */
UENUM(BlueprintType)
enum class EMOWoundType : uint8
{
	None = 0,
	Laceration,			// Cutting damage - bleeds heavily
	Puncture,			// Piercing - deep, high infection risk
	Blunt,				// Crushing - fractures, internal bleeding
	BurnFirst,			// Superficial burn (1st degree)
	BurnSecond,			// Partial thickness (2nd degree)
	BurnThird,			// Full thickness (3rd degree)
	Frostbite,			// Cold damage
	Fracture,			// Bone break
	Dislocation,		// Joint out of place
	InternalBleeding	// Hidden internal damage
};

/**
 * Types of medical conditions and diseases.
 */
UENUM(BlueprintType)
enum class EMOConditionType : uint8
{
	None = 0,
	Infection,			// Local infection - can progress to sepsis
	Sepsis,				// Systemic infection - critical
	BloodClot,			// DVT risk
	Concussion,			// Brain trauma
	Shock,				// Hypovolemic/traumatic shock
	FoodPoisoning,
	WaterborneDisease,
	Parasites,
	Hypothermia,
	Hyperthermia,
	Dehydration,
	Starvation
};

/**
 * Level of consciousness.
 */
UENUM(BlueprintType)
enum class EMOConsciousnessLevel : uint8
{
	Alert,			// Normal, full control
	Confused,		// Impaired decision making
	Drowsy,			// Slow reactions, difficulty focusing
	Unconscious,	// No control, vulnerable
	Comatose		// Deep unconsciousness, minimal responses
};

/**
 * Blood loss classification (hemorrhage stages).
 */
UENUM(BlueprintType)
enum class EMOBloodLossStage : uint8
{
	None,		// <15% loss - normal vitals
	Class1,		// 15-30% - compensated, HR up, pale, anxious
	Class2,		// 30-40% - decompensated, HR up significantly, confused, BP dropping
	Class3		// >40% - critical, unconscious, death imminent
};


// ============================================================================
// WOUND STRUCTURES
// ============================================================================

/**
 * Represents an active wound on a body part.
 * Uses FFastArraySerializerItem for efficient replication.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWound : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** Unique identifier for this wound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound", meta=(IgnoreForMemberInitializationTest))
	FGuid WoundId;

	/** Which body part this wound is on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound")
	EMOBodyPartType BodyPart = EMOBodyPartType::None;

	/** Type of wound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound")
	EMOWoundType WoundType = EMOWoundType::None;

	/** Severity of the wound (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound", meta=(ClampMin="0", ClampMax="100"))
	float Severity = 0.0f;

	/** Blood loss rate in mL/second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound", meta=(ClampMin="0"))
	float BleedRate = 0.0f;

	/** Probability of infection per tick (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound", meta=(ClampMin="0", ClampMax="1"))
	float InfectionRisk = 0.0f;

	/** Healing progress (0-100%). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound", meta=(ClampMin="0", ClampMax="100"))
	float HealingProgress = 0.0f;

	/** Whether the wound has been bandaged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound")
	bool bIsBandaged = false;

	/** Whether the wound has been sutured (for deep wounds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound")
	bool bIsSutured = false;

	/** Whether the wound is infected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound")
	bool bIsInfected = false;

	/** Severity of the infection if infected (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound", meta=(ClampMin="0", ClampMax="100"))
	float InfectionSeverity = 0.0f;

	/** Time since the wound was inflicted (game seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound")
	float TimeSinceInflicted = 0.0f;

	FMOWound()
	{
		WoundId = FGuid::NewGuid();
	}
};

/**
 * FastArray container for wounds.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWoundList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMOWound> Wounds;

	UPROPERTY(NotReplicated, Transient)
	TObjectPtr<UMOAnatomyComponent> OwnerComponent;

	void SetOwner(UMOAnatomyComponent* InOwner) { OwnerComponent = InOwner; }

	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FMOWound, FMOWoundList>(Wounds, DeltaParams, *this);
	}

	// Helper methods
	FMOWound* FindWoundById(const FGuid& WoundId);
	const FMOWound* FindWoundById(const FGuid& WoundId) const;
	void AddWound(const FMOWound& NewWound);
	bool RemoveWound(const FGuid& WoundId);
};

template<>
struct TStructOpsTypeTraits<FMOWoundList> : public TStructOpsTypeTraitsBase2<FMOWoundList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};


// ============================================================================
// CONDITION STRUCTURES
// ============================================================================

/**
 * Represents an active medical condition or disease.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOCondition : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** Unique identifier for this condition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Condition", meta=(IgnoreForMemberInitializationTest))
	FGuid ConditionId;

	/** Type of condition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Condition")
	EMOConditionType ConditionType = EMOConditionType::None;

	/** Affected body part (None = systemic). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Condition")
	EMOBodyPartType AffectedPart = EMOBodyPartType::None;

	/** Severity of the condition (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Condition", meta=(ClampMin="0", ClampMax="100"))
	float Severity = 0.0f;

	/** Time this condition has been active (game seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Condition")
	float Duration = 0.0f;

	/** Whether treatment has been applied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Condition")
	bool bIsTreated = false;

	FMOCondition()
	{
		ConditionId = FGuid::NewGuid();
	}
};

/**
 * FastArray container for conditions.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOConditionList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMOCondition> Conditions;

	UPROPERTY(NotReplicated, Transient)
	TObjectPtr<UMOAnatomyComponent> OwnerComponent;

	void SetOwner(UMOAnatomyComponent* InOwner) { OwnerComponent = InOwner; }

	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FMOCondition, FMOConditionList>(Conditions, DeltaParams, *this);
	}

	// Helper methods
	FMOCondition* FindConditionById(const FGuid& ConditionId);
	const FMOCondition* FindConditionById(const FGuid& ConditionId) const;
	FMOCondition* FindConditionByType(EMOConditionType Type);
	void AddCondition(const FMOCondition& NewCondition);
	bool RemoveCondition(const FGuid& ConditionId);
};

template<>
struct TStructOpsTypeTraits<FMOConditionList> : public TStructOpsTypeTraitsBase2<FMOConditionList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};


// ============================================================================
// BODY PART STRUCTURES
// ============================================================================

/**
 * State of a single body part.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOBodyPartState
{
	GENERATED_BODY()

	/** Which body part this represents. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|BodyPart")
	EMOBodyPartType PartType = EMOBodyPartType::None;

	/** Current status of the body part. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|BodyPart")
	EMOBodyPartStatus Status = EMOBodyPartStatus::Healthy;

	/** Current HP. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|BodyPart", meta=(ClampMin="0"))
	float CurrentHP = 100.0f;

	/** Maximum HP. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|BodyPart", meta=(ClampMin="1"))
	float MaxHP = 100.0f;

	/** Bone density multiplier (affects fracture resistance). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|BodyPart", meta=(ClampMin="0.1", ClampMax="2.0"))
	float BoneDensity = 1.0f;

	/** Get HP as percentage (0-1). */
	float GetHPPercent() const { return MaxHP > 0.f ? FMath::Clamp(CurrentHP / MaxHP, 0.f, 1.f) : 0.f; }

	/** Check if the body part is destroyed or missing. */
	bool IsDestroyed() const { return Status == EMOBodyPartStatus::Destroyed || Status == EMOBodyPartStatus::Missing; }

	/** Check if the body part is functional. */
	bool IsFunctional() const { return Status == EMOBodyPartStatus::Healthy || Status == EMOBodyPartStatus::Injured; }
};


// ============================================================================
// VITAL SIGNS STRUCTURES
// ============================================================================

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


// ============================================================================
// METABOLISM STRUCTURES
// ============================================================================

/**
 * Body composition metrics.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOBodyComposition
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
struct MOFRAMEWORK_API FMONutrientLevels
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
struct MOFRAMEWORK_API FMODigestingFood : public FFastArraySerializerItem
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
struct MOFRAMEWORK_API FMODigestingFoodList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMODigestingFood> Items;

	UPROPERTY(NotReplicated, Transient)
	TObjectPtr<UMOMetabolismComponent> OwnerComponent;

	void SetOwner(UMOMetabolismComponent* InOwner) { OwnerComponent = InOwner; }

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


// ============================================================================
// MENTAL STATE STRUCTURES
// ============================================================================

/**
 * Mental and cognitive state.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOMentalState
{
	GENERATED_BODY()

	/** Current consciousness level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Consciousness")
	EMOConsciousnessLevel Consciousness = EMOConsciousnessLevel::Alert;

	/** Shock accumulation from trauma (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Shock", meta=(ClampMin="0", ClampMax="100"))
	float ShockAccumulation = 0.0f;

	/** Traumatic stress from witnessing events (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Stress", meta=(ClampMin="0", ClampMax="100"))
	float TraumaticStress = 0.0f;

	/** Long-term morale fatigue (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Stress", meta=(ClampMin="0", ClampMax="100"))
	float MoraleFatigue = 0.0f;

	// ---- Visual/Motor Effects (0-1 intensity) ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Effects", meta=(ClampMin="0", ClampMax="1"))
	float AimShakeIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Effects", meta=(ClampMin="0", ClampMax="1"))
	float TunnelVisionIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Effects", meta=(ClampMin="0", ClampMax="1"))
	float BlurredVisionIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Effects", meta=(ClampMin="0", ClampMax="1"))
	float StumblingChance = 0.0f;

	// ---- Control Checks ----

	/** Check if character has motor control. */
	bool HasMotorControl() const
	{
		return Consciousness < EMOConsciousnessLevel::Unconscious;
	}

	/** Check if character can make decisions. */
	bool CanMakeDecisions() const
	{
		return Consciousness <= EMOConsciousnessLevel::Confused;
	}

	/** Check if character can perform complex actions. */
	bool CanPerformComplexActions() const
	{
		return Consciousness == EMOConsciousnessLevel::Alert;
	}
};
