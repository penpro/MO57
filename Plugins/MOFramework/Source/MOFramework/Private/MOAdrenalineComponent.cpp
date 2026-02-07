#include "MOAdrenalineComponent.h"
#include "MOVitalsComponent.h"
#include "MOAnatomyComponent.h"
#include "MOMentalStateComponent.h"
#include "MOSkillsComponent.h"
#include "MOMedicalProviderInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

// ============================================================================
// CONSTRUCTOR
// ============================================================================

UMOAdrenalineComponent::UMOAdrenalineComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// Initialize with default config
	Config = FMOAdrenalineConfig::GetDefault();
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void UMOAdrenalineComponent::BeginPlay()
{
	Super::BeginPlay();

	RefreshCachedComponents();

	// Start periodic tick
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(
			TickTimerHandle,
			this,
			&UMOAdrenalineComponent::TickAdrenaline,
			TickInterval,
			true
		);
	}
}

void UMOAdrenalineComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(TickTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UMOAdrenalineComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMOAdrenalineComponent, State);
}

// ============================================================================
// COMBAT API
// ============================================================================

void UMOAdrenalineComponent::EnterCombat(const FMOThreatInfo& ThreatInfo)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	bInCombat = true;
	State.TimeSinceCombat = 0.0f;

	// Assess threat and potentially dampen response
	EMOThreatLevel AssessedThreat = AssessThreatLevel(ThreatInfo);
	State.CurrentThreat = AssessedThreat;

	// Calculate spike based on threat
	float SpikeAmount = CalculateCombatEntrySpike(ThreatInfo);

	// Apply skill-based modifiers
	FMOAdrenalineSkillModifiers SkillMods = GetInterpolatedSkillModifiers();
	SpikeAmount *= SkillMods.SpikeRateMultiplier;

	// Cap at skill-determined maximum
	float MaxLevel = SkillMods.MaxAdrenalineLevel;
	State.CurrentLevel = FMath::Clamp(State.CurrentLevel + SpikeAmount, 0.0f, MaxLevel);

	// Transition to spiking phase
	if (State.Phase == EMOAdrenalinePhase::Baseline || State.Phase == EMOAdrenalinePhase::Crashing)
	{
		TransitionToPhase(EMOAdrenalinePhase::Spiking);
	}

	// Track this threat
	ActiveThreats.Add(ThreatInfo);

	// Broadcast
	OnThreatDetected.Broadcast(ThreatInfo.ThreatActor.Get(), AssessedThreat);

	RecalculateEffects();
}

void UMOAdrenalineComponent::ExitCombat()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	bInCombat = false;
	ActiveThreats.Empty();

	// Don't immediately transition - let combat timeout handle decay
}

void UMOAdrenalineComponent::OnWoundReceived(float WoundSeverity, float PainAmount, float BleedRate)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	// Wounds always refresh combat timer
	State.TimeSinceCombat = 0.0f;

	// Calculate wound-based spike
	float SpikeAmount = CalculateWoundSpike(WoundSeverity);

	// Wounds can push past skill threshold if severe enough
	FMOAdrenalineSkillModifiers SkillMods = GetInterpolatedSkillModifiers();

	// Critical wounds override skill dampening
	float MaxLevel = SkillMods.MaxAdrenalineLevel;
	if (WoundSeverity > 50.0f)
	{
		// Severe wounds can push to full adrenaline
		MaxLevel = FMath::Lerp(MaxLevel, 100.0f, (WoundSeverity - 50.0f) / 50.0f);
	}

	State.CurrentLevel = FMath::Clamp(State.CurrentLevel + SpikeAmount, 0.0f, MaxLevel);

	// Track masked pain debt
	float MaskedPain = PainAmount * State.PainMaskingPercent;
	State.MaskedPainDebt += MaskedPain;

	// Track bleed reduction
	State.ActiveBleedReduction = BleedRate * State.BleedReductionPercent;

	// Ensure we're in active phase
	if (State.Phase == EMOAdrenalinePhase::Baseline)
	{
		TransitionToPhase(EMOAdrenalinePhase::Spiking);
	}

	RecalculateEffects();
}

void UMOAdrenalineComponent::RegisterThreat(const FMOThreatInfo& ThreatInfo)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	// Check if this is a new threat
	for (const FMOThreatInfo& Existing : ActiveThreats)
	{
		if (Existing.ThreatActor == ThreatInfo.ThreatActor)
		{
			return; // Already tracking
		}
	}

	EMOThreatLevel AssessedThreat = AssessThreatLevel(ThreatInfo);

	// Only respond if threat exceeds threshold
	float Threshold = GetThreatThreshold();
	float ThreatRatio = ThreatInfo.ThreatPower / FMath::Max(GetCombatSkillLevel() + 10.0f, 1.0f);

	if (ThreatRatio > Threshold)
	{
		// Minor spike for awareness (not full combat)
		float SpikeAmount = Config.CombatEntrySpikeBase * 0.3f;
		FMOAdrenalineSkillModifiers SkillMods = GetInterpolatedSkillModifiers();
		SpikeAmount *= SkillMods.SpikeRateMultiplier;

		State.CurrentLevel = FMath::Clamp(State.CurrentLevel + SpikeAmount, 0.0f, SkillMods.MaxAdrenalineLevel);

		ActiveThreats.Add(ThreatInfo);
		OnThreatDetected.Broadcast(ThreatInfo.ThreatActor.Get(), AssessedThreat);

		RecalculateEffects();
	}
}

void UMOAdrenalineComponent::RefreshCombatTimer()
{
	State.TimeSinceCombat = 0.0f;
}

// ============================================================================
// EFFECT CALCULATIONS
// ============================================================================

float UMOAdrenalineComponent::CalculateEffectiveAccuracy(float BaseAccuracy) const
{
	// Apply accuracy penalty
	float Penalty = State.AccuracyPenalty;

	// Skill reduces penalty
	FMOAdrenalineSkillModifiers SkillMods = GetInterpolatedSkillModifiers();
	Penalty *= (1.0f - SkillMods.AccuracyPenaltyReduction);

	return BaseAccuracy * (1.0f - Penalty);
}

float UMOAdrenalineComponent::CalculateEffectiveBleedRate(float BaseBleedRate) const
{
	return BaseBleedRate * (1.0f - State.BleedReductionPercent);
}

// ============================================================================
// SKILL INTEGRATION
// ============================================================================

float UMOAdrenalineComponent::GetCombatSkillLevel() const
{
	return CachedCombatSkill;
}

float UMOAdrenalineComponent::GetThreatThreshold() const
{
	FMOAdrenalineSkillModifiers SkillMods = GetInterpolatedSkillModifiers();
	return SkillMods.ThreatThreshold;
}

EMOThreatLevel UMOAdrenalineComponent::AssessThreatLevel(const FMOThreatInfo& ThreatInfo) const
{
	float PlayerPower = GetCombatSkillLevel() + 10.0f; // Base 10 + skill
	return ThreatInfo.GetThreatLevel(PlayerPower);
}

FMOAdrenalineSkillModifiers UMOAdrenalineComponent::GetInterpolatedSkillModifiers() const
{
	if (Config.SkillModifiers.Num() == 0)
	{
		// Return defaults
		FMOAdrenalineSkillModifiers Default;
		Default.SkillLevel = 0.0f;
		Default.ThreatThreshold = 0.0f;
		Default.MaxAdrenalineLevel = 100.0f;
		Default.SpikeRateMultiplier = 1.0f;
		Default.DecayRateMultiplier = 1.0f;
		Default.AccuracyPenaltyReduction = 0.0f;
		return Default;
	}

	float Skill = CachedCombatSkill;

	// Find bracketing modifiers
	const FMOAdrenalineSkillModifiers* Lower = nullptr;
	const FMOAdrenalineSkillModifiers* Upper = nullptr;

	for (const FMOAdrenalineSkillModifiers& Mod : Config.SkillModifiers)
	{
		if (Mod.SkillLevel <= Skill)
		{
			if (!Lower || Mod.SkillLevel > Lower->SkillLevel)
			{
				Lower = &Mod;
			}
		}
		if (Mod.SkillLevel >= Skill)
		{
			if (!Upper || Mod.SkillLevel < Upper->SkillLevel)
			{
				Upper = &Mod;
			}
		}
	}

	// Handle edge cases
	if (!Lower && Upper)
	{
		return *Upper;
	}
	if (Lower && !Upper)
	{
		return *Lower;
	}
	if (!Lower && !Upper)
	{
		// Return first modifier if available, otherwise default
		return Config.SkillModifiers.Num() > 0
			? Config.SkillModifiers[0]
			: FMOAdrenalineSkillModifiers();
	}
	if (Lower == Upper)
	{
		return *Lower;
	}

	// Interpolate
	float Range = Upper->SkillLevel - Lower->SkillLevel;
	float Alpha = (Range > 0.0f) ? (Skill - Lower->SkillLevel) / Range : 0.0f;

	FMOAdrenalineSkillModifiers Result;
	Result.SkillLevel = Skill;
	Result.ThreatThreshold = FMath::Lerp(Lower->ThreatThreshold, Upper->ThreatThreshold, Alpha);
	Result.MaxAdrenalineLevel = FMath::Lerp(Lower->MaxAdrenalineLevel, Upper->MaxAdrenalineLevel, Alpha);
	Result.SpikeRateMultiplier = FMath::Lerp(Lower->SpikeRateMultiplier, Upper->SpikeRateMultiplier, Alpha);
	Result.DecayRateMultiplier = FMath::Lerp(Lower->DecayRateMultiplier, Upper->DecayRateMultiplier, Alpha);
	Result.AccuracyPenaltyReduction = FMath::Lerp(Lower->AccuracyPenaltyReduction, Upper->AccuracyPenaltyReduction, Alpha);

	return Result;
}

// ============================================================================
// PERSISTENCE
// ============================================================================

void UMOAdrenalineComponent::BuildSaveData(FMOAdrenalineSaveData& OutSaveData) const
{
	OutSaveData.State = State;
}

bool UMOAdrenalineComponent::ApplySaveData(const FMOAdrenalineSaveData& InSaveData)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	State = InSaveData.State;
	RecalculateEffects();
	return true;
}

// ============================================================================
// INTERNAL - TICK
// ============================================================================

void UMOAdrenalineComponent::TickAdrenaline()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	float DeltaTime = TickInterval * TimeScaleMultiplier;

	// Refresh skill cache periodically
	SkillCacheTimer -= TickInterval;
	if (SkillCacheTimer <= 0.0f)
	{
		RefreshCachedSkill();
		SkillCacheTimer = 5.0f; // Refresh every 5 seconds
	}

	// Update timing
	State.TimeInPhase += DeltaTime;

	if (!bInCombat)
	{
		State.TimeSinceCombat += DeltaTime;
	}

	float OldLevel = State.CurrentLevel;
	EMOAdrenalinePhase OldPhase = State.Phase;

	// Process current phase
	switch (State.Phase)
	{
	case EMOAdrenalinePhase::Baseline:
		// Nothing to do at baseline
		break;

	case EMOAdrenalinePhase::Spiking:
		ProcessSpikingPhase(DeltaTime);
		break;

	case EMOAdrenalinePhase::Sustained:
		ProcessSustainedPhase(DeltaTime);
		break;

	case EMOAdrenalinePhase::Crashing:
		ProcessCrashingPhase(DeltaTime);
		break;
	}

	// Update effects
	RecalculateEffects();

	// Apply modifiers to other components
	ApplyVitalsModifiers();

	// Broadcast changes
	CheckAndBroadcastChanges(OldLevel, OldPhase);
}

void UMOAdrenalineComponent::ProcessSpikingPhase(float DeltaTime)
{
	FMOAdrenalineSkillModifiers SkillMods = GetInterpolatedSkillModifiers();

	// Continue spiking toward max
	float SpikeRate = Config.BaseSpikeRate * SkillMods.SpikeRateMultiplier;
	State.CurrentLevel += SpikeRate * DeltaTime;
	State.CurrentLevel = FMath::Clamp(State.CurrentLevel, 0.0f, SkillMods.MaxAdrenalineLevel);

	// Transition to sustained when at max or combat timeout
	if (State.CurrentLevel >= SkillMods.MaxAdrenalineLevel - 1.0f)
	{
		TransitionToPhase(EMOAdrenalinePhase::Sustained);
	}
	else if (State.TimeSinceCombat > Config.CombatTimeoutBeforeDecay && !bInCombat)
	{
		TransitionToPhase(EMOAdrenalinePhase::Sustained);
	}
}

void UMOAdrenalineComponent::ProcessSustainedPhase(float DeltaTime)
{
	// In sustained phase, maintain level while in combat
	if (bInCombat)
	{
		// Slight decay even in combat (adrenaline fatigue)
		FMOAdrenalineSkillModifiers SkillMods = GetInterpolatedSkillModifiers();
		float FatigueDecay = Config.BaseDecayRate * 0.2f * SkillMods.DecayRateMultiplier;
		State.CurrentLevel -= FatigueDecay * DeltaTime;
		State.CurrentLevel = FMath::Max(State.CurrentLevel, 0.0f);
	}
	else
	{
		// Check for combat timeout to begin crash
		if (State.TimeSinceCombat > Config.CombatTimeoutBeforeDecay)
		{
			TransitionToPhase(EMOAdrenalinePhase::Crashing);
		}
	}
}

void UMOAdrenalineComponent::ProcessCrashingPhase(float DeltaTime)
{
	FMOAdrenalineSkillModifiers SkillMods = GetInterpolatedSkillModifiers();

	// Decay adrenaline
	float DecayRate = Config.BaseDecayRate * SkillMods.DecayRateMultiplier;
	State.CurrentLevel -= DecayRate * DeltaTime;

	// Return masked pain during crash
	if (State.MaskedPainDebt > 0.0f)
	{
		float PainReturn = Config.CrashPainReturnRate * DeltaTime;
		State.MaskedPainDebt -= PainReturn;
		State.MaskedPainDebt = FMath::Max(State.MaskedPainDebt, 0.0f);

		// Apply returned pain to mental state
		if (CachedMentalComp)
		{
			// Pain returns as shock/disorientation
			CachedMentalComp->AddShock(PainReturn * 0.5f);
		}
	}

	// Check for return to baseline
	if (State.CurrentLevel <= 0.0f || State.TimeInPhase > Config.CrashPhaseDuration)
	{
		State.CurrentLevel = 0.0f;
		State.MaskedPainDebt = 0.0f;
		State.ActiveBleedReduction = 0.0f;
		TransitionToPhase(EMOAdrenalinePhase::Baseline);
	}

	// Re-entering combat during crash can spike back up
	if (bInCombat)
	{
		TransitionToPhase(EMOAdrenalinePhase::Spiking);
	}
}

void UMOAdrenalineComponent::TransitionToPhase(EMOAdrenalinePhase NewPhase)
{
	EMOAdrenalinePhase OldPhase = State.Phase;

	if (OldPhase == NewPhase)
	{
		return;
	}

	State.Phase = NewPhase;
	State.TimeInPhase = 0.0f;

	// Fire specific events
	if (NewPhase == EMOAdrenalinePhase::Crashing)
	{
		OnCrashStarted.Broadcast();
	}
	else if (OldPhase == EMOAdrenalinePhase::Crashing && NewPhase == EMOAdrenalinePhase::Baseline)
	{
		OnCrashEnded.Broadcast();
	}

	OnPhaseChanged.Broadcast(OldPhase, NewPhase);
}

// ============================================================================
// INTERNAL - EFFECT CALCULATION
// ============================================================================

void UMOAdrenalineComponent::RecalculateEffects()
{
	float Level = State.CurrentLevel;
	float LevelPercent = Level / 100.0f;

	// Pain masking scales linearly with level
	State.PainMaskingPercent = LevelPercent * Config.MaxPainMasking;

	// Bleed reduction scales linearly
	State.BleedReductionPercent = LevelPercent * Config.MaxBleedReduction;

	// Accuracy penalty kicks in above threshold
	if (Level > Config.AccuracyPenaltyThreshold)
	{
		float PenaltyRange = 100.0f - Config.AccuracyPenaltyThreshold;
		float PenaltyProgress = (PenaltyRange > KINDA_SMALL_NUMBER)
			? (Level - Config.AccuracyPenaltyThreshold) / PenaltyRange
			: 1.0f;
		State.AccuracyPenalty = PenaltyProgress * Config.MaxAccuracyPenalty;
	}
	else
	{
		State.AccuracyPenalty = 0.0f;
	}

	// Tunnel vision kicks in above threshold
	if (Level > Config.TunnelVisionThreshold)
	{
		float VisionRange = 100.0f - Config.TunnelVisionThreshold;
		float VisionProgress = (VisionRange > KINDA_SMALL_NUMBER)
			? (Level - Config.TunnelVisionThreshold) / VisionRange
			: 1.0f;
		State.TunnelVisionIntensity = VisionProgress * Config.MaxTunnelVision;
	}
	else
	{
		State.TunnelVisionIntensity = 0.0f;
	}

	// Heart rate modifier
	State.HeartRateModifier = 1.0f + LevelPercent * (Config.MaxHeartRateMultiplier - 1.0f);

	// Stamina drain modifier
	if (State.Phase == EMOAdrenalinePhase::Crashing)
	{
		// Increased stamina drain during crash
		State.StaminaDrainModifier = Config.CrashStaminaDrainMultiplier;
	}
	else if (State.IsActive())
	{
		// Slight buff during active adrenaline (adrenaline push)
		State.StaminaDrainModifier = FMath::Lerp(1.0f, 0.8f, LevelPercent);
	}
	else
	{
		State.StaminaDrainModifier = 1.0f;
	}
}

void UMOAdrenalineComponent::ApplyVitalsModifiers()
{
	// Apply heart rate modifier to vitals
	// Note: The vitals component should read from us rather than us pushing
	// This is handled by GetHeartRateModifier() being called by vitals tick
}

// ============================================================================
// INTERNAL - HELPERS
// ============================================================================

void UMOAdrenalineComponent::RefreshCachedComponents()
{
	if (AActor* Owner = GetOwner())
	{
		// Use interface for medical components - avoids FindComponentByClass chains
		if (Owner->Implements<UMOMedicalProviderInterface>())
		{
			CachedVitalsComp = IMOMedicalProviderInterface::Execute_GetVitals(Owner);
			CachedAnatomyComp = IMOMedicalProviderInterface::Execute_GetAnatomy(Owner);
			CachedMentalComp = IMOMedicalProviderInterface::Execute_GetMentalState(Owner);
		}
		else
		{
			// Fallback for actors that don't implement the interface
			CachedVitalsComp = Owner->FindComponentByClass<UMOVitalsComponent>();
			CachedAnatomyComp = Owner->FindComponentByClass<UMOAnatomyComponent>();
			CachedMentalComp = Owner->FindComponentByClass<UMOMentalStateComponent>();
		}

		// Skills component not part of medical interface
		CachedSkillsComp = Owner->FindComponentByClass<UMOSkillsComponent>();
	}

	RefreshCachedSkill();
}

void UMOAdrenalineComponent::RefreshCachedSkill()
{
	CachedCombatSkill = 0.0f;

	if (CachedSkillsComp)
	{
		CachedCombatSkill = CachedSkillsComp->GetSkillLevel(Config.CombatSkillId);
	}
}

float UMOAdrenalineComponent::CalculateCombatEntrySpike(const FMOThreatInfo& ThreatInfo) const
{
	float BaseSpike = Config.CombatEntrySpikeBase;

	// Scale by threat level
	EMOThreatLevel ThreatLevel = AssessThreatLevel(ThreatInfo);
	switch (ThreatLevel)
	{
	case EMOThreatLevel::Minimal:
		BaseSpike *= 0.25f;
		break;
	case EMOThreatLevel::Moderate:
		BaseSpike *= 0.5f;
		break;
	case EMOThreatLevel::High:
		BaseSpike *= 1.0f;
		break;
	case EMOThreatLevel::Extreme:
		BaseSpike *= 1.5f;
		break;
	default:
		BaseSpike *= 0.1f;
		break;
	}

	// Active attacks spike more
	if (ThreatInfo.bIsAttacking)
	{
		BaseSpike *= 1.5f;
	}

	// Proximity increases spike
	if (ThreatInfo.Distance < 300.0f)
	{
		BaseSpike *= 1.5f;
	}
	else if (ThreatInfo.Distance < 1000.0f)
	{
		BaseSpike *= 1.2f;
	}

	return BaseSpike;
}

float UMOAdrenalineComponent::CalculateWoundSpike(float Severity) const
{
	float Spike = Severity * Config.WoundSpikePerSeverity;
	return FMath::Clamp(Spike, 0.0f, Config.MaxWoundSpike);
}

void UMOAdrenalineComponent::CheckAndBroadcastChanges(float OldLevel, EMOAdrenalinePhase OldPhase)
{
	// Broadcast level change if significant (>5 points)
	if (FMath::Abs(State.CurrentLevel - OldLevel) > 5.0f)
	{
		OnAdrenalineChanged.Broadcast(OldLevel, State.CurrentLevel);
	}

	// Phase changes are already broadcast in TransitionToPhase
}
