#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MOMedicalTypes.h"
#include "MOBodyPartDefinitionRow.generated.h"

/**
 * DataTable row defining properties of a body part.
 * Used to configure HP, damage effects, and anatomical relationships.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOBodyPartDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	// ---- Core Identification ----

	/** Which body part this row defines. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|BodyPart|Core")
	EMOBodyPartType PartType = EMOBodyPartType::None;

	/** Display name for UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|BodyPart|Core")
	FText DisplayName;

	/** Parent body part in the hierarchy (e.g., Hand's parent is Wrist). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|BodyPart|Core")
	EMOBodyPartType ParentPart = EMOBodyPartType::None;

	// ---- Stats ----

	/** Base HP for this body part. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|BodyPart|Stats", meta=(ClampMin="1"))
	float BaseHP = 100.0f;

	/** Multiplier for bleed rate when wounded. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|BodyPart|Stats", meta=(ClampMin="0"))
	float BleedMultiplier = 1.0f;

	/** Multiplier for infection risk when wounded. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|BodyPart|Stats", meta=(ClampMin="0"))
	float InfectionMultiplier = 1.0f;

	// ---- Death/Criticality ----

	/** If true, destroying this body part causes instant death (Brain, Heart). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|BodyPart|Criticality")
	bool bInstantDeathOnDestruction = false;

	/** Seconds until death when destroyed (0 = no death timer). Lungs = ~180s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|BodyPart|Criticality", meta=(ClampMin="0"))
	float DeathTimerOnDestruction = 0.0f;

	/** If true, damage causes internal bleeding (Liver, Gut). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|BodyPart|Criticality")
	bool bCausesInternalBleeding = false;

	// ---- Function Loss ----

	/** If true, destruction disables movement (Legs, Spine). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|BodyPart|Function")
	bool bDisablesMovement = false;

	/** If true, destruction disables grip (Hands, Fingers). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|BodyPart|Function")
	bool bDisablesGrip = false;

	/** If true, destruction disables vision (Eyes). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|BodyPart|Function")
	bool bDisablesVision = false;

	/** If true, destruction disables hearing (Ears). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|BodyPart|Function")
	bool bDisablesHearing = false;

	/** If true, destruction disables eating/speaking (Jaw). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|BodyPart|Function")
	bool bDisablesEating = false;

	// ---- Anatomy ----

	/** If true, this body part has bones that can fracture. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|BodyPart|Anatomy")
	bool bHasBone = true;

	/** If true, this is a joint that can dislocate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|BodyPart|Anatomy")
	bool bIsJoint = false;

	/** If true, this is an internal organ. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|BodyPart|Anatomy")
	bool bIsOrgan = false;

	/** If true, this body part can be amputated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|BodyPart|Anatomy")
	bool bCanBeAmputated = false;

	// ---- UI ----

	/** Icon for this body part. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|BodyPart|UI")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Position on a body diagram (normalized 0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|BodyPart|UI")
	FVector2D UIPosition = FVector2D(0.5f, 0.5f);

	/** UI layer for depth ordering on body diagram. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|BodyPart|UI")
	int32 UILayer = 0;
};

/**
 * DataTable row defining properties of a wound type.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWoundTypeDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Which wound type this row defines. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Wound|Core")
	EMOWoundType WoundType = EMOWoundType::None;

	/** Display name for UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Wound|Core")
	FText DisplayName;

	/** Description of the wound type. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Wound|Core", meta=(MultiLine=true))
	FText Description;

	// ---- Damage Characteristics ----

	/** Base damage multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Wound|Damage", meta=(ClampMin="0"))
	float BaseDamageMultiplier = 1.0f;

	/** Base bleed rate in mL/second per point of severity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Wound|Damage", meta=(ClampMin="0"))
	float BaseBleedRate = 1.0f;

	/** Base infection risk probability (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Wound|Damage", meta=(ClampMin="0", ClampMax="1"))
	float BaseInfectionRisk = 0.1f;

	// ---- Healing ----

	/** Base time to fully heal in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Wound|Healing", meta=(ClampMin="1"))
	float BaseHealingTime = 3600.0f;

	/** If true, requires suturing to heal properly. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Wound|Healing")
	bool bRequiresSuturing = false;

	/** If true, requires a splint (fractures). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Wound|Healing")
	bool bRequiresSplint = false;

	/** If true, requires reduction (dislocations). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Wound|Healing")
	bool bRequiresReduction = false;

	/** If true, wound can be treated with bandages. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Wound|Healing")
	bool bCanBeBandaged = true;

	// ---- Effects ----

	/** Pain multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Wound|Effects", meta=(ClampMin="0"))
	float PainMultiplier = 1.0f;

	/** Shock contribution when wound is inflicted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Wound|Effects", meta=(ClampMin="0"))
	float ShockContribution = 10.0f;

	/** How much this wound impairs the body part's function (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Wound|Effects", meta=(ClampMin="0", ClampMax="1"))
	float FunctionImpairment = 0.5f;

	// ---- UI ----

	/** Color tint for wound indicator. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Wound|UI")
	FLinearColor UIColor = FLinearColor::Red;

	/** Icon for this wound type. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Wound|UI")
	TSoftObjectPtr<UTexture2D> Icon;
};

/**
 * DataTable row defining properties of a medical condition.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOConditionDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Which condition this row defines. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Condition|Core")
	EMOConditionType ConditionType = EMOConditionType::None;

	/** Display name for UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Condition|Core")
	FText DisplayName;

	/** Description of the condition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Condition|Core", meta=(MultiLine=true))
	FText Description;

	// ---- Progression ----

	/** Severity increase per second if untreated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Condition|Progression", meta=(ClampMin="0"))
	float ProgressionRate = 1.0f;

	/** Severity at which death occurs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Condition|Progression", meta=(ClampMin="1"))
	float LethalSeverity = 100.0f;

	/** Condition this progresses to (e.g., Infection -> Sepsis). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Condition|Progression")
	EMOConditionType ProgressesTo = EMOConditionType::None;

	/** Severity threshold to trigger progression. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Condition|Progression", meta=(ClampMin="0", ClampMax="100"))
	float ProgressionThreshold = 80.0f;

	// ---- Vital Sign Effects (per point of severity) ----

	/** Heart rate modification per severity point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Condition|VitalEffects")
	float HeartRateModPerSeverity = 0.0f;

	/** Blood pressure modification per severity point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Condition|VitalEffects")
	float BPModPerSeverity = 0.0f;

	/** Temperature modification per severity point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Condition|VitalEffects")
	float TempModPerSeverity = 0.0f;

	/** SpO2 modification per severity point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Condition|VitalEffects")
	float SpO2ModPerSeverity = 0.0f;

	/** Respiratory rate modification per severity point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Condition|VitalEffects")
	float RespRateModPerSeverity = 0.0f;

	// ---- Mental Effects ----

	/** Severity at which confusion starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Condition|MentalEffects", meta=(ClampMin="0", ClampMax="100"))
	float ConfusionThreshold = 50.0f;

	/** Severity at which unconsciousness occurs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Condition|MentalEffects", meta=(ClampMin="0", ClampMax="100"))
	float UnconsciousThreshold = 80.0f;

	// ---- Treatment ----

	/** If true, condition can be treated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Condition|Treatment")
	bool bIsTreatable = true;

	/** Recovery rate per second when treated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Condition|Treatment", meta=(ClampMin="0"))
	float TreatedRecoveryRate = 1.0f;

	/** Natural recovery rate per second (0 = no natural recovery). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Condition|Treatment", meta=(ClampMin="0"))
	float NaturalRecoveryRate = 0.0f;

	// ---- UI ----

	/** Color for condition indicator. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Condition|UI")
	FLinearColor UIColor = FLinearColor::Yellow;

	/** Icon for this condition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Condition|UI")
	TSoftObjectPtr<UTexture2D> Icon;
};

/**
 * DataTable row defining a medical treatment.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOMedicalTreatmentRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Unique identifier for this treatment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Treatment|Core")
	FName TreatmentId;

	/** Display name for UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Treatment|Core")
	FText DisplayName;

	/** Description of the treatment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Treatment|Core", meta=(MultiLine=true))
	FText Description;

	// ---- What It Treats ----

	/** Wound types this treatment can address. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Treatment|Treats")
	TArray<EMOWoundType> TreatsWoundTypes;

	/** Condition types this treatment can address. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Treatment|Treats")
	TArray<EMOConditionType> TreatsConditions;

	// ---- Requirements ----

	/** Item IDs required to perform this treatment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Treatment|Requirements")
	TArray<FName> RequiredItemIds;

	/** Skill ID required (e.g., "medicine"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Treatment|Requirements")
	FName RequiredSkillId;

	/** Minimum skill level to attempt. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Treatment|Requirements", meta=(ClampMin="0"))
	int32 MinimumSkillLevel = 1;

	/** Time to perform the treatment in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Treatment|Requirements", meta=(ClampMin="0"))
	float TreatmentDuration = 10.0f;

	// ---- Effects ----

	/** Bleed rate multiplier after treatment (0.5 = halve bleed rate). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Treatment|Effects", meta=(ClampMin="0", ClampMax="1"))
	float BleedReduction = 0.5f;

	/** Infection risk reduction (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Treatment|Effects", meta=(ClampMin="0", ClampMax="1"))
	float InfectionReduction = 0.3f;

	/** Healing speed multiplier bonus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Treatment|Effects", meta=(ClampMin="0"))
	float HealingSpeedBonus = 0.5f;

	/** Immediate pain reduction (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Treatment|Effects", meta=(ClampMin="0", ClampMax="1"))
	float PainReduction = 0.2f;

	// ---- Skill Integration ----

	/** XP granted for performing this treatment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Treatment|Skill", meta=(ClampMin="0"))
	float SkillXPGrant = 10.0f;

	/** How much skill level improves treatment quality (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Treatment|Skill", meta=(ClampMin="0", ClampMax="1"))
	float QualitySkillScaling = 0.5f;

	// ---- Self-Treatment Penalties ----

	/** Effectiveness reduction when treating self (0-1, 0 = full effectiveness). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Treatment|SelfTreatment", meta=(ClampMin="0", ClampMax="1"))
	float SelfTreatmentPenalty = 0.3f;

	/** Body parts that cannot be self-treated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Treatment|SelfTreatment")
	TArray<EMOBodyPartType> UnreachableForSelf;

	// ---- UI ----

	/** Icon for this treatment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Treatment|UI")
	TSoftObjectPtr<UTexture2D> Icon;
};
