#pragma once

#include "CoreMinimal.h"
#include "MOBodyPartTypes.generated.h"

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
