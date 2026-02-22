#include "MOQuestSubsystem.h"
#include "MOFramework.h"
#include "MOCraftingSubsystem.h"
#include "MOSkillsComponent.h"
#include "MOInventoryComponent.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

// ============================================================================
// LIFECYCLE
// ============================================================================

void UMOQuestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Load quest definitions from DataTable
	LoadQuestDefinitions();

	// Bind to world initialization to subscribe to world subsystems
	WorldInitializedHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(
		this, &UMOQuestSubsystem::HandleWorldInitialized
	);

	UE_LOG(LogMOFramework, Log, TEXT("[MOQuestSubsystem] Initialized with %d quest definitions"),
		QuestDefinitions.Num());
}

void UMOQuestSubsystem::Deinitialize()
{
	FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitializedHandle);

	// Unbind from crafting subsystem if bound
	if (UMOCraftingSubsystem* CraftingSub = BoundCraftingSubsystem.Get())
	{
		CraftingSub->OnCraftCompleted.RemoveDynamic(this, &UMOQuestSubsystem::HandleCraftCompleted);
	}

	Super::Deinitialize();
}

void UMOQuestSubsystem::HandleWorldInitialized(UWorld* World, const UWorld::InitializationValues IVS)
{
	if (!World || World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE)
	{
		return;
	}

	// Bind to crafting subsystem
	if (UMOCraftingSubsystem* CraftingSub = World->GetSubsystem<UMOCraftingSubsystem>())
	{
		// Unbind from previous world's subsystem
		if (UMOCraftingSubsystem* OldSub = BoundCraftingSubsystem.Get())
		{
			OldSub->OnCraftCompleted.RemoveDynamic(this, &UMOQuestSubsystem::HandleCraftCompleted);
		}

		CraftingSub->OnCraftCompleted.AddDynamic(this, &UMOQuestSubsystem::HandleCraftCompleted);
		BoundCraftingSubsystem = CraftingSub;

		UE_LOG(LogMOFramework, Verbose, TEXT("[MOQuestSubsystem] Bound to crafting subsystem for world %s"),
			*World->GetName());
	}

	// Check for auto-start quests
	if (UMOQuestSettings::Get()->bAutoStartTutorials)
	{
		CheckAutoStartQuests();
	}
}

// ============================================================================
// QUEST MANAGEMENT
// ============================================================================

bool UMOQuestSubsystem::StartQuest(FName QuestId)
{
	// Check if already active
	if (ActiveQuests.Contains(QuestId))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOQuestSubsystem] Quest '%s' is already active"),
			*QuestId.ToString());
		return false;
	}

	// Check if already complete
	if (CompletedQuests.Contains(QuestId))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOQuestSubsystem] Quest '%s' is already complete"),
			*QuestId.ToString());
		return false;
	}

	// Get definition
	const FMOQuestDefinitionRow* Definition = QuestDefinitions.Find(QuestId);
	if (!Definition)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOQuestSubsystem] Quest '%s' not found in definitions"),
			*QuestId.ToString());
		return false;
	}

	// Check prerequisites
	if (!ArePrerequisitesMet(*Definition))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOQuestSubsystem] Prerequisites not met for quest '%s'"),
			*QuestId.ToString());
		return false;
	}

	// Create state
	FMOQuestState NewState;
	NewState.QuestId = QuestId;
	NewState.bIsTracked = true;
	NewState.StartTime = FDateTime::Now();

	// Initialize objective progress
	for (const FMOQuestObjective& Objective : Definition->Objectives)
	{
		NewState.ObjectiveProgress.Add(Objective.ObjectiveId, 0);
	}

	ActiveQuests.Add(QuestId, NewState);

	UE_LOG(LogMOFramework, Log, TEXT("[MOQuestSubsystem] Started quest '%s': %s"),
		*QuestId.ToString(), *Definition->DisplayName.ToString());

	OnQuestStarted.Broadcast(QuestId);
	return true;
}

bool UMOQuestSubsystem::AbandonQuest(FName QuestId)
{
	if (!ActiveQuests.Contains(QuestId))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOQuestSubsystem] Cannot abandon quest '%s' - not active"),
			*QuestId.ToString());
		return false;
	}

	ActiveQuests.Remove(QuestId);

	UE_LOG(LogMOFramework, Log, TEXT("[MOQuestSubsystem] Abandoned quest '%s'"), *QuestId.ToString());

	OnQuestAbandoned.Broadcast(QuestId);
	return true;
}

void UMOQuestSubsystem::SetQuestTracked(FName QuestId, bool bTracked)
{
	FMOQuestState* State = ActiveQuests.Find(QuestId);
	if (!State)
	{
		return;
	}

	State->bIsTracked = bTracked;

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOQuestSubsystem] Quest '%s' tracked: %s"),
		*QuestId.ToString(), bTracked ? TEXT("YES") : TEXT("NO"));
}

void UMOQuestSubsystem::ReportObjectiveProgress(FName QuestId, FName ObjectiveId, int32 Count)
{
	FMOQuestState* State = ActiveQuests.Find(QuestId);
	if (!State)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOQuestSubsystem] Cannot report progress - quest '%s' not active"),
			*QuestId.ToString());
		return;
	}

	const FMOQuestDefinitionRow* Definition = QuestDefinitions.Find(QuestId);
	if (!Definition)
	{
		return;
	}

	// Find the objective
	for (const FMOQuestObjective& Objective : Definition->Objectives)
	{
		if (Objective.ObjectiveId == ObjectiveId)
		{
			UpdateObjectiveProgress(*State, Objective, Count);
			CheckQuestCompletion(*State);
			return;
		}
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOQuestSubsystem] Objective '%s' not found in quest '%s'"),
		*ObjectiveId.ToString(), *QuestId.ToString());
}

void UMOQuestSubsystem::FireGameEvent(FName EventName)
{
	UE_LOG(LogMOFramework, Verbose, TEXT("[MOQuestSubsystem] Game event fired: %s"), *EventName.ToString());
	ProcessEventForObjectives(EMOObjectiveType::Event, EventName, 1);
}

// ============================================================================
// QUERIES
// ============================================================================

TArray<FMOQuestState> UMOQuestSubsystem::GetActiveQuests() const
{
	TArray<FMOQuestState> Result;
	ActiveQuests.GenerateValueArray(Result);
	return Result;
}

TArray<FMOQuestState> UMOQuestSubsystem::GetTrackedQuests() const
{
	TArray<FMOQuestState> Result;
	const int32 MaxTracked = UMOQuestSettings::Get()->MaxTrackedQuestsOnHUD;

	for (const auto& Pair : ActiveQuests)
	{
		if (Pair.Value.bIsTracked)
		{
			Result.Add(Pair.Value);
			if (Result.Num() >= MaxTracked)
			{
				break;
			}
		}
	}

	return Result;
}

bool UMOQuestSubsystem::GetQuestState(FName QuestId, FMOQuestState& OutState) const
{
	const FMOQuestState* State = ActiveQuests.Find(QuestId);
	if (State)
	{
		OutState = *State;
		return true;
	}
	return false;
}

bool UMOQuestSubsystem::IsQuestComplete(FName QuestId) const
{
	return CompletedQuests.Contains(QuestId);
}

bool UMOQuestSubsystem::IsQuestActive(FName QuestId) const
{
	return ActiveQuests.Contains(QuestId);
}

bool UMOQuestSubsystem::GetQuestDefinition(FName QuestId, FMOQuestDefinitionRow& OutDefinition) const
{
	const FMOQuestDefinitionRow* Found = QuestDefinitions.Find(QuestId);
	if (Found)
	{
		OutDefinition = *Found;
		return true;
	}
	return false;
}

const FMOQuestDefinitionRow* UMOQuestSubsystem::GetQuestDefinitionPtr(FName QuestId) const
{
	return QuestDefinitions.Find(QuestId);
}

TArray<FName> UMOQuestSubsystem::GetAvailableQuests() const
{
	TArray<FName> Result;

	for (const auto& Pair : QuestDefinitions)
	{
		const FName& QuestId = Pair.Key;

		// Skip if already active or complete
		if (ActiveQuests.Contains(QuestId) || CompletedQuests.Contains(QuestId))
		{
			continue;
		}

		// Check prerequisites
		if (ArePrerequisitesMet(Pair.Value))
		{
			Result.Add(QuestId);
		}
	}

	return Result;
}

TArray<FName> UMOQuestSubsystem::GetCompletedQuestIds() const
{
	return CompletedQuests.Array();
}

// ============================================================================
// PERSISTENCE
// ============================================================================

void UMOQuestSubsystem::BuildSaveData(FMOQuestSaveData& OutSaveData) const
{
	OutSaveData.ActiveQuests.Empty();
	ActiveQuests.GenerateValueArray(OutSaveData.ActiveQuests);

	OutSaveData.CompletedQuestIds = CompletedQuests.Array();
}

void UMOQuestSubsystem::ApplySaveData(const FMOQuestSaveData& SaveData)
{
	// Clear current state
	ActiveQuests.Empty();
	CompletedQuests.Empty();

	// Restore active quests
	for (const FMOQuestState& State : SaveData.ActiveQuests)
	{
		ActiveQuests.Add(State.QuestId, State);
	}

	// Restore completed quests
	for (const FName& QuestId : SaveData.CompletedQuestIds)
	{
		CompletedQuests.Add(QuestId);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOQuestSubsystem] Loaded %d active quests, %d completed"),
		ActiveQuests.Num(), CompletedQuests.Num());
}

void UMOQuestSubsystem::ResetAllQuests()
{
	ActiveQuests.Empty();
	CompletedQuests.Empty();

	UE_LOG(LogMOFramework, Log, TEXT("[MOQuestSubsystem] Reset all quest progress"));

	// Auto-start tutorials if enabled
	if (UMOQuestSettings::Get()->bAutoStartTutorials)
	{
		CheckAutoStartQuests();
	}
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void UMOQuestSubsystem::HandleCraftCompleted(FName RecipeId, const FMOCraftResult& Result)
{
	if (!Result.bSuccess)
	{
		return;
	}

	// Process each produced item for ItemCraft objectives
	for (const auto& Pair : Result.ProducedItems)
	{
		ProcessEventForObjectives(EMOObjectiveType::ItemCraft, Pair.Key, Pair.Value);
	}
}

void UMOQuestSubsystem::HandleItemPickedUp(FName ItemId, int32 Quantity)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOQuestSubsystem] HandleItemPickedUp: %s x%d (ActiveQuests=%d)"),
		*ItemId.ToString(), Quantity, ActiveQuests.Num());
	ProcessEventForObjectives(EMOObjectiveType::ItemPickup, ItemId, Quantity);
}

void UMOQuestSubsystem::HandleSkillLevelUp(FName SkillId, int32 OldLevel, int32 NewLevel)
{
	// For skill level ups, we fire once per level gained
	const int32 LevelsGained = NewLevel - OldLevel;
	if (LevelsGained > 0)
	{
		ProcessEventForObjectives(EMOObjectiveType::SkillLevelUp, SkillId, LevelsGained);
	}
}

// ============================================================================
// INTERNAL
// ============================================================================

void UMOQuestSubsystem::LoadQuestDefinitions()
{
	QuestDefinitions.Empty();

	const FSoftObjectPath& TablePath = UMOQuestSettings::Get()->QuestDefinitionTable;
	if (TablePath.IsNull())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOQuestSubsystem] No quest definition table configured"));
		return;
	}

	UDataTable* Table = Cast<UDataTable>(TablePath.TryLoad());
	if (!Table)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOQuestSubsystem] Failed to load quest table: %s"),
			*TablePath.ToString());
		return;
	}

	// Verify row struct
	if (Table->GetRowStruct() != FMOQuestDefinitionRow::StaticStruct())
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOQuestSubsystem] Quest table has wrong row struct"));
		return;
	}

	// Load all rows
	TArray<FName> RowNames = Table->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		const FMOQuestDefinitionRow* Row = Table->FindRow<FMOQuestDefinitionRow>(RowName, TEXT("LoadQuestDefinitions"));
		if (Row)
		{
			// Use QuestId from row, fallback to row name
			FName QuestId = Row->QuestId.IsNone() ? RowName : Row->QuestId;
			QuestDefinitions.Add(QuestId, *Row);
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOQuestSubsystem] Loaded %d quest definitions from %s"),
		QuestDefinitions.Num(), *Table->GetName());
}

bool UMOQuestSubsystem::ArePrerequisitesMet(const FMOQuestDefinitionRow& Quest) const
{
	for (const FName& PrereqId : Quest.Prerequisites)
	{
		if (!CompletedQuests.Contains(PrereqId))
		{
			return false;
		}
	}
	return true;
}

static const TCHAR* GetObjectiveTypeName(EMOObjectiveType Type)
{
	switch (Type)
	{
	case EMOObjectiveType::Event: return TEXT("Event");
	case EMOObjectiveType::ItemCraft: return TEXT("ItemCraft");
	case EMOObjectiveType::ItemPickup: return TEXT("ItemPickup");
	case EMOObjectiveType::ItemDrop: return TEXT("ItemDrop");
	case EMOObjectiveType::SkillLevelUp: return TEXT("SkillLevelUp");
	case EMOObjectiveType::LocationReach: return TEXT("LocationReach");
	case EMOObjectiveType::Custom: return TEXT("Custom");
	default: return TEXT("Unknown");
	}
}

void UMOQuestSubsystem::ProcessEventForObjectives(EMOObjectiveType Type, FName TargetId, int32 Count)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOQuestSubsystem] ProcessEvent: Type=%s, Target='%s', Count=%d, ActiveQuests=%d"),
		GetObjectiveTypeName(Type), *TargetId.ToString(), Count, ActiveQuests.Num());

	// Collect quests to check for completion after iteration
	// (CheckQuestCompletion may remove from ActiveQuests, invalidating iteration)
	TArray<FName> QuestsToCheck;

	// Iterate all active quests
	for (auto& Pair : ActiveQuests)
	{
		FMOQuestState& State = Pair.Value;
		if (State.bIsComplete)
		{
			continue;
		}

		const FMOQuestDefinitionRow* Definition = QuestDefinitions.Find(State.QuestId);
		if (!Definition)
		{
			continue;
		}

		bool bProgressMade = false;

		// Check each objective
		for (const FMOQuestObjective& Objective : Definition->Objectives)
		{
			// Skip if already complete
			if (State.IsObjectiveComplete(Objective.ObjectiveId))
			{
				continue;
			}

			// Skip if sequential and not current
			if (Objective.bSequential)
			{
				const FMOQuestObjective* Current = GetCurrentSequentialObjective(State, *Definition);
				if (!Current || Current->ObjectiveId != Objective.ObjectiveId)
				{
					continue;
				}
			}

			// Check type and target match
			if (Objective.Type == Type && Objective.TargetEventOrId == TargetId)
			{
				UpdateObjectiveProgress(State, Objective, Count);
				bProgressMade = true;
			}
			else if (Objective.Type == Type)
			{
				// Same type but different target - log at Verbose level
				UE_LOG(LogMOFramework, Verbose, TEXT("[MOQuestSubsystem]   Quest '%s' obj '%s': target mismatch (want '%s', got '%s')"),
					*State.QuestId.ToString(),
					*Objective.ObjectiveId.ToString(),
					*Objective.TargetEventOrId.ToString(),
					*TargetId.ToString());
			}
		}

		// Queue for completion check if progress was made
		if (bProgressMade)
		{
			QuestsToCheck.Add(State.QuestId);
		}
	}

	// Now check completion outside the iteration (safe to remove from ActiveQuests)
	for (const FName& QuestId : QuestsToCheck)
	{
		if (FMOQuestState* State = ActiveQuests.Find(QuestId))
		{
			CheckQuestCompletion(*State);
		}
	}
}

void UMOQuestSubsystem::UpdateObjectiveProgress(FMOQuestState& State, const FMOQuestObjective& Objective, int32 Count)
{
	int32& Progress = State.ObjectiveProgress.FindOrAdd(Objective.ObjectiveId);
	const int32 OldProgress = Progress;
	Progress = FMath::Min(Progress + Count, Objective.RequiredCount);

	if (Progress == OldProgress)
	{
		return; // No change
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOQuestSubsystem] Quest '%s' objective '%s': %d/%d"),
		*State.QuestId.ToString(), *Objective.ObjectiveId.ToString(), Progress, Objective.RequiredCount);

	OnObjectiveUpdated.Broadcast(State.QuestId, Objective.ObjectiveId, Progress);

	// Check if objective completed
	if (Progress >= Objective.RequiredCount && !State.IsObjectiveComplete(Objective.ObjectiveId))
	{
		State.CompletedObjectives.Add(Objective.ObjectiveId);

		UE_LOG(LogMOFramework, Log, TEXT("[MOQuestSubsystem] Quest '%s' objective '%s' COMPLETED"),
			*State.QuestId.ToString(), *Objective.ObjectiveId.ToString());

		OnObjectiveCompleted.Broadcast(State.QuestId, Objective.ObjectiveId);
	}
}

void UMOQuestSubsystem::CheckQuestCompletion(FMOQuestState& State)
{
	if (State.bIsComplete)
	{
		return;
	}

	const FMOQuestDefinitionRow* Definition = QuestDefinitions.Find(State.QuestId);
	if (!Definition)
	{
		return;
	}

	// Check all required objectives
	for (const FMOQuestObjective& Objective : Definition->Objectives)
	{
		if (Objective.bOptional)
		{
			continue;
		}

		if (!State.IsObjectiveComplete(Objective.ObjectiveId))
		{
			return; // Still have incomplete required objectives
		}
	}

	// All required objectives complete
	State.bIsComplete = true;
	State.CompletionTime = FDateTime::Now();

	// Move to completed set
	CompletedQuests.Add(State.QuestId);
	ActiveQuests.Remove(State.QuestId);

	UE_LOG(LogMOFramework, Log, TEXT("[MOQuestSubsystem] Quest '%s' COMPLETED: %s"),
		*State.QuestId.ToString(), *Definition->DisplayName.ToString());

	OnQuestCompleted.Broadcast(State.QuestId);

	// Check if completing this quest unlocks auto-start quests
	CheckAutoStartQuests();
}

void UMOQuestSubsystem::CheckAutoStartQuests()
{
	for (const auto& Pair : QuestDefinitions)
	{
		const FMOQuestDefinitionRow& Definition = Pair.Value;

		// Skip if not auto-start
		if (!Definition.bAutoStart)
		{
			continue;
		}

		// Skip if already active or complete
		if (ActiveQuests.Contains(Pair.Key) || CompletedQuests.Contains(Pair.Key))
		{
			continue;
		}

		// Check prerequisites
		if (ArePrerequisitesMet(Definition))
		{
			StartQuest(Pair.Key);
		}
	}
}

const FMOQuestObjective* UMOQuestSubsystem::GetCurrentSequentialObjective(
	const FMOQuestState& State,
	const FMOQuestDefinitionRow& Quest) const
{
	// Find the first incomplete sequential objective
	for (const FMOQuestObjective& Objective : Quest.Objectives)
	{
		if (!Objective.bSequential)
		{
			continue;
		}

		if (!State.IsObjectiveComplete(Objective.ObjectiveId))
		{
			return &Objective;
		}
	}

	return nullptr;
}
