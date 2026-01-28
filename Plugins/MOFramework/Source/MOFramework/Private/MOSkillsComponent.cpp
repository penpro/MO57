#include "MOSkillsComponent.h"
#include "MOSkillDatabaseSettings.h"
#include "MOFramework.h"
#include "Net/UnrealNetwork.h"

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
	return Progress ? Progress->Level : 1;
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
	NewProgress.Level = 1;
	NewProgress.CurrentXP = 0.0f;
	NewProgress.XPToNextLevel = CalculateXPForLevel(SkillDef, 2);

	Skills.Add(NewProgress);
}

void UMOSkillsComponent::SetSkillLevel(FName SkillId, int32 Level)
{
	if (SkillId.IsNone() || Level < 1)
	{
		return;
	}

	const FMOSkillDefinitionRow* SkillDef = GetSkillDefinition(SkillId);
	const int32 MaxLevel = SkillDef ? SkillDef->MaxLevel : 100;
	const int32 ClampedLevel = FMath::Clamp(Level, 1, MaxLevel);

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
	Progress->XPToNextLevel = (ClampedLevel < MaxLevel) ? CalculateXPForLevel(SkillDef, ClampedLevel + 1) : 0.0f;

	if (OldLevel != ClampedLevel)
	{
		OnSkillLevelUp.Broadcast(SkillId, OldLevel, ClampedLevel);
	}
}

float UMOSkillsComponent::CalculateXPForLevel(const FMOSkillDefinitionRow* SkillDef, int32 Level) const
{
	const float BaseXP = SkillDef ? SkillDef->BaseXPPerLevel : 100.0f;
	const float Exponent = SkillDef ? SkillDef->XPExponent : 1.5f;

	return BaseXP * FMath::Pow(static_cast<float>(Level), Exponent);
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

		// Calculate XP needed for next level
		if (Progress.Level < MaxLevel)
		{
			Progress.XPToNextLevel = CalculateXPForLevel(SkillDef, Progress.Level + 1);
		}
		else
		{
			Progress.XPToNextLevel = 0.0f;
			Progress.CurrentXP = 0.0f;
		}

		OnSkillLevelUp.Broadcast(Progress.SkillId, OldLevel, Progress.Level);

		UE_LOG(LogMOFramework, Log, TEXT("[MOSkillsComponent] Skill '%s' leveled up: %d -> %d"),
			*Progress.SkillId.ToString(), OldLevel, Progress.Level);
	}
}
