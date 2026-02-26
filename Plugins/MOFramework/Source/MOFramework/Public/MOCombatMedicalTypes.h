/**
 * =============================================================================
 * MOCombatMedicalTypes.h - Combat ↔ Medical System Integration Types
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Provides types and utilities for integrating a combat system with the
 * medical/anatomy system. This is a bridge layer - the actual combat system
 * will be implemented separately.
 *
 * =============================================================================
 * INTEGRATION OVERVIEW
 * =============================================================================
 *
 * The medical system provides these entry points for combat:
 *
 * 1. DAMAGE APPLICATION
 *    UMOAnatomyComponent::InflictDamage(Part, Damage, WoundType)
 *      - Applies HP damage to body part
 *      - Creates wound based on wound type
 *      - Triggers bleeding, infection risk based on wound
 *
 * 2. ACTIVITY TRACKING
 *    UMOVitalsComponent::SetActivityLevel(EMOActivityLevel::Combat)
 *      - Sets high exertion level
 *      - Drains stamina at combat rate
 *      - Affects heart rate, blood pressure
 *
 * 3. STRESS/SHOCK
 *    UMOVitalsComponent::AddStress(Amount)
 *      - Combat stress accumulation
 *      - Affects vital signs
 *    UMOMentalStateComponent::AddShock(Amount)
 *      - Traumatic shock from taking damage
 *      - Can cause confusion, unconsciousness
 *
 * =============================================================================
 * RECOMMENDED COMBAT FLOW
 * =============================================================================
 *
 * 1. Combat Start:
 *    VitalsComp->SetActivityLevel(EMOActivityLevel::Combat);
 *    VitalsComp->AddStress(InitialCombatStress);
 *
 * 2. Attack Hits:
 *    FMOCombatHitInfo HitInfo = CalculateHit(...);
 *    FMOWound Wound = MOCombatHelpers::DamageToWound(HitInfo);
 *    AnatomyComp->InflictWound(Wound);
 *    MentalComp->AddShock(HitInfo.GetShockAmount());
 *
 * 3. Stamina Cost:
 *    if (!VitalsComp->CanStartActivity(EMOActivityLevel::Combat))
 *    {
 *        // Too exhausted to attack - force rest or reduce effectiveness
 *    }
 *    VitalsComp->ModifyStamina(-AttackStaminaCost);
 *
 * 4. Combat End:
 *    VitalsComp->SetActivityLevel(EMOActivityLevel::Idle);
 *    // Stress will gradually recover
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] WOUND TYPE MAPPING: GetWoundType() maps damage category to wound type.
 *   Fire defaults to 2nd degree burn. Adjust in HitInfo for severity.
 *
 * [2024-02] SHOCK CALCULATION: GetShockAmount() is empirically tuned. Critical hits
 *   and vital organ hits multiply shock. May need adjustment after playtesting.
 *
 * [2024-02] STAMINA COSTS: FMOCombatStaminaCosts values are placeholder.
 *   Tune based on combat pacing requirements.
 *
 * =============================================================================
 * RELATED FILES: MOMedicalTypes.h, MOAnatomyComponent.h, MOVitalsComponent.h,
 *                MOCombatComponent.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOMedicalTypes.h"
#include "MOCombatMedicalTypes.generated.h"

// Forward declarations
class UMOAnatomyComponent;
class UMOVitalsComponent;
class UMOMentalStateComponent;

/**
 * Damage categories that map to wound types.
 */
UENUM(BlueprintType)
enum class EMODamageCategory : uint8
{
	/** Cutting damage (swords, axes) -> Laceration wounds */
	Slash,

	/** Piercing damage (arrows, spears) -> Puncture wounds */
	Pierce,

	/** Blunt impact (clubs, falls) -> Blunt wounds, fractures */
	Blunt,

	/** Fire damage -> Burn wounds */
	Fire,

	/** Cold damage -> Frostbite */
	Cold,

	/** Crushing damage (traps, boulders) -> Blunt + fractures */
	Crush,

	/** Bleeding damage over time -> Laceration wounds */
	Bleed,

	MAX UMETA(Hidden)
};

/**
 * Information about a combat hit for medical processing.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOCombatHitInfo
{
	GENERATED_BODY()

	/** The body part that was hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Combat")
	EMOBodyPartType TargetBodyPart = EMOBodyPartType::Torso;

	/** Base damage amount. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Combat", meta=(ClampMin="0"))
	float BaseDamage = 10.0f;

	/** Damage category (determines wound type). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Combat")
	EMODamageCategory DamageCategory = EMODamageCategory::Blunt;

	/** Armor penetration (0-1, affects wound severity). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Combat", meta=(ClampMin="0", ClampMax="1"))
	float ArmorPenetration = 1.0f;

	/** Critical hit multiplier (1.0 = normal). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Combat", meta=(ClampMin="1"))
	float CriticalMultiplier = 1.0f;

	/** Whether this hit bypasses armor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Combat")
	bool bIgnoresArmor = false;

	/** Whether this causes extra bleeding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Combat")
	bool bCausesHeavyBleeding = false;

	/** Whether this has high infection risk (dirty weapons, bite attacks). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Combat")
	bool bHighInfectionRisk = false;

	/**
	 * Calculate final damage after all modifiers.
	 */
	float GetFinalDamage() const
	{
		return BaseDamage * ArmorPenetration * CriticalMultiplier;
	}

	/**
	 * Get appropriate wound type based on damage category.
	 */
	EMOWoundType GetWoundType() const
	{
		switch (DamageCategory)
		{
		case EMODamageCategory::Slash:
		case EMODamageCategory::Bleed:
			return EMOWoundType::Laceration;

		case EMODamageCategory::Pierce:
			return EMOWoundType::Puncture;

		case EMODamageCategory::Blunt:
		case EMODamageCategory::Crush:
			return EMOWoundType::Blunt;

		case EMODamageCategory::Fire:
			return EMOWoundType::BurnSecond;  // Default to 2nd degree

		case EMODamageCategory::Cold:
			return EMOWoundType::Frostbite;

		default:
			return EMOWoundType::Blunt;
		}
	}

	/**
	 * Calculate shock amount from this hit.
	 */
	float GetShockAmount() const
	{
		float FinalDamage = GetFinalDamage();

		// Base shock is proportional to damage
		float Shock = FinalDamage * 0.5f;

		// Critical hits cause more shock
		if (CriticalMultiplier > 1.0f)
		{
			Shock *= 1.5f;
		}

		// Vital organ hits cause more shock
		if (TargetBodyPart == EMOBodyPartType::Heart ||
			TargetBodyPart == EMOBodyPartType::Brain ||
			TargetBodyPart == EMOBodyPartType::Torso)
		{
			Shock *= 1.5f;
		}

		return Shock;
	}

	/**
	 * Calculate wound severity (0-100) from damage.
	 */
	float GetWoundSeverity() const
	{
		// Severity is based on final damage, capped at 100
		return FMath::Clamp(GetFinalDamage(), 0.0f, 100.0f);
	}

	/**
	 * Calculate bleed rate based on wound type and severity.
	 */
	float GetBleedRate() const
	{
		float Severity = GetWoundSeverity();
		float BaseBleedRate = 0.0f;

		// Different wound types bleed differently
		switch (GetWoundType())
		{
		case EMOWoundType::Laceration:
			BaseBleedRate = Severity * 0.5f;  // Lacerations bleed heavily
			break;

		case EMOWoundType::Puncture:
			BaseBleedRate = Severity * 0.3f;  // Punctures bleed less externally
			break;

		case EMOWoundType::Blunt:
			BaseBleedRate = Severity * 0.1f;  // Blunt mostly internal
			break;

		default:
			BaseBleedRate = Severity * 0.2f;
			break;
		}

		// Heavy bleeding modifier
		if (bCausesHeavyBleeding)
		{
			BaseBleedRate *= 2.0f;
		}

		// Return mL/second bleed rate
		return BaseBleedRate;
	}

	/**
	 * Calculate infection risk based on wound type and flags.
	 */
	float GetInfectionRisk() const
	{
		float BaseRisk = 0.1f;  // 10% base

		// Punctures have higher infection risk
		if (GetWoundType() == EMOWoundType::Puncture)
		{
			BaseRisk = 0.3f;
		}

		// High infection risk flag (dirty weapons, bites)
		if (bHighInfectionRisk)
		{
			BaseRisk *= 3.0f;
		}

		return FMath::Clamp(BaseRisk, 0.0f, 1.0f);
	}
};

/**
 * Combat stamina costs for various actions.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOCombatStaminaCosts
{
	GENERATED_BODY()

	/** Stamina cost per melee attack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Combat")
	float LightAttack = 5.0f;

	/** Stamina cost for heavy/charged attack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Combat")
	float HeavyAttack = 15.0f;

	/** Stamina cost to dodge/roll. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Combat")
	float Dodge = 10.0f;

	/** Stamina cost to block (per second of blocking). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Combat")
	float BlockPerSecond = 3.0f;

	/** Stamina cost to parry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Combat")
	float Parry = 8.0f;

	/** Minimum stamina required to attack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Combat")
	float MinimumToAttack = 10.0f;

	/** Minimum stamina required to dodge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Combat")
	float MinimumToDodge = 5.0f;
};

/**
 * Helper functions for combat ↔ medical integration.
 * These are static utility functions that don't require instantiation.
 */
UCLASS()
class MOFRAMEWORK_API UMOCombatMedicalHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Convert a combat hit into a wound struct for the anatomy system.
	 * @param HitInfo The hit information from combat.
	 * @return A wound ready to apply via AnatomyComponent->InflictWound()
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat")
	static FMOWound HitInfoToWound(const FMOCombatHitInfo& HitInfo);

	/**
	 * Apply a combat hit to the medical system (anatomy + vitals + mental).
	 * @param HitInfo The hit information.
	 * @param AnatomyComp Target's anatomy component.
	 * @param VitalsComp Target's vitals component (optional).
	 * @param MentalComp Target's mental state component (optional).
	 * @return True if damage was applied.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat")
	static bool ApplyCombatDamage(
		const FMOCombatHitInfo& HitInfo,
		UMOAnatomyComponent* AnatomyComp,
		UMOVitalsComponent* VitalsComp = nullptr,
		UMOMentalStateComponent* MentalComp = nullptr);

	/**
	 * Check if a character can perform a combat action (has enough stamina).
	 * @param VitalsComp The vitals component to check.
	 * @param RequiredStamina Stamina required for the action.
	 * @return True if the action can be performed.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Combat")
	static bool CanPerformCombatAction(UMOVitalsComponent* VitalsComp, float RequiredStamina);

	/**
	 * Calculate body part hit based on hit location (requires skeletal mesh data).
	 * This is a simplified version - production would use collision component names
	 * or bone lookups.
	 * @param HitBoneName Name of the bone that was hit.
	 * @return The corresponding body part type.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat")
	static EMOBodyPartType BoneNameToBodyPart(FName HitBoneName);

	/**
	 * Get recommended target body part based on attack angle (simplified targeting).
	 * @param AttackerLocation Location of attacker.
	 * @param DefenderLocation Location of defender.
	 * @param AttackHeight Relative height of attack (0=low, 0.5=mid, 1=high).
	 * @return Best-guess body part for this attack angle.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Combat")
	static EMOBodyPartType GetTargetBodyPartFromAngle(
		const FVector& AttackerLocation,
		const FVector& DefenderLocation,
		float AttackHeight);

	/**
	 * Get damage multiplier for hitting a specific body part.
	 * Critical areas (head, heart) have higher multipliers.
	 * @param BodyPart The body part hit.
	 * @return Damage multiplier to apply.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Combat")
	static float GetBodyPartDamageMultiplier(EMOBodyPartType BodyPart);

	/**
	 * Check if a body part hit would be an instant kill at any damage.
	 * @param BodyPart The body part to check.
	 * @return True if destroying this part causes instant death.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Combat")
	static bool IsInstantDeathPart(EMOBodyPartType BodyPart);
};
