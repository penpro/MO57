#include "MOSurvivalStatsComponent.h"
#include "MOInventoryComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOMetabolismComponent.h"
#include "MOGameClockSubsystem.h"
#include "MOAnatomyComponent.h"
#include "MOMedicalSubsystem.h"
#include "MOWaterPhysiology.h"
#include "MOFramework.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace MOSurvivalPersistence
{
	const FName StatNames[] = {TEXT("Health"), TEXT("Stamina"), TEXT("Hunger"), TEXT("Thirst"), TEXT("Temperature"), TEXT("Energy")};
}

UMOSurvivalStatsComponent::UMOSurvivalStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// Default stat configurations
	Health.Current = 100.0f;
	Health.Max = 100.0f;
	Health.RegenRate = 1.0f;  // Slow health regen
	Health.DecayRate = 0.0f;

	Stamina.Current = 100.0f;
	Stamina.Max = 100.0f;
	Stamina.RegenRate = 10.0f;  // at rest; the pawn scales it by its work (SetStaminaRecoveryFraction, F52 B)
	Stamina.DecayRate = 0.0f;

	// Pacing (Wes, Oct 15 2026): a character starts fully fed, watered and rested, and needs only start to bother them after about
	// half an hour -- the HUD's warning moodle shows below 50 %. Full -> warning in 30 game minutes for thirst, 45 for hunger, 60 for
	// energy (full -> empty in twice that). Until then thirst decayed 0.8 per REAL second: "Dehydrated" about a minute into a new game.
	// Rates are per GAME second (AdvanceStats takes clock-scaled time).
	Hunger.Current = 100.0f;
	Hunger.Max = 100.0f;
	Hunger.RegenRate = 0.0f;
	Hunger.DecayRate = 100.0f / (90.0f * 60.0f);

	Thirst.Current = 100.0f;
	Thirst.Max = 100.0f;
	Thirst.RegenRate = 0.0f;
	Thirst.DecayRate = 100.0f / (60.0f * 60.0f);

	Temperature.Current = 37.0f;  // Body temperature in Celsius
	Temperature.Max = 42.0f;
	Temperature.RegenRate = 0.0f;
	Temperature.DecayRate = 0.0f;

	Energy.Current = 100.0f;
	Energy.Max = 100.0f;
	Energy.RegenRate = 0.0f;  // Only recovers by sleeping
	Energy.DecayRate = 100.0f / (120.0f * 60.0f);
}

void UMOSurvivalStatsComponent::BeginPlay()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UMOSurvivalStatsComponent_BeginPlay);   // spawn-cost breakdown in traces (-statnamedevents)
	Super::BeginPlay();

	// Only tick on server/authority
	if (GetOwnerRole() == ROLE_Authority)
	{
		GetWorld()->GetTimerManager().SetTimer(
			TickTimerHandle,
			this,
			&UMOSurvivalStatsComponent::TickStats,
			TickInterval,
			true
		);
	}
}

void UMOSurvivalStatsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UMOSurvivalStatsComponent, Health, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMOSurvivalStatsComponent, Stamina, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMOSurvivalStatsComponent, Hunger, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMOSurvivalStatsComponent, Thirst, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMOSurvivalStatsComponent, Temperature, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMOSurvivalStatsComponent, Energy, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMOSurvivalStatsComponent, NutritionStatus, COND_OwnerOnly);
}

void UMOSurvivalStatsComponent::ApplyNutrition(const FMOItemNutrition& Nutrition)
{
	// Apply macronutrients
	NutritionStatus.Calories += Nutrition.Calories;
	NutritionStatus.Hydration += Nutrition.WaterContent;
	NutritionStatus.Protein += Nutrition.Protein;
	NutritionStatus.Carbohydrates += Nutrition.Carbohydrates;
	NutritionStatus.Fat += Nutrition.Fat;

	// Apply vitamins (additive, clamped to reasonable range)
	NutritionStatus.VitaminA = FMath::Clamp(NutritionStatus.VitaminA + Nutrition.VitaminA, 0.0f, 200.0f);
	NutritionStatus.VitaminB = FMath::Clamp(NutritionStatus.VitaminB + Nutrition.VitaminB, 0.0f, 200.0f);
	NutritionStatus.VitaminC = FMath::Clamp(NutritionStatus.VitaminC + Nutrition.VitaminC, 0.0f, 200.0f);
	NutritionStatus.VitaminD = FMath::Clamp(NutritionStatus.VitaminD + Nutrition.VitaminD, 0.0f, 200.0f);

	// Apply minerals
	NutritionStatus.Iron = FMath::Clamp(NutritionStatus.Iron + Nutrition.Iron, 0.0f, 200.0f);
	NutritionStatus.Calcium = FMath::Clamp(NutritionStatus.Calcium + Nutrition.Calcium, 0.0f, 200.0f);
	NutritionStatus.Potassium = FMath::Clamp(NutritionStatus.Potassium + Nutrition.Potassium, 0.0f, 200.0f);
	NutritionStatus.Sodium = FMath::Clamp(NutritionStatus.Sodium + Nutrition.Sodium, 0.0f, 200.0f);

	// Convert nutrition to stat changes
	// Calories reduce hunger
	if (Nutrition.Calories > 0.0f)
	{
		ModifyStat(FName("Hunger"), Nutrition.Calories * 0.1f);  // 100 calories = 10 hunger restored
	}

	// Water content reduces thirst
	if (Nutrition.WaterContent > 0.0f)
	{
		ModifyStat(FName("Thirst"), Nutrition.WaterContent * 0.5f);  // 100ml = 50 thirst restored
	}

	OnNutritionApplied.Broadcast(Nutrition);
}

bool UMOSurvivalStatsComponent::ConsumeItem(UMOInventoryComponent* InventoryComponent, const FGuid& ItemGuid)
{
	if (!IsValid(InventoryComponent))
	{
		return false;
	}

	FMOInventoryEntry FoundEntry;
	if (!InventoryComponent->TryGetEntryByGuid(ItemGuid, FoundEntry))
	{
		return false;
	}

	// Get item definition
	FMOItemDefinitionRow ItemDef;
	if (!UMOItemDatabaseSettings::GetItemDefinition(FoundEntry.ItemDefinitionId, ItemDef))
	{
		return false;
	}

	// Check if item is consumable
	if (!ItemDef.bConsumable)
	{
		return false;
	}

	// Apply nutrition from item
	ApplyNutrition(ItemDef.Nutrition);

	// ... and the BODY eats it: the physiological model digests what the player eats (glycogen, hydration, vitamins -> glucose, blood pressure, mental
	// state). Until Oct 2026 only the counters above heard about a meal -- UMOMetabolismComponent::ConsumeFood had one caller, the colony manager's
	// villagers -- so the player's physiology (and the Status panel, which reads it) ran down however much the player ate and drank.
	// (Which of the two models the HUD's moodles show is an open design decision: Docs/Survival_Single_Source_Proposal.md.)
	if (UMOMetabolismComponent* Metabolism = GetOwner() ? GetOwner()->FindComponentByClass<UMOMetabolismComponent>() : nullptr)
	{
		Metabolism->ConsumeFood(ItemDef.Nutrition, FoundEntry.ItemDefinitionId);
	}

	// Remove one from inventory
	InventoryComponent->RemoveItemByGuid(ItemGuid, 1);

	return true;
}

float UMOSurvivalStatsComponent::IngestWater(float Millilitres, float SalinityGPerL)
{
	if (GetOwnerRole() != ROLE_Authority || Millilitres == 0.0f)
	{
		return 0.0f;
	}
	// A loss (negative volume) carries no salt; a draught pays for its salt in urine.
	const float Net = Millilitres > 0.0f ? FMOWaterPhysiology::NetBodyWaterMl(Millilitres, SalinityGPerL) : Millilitres;
	const float Sodium = Millilitres > 0.0f ? FMOWaterPhysiology::SodiumPercentOfDailyNeed(Millilitres, SalinityGPerL) : 0.0f;

	// The thirst counter feels body water at the rate items always used (100 ml = 50 points); sea water makes you THIRSTIER.
	ModifyStat(FName("Thirst"), Net * 0.5f);
	NutritionStatus.Hydration += Net;
	NutritionStatus.Sodium = FMath::Clamp(NutritionStatus.Sodium + Sodium, 0.0f, 200.0f);
	if (UMOMetabolismComponent* Metabolism = GetOwner() ? GetOwner()->FindComponentByClass<UMOMetabolismComponent>() : nullptr)
	{
		Metabolism->ChangeBodyWater(Net);
		Metabolism->AddSodium(Sodium);
	}
	return Net;
}

void UMOSurvivalStatsComponent::ApplyConditionEffects(float GameSeconds)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}
	const UMOAnatomyComponent* Anatomy = GetOwner() ? GetOwner()->FindComponentByClass<UMOAnatomyComponent>() : nullptr;
	const UMOMedicalSubsystem* Medical = GetWorld() && GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<UMOMedicalSubsystem>() : nullptr;
	if (!Anatomy || !Medical)
	{
		return;
	}
	// What an illness does to the needs while it has symptoms: fluid lost (diarrhoea / vomiting -- the danger of a stomach bug is dehydration) and fatigue. The
	// magnitudes are the condition's data at severity 100, scaled by its severity now.
	float FluidLossMl = 0.0f, EnergyDrain = 0.0f;
	for (const FMOCondition& Condition : Anatomy->GetConditions())
	{
		FMOConditionDefinitionRow Def;
		if (!Condition.IsSymptomatic() || Condition.Severity <= 0.0f || !Medical->GetConditionDefinition(Condition.ConditionType, Def))
		{
			continue;
		}
		const float Scale = Condition.Severity / 100.0f * GameSeconds / 3600.0f;
		FluidLossMl += Def.FluidLossMlPerHourAtFullSeverity * Scale;
		EnergyDrain += Def.EnergyDrainPerHourAtFullSeverity * Scale;
	}
	if (FluidLossMl > 0.0f)
	{
		IngestWater(-FluidLossMl, 0.0f);
	}
	if (EnergyDrain > 0.0f)
	{
		ModifyStat(FName("Energy"), -EnergyDrain);
	}
}

void UMOSurvivalStatsComponent::ModifyStat(FName StatName, float Delta)
{
	FMOSurvivalStat* Stat = GetStatByName(StatName);
	if (!Stat)
	{
		return;
	}

	const float OldValue = Stat->Current;
	Stat->Current = FMath::Clamp(Stat->Current + Delta, 0.0f, Stat->Max);
	const float NewValue = Stat->Current;

	if (!FMath::IsNearlyEqual(OldValue, NewValue))
	{
		OnStatChanged.Broadcast(StatName, OldValue, NewValue);

		if (Stat->IsDepleted() && OldValue > 0.0f)
		{
			OnStatDepleted.Broadcast(StatName);
		}
		else if (Stat->IsCritical(CriticalThreshold) && !FMOSurvivalStat{OldValue, Stat->Max, 0, 0}.IsCritical(CriticalThreshold))
		{
			OnStatCritical.Broadcast(StatName, Stat->GetPercent());
		}
	}
}

void UMOSurvivalStatsComponent::SetStat(FName StatName, float Value)
{
	FMOSurvivalStat* Stat = GetStatByName(StatName);
	if (!Stat)
	{
		return;
	}

	const float OldValue = Stat->Current;
	Stat->Current = FMath::Clamp(Value, 0.0f, Stat->Max);

	if (!FMath::IsNearlyEqual(OldValue, Stat->Current))
	{
		OnStatChanged.Broadcast(StatName, OldValue, Stat->Current);
	}
}

float UMOSurvivalStatsComponent::GetStatCurrent(FName StatName) const
{
	const FMOSurvivalStat* Stat = GetStatByName(StatName);
	return Stat ? Stat->Current : 0.0f;
}

float UMOSurvivalStatsComponent::GetStatPercent(FName StatName) const
{
	const FMOSurvivalStat* Stat = GetStatByName(StatName);
	return Stat ? Stat->GetPercent() : 0.0f;
}

bool UMOSurvivalStatsComponent::IsStatDepleted(FName StatName) const
{
	const FMOSurvivalStat* Stat = GetStatByName(StatName);
	return Stat ? Stat->IsDepleted() : true;
}

bool UMOSurvivalStatsComponent::IsStatCritical(FName StatName) const
{
	const FMOSurvivalStat* Stat = GetStatByName(StatName);
	return Stat ? Stat->IsCritical(CriticalThreshold) : true;
}

void UMOSurvivalStatsComponent::BuildSaveData(FMOSurvivalSaveData& OutData) const
{
	OutData = FMOSurvivalSaveData();
	for (const auto& Name : MOSurvivalPersistence::StatNames) OutData.CurrentValues.Add(Name, GetStatCurrent(Name));
	OutData.Nutrition = NutritionStatus;
	OutData.bHasValidData = true;
}

bool UMOSurvivalStatsComponent::ApplySaveDataAuthority(const FMOSurvivalSaveData& InData)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !InData.bHasValidData || InData.Version != 1) return false;
	struct FChange { FName Name; float Before; float After; };
	TArray<FChange> Changes;
	for (const auto& Name : MOSurvivalPersistence::StatNames)
	{
		const auto* Value = InData.CurrentValues.Find(Name);
		const auto* Stat = GetStatByName(Name);
		if (!Value || !FMath::IsFinite(*Value) || *Value < 0 || !FMath::IsFinite(Stat->Max) || Stat->Max < 0)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvival] Invalid saved meter %s; no survival state restored"), *Name.ToString());
			return false;
		}
		Changes.Add({Name, Stat->Current, FMath::Min(*Value, Stat->Max)});
	}
	for (TFieldIterator<FFloatProperty> Field(FMONutritionStatus::StaticStruct()); Field; ++Field)
	{
		if (!FMath::IsFinite(Field->GetPropertyValue_InContainer(&InData.Nutrition)))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvival] Invalid saved nutrition; no survival state restored"));
			return false;
		}
	}
	// Install the whole snapshot before a UI/save observer sees the first changed meter.
	for (const auto& Change : Changes) GetStatByName(Change.Name)->Current = Change.After;
	NutritionStatus = InData.Nutrition;
	for (const auto& Change : Changes)
		if (!FMath::IsNearlyEqual(Change.Before, Change.After)) OnStatChanged.Broadcast(Change.Name, Change.Before, Change.After);
	return true;
}

void UMOSurvivalStatsComponent::TickStats()
{
	// The body runs on the world clock like everything else in the simulation (TimeScale 1.0 = real time).
	const UWorld* World = GetWorld();
	const UMOGameClockSubsystem* Clock = World ? World->GetSubsystem<UMOGameClockSubsystem>() : nullptr;
	AdvanceStats(Clock ? Clock->GetScaledDeltaTime(TickInterval) : TickInterval);
}

void UMOSurvivalStatsComponent::AdvanceStats(float GameSeconds)
{
	if (GameSeconds <= 0.0f)
	{
		return;
	}
	ProcessStatTick(Health, FName("Health"), GameSeconds);
	ProcessStatTick(Stamina, FName("Stamina"), GameSeconds, StaminaRecoveryFraction);
	ProcessStatTick(Hunger, FName("Hunger"), GameSeconds);
	ProcessStatTick(Thirst, FName("Thirst"), GameSeconds);
	ProcessStatTick(Temperature, FName("Temperature"), GameSeconds);
	ProcessStatTick(Energy, FName("Energy"), GameSeconds);

	DecayNutrition(GameSeconds);
	ApplyConditionEffects(GameSeconds);
}

void UMOSurvivalStatsComponent::ProcessStatTick(FMOSurvivalStat& Stat, FName StatName, float DeltaTime, float RegenFraction)
{
	const float OldValue = Stat.Current;

	// Apply regen
	if (Stat.RegenRate > 0.0f && RegenFraction > 0.0f && Stat.Current < Stat.Max)
	{
		Stat.Current = FMath::Min(Stat.Current + (Stat.RegenRate * RegenFraction * DeltaTime), Stat.Max);
	}

	// Apply decay
	if (Stat.DecayRate > 0.0f && Stat.Current > 0.0f)
	{
		Stat.Current = FMath::Max(Stat.Current - (Stat.DecayRate * DeltaTime), 0.0f);
	}

	// Broadcast changes
	if (!FMath::IsNearlyEqual(OldValue, Stat.Current, 0.01f))
	{
		OnStatChanged.Broadcast(StatName, OldValue, Stat.Current);

		if (Stat.IsDepleted() && OldValue > 0.0f)
		{
			OnStatDepleted.Broadcast(StatName);
		}
		else if (Stat.IsCritical(CriticalThreshold))
		{
			// Check if we just entered critical
			FMOSurvivalStat OldStat;
			OldStat.Current = OldValue;
			OldStat.Max = Stat.Max;
			if (!OldStat.IsCritical(CriticalThreshold))
			{
				OnStatCritical.Broadcast(StatName, Stat.GetPercent());
			}
		}
	}
}

void UMOSurvivalStatsComponent::DecayNutrition(float DeltaTime)
{
	const float Decay = NutritionDecayRate * DeltaTime;

	// Decay vitamins and minerals toward baseline (50%)
	auto DecayToward = [Decay](float& Value, float Target)
	{
		if (Value > Target)
		{
			Value = FMath::Max(Value - Decay, Target);
		}
		else if (Value < Target)
		{
			Value = FMath::Min(Value + Decay * 0.5f, Target);  // Slower recovery
		}
	};

	DecayToward(NutritionStatus.VitaminA, 50.0f);
	DecayToward(NutritionStatus.VitaminB, 50.0f);
	DecayToward(NutritionStatus.VitaminC, 50.0f);
	DecayToward(NutritionStatus.VitaminD, 50.0f);
	DecayToward(NutritionStatus.Iron, 50.0f);
	DecayToward(NutritionStatus.Calcium, 50.0f);
	DecayToward(NutritionStatus.Potassium, 50.0f);
	DecayToward(NutritionStatus.Sodium, 50.0f);
}

FMOSurvivalStat* UMOSurvivalStatsComponent::GetStatByName(FName StatName)
{
	if (StatName == FName("Health")) return &Health;
	if (StatName == FName("Stamina")) return &Stamina;
	if (StatName == FName("Hunger")) return &Hunger;
	if (StatName == FName("Thirst")) return &Thirst;
	if (StatName == FName("Temperature")) return &Temperature;
	if (StatName == FName("Energy")) return &Energy;
	return nullptr;
}

const FMOSurvivalStat* UMOSurvivalStatsComponent::GetStatByName(FName StatName) const
{
	if (StatName == FName("Health")) return &Health;
	if (StatName == FName("Stamina")) return &Stamina;
	if (StatName == FName("Hunger")) return &Hunger;
	if (StatName == FName("Thirst")) return &Thirst;
	if (StatName == FName("Temperature")) return &Temperature;
	if (StatName == FName("Energy")) return &Energy;
	return nullptr;
}
