#include "MOMentalStateComponent.h"
#include "MOVitalsComponent.h"
#include "MOAnatomyComponent.h"
#include "MOMetabolismComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UMOMentalStateComponent::UMOMentalStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMOMentalStateComponent::BeginPlay()
{
	Super::BeginPlay();

	// Cache sibling components
	if (AActor* Owner = GetOwner())
	{
		CachedVitalsComp = Owner->FindComponentByClass<UMOVitalsComponent>();
		CachedAnatomyComp = Owner->FindComponentByClass<UMOAnatomyComponent>();
		CachedMetabolismComp = Owner->FindComponentByClass<UMOMetabolismComponent>();
	}

	// Initialize previous state
	PreviousConsciousness = MentalState.Consciousness;

	// Start tick timer on authority
	if (GetOwnerRole() == ROLE_Authority)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				TickTimerHandle,
				this,
				&UMOMentalStateComponent::TickMentalState,
				TickInterval,
				true
			);
		}
	}
}

void UMOMentalStateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UMOMentalStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UMOMentalStateComponent, MentalState, COND_OwnerOnly);
}

// ============================================================================
// SHOCK API
// ============================================================================

void UMOMentalStateComponent::AddShock(float Amount)
{
	if (GetOwnerRole() != ROLE_Authority || Amount <= 0.0f)
	{
		return;
	}

	float OldShock = MentalState.ShockAccumulation;
	MentalState.ShockAccumulation = FMath::Clamp(MentalState.ShockAccumulation + Amount, 0.0f, 100.0f);

	if (FMath::Abs(MentalState.ShockAccumulation - OldShock) >= 5.0f)
	{
		OnShockLevelChanged.Broadcast(OldShock, MentalState.ShockAccumulation);
	}
}

void UMOMentalStateComponent::RemoveShock(float Amount)
{
	if (GetOwnerRole() != ROLE_Authority || Amount <= 0.0f)
	{
		return;
	}

	float OldShock = MentalState.ShockAccumulation;
	MentalState.ShockAccumulation = FMath::Clamp(MentalState.ShockAccumulation - Amount, 0.0f, 100.0f);

	if (FMath::Abs(MentalState.ShockAccumulation - OldShock) >= 5.0f)
	{
		OnShockLevelChanged.Broadcast(OldShock, MentalState.ShockAccumulation);
	}
}

void UMOMentalStateComponent::AddTraumaticStress(float Amount)
{
	if (GetOwnerRole() != ROLE_Authority || Amount <= 0.0f)
	{
		return;
	}

	MentalState.TraumaticStress = FMath::Clamp(MentalState.TraumaticStress + Amount, 0.0f, 100.0f);
}

void UMOMentalStateComponent::AddMoraleFatigue(float Amount)
{
	if (GetOwnerRole() != ROLE_Authority || Amount <= 0.0f)
	{
		return;
	}

	MentalState.MoraleFatigue = FMath::Clamp(MentalState.MoraleFatigue + Amount, 0.0f, 100.0f);
}

// ============================================================================
// CONSCIOUSNESS API
// ============================================================================

void UMOMentalStateComponent::ForceConsciousnessLevel(EMOConsciousnessLevel Level)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	EMOConsciousnessLevel OldLevel = MentalState.Consciousness;
	MentalState.Consciousness = Level;
	bConsciousnessForced = true;

	if (OldLevel != Level)
	{
		OnConsciousnessChanged.Broadcast(OldLevel, Level);

		if (Level >= EMOConsciousnessLevel::Unconscious && OldLevel < EMOConsciousnessLevel::Unconscious)
		{
			OnLostConsciousness.Broadcast();
		}
		else if (Level < EMOConsciousnessLevel::Unconscious && OldLevel >= EMOConsciousnessLevel::Unconscious)
		{
			OnRegainedConsciousness.Broadcast();
		}
	}

	PreviousConsciousness = Level;
}

bool UMOMentalStateComponent::AttemptWakeUp()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	// Can't wake up if shock is too high
	if (MentalState.ShockAccumulation >= UnconsciousShockThreshold)
	{
		return false;
	}

	// Can't wake up if vital signs are too poor
	if (CachedVitalsComp)
	{
		// Too little blood
		if (CachedVitalsComp->GetBloodLossStage() >= EMOBloodLossStage::Class3)
		{
			return false;
		}

		// Too hypoxic
		if (CachedVitalsComp->Vitals.SpO2 < 70.0f)
		{
			return false;
		}

		// Too hypoglycemic
		if (CachedVitalsComp->Vitals.BloodGlucose < 30.0f)
		{
			return false;
		}
	}

	// Clear forced consciousness
	bConsciousnessForced = false;

	// Recalculate consciousness
	CalculateConsciousnessLevel();

	return MentalState.Consciousness < EMOConsciousnessLevel::Unconscious;
}

EMOConsciousnessLevel UMOMentalStateComponent::GetConsciousnessLevel() const
{
	return MentalState.Consciousness;
}

// ============================================================================
// QUERY API
// ============================================================================

bool UMOMentalStateComponent::CanPerformActions() const
{
	return MentalState.HasMotorControl();
}

float UMOMentalStateComponent::GetEnergyLevel() const
{
	// Energy is inverse of morale fatigue, clamped 0-1
	return FMath::Clamp(1.0f - (MentalState.MoraleFatigue / 100.0f), 0.0f, 1.0f);
}

bool UMOMentalStateComponent::HasFullCapacity() const
{
	return MentalState.Consciousness == EMOConsciousnessLevel::Alert;
}

bool UMOMentalStateComponent::IsUnconscious() const
{
	return MentalState.Consciousness >= EMOConsciousnessLevel::Unconscious;
}

float UMOMentalStateComponent::GetAimPenalty() const
{
	float Penalty = 1.0f;

	// Consciousness affects aim
	switch (MentalState.Consciousness)
	{
	case EMOConsciousnessLevel::Alert:
		Penalty = 1.0f;
		break;
	case EMOConsciousnessLevel::Confused:
		Penalty = 1.5f;
		break;
	case EMOConsciousnessLevel::Drowsy:
		Penalty = 2.0f;
		break;
	default:
		Penalty = 100.0f;  // Can't aim
		break;
	}

	// Aim shake adds to penalty
	Penalty *= (1.0f + MentalState.AimShakeIntensity * 2.0f);

	// Blurred vision adds to penalty
	Penalty *= (1.0f + MentalState.BlurredVisionIntensity * 1.5f);

	return Penalty;
}

float UMOMentalStateComponent::GetMovementPenalty() const
{
	float Multiplier = 1.0f;

	// Consciousness affects movement speed
	switch (MentalState.Consciousness)
	{
	case EMOConsciousnessLevel::Alert:
		Multiplier = 1.0f;
		break;
	case EMOConsciousnessLevel::Confused:
		Multiplier = 0.8f;
		break;
	case EMOConsciousnessLevel::Drowsy:
		Multiplier = 0.5f;
		break;
	default:
		Multiplier = 0.0f;  // Can't move
		break;
	}

	// Stumbling chance reduces effective speed
	Multiplier *= (1.0f - MentalState.StumblingChance * 0.3f);

	return Multiplier;
}

float UMOMentalStateComponent::GetTunnelVisionIntensity() const
{
	return MentalState.TunnelVisionIntensity;
}

float UMOMentalStateComponent::GetBlurredVisionIntensity() const
{
	return MentalState.BlurredVisionIntensity;
}

float UMOMentalStateComponent::GetAimShakeIntensity() const
{
	return MentalState.AimShakeIntensity;
}

float UMOMentalStateComponent::GetStumblingChance() const
{
	return MentalState.StumblingChance;
}

bool UMOMentalStateComponent::RollForStumble()
{
	return FMath::FRand() < MentalState.StumblingChance;
}

// ============================================================================
// PERSISTENCE
// ============================================================================

void UMOMentalStateComponent::BuildSaveData(FMOMentalStateSaveData& OutSaveData) const
{
	OutSaveData.MentalState = MentalState;
}

bool UMOMentalStateComponent::ApplySaveDataAuthority(const FMOMentalStateSaveData& InSaveData)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	MentalState = InSaveData.MentalState;
	PreviousConsciousness = MentalState.Consciousness;
	bConsciousnessForced = false;

	return true;
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

void UMOMentalStateComponent::TickMentalState()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	float ScaledDeltaTime = TickInterval * TimeScaleMultiplier;

	// Update external shock factors (blood loss, etc.)
	UpdateExternalShockFactors();

	// Process recovery
	ProcessShockRecovery(ScaledDeltaTime);
	ProcessStressRecovery(ScaledDeltaTime);

	// Calculate consciousness if not forced
	if (!bConsciousnessForced)
	{
		CalculateConsciousnessLevel();
	}

	// Calculate effects
	CalculateVisualEffects();
	CalculateMotorEffects();

	// Check for consciousness changes
	if (MentalState.Consciousness != PreviousConsciousness)
	{
		OnConsciousnessChanged.Broadcast(PreviousConsciousness, MentalState.Consciousness);

		if (MentalState.Consciousness >= EMOConsciousnessLevel::Unconscious &&
			PreviousConsciousness < EMOConsciousnessLevel::Unconscious)
		{
			OnLostConsciousness.Broadcast();
		}
		else if (MentalState.Consciousness < EMOConsciousnessLevel::Unconscious &&
				 PreviousConsciousness >= EMOConsciousnessLevel::Unconscious)
		{
			OnRegainedConsciousness.Broadcast();
		}

		PreviousConsciousness = MentalState.Consciousness;
	}

	// Broadcast general mental state changed for UI updates
	OnMentalStateChanged.Broadcast();
}

void UMOMentalStateComponent::CalculateConsciousnessLevel()
{
	// Get total shock contribution from all sources
	float TotalShock = GetTotalShockContribution();

	// Determine consciousness level based on shock
	EMOConsciousnessLevel NewLevel;

	if (TotalShock >= ComaShockThreshold)
	{
		NewLevel = EMOConsciousnessLevel::Comatose;
	}
	else if (TotalShock >= UnconsciousShockThreshold)
	{
		NewLevel = EMOConsciousnessLevel::Unconscious;
	}
	else if (TotalShock >= DrowsyShockThreshold)
	{
		NewLevel = EMOConsciousnessLevel::Drowsy;
	}
	else if (TotalShock >= ConfusionShockThreshold)
	{
		NewLevel = EMOConsciousnessLevel::Confused;
	}
	else
	{
		NewLevel = EMOConsciousnessLevel::Alert;
	}

	MentalState.Consciousness = NewLevel;
}

void UMOMentalStateComponent::CalculateVisualEffects()
{
	float TotalShock = GetTotalShockContribution();

	// Tunnel vision from blood loss and shock
	float TunnelVision = 0.0f;

	if (CachedVitalsComp)
	{
		// Blood loss causes tunnel vision
		EMOBloodLossStage Stage = CachedVitalsComp->GetBloodLossStage();
		switch (Stage)
		{
		case EMOBloodLossStage::Class1:
			TunnelVision += 0.1f;
			break;
		case EMOBloodLossStage::Class2:
			TunnelVision += 0.3f;
			break;
		case EMOBloodLossStage::Class3:
			TunnelVision += 0.6f;
			break;
		default:
			break;
		}

		// Low SpO2 causes tunnel vision
		if (CachedVitalsComp->Vitals.SpO2 < 90.0f)
		{
			TunnelVision += (90.0f - CachedVitalsComp->Vitals.SpO2) / 90.0f * 0.5f;
		}
	}

	// Shock adds tunnel vision
	TunnelVision += (TotalShock / 100.0f) * 0.3f;

	MentalState.TunnelVisionIntensity = FMath::Clamp(TunnelVision, 0.0f, 1.0f);

	// Blurred vision from consciousness level and other factors
	float Blur = 0.0f;

	switch (MentalState.Consciousness)
	{
	case EMOConsciousnessLevel::Alert:
		Blur = 0.0f;
		break;
	case EMOConsciousnessLevel::Confused:
		Blur = 0.2f;
		break;
	case EMOConsciousnessLevel::Drowsy:
		Blur = 0.5f;
		break;
	default:
		Blur = 1.0f;
		break;
	}

	// Low blood glucose causes blurred vision
	if (CachedVitalsComp && CachedVitalsComp->Vitals.IsHypoglycemic())
	{
		Blur += (70.0f - CachedVitalsComp->Vitals.BloodGlucose) / 70.0f * 0.3f;
	}

	// Concussion causes blurred vision
	if (CachedAnatomyComp && CachedAnatomyComp->HasCondition(EMOConditionType::Concussion))
	{
		FMOCondition ConcussionCondition;
		if (CachedAnatomyComp->GetConditionByType(EMOConditionType::Concussion, ConcussionCondition))
		{
			Blur += (ConcussionCondition.Severity / 100.0f) * 0.4f;
		}
	}

	MentalState.BlurredVisionIntensity = FMath::Clamp(Blur, 0.0f, 1.0f);
}

void UMOMentalStateComponent::CalculateMotorEffects()
{
	// Aim shake from various factors
	float Shake = 0.0f;

	// Pain causes shaking
	if (CachedVitalsComp)
	{
		float Pain = CachedVitalsComp->Exertion.PainLevel;
		Shake += (Pain / 100.0f) * 0.4f;

		// Fatigue causes shaking
		float Fatigue = CachedVitalsComp->Exertion.Fatigue;
		Shake += (Fatigue / 100.0f) * 0.2f;

		// Low blood sugar causes tremors
		if (CachedVitalsComp->Vitals.BloodGlucose < 70.0f)
		{
			Shake += (70.0f - CachedVitalsComp->Vitals.BloodGlucose) / 70.0f * 0.3f;
		}

		// Cold causes shivering
		if (CachedVitalsComp->Vitals.BodyTemperature < 36.0f)
		{
			Shake += (36.0f - CachedVitalsComp->Vitals.BodyTemperature) / 10.0f * 0.4f;
		}
	}

	// Stress causes shaking
	Shake += (MentalState.TraumaticStress / 100.0f) * 0.3f;

	// Shock causes shaking
	Shake += (MentalState.ShockAccumulation / 100.0f) * 0.2f;

	MentalState.AimShakeIntensity = FMath::Clamp(Shake, 0.0f, 1.0f);

	// Stumbling chance
	float Stumble = 0.0f;

	// Consciousness affects stumbling
	switch (MentalState.Consciousness)
	{
	case EMOConsciousnessLevel::Alert:
		Stumble = 0.0f;
		break;
	case EMOConsciousnessLevel::Confused:
		Stumble = 0.1f;
		break;
	case EMOConsciousnessLevel::Drowsy:
		Stumble = 0.3f;
		break;
	default:
		Stumble = 1.0f;  // Can't walk anyway
		break;
	}

	// Blood loss affects balance
	if (CachedVitalsComp)
	{
		EMOBloodLossStage Stage = CachedVitalsComp->GetBloodLossStage();
		switch (Stage)
		{
		case EMOBloodLossStage::Class2:
			Stumble += 0.1f;
			break;
		case EMOBloodLossStage::Class3:
			Stumble += 0.3f;
			break;
		default:
			break;
		}
	}

	// Leg injuries affect stumbling (check via anatomy component)
	if (CachedAnatomyComp)
	{
		if (!CachedAnatomyComp->CanMove())
		{
			Stumble = 1.0f;  // Can't walk
		}
		else
		{
			// Check for leg injuries that might cause stumbling
			FMOBodyPartState LeftLegState, RightLegState;
			if (CachedAnatomyComp->GetBodyPartState(EMOBodyPartType::ThighLeft, LeftLegState))
			{
				if (LeftLegState.Status == EMOBodyPartStatus::Injured)
				{
					Stumble += 0.2f * (1.0f - LeftLegState.GetHPPercent());
				}
			}
			if (CachedAnatomyComp->GetBodyPartState(EMOBodyPartType::ThighRight, RightLegState))
			{
				if (RightLegState.Status == EMOBodyPartStatus::Injured)
				{
					Stumble += 0.2f * (1.0f - RightLegState.GetHPPercent());
				}
			}
		}
	}

	// Dehydration affects coordination
	if (CachedMetabolismComp && CachedMetabolismComp->IsDehydrated())
	{
		Stumble += (100.0f - CachedMetabolismComp->Nutrients.HydrationLevel) / 100.0f * 0.2f;
	}

	MentalState.StumblingChance = FMath::Clamp(Stumble, 0.0f, 1.0f);
}

void UMOMentalStateComponent::ProcessShockRecovery(float DeltaTime)
{
	if (MentalState.ShockAccumulation <= 0.0f)
	{
		return;
	}

	// Recovery rate is reduced if still taking damage or bleeding
	float RecoveryMod = 1.0f;

	if (CachedAnatomyComp)
	{
		// Active bleeding reduces shock recovery
		float BleedRate = CachedAnatomyComp->GetTotalBleedRate();
		if (BleedRate > 0.0f)
		{
			RecoveryMod *= 0.5f;
		}

		// Active wounds reduce recovery
		if (CachedAnatomyComp->GetAllWounds().Num() > 0)
		{
			RecoveryMod *= 0.8f;
		}
	}

	float Recovery = ShockRecoveryRate * RecoveryMod * DeltaTime;
	MentalState.ShockAccumulation = FMath::Max(0.0f, MentalState.ShockAccumulation - Recovery);
}

void UMOMentalStateComponent::ProcessStressRecovery(float DeltaTime)
{
	// Traumatic stress recovers slowly
	if (MentalState.TraumaticStress > 0.0f)
	{
		float Recovery = StressRecoveryRate * DeltaTime;
		MentalState.TraumaticStress = FMath::Max(0.0f, MentalState.TraumaticStress - Recovery);
	}

	// Morale fatigue recovers very slowly
	if (MentalState.MoraleFatigue > 0.0f)
	{
		float Recovery = StressRecoveryRate * 0.5f * DeltaTime;
		MentalState.MoraleFatigue = FMath::Max(0.0f, MentalState.MoraleFatigue - Recovery);
	}
}

void UMOMentalStateComponent::UpdateExternalShockFactors()
{
	// Add shock from active medical conditions
	if (CachedAnatomyComp)
	{
		// Pain contributes to shock
		float Pain = CachedAnatomyComp->GetTotalPainLevel();
		if (Pain > 50.0f)
		{
			// High pain slowly adds shock
			float ShockFromPain = (Pain - 50.0f) / 100.0f * 0.1f;
			MentalState.ShockAccumulation = FMath::Min(100.0f, MentalState.ShockAccumulation + ShockFromPain);
		}
	}

	// Blood loss contributes to shock
	if (CachedVitalsComp)
	{
		EMOBloodLossStage Stage = CachedVitalsComp->GetBloodLossStage();
		float ShockFromBloodLoss = 0.0f;

		switch (Stage)
		{
		case EMOBloodLossStage::Class1:
			ShockFromBloodLoss = 0.05f;
			break;
		case EMOBloodLossStage::Class2:
			ShockFromBloodLoss = 0.2f;
			break;
		case EMOBloodLossStage::Class3:
			ShockFromBloodLoss = 0.5f;
			break;
		default:
			break;
		}

		if (ShockFromBloodLoss > 0.0f)
		{
			MentalState.ShockAccumulation = FMath::Min(100.0f, MentalState.ShockAccumulation + ShockFromBloodLoss);
		}
	}
}

float UMOMentalStateComponent::GetTotalShockContribution() const
{
	float TotalShock = MentalState.ShockAccumulation;

	// Add contributions from other factors

	// Blood loss directly contributes to consciousness
	if (CachedVitalsComp)
	{
		EMOBloodLossStage Stage = CachedVitalsComp->GetBloodLossStage();
		switch (Stage)
		{
		case EMOBloodLossStage::Class1:
			TotalShock += 10.0f;
			break;
		case EMOBloodLossStage::Class2:
			TotalShock += 30.0f;
			break;
		case EMOBloodLossStage::Class3:
			TotalShock += 60.0f;
			break;
		default:
			break;
		}

		// Hypoxia directly affects consciousness
		if (CachedVitalsComp->Vitals.SpO2 < 90.0f)
		{
			TotalShock += (90.0f - CachedVitalsComp->Vitals.SpO2);
		}

		// Hypoglycemia affects consciousness
		if (CachedVitalsComp->Vitals.BloodGlucose < 50.0f)
		{
			TotalShock += (50.0f - CachedVitalsComp->Vitals.BloodGlucose);
		}

		// Severe hypothermia
		if (CachedVitalsComp->Vitals.BodyTemperature < 32.0f)
		{
			TotalShock += (32.0f - CachedVitalsComp->Vitals.BodyTemperature) * 5.0f;
		}
	}

	// Concussion directly affects consciousness
	if (CachedAnatomyComp)
	{
		FMOCondition ConcussionCondition;
		if (CachedAnatomyComp->GetConditionByType(EMOConditionType::Concussion, ConcussionCondition))
		{
			TotalShock += ConcussionCondition.Severity * 0.5f;
		}
	}

	// Severe dehydration
	if (CachedMetabolismComp)
	{
		if (CachedMetabolismComp->Nutrients.HydrationLevel < 30.0f)
		{
			TotalShock += (30.0f - CachedMetabolismComp->Nutrients.HydrationLevel);
		}
	}

	return FMath::Clamp(TotalShock, 0.0f, 100.0f);
}
