#include "MOSkillsComponent.h"
#include "MOSkillDatabaseSettings.h"
#include "MOFramework.h"
#include "MOQuestSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Engine/GameInstance.h"

UMOSkillsComponent::UMOSkillsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMOSkillsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UMOSkillsComponent, Skills, COND_OwnerOnly);
}

FMOSkillProgress* UMOSkillsComponent::FindSkillProgress(FName SkillId)
{
	for (FMOSkillProgress& Progress : Skills)
	{
		if (Progress.SkillId == SkillId)
		{
			return &Progress;
		}
	}
	return nullptr;
}

const FMOSkillProgress* UMOSkillsComponent::FindSkillProgress(FName SkillId) const
{
	for (const FMOSkillProgress& Progress : Skills)
	{
		if (Progress.SkillId == SkillId)
		{
			return &Progress;
		}
	}
	return nullptr;
}

bool UMOSkillsComponent::AddExperience(FName SkillId, float XPAmount)
{
	if (SkillId.IsNone() || XPAmount <= 0.0f)
	{
		return false;
	}

	const FMOSkillDefinitionRow* SkillDef = GetSkillDefinition(SkillId);

	// Initialize skill if not present
	FMOSkillProgress* Progress = FindSkillProgress(SkillId);
	if (!Progress)
	{
		InitializeSkill(SkillId);
		Progress = FindSkillProgress(SkillId);
	}

	if (!Progress)
	{
		return false;
	}

	// Check if already at max level
	const int32 MaxLevel = SkillDef ? SkillDef->MaxLevel : 100;
	if (Progress->Level >= MaxLevel)
	{
		return false;
	}

	Progress->CurrentXP += XPAmount;
	OnExperienceGained.Broadcast(SkillId, XPAmount, Progress->CurrentXP);

	// Process any level ups
	ProcessLevelUps(*Progress, SkillDef);

	return true;
}

int32 UMOSkillsComponent::GetSkillLevel(FName SkillId) const
{
	const FMOSkillProgress* Progress = FindSkillProgress(SkillId);
	return Progress ? Progress->Level : 0;
}

bool UMOSkillsComponent::GetSkillProgress(FName SkillId, FMOSkillProgress& OutProgress) const
{
	const FMOSkillProgress* Progress = FindSkillProgress(SkillId);
	if (Progress)
	{
		OutProgress = *Progress;
		return true;
	}
	return false;
}

bool UMOSkillsComponent::HasSkillLevel(FName SkillId, int32 RequiredLevel) const
{
	return GetSkillLevel(SkillId) >= RequiredLevel;
}

void UMOSkillsComponent::GetAllSkillIds(TArray<FName>& OutSkillIds) const
{
	OutSkillIds.Empty();
	for (const FMOSkillProgress& Progress : Skills)
	{
		OutSkillIds.Add(Progress.SkillId);
	}
}

void UMOSkillsComponent::InitializeSkill(FName SkillId)
{
	if (SkillId.IsNone() || FindSkillProgress(SkillId) != nullptr)
	{
		return;
	}

	const FMOSkillDefinitionRow* SkillDef = GetSkillDefinition(SkillId);

	FMOSkillProgress NewProgress;
	NewProgress.SkillId = SkillId;
	NewProgress.Level = 0;  // Start at level 0, first XP gains get you to level 1
	NewProgress.CurrentXP = 0.0f;
	NewProgress.XPToNextLevel = CalculateXPForLevel(SkillDef, 0);  // XP needed to go from 0->1

	Skills.Add(NewProgress);
}

void UMOSkillsComponent::SetSkillLevel(FName SkillId, int32 Level)
{
	if (SkillId.IsNone() || Level < 0)
	{
		return;
	}

	const FMOSkillDefinitionRow* SkillDef = GetSkillDefinition(SkillId);
	const int32 MaxLevel = SkillDef ? SkillDef->MaxLevel : 100;
	const int32 ClampedLevel = FMath::Clamp(Level, 0, MaxLevel);

	FMOSkillProgress* Progress = FindSkillProgress(SkillId);
	if (!Progress)
	{
		FMOSkillProgress NewProgress;
		NewProgress.SkillId = SkillId;
		Skills.Add(NewProgress);
		Progress = &Skills.Last();
	}

	const int32 OldLevel = Progress->Level;

	Progress->Level = ClampedLevel;
	Progress->CurrentXP = 0.0f;
	Progress->XPToNextLevel = (ClampedLevel < MaxLevel) ? CalculateXPForLevel(SkillDef, ClampedLevel) : 0.0f;

	if (OldLevel != ClampedLevel)
	{
		OnSkillLevelUp.Broadcast(SkillId, OldLevel, ClampedLevel);

		// Notify quest subsystem
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (UMOQuestSubsystem* QuestSub = GI->GetSubsystem<UMOQuestSubsystem>())
				{
					QuestSub->HandleSkillLevelUp(SkillId, OldLevel, ClampedLevel);
				}
			}
		}
	}
}

float UMOSkillsComponent::CalculateXPForLevel(const FMOSkillDefinitionRow* SkillDef, int32 Level) const
{
	const float BaseXP = SkillDef ? SkillDef->BaseXPPerLevel : 100.0f;
	const float ScaleFactor = SkillDef ? SkillDef->XPExponent : 1.25f;

	// XP needed to go from Level to Level+1
	// Level 0->1: BaseXP (100), Level 1->2: BaseXP*1.25 (125), Level 2->3: BaseXP*1.25^2 (156), etc.
	return BaseXP * FMath::Pow(ScaleFactor, static_cast<float>(Level));
}

const FMOSkillDefinitionRow* UMOSkillsComponent::GetSkillDefinition(FName SkillId) const
{
	return UMOSkillDatabaseSettings::GetSkillDefinition(SkillId);
}

void UMOSkillsComponent::ProcessLevelUps(FMOSkillProgress& Progress, const FMOSkillDefinitionRow* SkillDef)
{
	const int32 MaxLevel = SkillDef ? SkillDef->MaxLevel : 100;

	while (Progress.CurrentXP >= Progress.XPToNextLevel && Progress.Level < MaxLevel)
	{
		const int32 OldLevel = Progress.Level;
		Progress.CurrentXP -= Progress.XPToNextLevel;
		Progress.Level++;

		// Calculate XP needed for next level (based on current level after increment)
		if (Progress.Level < MaxLevel)
		{
			Progress.XPToNextLevel = CalculateXPForLevel(SkillDef, Progress.Level);
		}
		else
		{
			Progress.XPToNextLevel = 0.0f;
			Progress.CurrentXP = 0.0f;
		}

		OnSkillLevelUp.Broadcast(Progress.SkillId, OldLevel, Progress.Level);

		// Notify quest subsystem
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (UMOQuestSubsystem* QuestSub = GI->GetSubsystem<UMOQuestSubsystem>())
				{
					QuestSub->HandleSkillLevelUp(Progress.SkillId, OldLevel, Progress.Level);
				}
			}
		}

		UE_LOG(LogMOFramework, Log, TEXT("[MOSkillsComponent] Skill '%s' leveled up: %d -> %d"),
			*Progress.SkillId.ToString(), OldLevel, Progress.Level);
	}
}

// ============================================================================
// SAVE/LOAD
// ============================================================================

void UMOSkillsComponent::BuildSaveData(FMOSkillsSaveData& OutSaveData) const
{
	OutSaveData.Skills = Skills;
}

bool UMOSkillsComponent::ApplySaveData(const FMOSkillsSaveData& InSaveData)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSkillsComponent] ApplySaveData called without authority"));
		return false;
	}

	Skills = InSaveData.Skills;

	UE_LOG(LogMOFramework, Log, TEXT("[MOSkillsComponent] Applied save data with %d skills"), Skills.Num());
	return true;
}
