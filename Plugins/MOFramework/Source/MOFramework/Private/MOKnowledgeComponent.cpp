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
	FMOItemKnowledgeProgress* Progress = FindItemKnowledge(ItemDefinitionId);
	if (!Progress)
	{
		FMOItemKnowledgeProgress NewProgress;
		NewProgress.ItemDefinitionId = ItemDefinitionId;
		ItemKnowledge.Add(NewProgress);
		Progress = &ItemKnowledge.Last();
	}

	Result.bFirstInspection = (Progress->InspectionCount == 0);

	// Increment inspection count
	Progress->InspectionCount++;

	// Calculate XP multiplier based on diminishing returns
	const float XPMultiplier = GetXPMultiplier(Progress->InspectionCount);

	// Grant skill XP from inspection
	if (XPMultiplier > 0.0f && IsValid(SkillsComponent))
	{
		for (const auto& XPGrant : ItemDef.Inspection.SkillExperienceGrants)
		{
			const FName SkillId = XPGrant.Key;
			const float BaseXP = XPGrant.Value;
			const float ActualXP = BaseXP * XPMultiplier;

			if (ActualXP > 0.0f)
			{
				SkillsComponent->AddExperience(SkillId, ActualXP);
				Result.XPGranted.Add(SkillId, ActualXP);
			}
		}
	}

	// Track skill level for gating checks
	if (IsValid(SkillsComponent) && ItemDef.Inspection.KnowledgeSkillRequirements.Num() > 0)
	{
		// Find the primary skill for this item (first one in requirements)
		for (const auto& Req : ItemDef.Inspection.KnowledgeSkillRequirements)
		{
			Progress->LastInspectionSkillLevel = static_cast<float>(SkillsComponent->GetSkillLevel(Req.Key));
			break;
		}
	}

	// Check which knowledge can be unlocked
	for (const FName& KnowledgeId : ItemDef.Inspection.KnowledgeIds)
	{
		// Skip if already known
		if (AllLearnedKnowledge.Contains(KnowledgeId))
		{
			continue;
		}

		// Check skill requirement
		bool bMeetsRequirement = true;
		if (const int32* RequiredLevel = ItemDef.Inspection.KnowledgeSkillRequirements.Find(KnowledgeId))
		{
			if (IsValid(SkillsComponent))
			{
				// Find which skill is required (look for the first skill that grants XP for this item)
				FName RequiredSkillId = NAME_None;
				for (const auto& XPGrant : ItemDef.Inspection.SkillExperienceGrants)
				{
					RequiredSkillId = XPGrant.Key;
					break;
				}

				if (!RequiredSkillId.IsNone())
				{
					bMeetsRequirement = SkillsComponent->HasSkillLevel(RequiredSkillId, *RequiredLevel);
				}
			}
			else
			{
				// No skills component, can't meet skill requirements
				bMeetsRequirement = (*RequiredLevel <= 0);
			}
		}

		if (bMeetsRequirement)
		{
			// Learn this knowledge
			AllLearnedKnowledge.Add(KnowledgeId);
			Progress->UnlockedKnowledge.Add(KnowledgeId);
			Result.NewKnowledge.Add(KnowledgeId);

			OnKnowledgeLearned.Broadcast(KnowledgeId, ItemDefinitionId);

			UE_LOG(LogMOFramework, Log, TEXT("[MOKnowledgeComponent] Learned knowledge '%s' from inspecting '%s'"),
				*KnowledgeId.ToString(), *ItemDefinitionId.ToString());
		}
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

float UMOKnowledgeComponent::GetXPMultiplier(int32 InspectionCount) const
{
	if (InspectionCount <= 0 || InspectionCount > MaxInspectionsForXP)
	{
		return 0.0f;
	}

	if (InspectionCount == 1)
	{
		return 1.0f;  // Full XP on first inspection
	}

	// Diminishing returns: factor^(count-1)
	return FMath::Pow(DiminishingReturnsFactor, static_cast<float>(InspectionCount - 1));
}
