#include "MOSkillsComponent.h"
#include "MOGameClockSubsystem.h"
#include "TimerManager.h"
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

	// Using a skill (gaining XP in it) resets its decay clock (V2.2).
	if (UWorld* World = GetWorld())
	{
		if (UMOGameClockSubsystem* Clock = World->GetSubsystem<UMOGameClockSubsystem>())
		{
			LastUsedGameSeconds.Add(SkillId, Clock->GetGameTimeSeconds());
		}
	}

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

// ============================================================================
// SKILL DECAY (V2.2)
// ============================================================================

void UMOSkillsComponent::BeginPlay()
{
	Super::BeginPlay();
	AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			// Slow real-time cadence; the pass itself works in GAME hours, so
			// accelerated clocks decay proportionally faster (honest sim).
			World->GetTimerManager().SetTimer(DecayTimer, this,
				&UMOSkillsComponent::ApplyDecayPass, 60.0f, /*bLoop=*/true);
		}
	}
}

float UMOSkillsComponent::ComputeSkillDecayXP(float GameHoursSinceLastPass, float UnusedGameHours,
	float GraceGameHours, float DecayXPPerGameHour)
{
	if (UnusedGameHours <= GraceGameHours || GameHoursSinceLastPass <= 0.0f || DecayXPPerGameHour <= 0.0f)
	{
		return 0.0f;
	}
	// Only the portion of this pass that lies PAST the grace boundary decays
	// (a skill crossing the boundary mid-pass doesn't lose a full pass).
	const float DecayingHours = FMath::Min(GameHoursSinceLastPass, UnusedGameHours - GraceGameHours);
	return DecayXPPerGameHour * FMath::Max(DecayingHours, 0.0f);
}

void UMOSkillsComponent::ApplyDecayPass()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !Owner->HasAuthority() || !World)
	{
		return;
	}
	UMOGameClockSubsystem* Clock = World->GetSubsystem<UMOGameClockSubsystem>();
	if (!Clock)
	{
		return;
	}
	const double Now = Clock->GetGameTimeSeconds();
	const float HoursSinceLastPass = (LastDecayPassGameSeconds >= 0.0)
		? static_cast<float>((Now - LastDecayPassGameSeconds) / 3600.0)
		: 0.0f;
	LastDecayPassGameSeconds = Now;
	if (bSkillMaintenance || HoursSinceLastPass <= 0.0f)
	{
		return;   // schooled villagers keep their whole tree sharp
	}

	for (FMOSkillProgress& Progress : Skills)
	{
		if (Progress.Level <= DecayFloorLevel && Progress.CurrentXP <= 0.0f)
		{
			continue;
		}
		// Never-used-since-load skills start their grace at first sight.
		if (!LastUsedGameSeconds.Contains(Progress.SkillId))
		{
			LastUsedGameSeconds.Add(Progress.SkillId, Now);
			continue;
		}
		const float UnusedHours = static_cast<float>((Now - LastUsedGameSeconds[Progress.SkillId]) / 3600.0);
		const float Loss = ComputeSkillDecayXP(HoursSinceLastPass, UnusedHours,
			DecayGraceGameHours, DecayXPPerGameHour);
		if (Loss <= 0.0f)
		{
			continue;
		}
		Progress.CurrentXP -= Loss;
		while (Progress.CurrentXP < 0.0f && Progress.Level > DecayFloorLevel)
		{
			// De-level: drop into the previous level near its top, mirroring
			// the level-up curve so re-learning is quick at first (rust, not
			// amnesia).
			Progress.Level -= 1;
			const FMOSkillDefinitionRow* SkillDef = GetSkillDefinition(Progress.SkillId);
			Progress.XPToNextLevel = CalculateXPForLevel(SkillDef, Progress.Level);
			Progress.CurrentXP += Progress.XPToNextLevel;
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSkills] %s: %s decayed to level %d"),
				*Owner->GetName(), *Progress.SkillId.ToString(), Progress.Level);
		}
		Progress.CurrentXP = FMath::Max(Progress.CurrentXP, 0.0f);
	}
}
