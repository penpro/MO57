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
