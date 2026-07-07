#include "MOVitalsComponent.h"
#include "MOFrameworkMedical.h"
#include "MOAnatomyComponent.h"
#include "MOMetabolismComponent.h"
#include "MOMentalStateComponent.h"
#include "MOAdrenalineComponent.h"           // (H15) heart-rate modifier consumer
#include "MOMedicalProviderInterface.h"      // (H15) resolve adrenaline sibling
#include "MOBodyPartTypes.h"
#include "MOGameClockSubsystem.h"
#include "MOAmbientEnvironmentProvider.h"
#include "MOInsulationSourceInterface.h"
#include "MOWeatherBlueprintLibrary.h"
#include "MOWeatherTypes.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UMOVitalsComponent::UMOVitalsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMOVitalsComponent::BeginPlay()
{
	Super::BeginPlay();

	// Cache sibling components
	if (AActor* Owner = GetOwner())
	{
		CachedAnatomyComp = Owner->FindComponentByClass<UMOAnatomyComponent>();
		CachedMetabolismComp = Owner->FindComponentByClass<UMOMetabolismComponent>();
		CachedMentalComp = Owner->FindComponentByClass<UMOMentalStateComponent>();
	}

	// Start tick timer on authority
	if (GetOwnerRole() == ROLE_Authority)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				TickTimerHandle,
				this,
				&UMOVitalsComponent::TickVitals,
				TickInterval,
				true
			);
		}
	}
}

void UMOVitalsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UMOVitalsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UMOVitalsComponent, Vitals, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMOVitalsComponent, Exertion, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMOVitalsComponent, Activity, COND_OwnerOnly);
}

// ============================================================================
// BLOOD API
// ============================================================================

void UMOVitalsComponent::ApplyBloodLoss(float AmountML)
{
	if (GetOwnerRole() != ROLE_Authority || AmountML <= 0.0f)
	{
		return;
	}

	float OldVolume = Vitals.BloodVolume;
	Vitals.BloodVolume = FMath::Max(0.0f, Vitals.BloodVolume - AmountML);

	CheckAndBroadcastChange(FName("BloodVolume"), OldVolume, Vitals.BloodVolume, 50.0f);

	// Check for blood loss stage change
	EMOBloodLossStage NewStage = GetBloodLossStage();
	if (NewStage != PreviousBloodLossStage)
	{
		OnBloodLossStageChanged.Broadcast(PreviousBloodLossStage, NewStage);
		PreviousBloodLossStage = NewStage;
	}

	// Check for death from blood loss (< 20% blood volume remaining)
	// GetBloodVolumePercent() returns a 0-100 percentage, so the death gate is 20.0,
	// not 0.20 (which fired at 0.2% blood and made bleed-out unreachable). (H11)
	float BloodVolumePercent = GetBloodVolumePercent();
	if (BloodVolumePercent <= 20.0f && !bBloodLossDeathTriggered)
	{
		bBloodLossDeathTriggered = true;
		UE_LOG(LogMOFrameworkMedical, Warning, TEXT("[MOVitals] CRITICAL BLOOD LOSS - triggering death (%.1f%% remaining)"),
			BloodVolumePercent);

		// Trigger death via anatomy component
		if (UMOAnatomyComponent* Anatomy = CachedAnatomyComp.Get())
		{
			// Use None as the body part since this is systemic blood loss, not organ destruction
			Anatomy->OnInstantDeath.Broadcast(EMOBodyPartType::None);
		}
	}
}

void UMOVitalsComponent::ApplyBloodTransfusion(float AmountML)
{
	if (GetOwnerRole() != ROLE_Authority || AmountML <= 0.0f)
	{
		return;
	}

	float OldVolume = Vitals.BloodVolume;
	Vitals.BloodVolume = FMath::Min(Vitals.MaxBloodVolume, Vitals.BloodVolume + AmountML);

	CheckAndBroadcastChange(FName("BloodVolume"), OldVolume, Vitals.BloodVolume, 50.0f);

	// Check for blood loss stage change
	EMOBloodLossStage NewStage = GetBloodLossStage();
	if (NewStage != PreviousBloodLossStage)
	{
		OnBloodLossStageChanged.Broadcast(PreviousBloodLossStage, NewStage);
		PreviousBloodLossStage = NewStage;
	}
}

EMOBloodLossStage UMOVitalsComponent::GetBloodLossStage() const
{
	float LossPercent = Vitals.GetBloodLossPercent() * 100.0f;

	if (LossPercent >= 40.0f)
	{
		return EMOBloodLossStage::Class3;
	}
	else if (LossPercent >= 30.0f)
	{
		return EMOBloodLossStage::Class2;
	}
	else if (LossPercent >= 15.0f)
	{
		return EMOBloodLossStage::Class1;
	}

	return EMOBloodLossStage::None;
}

float UMOVitalsComponent::GetBloodVolumePercent() const
{
	return Vitals.MaxBloodVolume > 0.0f ?
		(Vitals.BloodVolume / Vitals.MaxBloodVolume) * 100.0f : 0.0f;
}

// ============================================================================
// EXERTION API
// ============================================================================

void UMOVitalsComponent::SetExertionLevel(float NewExertion)
{
	Exertion.CurrentExertion = FMath::Clamp(NewExertion, 0.0f, 100.0f);
}

void UMOVitalsComponent::AddStress(float Amount)
{
	Exertion.StressLevel = FMath::Clamp(Exertion.StressLevel + Amount, 0.0f, 100.0f);
}

void UMOVitalsComponent::SetPainLevel(float NewPain)
{
	Exertion.PainLevel = FMath::Clamp(NewPain, 0.0f, 100.0f);
}

void UMOVitalsComponent::AddFatigue(float Amount)
{
	Exertion.Fatigue = FMath::Clamp(Exertion.Fatigue + Amount, 0.0f, 100.0f);
}

// ============================================================================
// ACTIVITY API
// ============================================================================

void UMOVitalsComponent::SetActivityLevel(EMOActivityLevel NewActivity)
{
	if (Activity.CurrentActivity == NewActivity)
	{
		return;
	}

	// Check if we can start this activity
	if (!CanStartActivity(NewActivity))
	{
		// Can't start this activity - stamina too low
		// Downgrade to the highest activity we can sustain
		if (NewActivity == EMOActivityLevel::Sprinting && CanStartActivity(EMOActivityLevel::Jogging))
		{
			NewActivity = EMOActivityLevel::Jogging;
		}
		else if ((NewActivity == EMOActivityLevel::Sprinting || NewActivity == EMOActivityLevel::Jogging)
				 && CanStartActivity(EMOActivityLevel::Walking))
		{
			NewActivity = EMOActivityLevel::Walking;
		}
		else
		{
			NewActivity = EMOActivityLevel::Idle;
		}

		// If still can't do it, just go idle
		if (!CanStartActivity(NewActivity))
		{
			NewActivity = EMOActivityLevel::Idle;
		}
	}

	EMOActivityLevel OldActivity = Activity.CurrentActivity;
	Activity.CurrentActivity = NewActivity;
	Activity.TimeInCurrentActivity = 0.0f;

	// Update exertion level based on new activity
	FMOActivityConfig Config = FMOActivityConfig::GetDefaultConfig(NewActivity);
	SetExertionLevel(Config.ExertionLevel);

	// Broadcast the change
	if (OldActivity != NewActivity)
	{
		OnActivityChanged.Broadcast(OldActivity, NewActivity);
	}
}

FMOActivityConfig UMOVitalsComponent::GetCurrentActivityConfig() const
{
	return FMOActivityConfig::GetDefaultConfig(Activity.CurrentActivity);
}

bool UMOVitalsComponent::CanSustainCurrentActivity() const
{
	FMOActivityConfig Config = GetCurrentActivityConfig();

	if (!Config.bRequiresStamina)
	{
		return true;
	}

	// Can sustain if we have any stamina left
	return Activity.CurrentStamina > 0.0f;
}

bool UMOVitalsComponent::CanStartActivity(EMOActivityLevel ActivityToCheck) const
{
	FMOActivityConfig Config = FMOActivityConfig::GetDefaultConfig(ActivityToCheck);

	if (!Config.bRequiresStamina)
	{
		return true;
	}

	// Check minimum stamina requirement
	return Activity.GetStaminaPercent() >= Config.MinimumStaminaToStart;
}

void UMOVitalsComponent::ModifyStamina(float Amount)
{
	float OldStamina = Activity.CurrentStamina;
	Activity.CurrentStamina = FMath::Clamp(Activity.CurrentStamina + Amount, 0.0f, Activity.MaxStamina);

	// Check for significant change
	if (FMath::Abs(Activity.CurrentStamina - OldStamina) >= 5.0f)
	{
		OnStaminaChanged.Broadcast(OldStamina, Activity.CurrentStamina);
	}

	// Check for depletion
	if (Activity.CurrentStamina <= 0.0f && OldStamina > 0.0f)
	{
		OnStaminaDepleted.Broadcast();
	}
}

float UMOVitalsComponent::GetCurrentCalorieMultiplier() const
{
	FMOActivityConfig Config = GetCurrentActivityConfig();
	float Multiplier = Config.CalorieMultiplier;

	// Fitness affects efficiency - fitter people burn slightly less for same activity
	// But this effect is small (maybe 10% at peak fitness)
	if (UMOMetabolismComponent* MetabComp = CachedMetabolismComp.Get())
	{
		float Fitness = MetabComp->BodyComposition.CardiovascularFitness;
		// At fitness 100, reduce calorie burn by 10% (more efficient)
		// At fitness 0, increase by 10% (less efficient)
		float FitnessModifier = 1.0f - ((Fitness - 50.0f) / 500.0f);  // 0.9 to 1.1
		Multiplier *= FitnessModifier;
	}

	return Multiplier;
}

// ============================================================================
// TEMPERATURE API
// ============================================================================

void UMOVitalsComponent::ApplyEnvironmentalTemperature(float AmbientTemp, float InsulationFactor, float DeltaTimeOverride)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	// Pull the shared TimeScale from the central clock. See
	// UMOGameClockSubsystem — single source of truth across medical components.
	const UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this);
	const float TimeScaleMultiplier = Clock ? Clock->GetTimeScale() : 1.0f;

	// (H12) Seconds of simulation this call represents. Throttled callers pass
	// their real poll interval so drift integrates correctly; a non-positive
	// override falls back to the legacy per-tick assumption.
	const float StepSeconds = (DeltaTimeOverride > 0.0f) ? DeltaTimeOverride : TickInterval;

	// Normal body temp target
	const float TargetTemp = 37.0f;

	// Calculate heat exchange based on insulation
	float ExposureFactor = 1.0f - FMath::Clamp(InsulationFactor, 0.0f, 1.0f);
	float TempDifference = AmbientTemp - Vitals.BodyTemperature;

	// Body naturally regulates temperature
	float RegulationRate = 0.1f;  // degrees per second back toward normal

	// Environmental effect
	float EnvironmentalRate = TempDifference * ExposureFactor * 0.01f;

	// Calculate new temperature
	float OldTemp = Vitals.BodyTemperature;

	// Move toward ambient if exposed
	Vitals.BodyTemperature += EnvironmentalRate * StepSeconds * TimeScaleMultiplier;

	// Body regulation toward normal
	if (Vitals.BodyTemperature > TargetTemp)
	{
		Vitals.BodyTemperature -= FMath::Min(RegulationRate * StepSeconds * TimeScaleMultiplier,
											  Vitals.BodyTemperature - TargetTemp);
	}
	else if (Vitals.BodyTemperature < TargetTemp)
	{
		Vitals.BodyTemperature += FMath::Min(RegulationRate * StepSeconds * TimeScaleMultiplier,
											  TargetTemp - Vitals.BodyTemperature);
	}

	// Clamp to survivable range
	Vitals.BodyTemperature = FMath::Clamp(Vitals.BodyTemperature, 25.0f, 45.0f);

	CheckAndBroadcastChange(FName("BodyTemperature"), OldTemp, Vitals.BodyTemperature, 0.5f);
}

// ============================================================================
// GLUCOSE API
// ============================================================================

void UMOVitalsComponent::ApplyGlucose(float Amount)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	float OldGlucose = Vitals.BloodGlucose;
	Vitals.BloodGlucose = FMath::Clamp(Vitals.BloodGlucose + Amount, 0.0f, 400.0f);

	CheckAndBroadcastChange(FName("BloodGlucose"), OldGlucose, Vitals.BloodGlucose, 10.0f);
}

void UMOVitalsComponent::ConsumeGlucose(float Amount)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	float OldGlucose = Vitals.BloodGlucose;
	Vitals.BloodGlucose = FMath::Max(0.0f, Vitals.BloodGlucose - Amount);

	CheckAndBroadcastChange(FName("BloodGlucose"), OldGlucose, Vitals.BloodGlucose, 10.0f);
}

// ============================================================================
// QUERY API
// ============================================================================

bool UMOVitalsComponent::IsInShock() const
{
	// Shock indicators:
	// - Low blood pressure
	// - High heart rate (compensatory)
	// - Low blood volume
	// - Low SpO2

	if (Vitals.IsHypotensive() && Vitals.IsTachycardic())
	{
		return true;
	}

	if (GetBloodLossStage() >= EMOBloodLossStage::Class2)
	{
		return true;
	}

	return false;
}

bool UMOVitalsComponent::IsCritical() const
{
	// Critical conditions
	if (Vitals.BloodVolume < Vitals.MaxBloodVolume * 0.5f)  // <50% blood
	{
		return true;
	}

	if (Vitals.SpO2 < 80.0f)  // Severe hypoxia
	{
		return true;
	}

	if (Vitals.HeartRate < 30.0f || Vitals.HeartRate > 200.0f)  // Extreme HR
	{
		return true;
	}

	if (Vitals.SystolicBP < 60.0f)  // Severe hypotension
	{
		return true;
	}

	if (Vitals.BodyTemperature < 30.0f || Vitals.BodyTemperature > 42.0f)  // Extreme temp
	{
		return true;
	}

	return false;
}

FString UMOVitalsComponent::GetBloodPressureString() const
{
	return FString::Printf(TEXT("%d/%d"),
		FMath::RoundToInt(Vitals.SystolicBP),
		FMath::RoundToInt(Vitals.DiastolicBP));
}

// ============================================================================
// PERSISTENCE
// ============================================================================

void UMOVitalsComponent::BuildSaveData(FMOVitalsSaveData& OutSaveData) const
{
	OutSaveData.Vitals = Vitals;
	OutSaveData.Exertion = Exertion;
	OutSaveData.Activity = Activity;
}

bool UMOVitalsComponent::ApplySaveDataAuthority(const FMOVitalsSaveData& InSaveData)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	Vitals = InSaveData.Vitals;
	Exertion = InSaveData.Exertion;
	Activity = InSaveData.Activity;

	// Update blood loss stage
	PreviousBloodLossStage = GetBloodLossStage();

	return true;
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

void UMOVitalsComponent::TickVitals()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	// Pull shared TimeScale from the central clock — see UMOGameClockSubsystem.
	const UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this);
	const float TimeScaleMultiplier = Clock ? Clock->GetTimeScale() : 1.0f;
	float ScaledDeltaTime = TickInterval * TimeScaleMultiplier;

	// Update pain level from anatomy component
	if (UMOAnatomyComponent* AnatomyComp = CachedAnatomyComp.Get())
	{
		SetPainLevel(AnatomyComp->GetTotalPainLevel());
	}

	// Process activity effects (stamina, calorie burn, fatigue)
	ProcessActivityEffects(ScaledDeltaTime);

	// Calculate all vital signs
	CalculateHeartRate();
	CalculateBloodPressure();
	CalculateRespiratoryRate();
	CalculateOxygenSaturation();

	// Natural processes
	RegenerateBlood(ScaledDeltaTime);
	ProcessExertionRecovery(ScaledDeltaTime);

	// Check for critical conditions
	CheckCriticalConditions();

	// Consume glucose based on activity
	float GlucoseConsumption = 0.01f;  // Base consumption per second
	GlucoseConsumption += (Exertion.CurrentExertion / 100.0f) * 0.05f;  // Activity adds consumption
	ConsumeGlucose(GlucoseConsumption * ScaledDeltaTime);

	// Thermal-comfort bucket transition — broadcast only on bucket change so
	// HUD widgets re-render at human-perceivable thresholds, not every tick.
	const EMOThermalComfort NewThermalComfort = GetThermalComfort();
	if (NewThermalComfort != LastThermalComfort)
	{
		const EMOThermalComfort OldThermalComfort = LastThermalComfort;
		LastThermalComfort = NewThermalComfort;
		OnThermalComfortChanged.Broadcast(OldThermalComfort, NewThermalComfort);
	}

	// Wetness integrates against the rain pipeline. Cheap accumulator that
	// gates the expensive 9-ray shelter trace to once every WetnessPollInterval
	// (default 2s) — most of the simulation cost is in the trace. Runs BEFORE
	// the environmental-temperature step so CachedOverheadCover is fresh for it.
	WetnessAccumulatorSeconds += ScaledDeltaTime;
	if (WetnessAccumulatorSeconds >= WetnessPollInterval)
	{
		UpdateWetness(WetnessAccumulatorSeconds);
		WetnessAccumulatorSeconds = 0.0f;
	}

	// (H12) Environmental temperature: drive the (previously dead) weather→body
	// exposure pipeline on a throttled cadence. Accumulate real (unscaled by
	// TimeScale) seconds — ApplyEnvironmentalTemperature applies the TimeScale
	// itself, so passing scaled time here would double-count it.
	if (bEnableEnvironmentalTemperature)
	{
		EnvironmentalTempAccumulatorSeconds += TickInterval;
		if (EnvironmentalTempAccumulatorSeconds >= EnvironmentalTempUpdateInterval)
		{
			UpdateEnvironmentalTemperature(EnvironmentalTempAccumulatorSeconds);
			EnvironmentalTempAccumulatorSeconds = 0.0f;
		}
	}

	// Broadcast general vitals changed for UI updates
	OnVitalsChanged.Broadcast();
}

EMOThermalComfort UMOVitalsComponent::GetThermalComfort() const
{
	const float Temp = Vitals.BodyTemperature;
	if (Temp < ThermalThresholds.VeryColdBelow) return EMOThermalComfort::VeryCold;
	if (Temp < ThermalThresholds.ColdBelow)     return EMOThermalComfort::Cold;
	if (Temp >= ThermalThresholds.VeryHotAbove) return EMOThermalComfort::VeryHot;
	if (Temp >= ThermalThresholds.HotAbove)     return EMOThermalComfort::Hot;
	return EMOThermalComfort::Comfortable;
}

UMOAdrenalineComponent* UMOVitalsComponent::GetAdrenalineComponent()
{
	// (H15) Lazy resolve + cache. Mirrors UMOAdrenalineComponent::RefreshCachedComponents:
	// prefer the medical provider interface, fall back to FindComponentByClass.
	if (UMOAdrenalineComponent* Cached = CachedAdrenalineComp.Get())
	{
		return Cached;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	UMOAdrenalineComponent* Resolved = nullptr;
	if (Owner->Implements<UMOMedicalProviderInterface>())
	{
		Resolved = IMOMedicalProviderInterface::Execute_GetAdrenaline(Owner);
	}
	if (!Resolved)
	{
		Resolved = Owner->FindComponentByClass<UMOAdrenalineComponent>();
	}

	CachedAdrenalineComp = Resolved;
	return Resolved;
}

void UMOVitalsComponent::CalculateHeartRate()
{
	float OldHR = Vitals.HeartRate;
	float BaseHR = Vitals.BaseHeartRate;

	// Exertion contribution (+0 to +80 BPM)
	float ExertionMod = (Exertion.CurrentExertion / 100.0f) * 80.0f;

	// Blood loss contribution (compensatory tachycardia)
	float BloodLossMod = 0.0f;
	switch (GetBloodLossStage())
	{
	case EMOBloodLossStage::Class1:
		BloodLossMod = 20.0f;
		break;
	case EMOBloodLossStage::Class2:
		BloodLossMod = 40.0f;
		break;
	case EMOBloodLossStage::Class3:
		BloodLossMod = 60.0f;
		break;
	default:
		break;
	}

	// Pain and stress contribution (+0 to +30 BPM)
	float StressMod = ((Exertion.PainLevel + Exertion.StressLevel) / 200.0f) * 30.0f;

	// Temperature effects
	float TempMod = 0.0f;
	if (Vitals.BodyTemperature > 38.0f)  // Fever
	{
		TempMod = (Vitals.BodyTemperature - 38.0f) * 10.0f;  // +10 BPM per degree
	}
	else if (Vitals.BodyTemperature < 35.0f)  // Hypothermia
	{
		// Initially HR increases, then decreases
		if (Vitals.BodyTemperature > 32.0f)
		{
			TempMod = (35.0f - Vitals.BodyTemperature) * 5.0f;  // Increased
		}
		else
		{
			TempMod = -((32.0f - Vitals.BodyTemperature) * 10.0f);  // Decreased (dangerous)
		}
	}

	// Low glucose can increase HR (adrenaline response)
	float GlucoseMod = 0.0f;
	if (Vitals.BloodGlucose < 70.0f)
	{
		GlucoseMod = (70.0f - Vitals.BloodGlucose) * 0.5f;
	}

	// Cardiovascular fitness effect (better fitness = lower HR)
	float FitnessMod = 0.0f;
	if (UMOMetabolismComponent* MetabComp = CachedMetabolismComp.Get())
	{
		// High fitness reduces resting HR and exertion HR
		float Fitness = MetabComp->BodyComposition.CardiovascularFitness;
		FitnessMod = -((Fitness - 50.0f) / 50.0f) * 10.0f;  // -10 to +10 BPM from fitness
	}

	// Calculate final HR
	Vitals.HeartRate = BaseHR + ExertionMod + BloodLossMod + StressMod + TempMod + GlucoseMod + FitnessMod;

	// (H15) Adrenaline heart-rate modifier. Previously computed in the adrenaline
	// component but never consumed (ApplyVitalsModifiers was empty). It is a
	// multiplier (1.0 = Baseline → no-op at rest, up to ~MaxHeartRateMultiplier
	// during a combat spike). Applied to the summed HR before the physiological
	// clamp so fight-or-flight tachycardia is real but still bounded by the
	// 20–220 BPM clamp below. Single application — see MOAdrenalineComponent.h
	// pitfall #4 (no double-apply); nothing else multiplies HR.
	if (UMOAdrenalineComponent* Adrenaline = GetAdrenalineComponent())
	{
		Vitals.HeartRate *= Adrenaline->GetHeartRateModifier();
	}

	// Clamp to physiological limits
	Vitals.HeartRate = FMath::Clamp(Vitals.HeartRate, 20.0f, 220.0f);

	// Severe blood loss eventually causes bradycardia as heart fails
	if (GetBloodLossStage() == EMOBloodLossStage::Class3 && Vitals.BloodVolume < Vitals.MaxBloodVolume * 0.4f)
	{
		// Heart is struggling
		Vitals.HeartRate = FMath::Max(30.0f, Vitals.HeartRate * 0.7f);
	}

	CheckAndBroadcastChange(FName("HeartRate"), OldHR, Vitals.HeartRate, 5.0f);
}

void UMOVitalsComponent::CalculateBloodPressure()
{
	float OldSystolic = Vitals.SystolicBP;
	float OldDiastolic = Vitals.DiastolicBP;

	// Base blood pressure (assuming healthy baseline)
	float BaseSystolic = 120.0f;
	float BaseDiastolic = 80.0f;

	// Blood volume effect (most critical)
	float VolumeRatio = Vitals.BloodVolume / Vitals.MaxBloodVolume;
	float VolumeMod = 0.0f;

	if (VolumeRatio < 0.85f)  // <15% loss
	{
		VolumeMod = 0.0f;  // Compensated
	}
	else if (VolumeRatio < 0.70f)  // 15-30% loss
	{
		// Pulse pressure narrows (systolic drops more than diastolic)
		VolumeMod = -(0.85f - VolumeRatio) * 100.0f;  // Systolic drops
	}
	else if (VolumeRatio < 0.60f)  // 30-40% loss
	{
		VolumeMod = -40.0f - (0.70f - VolumeRatio) * 200.0f;  // Significant drop
	}
	else  // >40% loss
	{
		VolumeMod = -60.0f - (0.60f - VolumeRatio) * 300.0f;  // Severe drop
	}

	// Exertion increases BP (systolic more than diastolic)
	float ExertionSystolicMod = (Exertion.CurrentExertion / 100.0f) * 40.0f;
	float ExertionDiastolicMod = (Exertion.CurrentExertion / 100.0f) * 10.0f;

	// Stress increases BP
	float StressMod = ((Exertion.StressLevel + Exertion.PainLevel) / 200.0f) * 20.0f;

	// Calculate final BP
	Vitals.SystolicBP = BaseSystolic + VolumeMod + ExertionSystolicMod + StressMod;
	Vitals.DiastolicBP = BaseDiastolic + (VolumeMod * 0.5f) + ExertionDiastolicMod + (StressMod * 0.5f);

	// Clamp to physiological limits
	Vitals.SystolicBP = FMath::Clamp(Vitals.SystolicBP, 40.0f, 220.0f);
	Vitals.DiastolicBP = FMath::Clamp(Vitals.DiastolicBP, 20.0f, 140.0f);

	// Ensure systolic > diastolic
	if (Vitals.SystolicBP <= Vitals.DiastolicBP)
	{
		Vitals.DiastolicBP = Vitals.SystolicBP - 10.0f;
	}

	CheckAndBroadcastChange(FName("SystolicBP"), OldSystolic, Vitals.SystolicBP, 5.0f);
	CheckAndBroadcastChange(FName("DiastolicBP"), OldDiastolic, Vitals.DiastolicBP, 5.0f);
}

void UMOVitalsComponent::CalculateRespiratoryRate()
{
	float OldRR = Vitals.RespiratoryRate;
	float BaseRR = 16.0f;

	// Exertion increases RR significantly
	float ExertionMod = (Exertion.CurrentExertion / 100.0f) * 30.0f;  // Up to 46/min during heavy exercise

	// Low SpO2 increases RR (compensation)
	float SpO2Mod = 0.0f;
	if (Vitals.SpO2 < 95.0f)
	{
		SpO2Mod = (95.0f - Vitals.SpO2) * 0.5f;
	}

	// Blood loss increases RR
	float BloodLossMod = 0.0f;
	switch (GetBloodLossStage())
	{
	case EMOBloodLossStage::Class1:
		BloodLossMod = 4.0f;
		break;
	case EMOBloodLossStage::Class2:
		BloodLossMod = 8.0f;
		break;
	case EMOBloodLossStage::Class3:
		BloodLossMod = 12.0f;
		break;
	default:
		break;
	}

	// Fever increases RR
	float TempMod = 0.0f;
	if (Vitals.BodyTemperature > 38.0f)
	{
		TempMod = (Vitals.BodyTemperature - 38.0f) * 2.0f;
	}

	Vitals.RespiratoryRate = BaseRR + ExertionMod + SpO2Mod + BloodLossMod + TempMod;
	Vitals.RespiratoryRate = FMath::Clamp(Vitals.RespiratoryRate, 4.0f, 60.0f);

	CheckAndBroadcastChange(FName("RespiratoryRate"), OldRR, Vitals.RespiratoryRate, 2.0f);
}

void UMOVitalsComponent::CalculateOxygenSaturation()
{
	float OldSpO2 = Vitals.SpO2;
	float BaseSpO2 = 98.0f;

	// Blood volume affects oxygen capacity
	float VolumeRatio = Vitals.BloodVolume / Vitals.MaxBloodVolume;
	float VolumeMod = 0.0f;
	if (VolumeRatio < 0.7f)
	{
		VolumeMod = -((0.7f - VolumeRatio) * 30.0f);  // Significant drop at low blood volume
	}

	// Lung damage (from anatomy component) - TODO: integrate with anatomy
	float LungMod = 0.0f;
	if (UMOAnatomyComponent* AnatomyComp = CachedAnatomyComp.Get())
	{
		bool bLeftLungFunctional = AnatomyComp->IsBodyPartFunctional(EMOBodyPartType::LungLeft);
		bool bRightLungFunctional = AnatomyComp->IsBodyPartFunctional(EMOBodyPartType::LungRight);

		if (!bLeftLungFunctional && !bRightLungFunctional)
		{
			LungMod = -50.0f;  // Both lungs destroyed - critical
		}
		else if (!bLeftLungFunctional || !bRightLungFunctional)
		{
			LungMod = -15.0f;  // One lung compromised
		}
	}

	// Temperature affects oxygen binding
	float TempMod = 0.0f;
	if (Vitals.BodyTemperature < 35.0f)  // Hypothermia
	{
		// Paradoxically, oxygen binds better to hemoglobin in cold, but tissues use less
		// Net effect: initially stable, then drops
		if (Vitals.BodyTemperature < 32.0f)
		{
			TempMod = -((32.0f - Vitals.BodyTemperature) * 3.0f);
		}
	}

	Vitals.SpO2 = BaseSpO2 + VolumeMod + LungMod + TempMod;
	Vitals.SpO2 = FMath::Clamp(Vitals.SpO2, 0.0f, 100.0f);

	CheckAndBroadcastChange(FName("SpO2"), OldSpO2, Vitals.SpO2, 2.0f);
}

void UMOVitalsComponent::RegenerateBlood(float DeltaTime)
{
	if (Vitals.BloodVolume >= Vitals.MaxBloodVolume)
	{
		return;
	}

	// Blood regeneration (natural recovery)
	// Typical: ~500mL/day = ~0.0058 mL/second
	float RegenPerSecond = BloodRegenerationRate / 86400.0f;  // 86400 seconds in a day

	// Nutrition affects regeneration (need iron, protein)
	float NutritionMod = 1.0f;
	if (UMOMetabolismComponent* MetabComp = CachedMetabolismComp.Get())
	{
		// Iron deficiency slows blood regeneration
		if (MetabComp->Nutrients.Iron < 50.0f)
		{
			NutritionMod *= (MetabComp->Nutrients.Iron / 50.0f);
		}
		// Protein needed for RBC production
		if (MetabComp->Nutrients.ProteinBalance < 0.0f)
		{
			NutritionMod *= 0.5f;
		}
	}

	float Regen = RegenPerSecond * NutritionMod * DeltaTime;
	Vitals.BloodVolume = FMath::Min(Vitals.MaxBloodVolume, Vitals.BloodVolume + Regen);
}

void UMOVitalsComponent::ProcessExertionRecovery(float DeltaTime)
{
	// Exertion recovery rate (faster at rest)
	float ExertionRecovery = 10.0f * DeltaTime;  // Recover 10% per second at rest

	// Cardiovascular fitness improves recovery
	if (UMOMetabolismComponent* MetabComp = CachedMetabolismComp.Get())
	{
		float Fitness = MetabComp->BodyComposition.CardiovascularFitness;
		ExertionRecovery *= (0.5f + (Fitness / 100.0f));  // 0.5x to 1.5x based on fitness
	}

	Exertion.CurrentExertion = FMath::Max(0.0f, Exertion.CurrentExertion - ExertionRecovery);

	// Stress recovery (slower)
	float StressRecovery = 2.0f * DeltaTime;
	Exertion.StressLevel = FMath::Max(0.0f, Exertion.StressLevel - StressRecovery);

	// Fatigue recovery (very slow, mainly through rest/sleep)
	float FatigueRecovery = 0.5f * DeltaTime;
	Exertion.Fatigue = FMath::Max(0.0f, Exertion.Fatigue - FatigueRecovery);
}

void UMOVitalsComponent::CheckCriticalConditions()
{
	// Check for cardiac arrest
	if (Vitals.HeartRate < 10.0f || Vitals.BloodVolume < Vitals.MaxBloodVolume * 0.3f)
	{
		OnCardiacArrest.Broadcast();
	}

	// Check for respiratory failure
	if (Vitals.SpO2 < 50.0f || Vitals.RespiratoryRate < 4.0f)
	{
		OnRespiratoryFailure.Broadcast();
	}
}

void UMOVitalsComponent::CheckAndBroadcastChange(FName VitalName, float OldValue, float NewValue, float Threshold)
{
	if (FMath::Abs(NewValue - OldValue) >= Threshold)
	{
		OnVitalSignChanged.Broadcast(VitalName, OldValue, NewValue);
	}
}

// ============================================================================
// ACTIVITY PROCESSING
// ============================================================================

void UMOVitalsComponent::ProcessActivityEffects(float DeltaTime)
{
	// Update time tracking
	Activity.TimeInCurrentActivity += DeltaTime;

	if (Activity.IsActive())
	{
		Activity.TotalActiveTimeToday += DeltaTime;
	}

	// Update max stamina based on fitness
	UpdateMaxStamina();

	// Process stamina drain/recovery
	UpdateStamina(DeltaTime);

	// Apply calorie burn to metabolism
	ApplyActivityCalorieBurn(DeltaTime);

	// Apply fatigue accumulation
	FMOActivityConfig Config = GetCurrentActivityConfig();
	if (Config.FatiguePerHour > 0.0f)
	{
		// FatiguePerHour is the fatigue gained over 1 hour of sustained activity
		// Convert to per second: FatiguePerHour / 3600
		float FatiguePerSecond = Config.FatiguePerHour / 3600.0f;
		AddFatigue(FatiguePerSecond * DeltaTime);
	}

	// Apply training effects if applicable
	if (Config.bIsCardioTraining || Config.bIsStrengthTraining)
	{
		if (UMOMetabolismComponent* MetabComp = CachedMetabolismComp.Get())
		{
			if (Config.bIsCardioTraining)
			{
				MetabComp->ApplyCardioTraining(Config.TrainingIntensity, DeltaTime);
			}
			if (Config.bIsStrengthTraining)
			{
				MetabComp->ApplyStrengthTraining(Config.TrainingIntensity, DeltaTime);
			}
		}
	}

	// Check if we can no longer sustain current activity
	if (!CanSustainCurrentActivity())
	{
		// Force downgrade to a sustainable activity
		if (Activity.CurrentActivity == EMOActivityLevel::Sprinting)
		{
			SetActivityLevel(EMOActivityLevel::Jogging);
		}
		else if (Activity.CurrentActivity == EMOActivityLevel::Jogging)
		{
			SetActivityLevel(EMOActivityLevel::Walking);
		}
		else if (Activity.CurrentActivity == EMOActivityLevel::HeavyWork)
		{
			SetActivityLevel(EMOActivityLevel::MediumWork);
		}
		else if (Activity.CurrentActivity == EMOActivityLevel::MediumWork)
		{
			SetActivityLevel(EMOActivityLevel::LightWork);
		}
		else if (Activity.CurrentActivity == EMOActivityLevel::Combat)
		{
			// Can't downgrade combat - stay in combat but at reduced effectiveness
			// Combat system should check stamina for action costs
		}
		else
		{
			SetActivityLevel(EMOActivityLevel::Idle);
		}
	}
}

void UMOVitalsComponent::UpdateStamina(float DeltaTime)
{
	FMOActivityConfig Config = GetCurrentActivityConfig();

	float OldStamina = Activity.CurrentStamina;

	if (Config.StaminaDrainPerSecond > 0.0f)
	{
		// Drain stamina based on activity
		float DrainRate = Config.StaminaDrainPerSecond;

		// Fatigue increases stamina drain
		float FatiguePenalty = 1.0f + (Exertion.Fatigue / 100.0f) * 0.5f;  // Up to 50% more drain when fatigued
		DrainRate *= FatiguePenalty;

		// Hydration affects stamina drain
		if (UMOMetabolismComponent* MetabComp = CachedMetabolismComp.Get())
		{
			if (MetabComp->IsDehydrated())
			{
				DrainRate *= 1.5f;  // 50% faster drain when dehydrated
			}
		}

		// (H15) Adrenaline stamina-drain modifier. Multiplier (1.0 = Baseline →
		// no-op at rest): a slight buff (<1.0) during the active adrenaline push,
		// a penalty (>1.0) during the post-combat crash. Previously computed but
		// never consumed (ApplyVitalsModifiers was empty).
		if (UMOAdrenalineComponent* Adrenaline = GetAdrenalineComponent())
		{
			DrainRate *= Adrenaline->GetStaminaDrainModifier();
		}

		Activity.CurrentStamina = FMath::Max(0.0f, Activity.CurrentStamina - DrainRate * DeltaTime);
	}
	else
	{
		// Recover stamina when not draining
		// Base recovery rate: recover full stamina in ~30 seconds at rest
		float RecoveryRate = Activity.MaxStamina / 30.0f;

		// Recovery is slower if fatigued
		float FatigueModifier = 1.0f - (Exertion.Fatigue / 100.0f) * 0.5f;  // Up to 50% slower recovery
		RecoveryRate *= FatigueModifier;

		// Cardiovascular fitness improves recovery
		if (UMOMetabolismComponent* MetabComp = CachedMetabolismComp.Get())
		{
			float Fitness = MetabComp->BodyComposition.CardiovascularFitness;
			RecoveryRate *= (0.7f + (Fitness / 100.0f) * 0.6f);  // 0.7x to 1.3x based on fitness
		}

		// Only recover if resting or idle
		if (Activity.CurrentActivity <= EMOActivityLevel::Walking)
		{
			Activity.CurrentStamina = FMath::Min(Activity.MaxStamina, Activity.CurrentStamina + RecoveryRate * DeltaTime);
		}
	}

	// Check for significant change
	if (FMath::Abs(Activity.CurrentStamina - OldStamina) >= 5.0f)
	{
		OnStaminaChanged.Broadcast(OldStamina, Activity.CurrentStamina);
	}

	// Check for depletion
	if (Activity.CurrentStamina <= 0.0f && OldStamina > 0.0f)
	{
		OnStaminaDepleted.Broadcast();
	}
}

void UMOVitalsComponent::ApplyActivityCalorieBurn(float DeltaTime)
{
	if (!CachedMetabolismComp)
	{
		return;
	}

	// Get base BMR in kcal/day and convert to kcal/second
	float BMR = CachedMetabolismComp->GetCurrentBMR();
	float BMRPerSecond = BMR / 86400.0f;

	// Apply activity multiplier
	float CalorieMultiplier = GetCurrentCalorieMultiplier();

	// Calculate calories burned this tick
	// We subtract 1.0 from multiplier because basal metabolism is already handled
	// We only add the EXTRA calories from activity
	float ExtraCalories = BMRPerSecond * (CalorieMultiplier - 1.0f) * DeltaTime;

	if (ExtraCalories > 0.0f)
	{
		CachedMetabolismComp->ApplyCalorieBurn(ExtraCalories);
	}
}

void UMOVitalsComponent::UpdateMaxStamina()
{
	// Max stamina is based on cardiovascular fitness
	// Base: 100, Range: 70-130 based on fitness
	float BaseMaxStamina = 100.0f;

	if (UMOMetabolismComponent* MetabComp = CachedMetabolismComp.Get())
	{
		float Fitness = MetabComp->BodyComposition.CardiovascularFitness;
		// Fitness 0 = 70 max stamina, Fitness 100 = 130 max stamina
		Activity.MaxStamina = 70.0f + (Fitness / 100.0f) * 60.0f;
	}
	else
	{
		Activity.MaxStamina = BaseMaxStamina;
	}

	// Clamp current stamina to new max
	Activity.CurrentStamina = FMath::Min(Activity.CurrentStamina, Activity.MaxStamina);
}

// ============================================================================
// WETNESS
// ============================================================================

EMOWetnessState UMOVitalsComponent::GetWetnessState() const
{
	return ComputeWetnessState(WetnessLevel);
}

EMOWetnessState UMOVitalsComponent::ComputeWetnessState(float Level) const
{
	if (Level >= WetnessThresholds.SoakedAbove) return EMOWetnessState::Soaked;
	if (Level >= WetnessThresholds.WetAbove)    return EMOWetnessState::Wet;
	if (Level >= WetnessThresholds.DampAbove)   return EMOWetnessState::Damp;
	return EMOWetnessState::Dry;
}

void UMOVitalsComponent::SetWetnessLevel(float NewLevel)
{
	const float Clamped = FMath::Clamp(NewLevel, 0.0f, 1.0f);
	if (FMath::IsNearlyEqual(Clamped, WetnessLevel, 0.001f))
	{
		return;
	}
	WetnessLevel = Clamped;

	const EMOWetnessState NewState = ComputeWetnessState(WetnessLevel);
	if (NewState != LastWetnessState)
	{
		const EMOWetnessState OldState = LastWetnessState;
		LastWetnessState = NewState;
		OnWetnessStateChanged.Broadcast(OldState, NewState);
	}
}

void UMOVitalsComponent::UpdateWetness(float DeltaSeconds)
{
	AActor* Owner = GetOwner();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	// Read the current weather state. Without rain there's nothing wetting
	// the pawn — drive WetnessLevel toward 0.
	// Ambient inputs come through the Core registry (C1) — the medical layer
	// never knows the weather subsystem.
	UMOAmbientEnvironmentRegistry* Ambient = UMOAmbientEnvironmentRegistry::Get(this);
	IMOAmbientEnvironmentProvider* Env = Ambient ? Ambient->GetProvider() : nullptr;
	const float RainIntensity = Env ? Env->GetAmbientRainIntensity01() : 0.0f;

	float NewLevel = WetnessLevel;

	if (RainIntensity > KINDA_SMALL_NUMBER)
	{
		// Cheap overhead shelter test — 9 line traces. WetnessPollInterval
		// (default 2s) caps the trace rate. Pass owner so its own collision
		// doesn't block the rays.
		const FVector Origin = Owner->GetActorLocation();
		const FMOExposureShelter Shelter = UMOWeatherBlueprintLibrary::TestOverheadCover(
			World, Origin, Owner);

		// (H12) Cache the fresh overhead-cover for the environmental-temperature
		// step so it gets a shelter insulation bonus without a second trace.
		CachedOverheadCover = FMath::Clamp(Shelter.OverheadCover, 0.0f, 1.0f);

		// Coverage 0 = fully exposed, 1 = fully covered. Effective rain on
		// the pawn = intensity × (1 − coverage). Damp/Wet/Soaked accumulator
		// then ticks up over DeltaSeconds.
		const float EffectiveRain = RainIntensity * FMath::Max(0.0f, 1.0f - CachedOverheadCover);
		NewLevel += EffectiveRain * WetnessGainPerSecond * DeltaSeconds;
	}
	else
	{
		// No rain — clothing dries.
		NewLevel -= WetnessDecayPerSecond * DeltaSeconds;

		// (H12) No trace fires in clear weather, so the cached overhead-cover
		// would go stale. Decay it toward 0 (treat as "no shelter info") so a
		// pawn that walked out from under a roof loses the phantom bonus. Fast
		// linear decay — fully forgotten after ~10s of clear sky.
		CachedOverheadCover = FMath::Max(0.0f, CachedOverheadCover - 0.1f * DeltaSeconds);
	}

	SetWetnessLevel(FMath::Clamp(NewLevel, 0.0f, 1.0f));
}

// (H12) Throttled weather→body exposure step. This is the single caller that
// finally wires ApplyEnvironmentalTemperature (formerly dead code). Conservative
// by construction: feels-like ambient is gentle, insulation is forgiving, and
// the body temp is hard-clamped 25–45°C inside ApplyEnvironmentalTemperature.
void UMOVitalsComponent::UpdateEnvironmentalTemperature(float DeltaSeconds)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	AActor* Owner = GetOwner();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	// Ambient signal: prefer "feels-like" (folds in wind chill, wet-cold, and
	// heat index) so weather genuinely drives body temp — the survival pillar.
	// With no weather provider the subsystem returns its configured default
	// (20°C), so this gently holds the pawn near comfort rather than doing
	// anything dangerous.
	UMOAmbientEnvironmentRegistry* Ambient = UMOAmbientEnvironmentRegistry::Get(this);
	IMOAmbientEnvironmentProvider* Env = Ambient ? Ambient->GetProvider() : nullptr;
	if (!Env)
	{
		// Per-instance latch (a process-static would mask later PIE sessions).
		if (!bWarnedNoAmbientProvider)
		{
			bWarnedNoAmbientProvider = true;
			UE_LOG(LogMOFrameworkMedical, Warning,
				TEXT("[MOVitals] AMBIENT SEAM: no environment provider (registry=%s) — env temperature drift disabled"),
				Ambient ? TEXT("present") : TEXT("MISSING"));
		}
		return;   // no provider yet — hold at comfort rather than guess
	}
	bWarnedNoAmbientProvider = false;

	const FVector Origin = Owner->GetActorLocation();
	// Point heat sources (campfire, forge) add on top of the global feels-like.
	// The registry aggregates whatever registered above; vitals just feels it.
	const float FeelsLikeC = Env->GetAmbientFeelsLikeCelsius(Origin)
		+ Ambient->GetLocalHeatDeltaAt(Origin);

	// Insulation = tunable clothing default + a cheap shelter bonus (reusing the
	// wetness trace's cached overhead cover — no extra trace). Clamped 0..1.
	// FLAG: clothing warmth is a flat default until equipment exposes a real
	// per-item warmth field (see report). BaseInsulationFactor is deliberately
	// forgiving so an un-tuned game never cooks/freezes the player.
	// Clothing warmth comes from whatever component implements the Core
	// IMOInsulationSource contract (equipment sums equipped Warmth scalars) —
	// vitals consumes the number, it does not own clothing policy.
	float ClothingBonus = 0.0f;
	if (const AActor* InsulOwner = GetOwner())
	{
		for (UActorComponent* Comp : InsulOwner->GetComponents())
		{
			if (Comp && Comp->GetClass()->ImplementsInterface(UMOInsulationSource::StaticClass()))
			{
				if (const IMOInsulationSource* Source = Cast<IMOInsulationSource>(Comp))
				{
					ClothingBonus += Source->GetClothingInsulationBonus01();
				}
			}
		}
	}
	const float Insulation = FMath::Clamp(
		BaseInsulationFactor + ClothingBonus + ShelterInsulationBonus * CachedOverheadCover,
		0.0f, 0.95f);

	// Pass the real elapsed interval so drift integrates correctly despite the
	// throttle (ApplyEnvironmentalTemperature still applies the clock TimeScale).
	ApplyEnvironmentalTemperature(FeelsLikeC, Insulation, DeltaSeconds);
}
