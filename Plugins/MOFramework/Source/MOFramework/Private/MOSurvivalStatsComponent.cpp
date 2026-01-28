#include "MOSurvivalStatsComponent.h"
#include "MOInventoryComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOFramework.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

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
	Stamina.RegenRate = 10.0f;  // Fast stamina regen
	Stamina.DecayRate = 0.0f;

	Hunger.Current = 100.0f;
	Hunger.Max = 100.0f;
	Hunger.RegenRate = 0.0f;
	Hunger.DecayRate = 0.5f;  // Slow hunger decay

	Thirst.Current = 100.0f;
	Thirst.Max = 100.0f;
	Thirst.RegenRate = 0.0f;
	Thirst.DecayRate = 0.8f;  // Faster thirst decay

	Temperature.Current = 37.0f;  // Body temperature in Celsius
	Temperature.Max = 42.0f;
	Temperature.RegenRate = 0.0f;
	Temperature.DecayRate = 0.0f;

	Energy.Current = 100.0f;
	Energy.Max = 100.0f;
	Energy.RegenRate = 0.0f;  // Only recovers by sleeping
	Energy.DecayRate = 0.2f;
}

void UMOSurvivalStatsComponent::BeginPlay()
{
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

	// Remove one from inventory
	InventoryComponent->RemoveItemByGuid(ItemGuid, 1);

	return true;
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

void UMOSurvivalStatsComponent::TickStats()
{
	ProcessStatTick(Health, FName("Health"), TickInterval);
	ProcessStatTick(Stamina, FName("Stamina"), TickInterval);
	ProcessStatTick(Hunger, FName("Hunger"), TickInterval);
	ProcessStatTick(Thirst, FName("Thirst"), TickInterval);
	ProcessStatTick(Temperature, FName("Temperature"), TickInterval);
	ProcessStatTick(Energy, FName("Energy"), TickInterval);

	DecayNutrition(TickInterval);
}

void UMOSurvivalStatsComponent::ProcessStatTick(FMOSurvivalStat& Stat, FName StatName, float DeltaTime)
{
	const float OldValue = Stat.Current;

	// Apply regen
	if (Stat.RegenRate > 0.0f && Stat.Current < Stat.Max)
	{
		Stat.Current = FMath::Min(Stat.Current + (Stat.RegenRate * DeltaTime), Stat.Max);
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
