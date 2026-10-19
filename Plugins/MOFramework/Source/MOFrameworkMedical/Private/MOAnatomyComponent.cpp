#include "MOAnatomyComponent.h"
#include "MOAdrenalineComponent.h"
#include "MOFrameworkMedical.h"
#include "MOVitalsComponent.h"
#include "MOMentalStateComponent.h"
#include "MOBodyPartDefinitionRow.h"
#include "MOMedicalSubsystem.h"
#include "MOMedicalProviderInterface.h"
#include "MOGameClockSubsystem.h"
#include "MOWaterPhysiology.h"
#include "Net/UnrealNetwork.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

UMOAnatomyComponent::UMOAnatomyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	Wounds.SetOwner(this);
	Conditions.SetOwner(this);
}

void UMOAnatomyComponent::BeginPlay()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UMOAnatomyComponent_BeginPlay);   // spawn-cost breakdown in traces (-statnamedevents)
	Super::BeginPlay();

	// Cache sibling components via interface - avoids FindComponentByClass chains
	if (AActor* Owner = GetOwner())
	{
		if (Owner->Implements<UMOMedicalProviderInterface>())
		{
			CachedVitalsComp = IMOMedicalProviderInterface::Execute_GetVitals(Owner);
			CachedMentalComp = IMOMedicalProviderInterface::Execute_GetMentalState(Owner);
		}
		else
		{
			CachedVitalsComp = Owner->FindComponentByClass<UMOVitalsComponent>();
			CachedMentalComp = Owner->FindComponentByClass<UMOMentalStateComponent>();
		}
	}

	// Initialize body parts on authority
	if (GetOwnerRole() == ROLE_Authority)
	{
		InitializeBodyParts();

		// Start tick timer
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				TickTimerHandle,
				this,
				&UMOAnatomyComponent::TickAnatomy,
				TickInterval,
				true
			);
		}
	}
}

void UMOAnatomyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickTimerHandle);
		World->GetTimerManager().ClearTimer(DeathTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UMOAnatomyComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UMOAnatomyComponent, BodyParts, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMOAnatomyComponent, Wounds, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMOAnatomyComponent, Conditions, COND_OwnerOnly);
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void UMOAnatomyComponent::InitializeBodyParts()
{
	BodyParts.Empty();

	// Create state for all body parts
	for (uint8 i = 1; i < static_cast<uint8>(EMOBodyPartType::MAX); ++i)
	{
		EMOBodyPartType PartType = static_cast<EMOBodyPartType>(i);

		FMOBodyPartState PartState;
		PartState.PartType = PartType;
		PartState.Status = EMOBodyPartStatus::Healthy;

		// Try to get definition from DataTable
		FMOBodyPartDefinitionRow PartDef;
		if (GetBodyPartDefinition(PartType, PartDef))
		{
			PartState.MaxHP = PartDef.BaseHP;
			PartState.CurrentHP = PartDef.BaseHP;
		}
		else
		{
			// Default values if no DataTable entry
			PartState.MaxHP = 100.0f;
			PartState.CurrentHP = 100.0f;
		}

		BodyParts.Add(PartState);
	}
}

// ============================================================================
// DAMAGE API
// ============================================================================

bool UMOAnatomyComponent::InflictDamage(EMOBodyPartType Part, float Damage, EMOWoundType WoundType, AActor* DamageInstigator)
{
	AActor* Owner = GetOwner();

	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogMOFrameworkMedical, Warning, TEXT("[MOAnatomy] InflictDamage REJECTED: Not authority (Role=%d) on %s"),
			static_cast<int32>(GetOwnerRole()), Owner ? *Owner->GetName() : TEXT("NULL"));
		return false;
	}

	if (Part == EMOBodyPartType::None || Damage <= 0.0f)
	{
		UE_LOG(LogMOFrameworkMedical, Warning, TEXT("[MOAnatomy] InflictDamage REJECTED: Invalid part (%d) or damage (%.1f)"),
			static_cast<int32>(Part), Damage);
		return false;
	}

	FMOBodyPartState* PartState = GetBodyPartStateMutable(Part);
	if (!PartState || PartState->IsDestroyed())
	{
		UE_LOG(LogMOFrameworkMedical, Warning, TEXT("[MOAnatomy] InflictDamage REJECTED: PartState null or destroyed (Part=%d)"),
			static_cast<int32>(Part));
		return false;
	}

	// Get wound type definition
	FMOWoundTypeDefinitionRow WoundDef;
	GetWoundTypeDefinition(WoundType, WoundDef);

	// Get body part definition
	FMOBodyPartDefinitionRow PartDef;
	GetBodyPartDefinition(Part, PartDef);

	// Who did it, recorded before any event below fires: a death (CheckDeathConditions) or a creature's flight reads it from them.
	LastDamageInstigator = DamageInstigator;
	LastDamageTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	// Apply damage
	float ActualDamage = Damage * WoundDef.BaseDamageMultiplier;
	float OldHP = PartState->CurrentHP;
	PartState->CurrentHP = FMath::Max(0.0f, PartState->CurrentHP - ActualDamage);

	UE_LOG(LogMOFrameworkMedical, Log, TEXT("[MOAnatomy] InflictDamage: Part=%d, Damage=%.1f (actual=%.1f), HP %.1f -> %.1f, by %s"),
		static_cast<int32>(Part), Damage, ActualDamage, OldHP, PartState->CurrentHP, DamageInstigator ? *DamageInstigator->GetName() : TEXT("nobody"));

	// Update status
	if (PartState->CurrentHP <= 0.0f)
	{
		PartState->Status = EMOBodyPartStatus::Destroyed;
		UE_LOG(LogMOFrameworkMedical, Warning, TEXT("[MOAnatomy] Body part %d DESTROYED! Checking death conditions..."),
			static_cast<int32>(Part));
	}
	else if (PartState->CurrentHP < PartState->MaxHP)
	{
		PartState->Status = EMOBodyPartStatus::Injured;
	}

	// Create wound if applicable
	if (WoundType != EMOWoundType::None)
	{
		FMOWound NewWound;
		NewWound.WoundId = FGuid::NewGuid();
		NewWound.BodyPart = Part;
		NewWound.WoundType = WoundType;
		NewWound.Severity = FMath::Clamp(ActualDamage, 0.0f, 100.0f);
		NewWound.BleedRate = WoundDef.BaseBleedRate * PartDef.BleedMultiplier * (NewWound.Severity / 100.0f);
		NewWound.InfectionRisk = WoundDef.BaseInfectionRisk * PartDef.InfectionMultiplier;
		NewWound.HealingProgress = 0.0f;
		NewWound.TimeSinceInflicted = 0.0f;

		Wounds.AddWound(NewWound);

		// Apply shock from wound
		ApplyShock(WoundDef.ShockContribution * (NewWound.Severity / 100.0f));

		OnWoundInflicted.Broadcast(NewWound.WoundId, WoundType);
	}

	// Broadcast damage event
	OnBodyPartDamaged.Broadcast(Part, ActualDamage, PartState->CurrentHP);

	// Check for destruction effects
	if (PartState->CurrentHP <= 0.0f)
	{
		CheckDeathConditions(Part);
		OnBodyPartDestroyed.Broadcast(Part, PartDef.bInstantDeathOnDestruction);
	}

	return true;
}

bool UMOAnatomyComponent::InflictWound(const FMOWound& Wound)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (Wound.BodyPart == EMOBodyPartType::None)
	{
		return false;
	}

	FMOBodyPartState* PartState = GetBodyPartStateMutable(Wound.BodyPart);
	if (!PartState || PartState->IsDestroyed())
	{
		return false;
	}

	// Add wound with new GUID if needed
	FMOWound NewWound = Wound;
	if (!NewWound.WoundId.IsValid())
	{
		NewWound.WoundId = FGuid::NewGuid();
	}

	Wounds.AddWound(NewWound);
	OnWoundInflicted.Broadcast(NewWound.WoundId, NewWound.WoundType);

	return true;
}

bool UMOAnatomyComponent::HealWound(const FGuid& WoundId, float HealAmount)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	FMOWound* Wound = Wounds.FindWoundById(WoundId);
	if (!Wound)
	{
		return false;
	}

	Wound->HealingProgress = FMath::Clamp(Wound->HealingProgress + HealAmount, 0.0f, 100.0f);
	Wounds.MarkItemDirty(*Wound);

	// Check if fully healed
	if (Wound->HealingProgress >= 100.0f)
	{
		FGuid HealedId = WoundId;
		Wounds.RemoveWound(WoundId);
		OnWoundHealed.Broadcast(HealedId);
	}

	return true;
}

// ============================================================================
// TREATMENT API
// ============================================================================

bool UMOAnatomyComponent::ApplyTreatment(const FGuid& WoundId, FName TreatmentId, int32 MedicSkillLevel, bool bIsSelfTreatment)
{
	FMOMedicalTreatmentRow TreatmentDef;
	if (ValidateTreatment(WoundId, TreatmentId, MedicSkillLevel, bIsSelfTreatment, TreatmentDef) != EMOWoundTreatmentFailure::None)
	{
		return false;
	}
	ApplyResolvedTreatment(WoundId, TreatmentDef, MedicSkillLevel, bIsSelfTreatment);
	return true;
}

EMOWoundTreatmentFailure UMOAnatomyComponent::ValidateTreatment(const FGuid& WoundId, FName TreatmentId, int32 MedicSkillLevel,
	bool bIsSelfTreatment, FMOMedicalTreatmentRow& OutDefinition) const
{
	if (!GetOwner() || GetOwnerRole() != ROLE_Authority) return EMOWoundTreatmentFailure::NotAuthority;
	const FMOWound* Wound = Wounds.FindWoundById(WoundId);
	if (!Wound) return EMOWoundTreatmentFailure::MissingWound;
	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UMOMedicalSubsystem* Medical = GI ? GI->GetSubsystem<UMOMedicalSubsystem>() : nullptr;
	if (TreatmentId.IsNone() || !Medical || !Medical->GetTreatmentDefinition(TreatmentId, OutDefinition))
	{
		return EMOWoundTreatmentFailure::UnknownTreatment;
	}
	return ValidateTreatmentDefinition(*Wound, OutDefinition, MedicSkillLevel, bIsSelfTreatment);
}

EMOWoundTreatmentFailure UMOAnatomyComponent::ValidateTreatmentDefinition(const FMOWound& Wound, const FMOMedicalTreatmentRow& Def,
	int32 MedicSkillLevel, bool bIsSelfTreatment)
{
	auto Fraction = [](float Value) { return FMath::IsFinite(Value) && Value >= 0 && Value <= 1; };
	if (Def.TreatmentId.IsNone() || !FMath::IsFinite(Def.TreatmentDuration) || Def.TreatmentDuration <= 0 ||
		Def.MinimumSkillLevel < 0 || !Fraction(Def.BleedReduction) || !Fraction(Def.InfectionReduction) ||
		!Fraction(Def.SelfTreatmentPenalty) || !Fraction(Def.QualitySkillScaling) ||
		!FMath::IsFinite(Def.SkillXPGrant) || Def.SkillXPGrant < 0 || (Def.RequiredSkillId.IsNone() && Def.MinimumSkillLevel > 0))
	{
		return EMOWoundTreatmentFailure::InvalidDefinition;
	}
	if (!FMath::IsFinite(Wound.BleedRate) || Wound.BleedRate < 0 || !FMath::IsFinite(Wound.InfectionRisk) || Wound.InfectionRisk < 0 ||
		!FMath::IsFinite(Wound.HealingProgress) || Wound.HealingProgress >= 100) return EMOWoundTreatmentFailure::InvalidWound;
	if (!Def.TreatsWoundTypes.Contains(Wound.WoundType)) return EMOWoundTreatmentFailure::WrongWoundType;
	if (MedicSkillLevel < Def.MinimumSkillLevel) return EMOWoundTreatmentFailure::InsufficientSkill;
	if (bIsSelfTreatment && Def.UnreachableForSelf.Contains(Wound.BodyPart)) return EMOWoundTreatmentFailure::UnreachableForSelf;
	return EMOWoundTreatmentFailure::None;
}

void UMOAnatomyComponent::ApplyResolvedTreatment(const FGuid& WoundId, const FMOMedicalTreatmentRow& TreatmentDef, int32 MedicSkillLevel, bool bIsSelfTreatment)
{
	FMOWound* Wound = Wounds.FindWoundById(WoundId);
	check(Wound);
	// Preserve the established quality model. Clamp reductions so high skill cannot create negative bleeding/infection.
	float Quality = FMath::Clamp(static_cast<float>(MedicSkillLevel) / 100.0f, 0.1f, 1.0f);
	Quality += (MedicSkillLevel - TreatmentDef.MinimumSkillLevel) * TreatmentDef.QualitySkillScaling * 0.01f;
	if (bIsSelfTreatment) Quality *= (1.0f - TreatmentDef.SelfTreatmentPenalty);
	Quality = FMath::Clamp(Quality, 0.1f, 1.5f);
	Wound->BleedRate *= 1.0f - FMath::Clamp(TreatmentDef.BleedReduction * Quality, 0.0f, 1.0f);
	Wound->InfectionRisk *= 1.0f - FMath::Clamp(TreatmentDef.InfectionReduction * Quality, 0.0f, 1.0f);
	Wounds.MarkItemDirty(*Wound);
}

bool UMOAnatomyComponent::ApplyBandage(const FGuid& WoundId, float Quality)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	FMOWound* Wound = Wounds.FindWoundById(WoundId);
	if (!Wound || Wound->bIsBandaged)
	{
		return false;
	}

	Wound->bIsBandaged = true;
	Wound->BleedRate *= (1.0f - (0.7f * Quality));  // Reduce bleed significantly
	Wound->InfectionRisk *= (1.0f - (0.2f * Quality));

	Wounds.MarkItemDirty(*Wound);
	return true;
}

bool UMOAnatomyComponent::ApplySutures(const FGuid& WoundId, float Quality)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	FMOWound* Wound = Wounds.FindWoundById(WoundId);
	if (!Wound || Wound->bIsSutured)
	{
		return false;
	}

	Wound->bIsSutured = true;
	Wound->BleedRate *= (1.0f - (0.9f * Quality));  // Nearly stop bleeding

	Wounds.MarkItemDirty(*Wound);
	return true;
}

// ============================================================================
// CONDITION API
// ============================================================================

bool UMOAnatomyComponent::AddCondition(EMOConditionType ConditionType, EMOBodyPartType Part, float InitialSeverity)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (ConditionType == EMOConditionType::None)
	{
		return false;
	}

	// Check if already have this condition
	if (HasCondition(ConditionType))
	{
		return false;
	}

	FMOCondition NewCondition;
	NewCondition.ConditionId = FGuid::NewGuid();
	NewCondition.ConditionType = ConditionType;
	NewCondition.AffectedPart = Part;
	NewCondition.Severity = FMath::Clamp(InitialSeverity, 0.0f, 100.0f);
	NewCondition.Duration = 0.0f;
	NewCondition.bIsTreated = false;

	// A condition that follows a course (an illness) starts silent: its incubation and its length are rolled now, from its definition.
	FMOConditionDefinitionRow Def;
	const UMOMedicalSubsystem* Medical = GetWorld() && GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<UMOMedicalSubsystem>() : nullptr;
	if (Medical && Medical->GetConditionDefinition(ConditionType, Def) && Def.bFollowsCourse)
	{
		NewCondition.IncubationRemaining = FMath::FRandRange(Def.IncubationGameHoursMin, FMath::Max(Def.IncubationGameHoursMin, Def.IncubationGameHoursMax)) * 3600.0f;
		NewCondition.CourseSeconds = FMath::Max(60.0f, FMath::FRandRange(Def.CourseGameHoursMin, FMath::Max(Def.CourseGameHoursMin, Def.CourseGameHoursMax)) * 3600.0f);
		NewCondition.CourseElapsed = 0.0f;
		NewCondition.Severity = 0.0f;
		UE_LOG(LogMOFrameworkMedical, Log, TEXT("[MOAnatomy] %s contracted %s: symptoms in %.1f game h, lasting %.1f game h"), *GetNameSafe(GetOwner()),
			*UEnum::GetValueAsString(ConditionType), NewCondition.IncubationRemaining / 3600.0f, NewCondition.CourseSeconds / 3600.0f);
	}

	Conditions.AddCondition(NewCondition);
	OnConditionAdded.Broadcast(NewCondition.ConditionId, ConditionType);
	OnConditionsChanged.Broadcast();

	return true;
}

bool UMOAnatomyComponent::ForEachConditionForDebug(TFunctionRef<void(FMOCondition&)> Edit)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}
	for (FMOCondition& Condition : Conditions.Conditions)
	{
		Edit(Condition);
		Conditions.MarkItemDirty(Condition);
	}
	OnConditionsChanged.Broadcast();
	return true;
}

bool UMOAnatomyComponent::ExposeToCondition(EMOConditionType ConditionType, float Chance)
{
	if (GetOwnerRole() != ROLE_Authority || Chance <= 0.0f || HasCondition(ConditionType))
	{
		return false;
	}
	return FMath::FRand() < Chance && AddCondition(ConditionType);
}

bool UMOAnatomyComponent::RemoveCondition(const FGuid& ConditionId)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	const FMOCondition* Condition = Conditions.FindConditionById(ConditionId);
	if (!Condition)
	{
		return false;
	}

	EMOConditionType Type = Condition->ConditionType;
	if (Conditions.RemoveCondition(ConditionId))
	{
		OnConditionRemoved.Broadcast(ConditionId, Type);
		return true;
	}

	return false;
}

bool UMOAnatomyComponent::HasCondition(EMOConditionType ConditionType) const
{
	return Conditions.Conditions.ContainsByPredicate([ConditionType](const FMOCondition& Cond)
	{
		return Cond.ConditionType == ConditionType;
	});
}

bool UMOAnatomyComponent::GetConditionByType(EMOConditionType ConditionType, FMOCondition& OutCondition) const
{
	const FMOCondition* Found = Conditions.Conditions.FindByPredicate([ConditionType](const FMOCondition& Cond)
	{
		return Cond.ConditionType == ConditionType;
	});

	if (Found)
	{
		OutCondition = *Found;
		return true;
	}
	return false;
}

// ============================================================================
// QUERY API
// ============================================================================

float UMOAnatomyComponent::GetTotalBleedRate() const
{
	float TotalRate = 0.0f;
	for (const FMOWound& Wound : Wounds.Wounds)
	{
		TotalRate += Wound.BleedRate;
	}
	return TotalRate;
}

float UMOAnatomyComponent::GetTotalPainLevel() const
{
	float TotalPain = 0.0f;

	// Pain from wounds
	for (const FMOWound& Wound : Wounds.Wounds)
	{
		FMOWoundTypeDefinitionRow WoundDef;
		if (GetWoundTypeDefinition(Wound.WoundType, WoundDef))
		{
			TotalPain += Wound.Severity * WoundDef.PainMultiplier * 0.3f;
		}
		else
		{
			TotalPain += Wound.Severity * 0.3f;
		}

		// Infected wounds hurt more
		if (Wound.bIsInfected)
		{
			TotalPain += Wound.InfectionSeverity * 0.2f;
		}
	}

	return FMath::Clamp(TotalPain, 0.0f, 100.0f);
}

bool UMOAnatomyComponent::IsBodyPartFunctional(EMOBodyPartType Part) const
{
	FMOBodyPartState State;
	if (GetBodyPartState(Part, State))
	{
		return State.IsFunctional();
	}
	return false;
}

bool UMOAnatomyComponent::GetBodyPartState(EMOBodyPartType Part, FMOBodyPartState& OutState) const
{
	const FMOBodyPartState* Found = BodyParts.FindByPredicate([Part](const FMOBodyPartState& State)
	{
		return State.PartType == Part;
	});

	if (Found)
	{
		OutState = *Found;
		return true;
	}
	return false;
}

TArray<FMOWound> UMOAnatomyComponent::GetWoundsOnPart(EMOBodyPartType Part) const
{
	TArray<FMOWound> Result;
	for (const FMOWound& Wound : Wounds.Wounds)
	{
		if (Wound.BodyPart == Part)
		{
			Result.Add(Wound);
		}
	}
	return Result;
}

TArray<FMOWound> UMOAnatomyComponent::GetAllWounds() const
{
	return Wounds.Wounds;
}

TArray<FMOCondition> UMOAnatomyComponent::GetAllConditions() const
{
	return Conditions.Conditions;
}

bool UMOAnatomyComponent::CanMove() const
{
	// Need at least one functional leg
	bool bHasLeftLeg = IsBodyPartFunctional(EMOBodyPartType::ThighLeft) &&
					   IsBodyPartFunctional(EMOBodyPartType::CalfLeft);
	bool bHasRightLeg = IsBodyPartFunctional(EMOBodyPartType::ThighRight) &&
						IsBodyPartFunctional(EMOBodyPartType::CalfRight);

	// Need functional spine
	bool bHasSpine = IsBodyPartFunctional(EMOBodyPartType::SpineLumbar) &&
					 IsBodyPartFunctional(EMOBodyPartType::SpineThoracic);

	return (bHasLeftLeg || bHasRightLeg) && bHasSpine;
}

bool UMOAnatomyComponent::CanGrip() const
{
	// Need at least one functional hand with thumb and at least one finger
	auto CheckHand = [this](EMOBodyPartType Hand, EMOBodyPartType Thumb, EMOBodyPartType Index,
							EMOBodyPartType Middle, EMOBodyPartType Ring, EMOBodyPartType Pinky) -> bool
	{
		if (!IsBodyPartFunctional(Hand) || !IsBodyPartFunctional(Thumb))
		{
			return false;
		}
		return IsBodyPartFunctional(Index) || IsBodyPartFunctional(Middle) ||
			   IsBodyPartFunctional(Ring) || IsBodyPartFunctional(Pinky);
	};

	return CheckHand(EMOBodyPartType::HandLeft, EMOBodyPartType::ThumbLeft,
					 EMOBodyPartType::IndexFingerLeft, EMOBodyPartType::MiddleFingerLeft,
					 EMOBodyPartType::RingFingerLeft, EMOBodyPartType::PinkyFingerLeft) ||
		   CheckHand(EMOBodyPartType::HandRight, EMOBodyPartType::ThumbRight,
					 EMOBodyPartType::IndexFingerRight, EMOBodyPartType::MiddleFingerRight,
					 EMOBodyPartType::RingFingerRight, EMOBodyPartType::PinkyFingerRight);
}

bool UMOAnatomyComponent::CanSee() const
{
	return IsBodyPartFunctional(EMOBodyPartType::EyeLeft) ||
		   IsBodyPartFunctional(EMOBodyPartType::EyeRight);
}

bool UMOAnatomyComponent::CanHear() const
{
	return IsBodyPartFunctional(EMOBodyPartType::EarLeft) ||
		   IsBodyPartFunctional(EMOBodyPartType::EarRight);
}

// ============================================================================
// PERSISTENCE
// ============================================================================

void UMOAnatomyComponent::BuildSaveData(FMOAnatomySaveData& OutSaveData) const
{
	OutSaveData.BodyParts.Empty();
	OutSaveData.Wounds.Empty();
	OutSaveData.Conditions.Empty();

	// Save body parts
	for (const FMOBodyPartState& Part : BodyParts)
	{
		FMOBodyPartSaveEntry Entry;
		Entry.PartType = Part.PartType;
		Entry.Status = Part.Status;
		Entry.CurrentHP = Part.CurrentHP;
		Entry.MaxHP = Part.MaxHP;
		Entry.BoneDensity = Part.BoneDensity;
		OutSaveData.BodyParts.Add(Entry);
	}

	// Save wounds
	for (const FMOWound& Wound : Wounds.Wounds)
	{
		FMOWoundSaveEntry Entry;
		Entry.WoundId = Wound.WoundId;
		Entry.BodyPart = Wound.BodyPart;
		Entry.WoundType = Wound.WoundType;
		Entry.Severity = Wound.Severity;
		Entry.BleedRate = Wound.BleedRate;
		Entry.InfectionRisk = Wound.InfectionRisk;
		Entry.HealingProgress = Wound.HealingProgress;
		Entry.bIsBandaged = Wound.bIsBandaged;
		Entry.bIsSutured = Wound.bIsSutured;
		Entry.bIsInfected = Wound.bIsInfected;
		Entry.InfectionSeverity = Wound.InfectionSeverity;
		Entry.TimeSinceInflicted = Wound.TimeSinceInflicted;
		OutSaveData.Wounds.Add(Entry);
	}

	// Save conditions
	for (const FMOCondition& Condition : Conditions.Conditions)
	{
		FMOConditionSaveEntry Entry;
		Entry.ConditionId = Condition.ConditionId;
		Entry.ConditionType = Condition.ConditionType;
		Entry.AffectedPart = Condition.AffectedPart;
		Entry.Severity = Condition.Severity;
		Entry.Duration = Condition.Duration;
		Entry.bIsTreated = Condition.bIsTreated;
		Entry.IncubationRemaining = Condition.IncubationRemaining;
		Entry.CourseSeconds = Condition.CourseSeconds;
		Entry.CourseElapsed = Condition.CourseElapsed;
		OutSaveData.Conditions.Add(Entry);
	}
}

bool UMOAnatomyComponent::ApplySaveDataAuthority(const FMOAnatomySaveData& InSaveData)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	// Clear current state
	BodyParts.Empty();
	Wounds.Wounds.Empty();
	Conditions.Conditions.Empty();

	// Restore body parts
	for (const FMOBodyPartSaveEntry& Entry : InSaveData.BodyParts)
	{
		FMOBodyPartState Part;
		Part.PartType = Entry.PartType;
		Part.Status = Entry.Status;
		Part.CurrentHP = Entry.CurrentHP;
		Part.MaxHP = Entry.MaxHP;
		Part.BoneDensity = Entry.BoneDensity;
		BodyParts.Add(Part);
	}

	// If no body parts were saved, initialize defaults
	if (BodyParts.Num() == 0)
	{
		InitializeBodyParts();
	}

	// Restore wounds
	for (const FMOWoundSaveEntry& Entry : InSaveData.Wounds)
	{
		FMOWound Wound;
		Wound.WoundId = Entry.WoundId;
		Wound.BodyPart = Entry.BodyPart;
		Wound.WoundType = Entry.WoundType;
		Wound.Severity = Entry.Severity;
		Wound.BleedRate = Entry.BleedRate;
		Wound.InfectionRisk = Entry.InfectionRisk;
		Wound.HealingProgress = Entry.HealingProgress;
		Wound.bIsBandaged = Entry.bIsBandaged;
		Wound.bIsSutured = Entry.bIsSutured;
		Wound.bIsInfected = Entry.bIsInfected;
		Wound.InfectionSeverity = Entry.InfectionSeverity;
		Wound.TimeSinceInflicted = Entry.TimeSinceInflicted;
		Wounds.AddWound(Wound);
	}

	// Restore conditions
	for (const FMOConditionSaveEntry& Entry : InSaveData.Conditions)
	{
		FMOCondition Condition;
		Condition.ConditionId = Entry.ConditionId;
		Condition.ConditionType = Entry.ConditionType;
		Condition.AffectedPart = Entry.AffectedPart;
		Condition.Severity = Entry.Severity;
		Condition.Duration = Entry.Duration;
		Condition.bIsTreated = Entry.bIsTreated;
		Condition.IncubationRemaining = Entry.IncubationRemaining;
		Condition.CourseSeconds = Entry.CourseSeconds;
		Condition.CourseElapsed = Entry.CourseElapsed;
		Conditions.AddCondition(Condition);
	}

	return true;
}

// ============================================================================
// REPLICATION CALLBACKS
// ============================================================================

void UMOAnatomyComponent::OnWoundReplicatedAdd(const FMOWound& Wound)
{
	// Client-side notification
	OnWoundInflicted.Broadcast(Wound.WoundId, Wound.WoundType);
}

void UMOAnatomyComponent::OnWoundReplicatedChange(const FMOWound& Wound)
{
	// Optional: notify UI of wound updates
}

void UMOAnatomyComponent::OnWoundReplicatedRemove(const FMOWound& Wound)
{
	// Client-side notification
	OnWoundHealed.Broadcast(Wound.WoundId);
}

void UMOAnatomyComponent::OnConditionReplicatedAdd(const FMOCondition& Condition)
{
	OnConditionAdded.Broadcast(Condition.ConditionId, Condition.ConditionType);
}

void UMOAnatomyComponent::OnConditionReplicatedChange(const FMOCondition& Condition)
{
}

void UMOAnatomyComponent::OnConditionReplicatedRemove(const FMOCondition& Condition)
{
	OnConditionRemoved.Broadcast(Condition.ConditionId, Condition.ConditionType);
}

void UMOAnatomyComponent::OnConditionListReplicated()
{
	// After the receive is applied (a removed condition is gone): the status panel and moodles re-read the whole list on this.
	OnConditionsChanged.Broadcast();
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

FMOBodyPartState* UMOAnatomyComponent::GetBodyPartStateMutable(EMOBodyPartType Part)
{
	return BodyParts.FindByPredicate([Part](const FMOBodyPartState& State)
	{
		return State.PartType == Part;
	});
}

void UMOAnatomyComponent::TickAnatomy()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	// Pull shared TimeScale from the central clock — see UMOGameClockSubsystem.
	const UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this);
	const float TimeScaleMultiplier = Clock ? Clock->GetTimeScale() : 1.0f;
	float ScaledDeltaTime = TickInterval * TimeScaleMultiplier;

	// Process wounds. ProcessWound may remove a fully-healed wound, which
	// invalidates the index — so the bleed contribution comes back as the
	// return value instead of re-reading Wounds.Wounds[i] after the call.
	float TotalBleedRate = 0.0f;
	for (int32 i = Wounds.Wounds.Num() - 1; i >= 0; --i)
	{
		TotalBleedRate += ProcessWound(Wounds.Wounds[i], ScaledDeltaTime);
	}

	// Adrenaline vasoconstriction slows bleeding (codex review: the producer
	// consumes GetBleedReductionPercent instead of leaving it dangling).
	if (TotalBleedRate > 0.0f)
	{
		if (const UMOAdrenalineComponent* Adrenaline = GetOwner() ? GetOwner()->FindComponentByClass<UMOAdrenalineComponent>() : nullptr)
		{
			TotalBleedRate *= FMath::Clamp(1.0f - Adrenaline->GetBleedReductionPercent() / 100.0f, 0.0f, 1.0f);
		}
	}

	// Apply blood loss to vitals
	if (TotalBleedRate > 0.0f)
	{
		ApplyBloodLoss(TotalBleedRate * ScaledDeltaTime);
	}

	// Process conditions
	for (int32 i = Conditions.Conditions.Num() - 1; i >= 0; --i)
	{
		ProcessCondition(Conditions.Conditions[i], ScaledDeltaTime);
	}

	// Update pain level in exertion state
	if (UMOVitalsComponent* VitalsComp = CachedVitalsComp.Get())
	{
		// This would be accessed differently - TODO: Add setter in VitalsComponent
	}
}

float UMOAnatomyComponent::ProcessWound(FMOWound& Wound, float DeltaTime)
{
	// Update time
	Wound.TimeSinceInflicted += DeltaTime;

	// Check for infection
	if (!Wound.bIsInfected && Wound.InfectionRisk > 0.0f)
	{
		float InfectionChance = Wound.InfectionRisk * DeltaTime * 0.001f;  // Per-tick chance
		if (FMath::FRand() < InfectionChance)
		{
			Wound.bIsInfected = true;
			Wound.InfectionSeverity = 10.0f;
		}
	}

	// Progress infection
	if (Wound.bIsInfected)
	{
		float InfectionGrowth = Wound.bIsBandaged ? 0.5f : 1.0f;  // Bandages slow infection
		Wound.InfectionSeverity = FMath::Min(100.0f, Wound.InfectionSeverity + InfectionGrowth * DeltaTime * 0.01f);

		// Severe infection can become systemic
		if (Wound.InfectionSeverity >= 80.0f && !HasCondition(EMOConditionType::Sepsis))
		{
			AddCondition(EMOConditionType::Sepsis, EMOBodyPartType::None, 20.0f);
		}
	}

	// Natural healing (very slow without treatment)
	float HealRate = 0.001f;  // Base rate per second

	if (Wound.bIsBandaged)
	{
		HealRate *= 2.0f;
	}
	if (Wound.bIsSutured)
	{
		HealRate *= 3.0f;
	}
	if (Wound.bIsInfected)
	{
		HealRate *= 0.1f;  // Infections prevent healing
	}

	// Check if wound requires special treatment
	FMOWoundTypeDefinitionRow WoundDef;
	if (GetWoundTypeDefinition(Wound.WoundType, WoundDef))
	{
		if (WoundDef.bRequiresSuturing && !Wound.bIsSutured)
		{
			HealRate *= 0.1f;  // Almost no healing without sutures
		}
		if (WoundDef.bRequiresSplint && Wound.WoundType == EMOWoundType::Fracture)
		{
			HealRate *= 0.2f;  // Slow healing without splint
		}
	}

	Wound.HealingProgress = FMath::Min(100.0f, Wound.HealingProgress + HealRate * DeltaTime);

	// Reduce bleed rate as wound heals
	float HealFactor = 1.0f - (Wound.HealingProgress / 100.0f);
	// Bleed rate naturally decreases as wound closes

	// Check if fully healed. A healed wound is closed — it contributes no
	// bleed this tick, and removal invalidates the caller's reference/index,
	// which is why the contribution goes back as the return value.
	if (Wound.HealingProgress >= 100.0f)
	{
		FGuid HealedId = Wound.WoundId;
		Wounds.RemoveWound(HealedId);
		OnWoundHealed.Broadcast(HealedId);
		return 0.0f;
	}

	Wounds.MarkItemDirty(Wound);
	// Clotting: the bleed contribution tapers with closure (HealFactor 1->0 as
	// HealingProgress 0->100%) instead of running at full rate until the wound
	// vanishes, so an untreated cut slows rather than staying lethal forever. (H16)
	return Wound.BleedRate * HealFactor;
}

void UMOAnatomyComponent::ProcessCondition(FMOCondition& Condition, float DeltaTime)
{
	Condition.Duration += DeltaTime;

	// Get condition definition from DataTable via MOMedicalSubsystem
	FMOConditionDefinitionRow ConditionDef;
	bool bHasDefinition = false;

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UMOMedicalSubsystem* MedicalSubsystem = GameInstance->GetSubsystem<UMOMedicalSubsystem>())
			{
				bHasDefinition = MedicalSubsystem->GetConditionDefinition(Condition.ConditionType, ConditionDef);
			}
		}
	}

	// An illness with a course: silent incubation, then a severity that rises to the peak and fades; gone at the end. Its harm is in its body effects (fluid loss ...,
	// applied by the survival stats), not in a severity that only climbs.
	if (Condition.FollowsCourse())
	{
		const bool bWasSymptomatic = Condition.IsSymptomatic();
		const float OldSeverity = Condition.Severity;
		if (Condition.IncubationRemaining > 0.0f)
		{
			Condition.IncubationRemaining = FMath::Max(0.0f, Condition.IncubationRemaining - DeltaTime);
		}
		else
		{
			Condition.CourseElapsed += DeltaTime;
			Condition.Severity = FMOWaterPhysiology::CourseSeverity(Condition.CourseElapsed, Condition.CourseSeconds, bHasDefinition ? ConditionDef.PeakSeverity : 60.0f);
		}
		if (Condition.IsSymptomatic() && Condition.CourseElapsed >= Condition.CourseSeconds)
		{
			const FGuid RemovedId = Condition.ConditionId;
			const EMOConditionType RemovedType = Condition.ConditionType;
			UE_LOG(LogMOFrameworkMedical, Log, TEXT("[MOAnatomy] %s recovered from %s"), *GetNameSafe(GetOwner()), *UEnum::GetValueAsString(RemovedType));
			Conditions.RemoveCondition(RemovedId);
			OnConditionRemoved.Broadcast(RemovedId, RemovedType);
			OnConditionsChanged.Broadcast();
			return;
		}
		if (bWasSymptomatic != Condition.IsSymptomatic())
		{
			UE_LOG(LogMOFrameworkMedical, Log, TEXT("[MOAnatomy] %s: %s symptoms start (%.1f game h course)"), *GetNameSafe(GetOwner()),
				*UEnum::GetValueAsString(Condition.ConditionType), Condition.CourseSeconds / 3600.0f);
		}
		Conditions.MarkItemDirty(Condition);
		if (bWasSymptomatic != Condition.IsSymptomatic() || FMath::Abs(Condition.Severity - OldSeverity) >= 1.0f)
		{
			OnConditionsChanged.Broadcast();
		}
		return;
	}

	// Use DataTable values if available, otherwise use defaults
	float ProgressionRate = bHasDefinition ? ConditionDef.ProgressionRate : 0.1f;
	float TreatedRecoveryRate = bHasDefinition ? ConditionDef.TreatedRecoveryRate : 0.05f;
	float NaturalRecoveryRate = bHasDefinition ? ConditionDef.NaturalRecoveryRate : 0.0f;
	EMOConditionType ProgressesTo = bHasDefinition ? ConditionDef.ProgressesTo : EMOConditionType::None;
	float ProgressionThreshold = bHasDefinition ? ConditionDef.ProgressionThreshold : 80.0f;

	if (!Condition.bIsTreated)
	{
		// Condition worsens if untreated (minus any natural recovery)
		float NetRate = ProgressionRate - NaturalRecoveryRate;
		Condition.Severity = FMath::Min(100.0f, Condition.Severity + NetRate * DeltaTime);
	}
	else
	{
		// Treated conditions improve at treated recovery rate
		Condition.Severity = FMath::Max(0.0f, Condition.Severity - TreatedRecoveryRate * DeltaTime);
	}

	// Decide progression now, but apply it LAST: AddCondition appends to the
	// Conditions array this Condition& points into, and a reallocation would
	// leave every use of Condition below reading (and MarkItemDirty writing)
	// through a dangling reference.
	const bool bProgress = ProgressesTo != EMOConditionType::None
		&& Condition.Severity >= ProgressionThreshold
		&& !HasCondition(ProgressesTo);

	// Check for condition resolution
	if (Condition.Severity <= 0.0f)
	{
		FGuid RemovedId = Condition.ConditionId;
		EMOConditionType RemovedType = Condition.ConditionType;
		Conditions.RemoveCondition(RemovedId);
		OnConditionRemoved.Broadcast(RemovedId, RemovedType);
	}
	else
	{
		Conditions.MarkItemDirty(Condition);
	}

	// Progression (e.g., Infection -> Sepsis) after the final use of
	// Condition. Appending during TickAnatomy's reverse index loop is safe:
	// new entries land above the current index and get processed next tick.
	if (bProgress)
	{
		AddCondition(ProgressesTo, EMOBodyPartType::None, 20.0f);
	}
}

void UMOAnatomyComponent::CheckDeathConditions(EMOBodyPartType DestroyedPart)
{
	FMOBodyPartDefinitionRow PartDef;
	if (!GetBodyPartDefinition(DestroyedPart, PartDef))
	{
		// Fallback for vital organs
		if (DestroyedPart == EMOBodyPartType::Brain || DestroyedPart == EMOBodyPartType::Heart)
		{
			OnInstantDeath.Broadcast(DestroyedPart);
			return;
		}
		if (DestroyedPart == EMOBodyPartType::LungLeft || DestroyedPart == EMOBodyPartType::LungRight)
		{
			// Check if both lungs are destroyed
			bool bBothLungsDestroyed = !IsBodyPartFunctional(EMOBodyPartType::LungLeft) &&
									   !IsBodyPartFunctional(EMOBodyPartType::LungRight);
			if (bBothLungsDestroyed)
			{
				StartDeathTimer(DestroyedPart, 180.0f);  // ~3 minutes
			}
		}
		return;
	}

	if (PartDef.bInstantDeathOnDestruction)
	{
		OnInstantDeath.Broadcast(DestroyedPart);
	}
	else if (PartDef.DeathTimerOnDestruction > 0.0f)
	{
		StartDeathTimer(DestroyedPart, PartDef.DeathTimerOnDestruction);
	}
}

void UMOAnatomyComponent::StartDeathTimer(EMOBodyPartType Part, float Seconds)
{
	if (DeathTimerHandle.IsValid())
	{
		// Already have a death timer running
		return;
	}

	DeathTimerPart = Part;
	DeathTimerRemaining = Seconds;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DeathTimerHandle,
			this,
			&UMOAnatomyComponent::TickDeathTimer,
			1.0f,
			true
		);
	}
}

void UMOAnatomyComponent::TickDeathTimer()
{
	// Pull shared TimeScale from the central clock — see UMOGameClockSubsystem.
	const UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this);
	const float TimeScaleMultiplier = Clock ? Clock->GetTimeScale() : 1.0f;
	DeathTimerRemaining -= TimeScaleMultiplier;

	OnDeathTimer.Broadcast(DeathTimerPart, DeathTimerRemaining);

	if (DeathTimerRemaining <= 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(DeathTimerHandle);
		}
		OnInstantDeath.Broadcast(DeathTimerPart);
	}
}

void UMOAnatomyComponent::ApplyBloodLoss(float AmountML)
{
	if (UMOVitalsComponent* VitalsComp = CachedVitalsComp.Get())
	{
		VitalsComp->ApplyBloodLoss(AmountML);
	}
}

void UMOAnatomyComponent::ApplyShock(float Amount)
{
	if (UMOMentalStateComponent* MentalComp = CachedMentalComp.Get())
	{
		MentalComp->AddShock(Amount);
	}
}

bool UMOAnatomyComponent::GetBodyPartDefinition(EMOBodyPartType Part, FMOBodyPartDefinitionRow& OutDef) const
{
	// Look up via MOMedicalSubsystem
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UMOMedicalSubsystem* MedicalSubsystem = GameInstance->GetSubsystem<UMOMedicalSubsystem>())
			{
				return MedicalSubsystem->GetBodyPartDefinition(Part, OutDef);
			}
		}
	}
	return false;
}

bool UMOAnatomyComponent::GetWoundTypeDefinition(EMOWoundType Type, FMOWoundTypeDefinitionRow& OutDef) const
{
	// Look up via MOMedicalSubsystem
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UMOMedicalSubsystem* MedicalSubsystem = GameInstance->GetSubsystem<UMOMedicalSubsystem>())
			{
				return MedicalSubsystem->GetWoundTypeDefinition(Type, OutDef);
			}
		}
	}
	return false;
}
