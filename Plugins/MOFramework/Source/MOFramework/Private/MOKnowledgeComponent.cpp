#include "MOKnowledgeComponent.h"
#include "MOSkillsComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOFramework.h"
#include "Net/UnrealNetwork.h"

UMOKnowledgeComponent::UMOKnowledgeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMOKnowledgeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UMOKnowledgeComponent, ItemKnowledge, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMOKnowledgeComponent, AllLearnedKnowledge, COND_OwnerOnly);
}

FMOItemKnowledgeProgress* UMOKnowledgeComponent::FindItemKnowledge(FName ItemDefinitionId)
{
	for (FMOItemKnowledgeProgress& Progress : ItemKnowledge)
	{
		if (Progress.ItemDefinitionId == ItemDefinitionId)
		{
			return &Progress;
		}
	}
	return nullptr;
}

const FMOItemKnowledgeProgress* UMOKnowledgeComponent::FindItemKnowledge(FName ItemDefinitionId) const
{
	for (const FMOItemKnowledgeProgress& Progress : ItemKnowledge)
	{
		if (Progress.ItemDefinitionId == ItemDefinitionId)
		{
			return &Progress;
		}
	}
	return nullptr;
}

FMOInspectionResult UMOKnowledgeComponent::InspectItem(FName ItemDefinitionId, UMOSkillsComponent* SkillsComponent)
{
	FMOInspectionResult Result;

	if (ItemDefinitionId.IsNone())
	{
		return Result;
	}

	// Get item definition for inspection data
	FMOItemDefinitionRow ItemDef;
	if (!UMOItemDatabaseSettings::GetItemDefinition(ItemDefinitionId, ItemDef))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOKnowledgeComponent] InspectItem: Item '%s' not found in database"),
			*ItemDefinitionId.ToString());
		return Result;
	}

	Result.bSuccess = true;

	// Get or create inspection progress
	FMOItemKnowledgeProgress* ItemProgress = FindItemKnowledge(ItemDefinitionId);
	if (!ItemProgress)
	{
		FMOItemKnowledgeProgress NewProgress;
		NewProgress.ItemDefinitionId = ItemDefinitionId;
		ItemKnowledge.Add(NewProgress);
		ItemProgress = &ItemKnowledge.Last();
	}

	Result.bFirstInspection = (ItemProgress->InspectionCount == 0);
	ItemProgress->InspectionCount++;

	UE_LOG(LogMOFramework, Log, TEXT("[MOKnowledge] InspectItem '%s': InspectionCount=%d, Grants=%d"),
		*ItemDefinitionId.ToString(),
		ItemProgress->InspectionCount,
		ItemDef.Inspection.Grants.Num());

	// Process each grant entry (both skills and knowledge)
	if (IsValid(SkillsComponent))
	{
		for (const FMOInspectionGrant& Grant : ItemDef.Inspection.Grants)
		{
			if (Grant.Id.IsNone() || Grant.XPAmount <= 0.0f)
			{
				continue;
			}

			// Get current level for this entry
			const int32 CurrentLevel = SkillsComponent->GetSkillLevel(Grant.Id);

			// Check if at max level for this item
			const bool bAtMaxLevel = (Grant.MaxLevel > 0 && CurrentLevel >= Grant.MaxLevel);

			UE_LOG(LogMOFramework, Log, TEXT("[MOKnowledge]   Grant: Id='%s', bIsKnowledge=%s, XPAmount=%.1f, MaxLevel=%d, CurrentLevel=%d, bAtMaxLevel=%s"),
				*Grant.Id.ToString(),
				Grant.bIsKnowledge ? TEXT("true") : TEXT("false"),
				Grant.XPAmount,
				Grant.MaxLevel,
				CurrentLevel,
				bAtMaxLevel ? TEXT("true") : TEXT("false"));

			if (!bAtMaxLevel)
			{
				// Grant XP (SkillsComponent handles both skills and knowledge)
				SkillsComponent->AddExperience(Grant.Id, Grant.XPAmount);

				// Get the new level after XP grant
				const int32 NewLevel = SkillsComponent->GetSkillLevel(Grant.Id);

				// Record this grant for notification cycling
				FMOInspectionXPGrant XPGrant;
				XPGrant.Id = Grant.Id;
				XPGrant.bIsKnowledge = Grant.bIsKnowledge;
				XPGrant.XPAmount = Grant.XPAmount;
				XPGrant.LevelBefore = CurrentLevel;
				XPGrant.LevelAfter = NewLevel;
				XPGrant.bLeveledUp = (NewLevel > CurrentLevel);
				Result.XPGrants.Add(XPGrant);

				// Track knowledge in AllLearnedKnowledge for quick lookups (if it's knowledge and level > 0)
				if (Grant.bIsKnowledge && NewLevel > 0 && !AllLearnedKnowledge.Contains(Grant.Id))
				{
					AllLearnedKnowledge.Add(Grant.Id);
					ItemProgress->UnlockedKnowledge.AddUnique(Grant.Id);
					OnKnowledgeLearned.Broadcast(Grant.Id, ItemDefinitionId);

					UE_LOG(LogMOFramework, Log, TEXT("[MOKnowledgeComponent] Learned knowledge '%s' from inspecting '%s'"),
						*Grant.Id.ToString(), *ItemDefinitionId.ToString());
				}

				UE_LOG(LogMOFramework, Log, TEXT("[MOKnowledge]     -> XP granted, level %d -> %d"),
					CurrentLevel, NewLevel);
			}
		}
	}

	// Determine learning potential and set feedback message
	Result.LearningPotential = GetLearningPotential(ItemDefinitionId, SkillsComponent);

	if (Result.LearningPotential == EMOLearningPotential::None)
	{
		// Only show "nothing more" if we actually tried to learn something this inspection
		// (i.e., there were grants defined but all were already maxed)
		Result.bNothingMoreToLearn = true;
		if (Result.XPGrants.Num() == 0)
		{
			// No XP was granted this inspection because everything was already maxed
			Result.FeedbackMessage = NSLOCTEXT("MO", "InspectNothingMore", "There's nothing more you can learn from this item.");
		}
		else
		{
			// XP was granted but now everything is maxed (just hit the cap this inspection)
			Result.FeedbackMessage = NSLOCTEXT("MO", "InspectMasteredItem", "You've learned everything this item can teach you.");
		}
	}
	else if (Result.LearningPotential == EMOLearningPotential::Partial)
	{
		// Some grants are maxed, some aren't - only show message if nothing was learned this time
		if (Result.XPGrants.Num() == 0)
		{
			Result.FeedbackMessage = NSLOCTEXT("MO", "InspectMoreToLearn", "Some skills can still be improved by inspecting this item.");
		}
		else
		{
			// XP was granted, the popups will inform the player - no need for extra message
			Result.FeedbackMessage = FText::GetEmpty();
		}
	}
	else
	{
		// Full potential - no message needed, popups show the progress
		Result.FeedbackMessage = FText::GetEmpty();
	}

	OnItemInspected.Broadcast(ItemDefinitionId, Result);

	return Result;
}

bool UMOKnowledgeComponent::HasKnowledge(FName KnowledgeId) const
{
	return AllLearnedKnowledge.Contains(KnowledgeId);
}

bool UMOKnowledgeComponent::HasAllKnowledge(const TArray<FName>& KnowledgeIds) const
{
	for (const FName& KnowledgeId : KnowledgeIds)
	{
		if (!AllLearnedKnowledge.Contains(KnowledgeId))
		{
			return false;
		}
	}
	return true;
}

bool UMOKnowledgeComponent::HasAnyKnowledge(const TArray<FName>& KnowledgeIds) const
{
	for (const FName& KnowledgeId : KnowledgeIds)
	{
		if (AllLearnedKnowledge.Contains(KnowledgeId))
		{
			return true;
		}
	}
	return KnowledgeIds.Num() == 0;  // Return true if empty (no requirements)
}

bool UMOKnowledgeComponent::GetInspectionProgress(FName ItemDefinitionId, FMOItemKnowledgeProgress& OutProgress) const
{
	const FMOItemKnowledgeProgress* Progress = FindItemKnowledge(ItemDefinitionId);
	if (Progress)
	{
		OutProgress = *Progress;
		return true;
	}
	return false;
}

void UMOKnowledgeComponent::GetAllLearnedKnowledge(TArray<FName>& OutKnowledgeIds) const
{
	OutKnowledgeIds = AllLearnedKnowledge;
}

void UMOKnowledgeComponent::GetAllInspectedItems(TArray<FName>& OutItemIds) const
{
	OutItemIds.Empty();
	for (const FMOItemKnowledgeProgress& Progress : ItemKnowledge)
	{
		OutItemIds.Add(Progress.ItemDefinitionId);
	}
}

bool UMOKnowledgeComponent::GrantKnowledge(FName KnowledgeId)
{
	if (KnowledgeId.IsNone() || AllLearnedKnowledge.Contains(KnowledgeId))
	{
		return false;
	}

	AllLearnedKnowledge.Add(KnowledgeId);
	OnKnowledgeLearned.Broadcast(KnowledgeId, NAME_None);

	UE_LOG(LogMOFramework, Log, TEXT("[MOKnowledgeComponent] Knowledge '%s' granted directly"),
		*KnowledgeId.ToString());

	return true;
}

bool UMOKnowledgeComponent::CanLearnMoreFromItem(FName ItemDefinitionId, UMOSkillsComponent* SkillsComponent) const
{
	return GetLearningPotential(ItemDefinitionId, SkillsComponent) != EMOLearningPotential::None;
}

EMOLearningPotential UMOKnowledgeComponent::GetLearningPotential(FName ItemDefinitionId, UMOSkillsComponent* SkillsComponent) const
{
	if (ItemDefinitionId.IsNone())
	{
		return EMOLearningPotential::None;
	}

	// Get item definition
	FMOItemDefinitionRow ItemDef;
	if (!UMOItemDatabaseSettings::GetItemDefinition(ItemDefinitionId, ItemDef))
	{
		return EMOLearningPotential::None;
	}

	// No grants defined = nothing to learn
	if (ItemDef.Inspection.Grants.Num() == 0)
	{
		return EMOLearningPotential::None;
	}

	int32 TotalGrants = 0;
	int32 MaxedOutGrants = 0;

	for (const FMOInspectionGrant& Grant : ItemDef.Inspection.Grants)
	{
		if (Grant.Id.IsNone() || Grant.XPAmount <= 0.0f)
		{
			continue;
		}

		TotalGrants++;

		// Check if this grant is maxed out
		if (Grant.MaxLevel > 0 && IsValid(SkillsComponent))
		{
			const int32 CurrentLevel = SkillsComponent->GetSkillLevel(Grant.Id);
			if (CurrentLevel >= Grant.MaxLevel)
			{
				MaxedOutGrants++;
			}
		}
		// If MaxLevel == 0, it's unlimited so never maxed out
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOKnowledge] GetLearningPotential for '%s': TotalGrants=%d, MaxedOutGrants=%d"),
		*ItemDefinitionId.ToString(),
		TotalGrants,
		MaxedOutGrants);

	// Determine learning potential
	if (TotalGrants == 0)
	{
		return EMOLearningPotential::None;
	}
	else if (MaxedOutGrants >= TotalGrants)
	{
		return EMOLearningPotential::None;
	}
	else if (MaxedOutGrants > 0)
	{
		return EMOLearningPotential::Partial;
	}

	return EMOLearningPotential::Full;
}
