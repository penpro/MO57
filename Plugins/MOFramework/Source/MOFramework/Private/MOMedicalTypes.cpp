#include "MOMedicalTypes.h"
#include "MOAnatomyComponent.h"
#include "MOMetabolismComponent.h"

// ============================================================================
// FMOWoundList Implementation
// ============================================================================

void FMOWoundList::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize)
{
	if (UMOAnatomyComponent* Owner = OwnerComponent.Get())
	{
		for (int32 Index : AddedIndices)
		{
			if (Wounds.IsValidIndex(Index))
			{
				Owner->OnWoundReplicatedAdd(Wounds[Index]);
			}
		}
	}
}

void FMOWoundList::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
	if (UMOAnatomyComponent* Owner = OwnerComponent.Get())
	{
		for (int32 Index : ChangedIndices)
		{
			if (Wounds.IsValidIndex(Index))
			{
				Owner->OnWoundReplicatedChange(Wounds[Index]);
			}
		}
	}
}

void FMOWoundList::PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize)
{
	if (UMOAnatomyComponent* Owner = OwnerComponent.Get())
	{
		for (int32 Index : RemovedIndices)
		{
			if (Wounds.IsValidIndex(Index))
			{
				Owner->OnWoundReplicatedRemove(Wounds[Index]);
			}
		}
	}
}

FMOWound* FMOWoundList::FindWoundById(const FGuid& WoundId)
{
	return Wounds.FindByPredicate([&WoundId](const FMOWound& Wound)
	{
		return Wound.WoundId == WoundId;
	});
}

const FMOWound* FMOWoundList::FindWoundById(const FGuid& WoundId) const
{
	return Wounds.FindByPredicate([&WoundId](const FMOWound& Wound)
	{
		return Wound.WoundId == WoundId;
	});
}

void FMOWoundList::AddWound(const FMOWound& NewWound)
{
	Wounds.Add(NewWound);
	MarkItemDirty(Wounds.Last());
}

bool FMOWoundList::RemoveWound(const FGuid& WoundId)
{
	int32 Index = Wounds.IndexOfByPredicate([&WoundId](const FMOWound& Wound)
	{
		return Wound.WoundId == WoundId;
	});

	if (Index != INDEX_NONE)
	{
		Wounds.RemoveAt(Index);
		MarkArrayDirty();
		return true;
	}
	return false;
}

// ============================================================================
// FMOConditionList Implementation
// ============================================================================

void FMOConditionList::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize)
{
	if (UMOAnatomyComponent* Owner = OwnerComponent.Get())
	{
		for (int32 Index : AddedIndices)
		{
			if (Conditions.IsValidIndex(Index))
			{
				Owner->OnConditionReplicatedAdd(Conditions[Index]);
			}
		}
	}
}

void FMOConditionList::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
	if (UMOAnatomyComponent* Owner = OwnerComponent.Get())
	{
		for (int32 Index : ChangedIndices)
		{
			if (Conditions.IsValidIndex(Index))
			{
				Owner->OnConditionReplicatedChange(Conditions[Index]);
			}
		}
	}
}

void FMOConditionList::PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize)
{
	if (UMOAnatomyComponent* Owner = OwnerComponent.Get())
	{
		for (int32 Index : RemovedIndices)
		{
			if (Conditions.IsValidIndex(Index))
			{
				Owner->OnConditionReplicatedRemove(Conditions[Index]);
			}
		}
	}
}

FMOCondition* FMOConditionList::FindConditionById(const FGuid& ConditionId)
{
	return Conditions.FindByPredicate([&ConditionId](const FMOCondition& Condition)
	{
		return Condition.ConditionId == ConditionId;
	});
}

const FMOCondition* FMOConditionList::FindConditionById(const FGuid& ConditionId) const
{
	return Conditions.FindByPredicate([&ConditionId](const FMOCondition& Condition)
	{
		return Condition.ConditionId == ConditionId;
	});
}

FMOCondition* FMOConditionList::FindConditionByType(EMOConditionType Type)
{
	return Conditions.FindByPredicate([Type](const FMOCondition& Condition)
	{
		return Condition.ConditionType == Type;
	});
}

void FMOConditionList::AddCondition(const FMOCondition& NewCondition)
{
	Conditions.Add(NewCondition);
	MarkItemDirty(Conditions.Last());
}

bool FMOConditionList::RemoveCondition(const FGuid& ConditionId)
{
	int32 Index = Conditions.IndexOfByPredicate([&ConditionId](const FMOCondition& Condition)
	{
		return Condition.ConditionId == ConditionId;
	});

	if (Index != INDEX_NONE)
	{
		Conditions.RemoveAt(Index);
		MarkArrayDirty();
		return true;
	}
	return false;
}

// ============================================================================
// FMODigestingFoodList Implementation
// ============================================================================

void FMODigestingFoodList::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize)
{
	// Optional: notify owner of new digesting food
}

void FMODigestingFoodList::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
	// Optional: notify owner of digestion progress changes
}

void FMODigestingFoodList::PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize)
{
	// Optional: notify owner of completed digestion
}

void FMODigestingFoodList::AddFood(const FMODigestingFood& NewFood)
{
	Items.Add(NewFood);
	MarkItemDirty(Items.Last());
}

void FMODigestingFoodList::RemoveCompletedItems()
{
	bool bRemoved = false;
	for (int32 i = Items.Num() - 1; i >= 0; --i)
	{
		if (Items[i].IsDigestionComplete())
		{
			Items.RemoveAt(i);
			bRemoved = true;
		}
	}

	if (bRemoved)
	{
		MarkArrayDirty();
	}
}

// ============================================================================
// FMOActivityConfig Implementation
// ============================================================================

/**
 * Default activity configurations based on physiological data:
 *
 * Calorie Multipliers (MET values simplified):
 *   Resting: 0.9x BMR (sleeping metabolism is slightly lower)
 *   Idle: 1.0x BMR
 *   Walking: 2.5-3.5x BMR (~3-4 METs)
 *   Jogging: 7-8x BMR (~7-8 METs)
 *   Sprinting: 12-15x BMR (~12-15 METs)
 *   Light Work: 2-3x BMR
 *   Medium Work: 4-6x BMR
 *   Heavy Work: 8-10x BMR
 *   Combat: 10-15x BMR (high intensity interval)
 *
 * Stamina drain is game-balanced, not purely realistic.
 */
FMOActivityConfig FMOActivityConfig::GetDefaultConfig(EMOActivityLevel Level)
{
	FMOActivityConfig Config;

	switch (Level)
	{
	case EMOActivityLevel::Resting:
		Config.CalorieMultiplier = 0.9f;
		Config.ExertionLevel = 0.0f;
		Config.StaminaDrainPerSecond = 0.0f;
		Config.FatiguePerHour = 0.0f;
		Config.bIsCardioTraining = false;
		Config.bIsStrengthTraining = false;
		Config.TrainingIntensity = 0.0f;
		Config.bRequiresStamina = false;
		Config.MinimumStaminaToStart = 0.0f;
		break;

	case EMOActivityLevel::Idle:
		Config.CalorieMultiplier = 1.0f;
		Config.ExertionLevel = 0.0f;
		Config.StaminaDrainPerSecond = 0.0f;
		Config.FatiguePerHour = 0.0f;
		Config.bIsCardioTraining = false;
		Config.bIsStrengthTraining = false;
		Config.TrainingIntensity = 0.0f;
		Config.bRequiresStamina = false;
		Config.MinimumStaminaToStart = 0.0f;
		break;

	case EMOActivityLevel::Walking:
		Config.CalorieMultiplier = 3.0f;
		Config.ExertionLevel = 15.0f;
		Config.StaminaDrainPerSecond = 0.0f;  // Walking doesn't drain stamina
		Config.FatiguePerHour = 2.0f;
		Config.bIsCardioTraining = false;  // Too low intensity
		Config.bIsStrengthTraining = false;
		Config.TrainingIntensity = 0.0f;
		Config.bRequiresStamina = false;
		Config.MinimumStaminaToStart = 0.0f;
		break;

	case EMOActivityLevel::Jogging:
		Config.CalorieMultiplier = 7.5f;
		Config.ExertionLevel = 50.0f;
		Config.StaminaDrainPerSecond = 2.0f;  // Moderate stamina drain
		Config.FatiguePerHour = 8.0f;
		Config.bIsCardioTraining = true;
		Config.bIsStrengthTraining = false;
		Config.TrainingIntensity = 0.5f;
		Config.bRequiresStamina = true;
		Config.MinimumStaminaToStart = 0.1f;
		break;

	case EMOActivityLevel::Sprinting:
		Config.CalorieMultiplier = 14.0f;
		Config.ExertionLevel = 95.0f;
		Config.StaminaDrainPerSecond = 10.0f;  // High stamina drain
		Config.FatiguePerHour = 25.0f;
		Config.bIsCardioTraining = true;
		Config.bIsStrengthTraining = false;
		Config.TrainingIntensity = 0.9f;
		Config.bRequiresStamina = true;
		Config.MinimumStaminaToStart = 0.2f;
		break;

	case EMOActivityLevel::LightWork:
		Config.CalorieMultiplier = 2.5f;
		Config.ExertionLevel = 20.0f;
		Config.StaminaDrainPerSecond = 0.5f;
		Config.FatiguePerHour = 3.0f;
		Config.bIsCardioTraining = false;
		Config.bIsStrengthTraining = false;
		Config.TrainingIntensity = 0.0f;
		Config.bRequiresStamina = false;
		Config.MinimumStaminaToStart = 0.0f;
		break;

	case EMOActivityLevel::MediumWork:
		Config.CalorieMultiplier = 5.0f;
		Config.ExertionLevel = 45.0f;
		Config.StaminaDrainPerSecond = 1.5f;
		Config.FatiguePerHour = 6.0f;
		Config.bIsCardioTraining = false;
		Config.bIsStrengthTraining = true;
		Config.TrainingIntensity = 0.3f;
		Config.bRequiresStamina = true;
		Config.MinimumStaminaToStart = 0.05f;
		break;

	case EMOActivityLevel::HeavyWork:
		Config.CalorieMultiplier = 9.0f;
		Config.ExertionLevel = 75.0f;
		Config.StaminaDrainPerSecond = 4.0f;
		Config.FatiguePerHour = 12.0f;
		Config.bIsCardioTraining = false;
		Config.bIsStrengthTraining = true;
		Config.TrainingIntensity = 0.6f;
		Config.bRequiresStamina = true;
		Config.MinimumStaminaToStart = 0.15f;
		break;

	case EMOActivityLevel::Combat:
		Config.CalorieMultiplier = 12.0f;
		Config.ExertionLevel = 90.0f;
		Config.StaminaDrainPerSecond = 8.0f;
		Config.FatiguePerHour = 20.0f;
		Config.bIsCardioTraining = true;
		Config.bIsStrengthTraining = true;
		Config.TrainingIntensity = 0.8f;
		Config.bRequiresStamina = true;
		Config.MinimumStaminaToStart = 0.1f;
		break;

	default:
		// Default to idle
		Config.CalorieMultiplier = 1.0f;
		Config.ExertionLevel = 0.0f;
		break;
	}

	return Config;
}
