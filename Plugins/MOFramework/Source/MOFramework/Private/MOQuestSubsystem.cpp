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

	// Refresh which events have listeners — new objectives just became active.
	RebuildActiveEventListeners();

	OnQuestStarted.Broadcast(QuestId);

	// Starting a tutorial quest may surface a new hint — let listeners refresh.
	if (Definition->bIsTutorial)
	{
		OnTutorialHintChanged.Broadcast();
	}

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
	RebuildActiveEventListeners();

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

	// Active quests changed wholesale — rebuild the listener cache so the
	// per-tick broadcasters know which events have receivers post-load.
	RebuildActiveEventListeners();

	UE_LOG(LogMOFramework, Log, TEXT("[MOQuestSubsystem] Loaded %d active quests, %d completed"),
		ActiveQuests.Num(), CompletedQuests.Num());
}

void UMOQuestSubsystem::ResetAllQuests()
{
	ActiveQuests.Empty();
	CompletedQuests.Empty();
	ActiveEventListeners.Empty();

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

void UMOQuestSubsystem::HandleBuildingCompleted(FName RecipeId)
{
	if (RecipeId.IsNone())
	{
		return;
	}
	// Buildings produce themselves as the "crafted item" — Tutorial_BuildCampfire
	// uses Type=ItemCraft, TargetEventOrId="BuildCampfire" (the recipe ROW name)
	// exactly as a regular hand-craft would. We route through the same ItemCraft
	// objective type so designers don't need a new objective type for buildings.
	// NOTE: the craft path (HandleCraftCompleted) keys objectives by PRODUCED
	// ITEM ids; this building path keys by RECIPE id — two id namespaces feed
	// the same objective type, so quest authors must match the right one.
	UE_LOG(LogMOFramework, Log, TEXT("[MOQuestSubsystem] HandleBuildingCompleted: %s"), *RecipeId.ToString());
	ProcessEventForObjectives(EMOObjectiveType::ItemCraft, RecipeId, 1);
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

	// Signal readiness to UI controllers (and anyone else subscribed). Listeners
	// that subscribe AFTER this broadcast check IsReady() and act synchronously.
	// Set the flag BEFORE broadcasting so handlers that re-query IsReady inside
	// their own handler see true.
	bIsReady = true;
	OnQuestSystemReady.Broadcast();
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

		// An objective just became complete — refresh listeners so high-freq
		// broadcasters (per-tick move/look) stop firing for events nobody
		// listens to anymore.
		RebuildActiveEventListeners();

		OnObjectiveCompleted.Broadcast(State.QuestId, Objective.ObjectiveId);
	}

	// Hint widget tracks the active tutorial hint by querying us — broadcast
	// on ANY tutorial-quest objective change. Don't gate on this objective's
	// own bShowAsTutorialPopup: a silent sequential gate (Obj1) completing
	// is what unblocks the next popup objective (Obj2). If we only broadcast
	// when popup objectives progressed, the popup widget would never learn
	// that a gate fired and would stay blank.
	const FMOQuestDefinitionRow* Definition = QuestDefinitions.Find(State.QuestId);
	if (Definition && Definition->bIsTutorial)
	{
		OnTutorialHintChanged.Broadcast();
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

	// Snapshot the id before Remove frees the ActiveQuests entry that `State`
	// references; every read below must use the local, not the dangling ref. (H27)
	const FName CompletedQuestId = State.QuestId;

	// Move to completed set
	CompletedQuests.Add(CompletedQuestId);
	ActiveQuests.Remove(CompletedQuestId);
	// NOTE: `State` is dangling past this point — do not read it again.

	// Now that the quest is gone from ActiveQuests, its objective events
	// no longer have listeners — refresh the cache.
	RebuildActiveEventListeners();

	UE_LOG(LogMOFramework, Log, TEXT("[MOQuestSubsystem] Quest '%s' COMPLETED: %s"),
		*CompletedQuestId.ToString(), *Definition->DisplayName.ToString());

	OnQuestCompleted.Broadcast(CompletedQuestId);

	// Fire the reward hook so external systems can grant XP/knowledge/items.
	// The subsystem deliberately does NOT apply rewards itself — that's left
	// to listeners (currently no-ops). Reward specs live on the quest row's
	// Rewards array; listeners read them via GetQuestDefinition.
	OnApplyQuestRewards.Broadcast(CompletedQuestId);

	// Completing a tutorial quest changes which hint should be shown — let
	// the popup widget re-evaluate.
	if (Definition->bIsTutorial)
	{
		OnTutorialHintChanged.Broadcast();
	}

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

// ============================================================================
// TUTORIAL HINT API
// ============================================================================

bool UMOQuestSubsystem::GetActiveTutorialHint(FName& OutQuestId, FName& OutObjectiveId,
	FText& OutHintTitle, FText& OutHintBody) const
{
	OutQuestId = NAME_None;
	OutObjectiveId = NAME_None;
	OutHintTitle = FText::GetEmpty();
	OutHintBody = FText::GetEmpty();

	if (!bTutorialPopupsEnabled)
	{
		return false;
	}

	// Pick the first active tutorial quest with an incomplete objective that
	// wants to surface a popup AND is currently allowed to be active (sequential
	// gating). The "silent-gate + popup-payload" pattern relies on this — a
	// silent sequential objective comes first, and the popup objective only
	// shows once that gate has fired. Without honoring sequential here, the
	// popup objective would surface immediately when the quest starts.
	for (const auto& Pair : ActiveQuests)
	{
		const FMOQuestState& State = Pair.Value;
		const FMOQuestDefinitionRow* Definition = QuestDefinitions.Find(Pair.Key);
		if (!Definition || !Definition->bIsTutorial)
		{
			continue;
		}

		// Cache the current sequential objective once per quest. nullptr if
		// the quest has no sequential objectives left or has none at all.
		const FMOQuestObjective* CurrentSequential = GetCurrentSequentialObjective(State, *Definition);

		for (const FMOQuestObjective& Objective : Definition->Objectives)
		{
			if (!Objective.bShowAsTutorialPopup)
			{
				continue;
			}
			if (State.IsObjectiveComplete(Objective.ObjectiveId))
			{
				continue;
			}

			// Sequential gating: if this objective is sequential, only show
			// it when it's the current sequential objective. Non-sequential
			// objectives bypass this and show whenever incomplete.
			if (Objective.bSequential)
			{
				if (!CurrentSequential || CurrentSequential->ObjectiveId != Objective.ObjectiveId)
				{
					continue;
				}
			}

			OutQuestId = Pair.Key;
			OutObjectiveId = Objective.ObjectiveId;
			OutHintTitle = Objective.HintTitle;
			OutHintBody = Objective.HintBody;
			return true;
		}
	}

	return false;
}

void UMOQuestSubsystem::SkipCurrentTutorialObjective()
{
	FName QuestId, ObjectiveId;
	FText IgnoredTitle, IgnoredBody;
	if (!GetActiveTutorialHint(QuestId, ObjectiveId, IgnoredTitle, IgnoredBody))
	{
		return;
	}

	FMOQuestState* State = ActiveQuests.Find(QuestId);
	const FMOQuestDefinitionRow* Definition = QuestDefinitions.Find(QuestId);
	if (!State || !Definition)
	{
		return;
	}

	// Find the objective and force it to RequiredCount via the normal update
	// path — that fires OnObjectiveCompleted, advances the chain, and triggers
	// hint refresh for free.
	for (const FMOQuestObjective& Objective : Definition->Objectives)
	{
		if (Objective.ObjectiveId == ObjectiveId)
		{
			const int32 CurrentProgress = State->GetObjectiveProgress(ObjectiveId);
			const int32 Remaining = Objective.RequiredCount - CurrentProgress;
			if (Remaining > 0)
			{
				UpdateObjectiveProgress(*State, Objective, Remaining);
				CheckQuestCompletion(*State);
			}
			UE_LOG(LogMOFramework, Log, TEXT("[MOQuestSubsystem] Skipped tutorial objective %s.%s"),
				*QuestId.ToString(), *ObjectiveId.ToString());
			return;
		}
	}
}

void UMOQuestSubsystem::SetTutorialPopupsEnabled(bool bEnabled)
{
	if (bTutorialPopupsEnabled == bEnabled)
	{
		return;
	}
	bTutorialPopupsEnabled = bEnabled;
	UE_LOG(LogMOFramework, Log, TEXT("[MOQuestSubsystem] Tutorial popups %s"),
		bEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));
	OnTutorialHintChanged.Broadcast();
}

// ============================================================================
// EVENT LISTENER CACHE
// ============================================================================

bool UMOQuestSubsystem::IsAnyQuestListeningForEvent(FName EventName) const
{
	return ActiveEventListeners.Contains(EventName);
}

void UMOQuestSubsystem::RebuildActiveEventListeners()
{
	ActiveEventListeners.Reset();

	for (const auto& Pair : ActiveQuests)
	{
		const FMOQuestState& State = Pair.Value;
		const FMOQuestDefinitionRow* Definition = QuestDefinitions.Find(Pair.Key);
		if (!Definition)
		{
			continue;
		}

		for (const FMOQuestObjective& Objective : Definition->Objectives)
		{
			// Only Event-type objectives go through FireGameEvent → here.
			// Other objective types (ItemPickup, ItemCraft, SkillLevelUp)
			// have their own dedicated entry points and don't need this.
			if (Objective.Type != EMOObjectiveType::Event)
			{
				continue;
			}
			if (State.IsObjectiveComplete(Objective.ObjectiveId))
			{
				continue;
			}
			if (Objective.TargetEventOrId.IsNone())
			{
				continue;
			}

			// We don't honor sequential gating here on purpose: if a quest
			// has a silent-gate sequential first objective followed by a
			// popup payload sequential second objective, BOTH event names
			// should be in the cache so the gate event can actually arrive
			// and fire. ProcessEventForObjectives does the precise gating.
			ActiveEventListeners.Add(Objective.TargetEventOrId);
		}
	}

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOQuestSubsystem] Rebuilt event listener cache: %d entries"),
		ActiveEventListeners.Num());
}
