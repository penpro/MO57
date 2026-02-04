#include "MOCombatMedicalTypes.h"
#include "MOAnatomyComponent.h"
#include "MOVitalsComponent.h"
#include "MOMentalStateComponent.h"
#include "MOAdrenalineComponent.h"
#include "MOSkeletonMappingTypes.h"

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
		float PainAmount = HitInfo.GetWoundSeverity() * 0.8f; // Pain proportional to severity
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
		VitalsComp->AddStress(HitInfo.GetFinalDamage() * 0.3f);

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

	if (AttackHeight < 0.25f)
	{
		// Low attack - legs
		return EMOBodyPartType::ThighLeft;  // Could randomize left/right
	}
	else if (AttackHeight < 0.6f)
	{
		// Mid attack - torso/arms
		return EMOBodyPartType::Torso;
	}
	else if (AttackHeight < 0.85f)
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
	switch (BodyPart)
	{
	// Instant death parts - maximum damage multiplier
	case EMOBodyPartType::Brain:
		return 5.0f;
	case EMOBodyPartType::Heart:
		return 4.0f;

	// Vital organs - high multiplier
	case EMOBodyPartType::LungLeft:
	case EMOBodyPartType::LungRight:
		return 2.5f;
	case EMOBodyPartType::Liver:
		return 2.0f;
	case EMOBodyPartType::Intestines:
	case EMOBodyPartType::KidneyLeft:
	case EMOBodyPartType::KidneyRight:
		return 1.8f;

	// Head (but not brain)
	case EMOBodyPartType::Head:
		return 2.0f;

	// Spine - high damage, risk of paralysis
	case EMOBodyPartType::SpineCervical:
		return 2.5f;
	case EMOBodyPartType::SpineThoracic:
	case EMOBodyPartType::SpineLumbar:
		return 1.8f;

	// Torso/core
	case EMOBodyPartType::Torso:
	case EMOBodyPartType::Stomach:
		return 1.2f;

	// Limbs - standard damage
	case EMOBodyPartType::ShoulderLeft:
	case EMOBodyPartType::ShoulderRight:
	case EMOBodyPartType::HipLeft:
	case EMOBodyPartType::HipRight:
		return 1.0f;

	case EMOBodyPartType::UpperArmLeft:
	case EMOBodyPartType::UpperArmRight:
	case EMOBodyPartType::ThighLeft:
	case EMOBodyPartType::ThighRight:
		return 0.9f;

	case EMOBodyPartType::ForearmLeft:
	case EMOBodyPartType::ForearmRight:
	case EMOBodyPartType::CalfLeft:
	case EMOBodyPartType::CalfRight:
		return 0.8f;

	// Extremities - low damage but disabling
	case EMOBodyPartType::HandLeft:
	case EMOBodyPartType::HandRight:
	case EMOBodyPartType::FootLeft:
	case EMOBodyPartType::FootRight:
		return 0.6f;

	// Sensory organs
	case EMOBodyPartType::EyeLeft:
	case EMOBodyPartType::EyeRight:
		return 1.5f;  // Not lethal but important
	case EMOBodyPartType::EarLeft:
	case EMOBodyPartType::EarRight:
		return 0.7f;

	// Fingers/toes - minimal damage
	default:
		return 0.4f;
	}
}

bool UMOCombatMedicalHelpers::IsInstantDeathPart(EMOBodyPartType BodyPart)
{
	// Delegate to skeleton mapping for vital organ checks
	return UMOSkeletonMapping::IsVitalOrgan(BodyPart);
}
