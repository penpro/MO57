#include "MOCombatMedicalTypes.h"
#include "MOAnatomyComponent.h"
#include "MOVitalsComponent.h"
#include "MOMentalStateComponent.h"
#include "MOAdrenalineComponent.h"
#include "MOSkeletonMappingTypes.h"

// ============================================================================
// Combat Medical Constants
// ============================================================================
namespace MOCombatConstants
{
	// Damage and stress modifiers
	constexpr float PainToSeverityRatio = 0.8f;
	constexpr float DamageToStressRatio = 0.3f;

	// Attack height thresholds (0.0 = feet, 1.0 = head)
	constexpr float LowAttackThreshold = 0.25f;
	constexpr float MidAttackThreshold = 0.6f;
	constexpr float HighAttackThreshold = 0.85f;

	// Body part damage multipliers
	namespace DamageMultiplier
	{
		// Instant death parts
		constexpr float Brain = 5.0f;
		constexpr float Heart = 4.0f;

		// Vital organs
		constexpr float Lung = 2.5f;
		constexpr float Liver = 2.0f;
		constexpr float Intestines = 1.8f;
		constexpr float Kidney = 1.8f;

		// Head and spine
		constexpr float Head = 2.0f;
		constexpr float SpineCervical = 2.5f;
		constexpr float SpineThoracolumbar = 1.8f;

		// Torso
		constexpr float Torso = 1.2f;

		// Limbs - proximal
		constexpr float ShoulderHip = 1.0f;
		constexpr float UpperLimb = 0.9f;
		constexpr float LowerLimb = 0.8f;

		// Extremities
		constexpr float HandFoot = 0.6f;
		constexpr float Eyes = 1.5f;
		constexpr float Ears = 0.7f;
		constexpr float Default = 0.4f;
	}
}

// ============================================================================
// UMOCombatMedicalHelpers Implementation
// ============================================================================

FMOWound UMOCombatMedicalHelpers::HitInfoToWound(const FMOCombatHitInfo& HitInfo)
{
	FMOWound Wound;
	Wound.WoundId = FGuid::NewGuid();
	Wound.BodyPart = HitInfo.TargetBodyPart;
	Wound.WoundType = HitInfo.GetWoundType();
	Wound.Severity = HitInfo.GetWoundSeverity();
	Wound.BleedRate = HitInfo.GetBleedRate();
	Wound.InfectionRisk = HitInfo.GetInfectionRisk();
	Wound.HealingProgress = 0.0f;
	Wound.bIsBandaged = false;
	Wound.bIsSutured = false;
	Wound.bIsInfected = false;
	Wound.InfectionSeverity = 0.0f;
	Wound.TimeSinceInflicted = 0.0f;

	return Wound;
}

bool UMOCombatMedicalHelpers::ApplyCombatDamage(
	const FMOCombatHitInfo& HitInfo,
	UMOAnatomyComponent* AnatomyComp,
	UMOVitalsComponent* VitalsComp,
	UMOMentalStateComponent* MentalComp)
{
	if (!AnatomyComp)
	{
		return false;
	}

	// Create wound from hit info
	FMOWound Wound = HitInfoToWound(HitInfo);

	// Check for adrenaline component to apply bleed reduction
	AActor* Owner = AnatomyComp->GetOwner();
	UMOAdrenalineComponent* AdrenalineComp = Owner ? Owner->FindComponentByClass<UMOAdrenalineComponent>() : nullptr;

	// Apply adrenaline-based bleed reduction
	float EffectiveBleedRate = Wound.BleedRate;
	if (AdrenalineComp && AdrenalineComp->IsAdrenalineActive())
	{
		EffectiveBleedRate = AdrenalineComp->CalculateEffectiveBleedRate(Wound.BleedRate);
	}
	Wound.BleedRate = EffectiveBleedRate;

	// Apply the wound
	bool bApplied = AnatomyComp->InflictWound(Wound);

	if (!bApplied)
	{
		return false;
	}

	// Notify adrenaline system of wound (triggers spike, pain masking)
	if (AdrenalineComp)
	{
		float PainAmount = HitInfo.GetWoundSeverity() * MOCombatConstants::PainToSeverityRatio;
		AdrenalineComp->OnWoundReceived(HitInfo.GetWoundSeverity(), PainAmount, HitInfo.GetBleedRate());
	}

	// Apply shock to mental state (reduced by adrenaline pain masking)
	if (MentalComp)
	{
		float ShockAmount = HitInfo.GetShockAmount();

		// Adrenaline reduces perceived shock
		if (AdrenalineComp && AdrenalineComp->IsAdrenalineActive())
		{
			ShockAmount *= (1.0f - AdrenalineComp->GetPainMaskingPercent());
		}

		MentalComp->AddShock(ShockAmount);
	}

	// Add combat stress if not already in combat
	if (VitalsComp)
	{
		// Taking damage is stressful
		VitalsComp->AddStress(HitInfo.GetFinalDamage() * MOCombatConstants::DamageToStressRatio);

		// Ensure combat activity level
		if (VitalsComp->GetActivityLevel() != EMOActivityLevel::Combat)
		{
			VitalsComp->SetActivityLevel(EMOActivityLevel::Combat);
		}
	}

	return true;
}

bool UMOCombatMedicalHelpers::CanPerformCombatAction(UMOVitalsComponent* VitalsComp, float RequiredStamina)
{
	if (!VitalsComp)
	{
		return true;  // No vitals component = no stamina check
	}

	return VitalsComp->Activity.CurrentStamina >= RequiredStamina;
}

EMOBodyPartType UMOCombatMedicalHelpers::BoneNameToBodyPart(FName HitBoneName)
{
	// Delegate to the comprehensive skeleton mapping system
	// Supports UE5 Mannequin, MetaHuman, and custom skeletons
	return UMOSkeletonMapping::BoneToBodyPart(HitBoneName);
}

EMOBodyPartType UMOCombatMedicalHelpers::GetTargetBodyPartFromAngle(
	const FVector& AttackerLocation,
	const FVector& DefenderLocation,
	float AttackHeight)
{
	// Simple targeting based on attack height
	// 0.0 = low (feet/legs)
	// 0.5 = mid (torso)
	// 1.0 = high (head)

	if (AttackHeight < MOCombatConstants::LowAttackThreshold)
	{
		// Low attack - legs
		return EMOBodyPartType::ThighLeft;  // Could randomize left/right
	}
	else if (AttackHeight < MOCombatConstants::MidAttackThreshold)
	{
		// Mid attack - torso/arms
		return EMOBodyPartType::Torso;
	}
	else if (AttackHeight < MOCombatConstants::HighAttackThreshold)
	{
		// Upper attack - chest/shoulders
		return EMOBodyPartType::ShoulderLeft;
	}
	else
	{
		// High attack - head
		return EMOBodyPartType::Head;
	}
}

float UMOCombatMedicalHelpers::GetBodyPartDamageMultiplier(EMOBodyPartType BodyPart)
{
	using namespace MOCombatConstants::DamageMultiplier;

	switch (BodyPart)
	{
	// Instant death parts - maximum damage multiplier
	case EMOBodyPartType::Brain:
		return Brain;
	case EMOBodyPartType::Heart:
		return Heart;

	// Vital organs - high multiplier
	case EMOBodyPartType::LungLeft:
	case EMOBodyPartType::LungRight:
		return Lung;
	case EMOBodyPartType::Liver:
		return Liver;
	case EMOBodyPartType::Intestines:
		return Intestines;
	case EMOBodyPartType::KidneyLeft:
	case EMOBodyPartType::KidneyRight:
		return Kidney;

	// Head (but not brain)
	case EMOBodyPartType::Head:
		return Head;

	// Spine - high damage, risk of paralysis
	case EMOBodyPartType::SpineCervical:
		return SpineCervical;
	case EMOBodyPartType::SpineThoracic:
	case EMOBodyPartType::SpineLumbar:
		return SpineThoracolumbar;

	// Torso/core
	case EMOBodyPartType::Torso:
	case EMOBodyPartType::Stomach:
		return Torso;

	// Limbs - standard damage
	case EMOBodyPartType::ShoulderLeft:
	case EMOBodyPartType::ShoulderRight:
	case EMOBodyPartType::HipLeft:
	case EMOBodyPartType::HipRight:
		return ShoulderHip;

	case EMOBodyPartType::UpperArmLeft:
	case EMOBodyPartType::UpperArmRight:
	case EMOBodyPartType::ThighLeft:
	case EMOBodyPartType::ThighRight:
		return UpperLimb;

	case EMOBodyPartType::ForearmLeft:
	case EMOBodyPartType::ForearmRight:
	case EMOBodyPartType::CalfLeft:
	case EMOBodyPartType::CalfRight:
		return LowerLimb;

	// Extremities - low damage but disabling
	case EMOBodyPartType::HandLeft:
	case EMOBodyPartType::HandRight:
	case EMOBodyPartType::FootLeft:
	case EMOBodyPartType::FootRight:
		return HandFoot;

	// Sensory organs
	case EMOBodyPartType::EyeLeft:
	case EMOBodyPartType::EyeRight:
		return Eyes;
	case EMOBodyPartType::EarLeft:
	case EMOBodyPartType::EarRight:
		return Ears;

	// Fingers/toes - minimal damage
	default:
		return Default;
	}
}

bool UMOCombatMedicalHelpers::IsInstantDeathPart(EMOBodyPartType BodyPart)
{
	// Delegate to skeleton mapping for vital organ checks
	return UMOSkeletonMapping::IsVitalOrgan(BodyPart);
}
