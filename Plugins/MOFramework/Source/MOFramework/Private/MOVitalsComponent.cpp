#include "MOVitalsComponent.h"
#include "MOAnatomyComponent.h"
#include "MOMetabolismComponent.h"
#include "MOMentalStateComponent.h"
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
// TEMPERATURE API
// ============================================================================

void UMOVitalsComponent::ApplyEnvironmentalTemperature(float AmbientTemp, float InsulationFactor)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

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
	Vitals.BodyTemperature += EnvironmentalRate * TickInterval * TimeScaleMultiplier;

	// Body regulation toward normal
	if (Vitals.BodyTemperature > TargetTemp)
	{
		Vitals.BodyTemperature -= FMath::Min(RegulationRate * TickInterval * TimeScaleMultiplier,
											  Vitals.BodyTemperature - TargetTemp);
	}
	else if (Vitals.BodyTemperature < TargetTemp)
	{
		Vitals.BodyTemperature += FMath::Min(RegulationRate * TickInterval * TimeScaleMultiplier,
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
}

bool UMOVitalsComponent::ApplySaveDataAuthority(const FMOVitalsSaveData& InSaveData)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	Vitals = InSaveData.Vitals;
	Exertion = InSaveData.Exertion;

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

	float ScaledDeltaTime = TickInterval * TimeScaleMultiplier;

	// Update pain level from anatomy component
	if (UMOAnatomyComponent* AnatomyComp = CachedAnatomyComp.Get())
	{
		SetPainLevel(AnatomyComp->GetTotalPainLevel());
	}

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

	// Broadcast general vitals changed for UI updates
	OnVitalsChanged.Broadcast();
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
