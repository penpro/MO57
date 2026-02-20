#include "MOQuestHUDWidget.h"
#include "MOQuestSubsystem.h"
#include "MOQuestTrackerEntry.h"
#include "MOFramework.h"
#include "Components/VerticalBox.h"
#include "Kismet/GameplayStatics.h"

UMOQuestHUDWidget::UMOQuestHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOQuestHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindToQuestSubsystem();
	RefreshTrackedQuests();
}

void UMOQuestHUDWidget::NativeDestruct()
{
	UnbindFromQuestSubsystem();
	Super::NativeDestruct();
}

void UMOQuestHUDWidget::BindToQuestSubsystem()
{
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
	if (!GameInstance)
	{
		return;
	}

	UMOQuestSubsystem* Subsystem = GameInstance->GetSubsystem<UMOQuestSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	QuestSubsystem = Subsystem;

	Subsystem->OnQuestStarted.AddDynamic(this, &UMOQuestHUDWidget::HandleQuestStarted);
	Subsystem->OnObjectiveUpdated.AddDynamic(this, &UMOQuestHUDWidget::HandleObjectiveUpdated);
	Subsystem->OnObjectiveCompleted.AddDynamic(this, &UMOQuestHUDWidget::HandleObjectiveCompleted);
	Subsystem->OnQuestCompleted.AddDynamic(this, &UMOQuestHUDWidget::HandleQuestCompleted);
	Subsystem->OnQuestAbandoned.AddDynamic(this, &UMOQuestHUDWidget::HandleQuestAbandoned);
}

void UMOQuestHUDWidget::UnbindFromQuestSubsystem()
{
	if (UMOQuestSubsystem* Subsystem = QuestSubsystem.Get())
	{
		Subsystem->OnQuestStarted.RemoveDynamic(this, &UMOQuestHUDWidget::HandleQuestStarted);
		Subsystem->OnObjectiveUpdated.RemoveDynamic(this, &UMOQuestHUDWidget::HandleObjectiveUpdated);
		Subsystem->OnObjectiveCompleted.RemoveDynamic(this, &UMOQuestHUDWidget::HandleObjectiveCompleted);
		Subsystem->OnQuestCompleted.RemoveDynamic(this, &UMOQuestHUDWidget::HandleQuestCompleted);
		Subsystem->OnQuestAbandoned.RemoveDynamic(this, &UMOQuestHUDWidget::HandleQuestAbandoned);
	}
}

void UMOQuestHUDWidget::RefreshTrackedQuests()
{
	if (!TrackedQuestsContainer)
	{
		return;
	}

	UMOQuestSubsystem* Subsystem = QuestSubsystem.Get();
	if (!Subsystem)
	{
		// Try to get it again
		if (UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this))
		{
			Subsystem = GameInstance->GetSubsystem<UMOQuestSubsystem>();
			QuestSubsystem = Subsystem;
		}
	}

	if (!Subsystem)
	{
		return;
	}

	// Clear existing entries
	TrackedQuestsContainer->ClearChildren();
	TrackerEntries.Empty();

	// Get tracked quests
	TArray<FMOQuestState> TrackedQuests = Subsystem->GetTrackedQuests();

	// Create entries
	for (const FMOQuestState& State : TrackedQuests)
	{
		if (!TrackerEntryClass)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQuestHUDWidget] TrackerEntryClass not set"));
			return;
		}

		UMOQuestTrackerEntry* Entry = CreateWidget<UMOQuestTrackerEntry>(this, TrackerEntryClass);
		if (!Entry)
		{
			continue;
		}

		FMOQuestTrackerDisplayData DisplayData = CreateDisplayData(State);
		Entry->SetupEntry(DisplayData);

		TrackedQuestsContainer->AddChild(Entry);
		TrackerEntries.Add(Entry);
	}
}

void UMOQuestHUDWidget::SetTrackerEntryClass(TSubclassOf<UMOQuestTrackerEntry> InClass)
{
	TrackerEntryClass = InClass;
}

FMOQuestTrackerDisplayData UMOQuestHUDWidget::CreateDisplayData(const FMOQuestState& State) const
{
	FMOQuestTrackerDisplayData Data;
	Data.QuestId = State.QuestId;
	Data.bIsComplete = State.bIsComplete;

	UMOQuestSubsystem* Subsystem = QuestSubsystem.Get();
	if (!Subsystem)
	{
		return Data;
	}

	const FMOQuestDefinitionRow* Definition = Subsystem->GetQuestDefinitionPtr(State.QuestId);
	if (!Definition)
	{
		return Data;
	}

	Data.QuestName = Definition->DisplayName;

	// Find current objective (first incomplete non-optional, or first incomplete)
	const FMOQuestObjective* CurrentObjective = nullptr;
	for (const FMOQuestObjective& Objective : Definition->Objectives)
	{
		if (!State.IsObjectiveComplete(Objective.ObjectiveId))
		{
			CurrentObjective = &Objective;
			break;
		}
	}

	if (CurrentObjective)
	{
		Data.CurrentObjectiveText = CurrentObjective->Description;
		Data.RequiredProgress = CurrentObjective->RequiredCount;
		Data.CurrentProgress = State.GetObjectiveProgress(CurrentObjective->ObjectiveId);
		Data.ProgressPercent = Data.RequiredProgress > 0
			? static_cast<float>(Data.CurrentProgress) / static_cast<float>(Data.RequiredProgress)
			: 0.0f;
	}

	return Data;
}

UMOQuestTrackerEntry* UMOQuestHUDWidget::FindEntryForQuest(FName QuestId) const
{
	for (UMOQuestTrackerEntry* Entry : TrackerEntries)
	{
		if (Entry && Entry->GetQuestId() == QuestId)
		{
			return Entry;
		}
	}
	return nullptr;
}

void UMOQuestHUDWidget::HandleQuestStarted(FName QuestId)
{
	// Refresh the entire list when a new quest starts
	RefreshTrackedQuests();
}

void UMOQuestHUDWidget::HandleObjectiveUpdated(FName QuestId, FName ObjectiveId, int32 NewProgress)
{
	// Find the entry and update it
	if (UMOQuestTrackerEntry* Entry = FindEntryForQuest(QuestId))
	{
		UMOQuestSubsystem* Subsystem = QuestSubsystem.Get();
		if (Subsystem)
		{
			FMOQuestState State;
			if (Subsystem->GetQuestState(QuestId, State))
			{
				FMOQuestTrackerDisplayData DisplayData = CreateDisplayData(State);
				Entry->SetupEntry(DisplayData);
				Entry->PlayUpdateAnimation();
			}
		}
	}
}

void UMOQuestHUDWidget::HandleObjectiveCompleted(FName QuestId, FName ObjectiveId)
{
	// Refresh to show next objective
	if (UMOQuestTrackerEntry* Entry = FindEntryForQuest(QuestId))
	{
		UMOQuestSubsystem* Subsystem = QuestSubsystem.Get();
		if (Subsystem)
		{
			FMOQuestState State;
			if (Subsystem->GetQuestState(QuestId, State))
			{
				FMOQuestTrackerDisplayData DisplayData = CreateDisplayData(State);
				Entry->SetupEntry(DisplayData);
			}
		}
	}
}

void UMOQuestHUDWidget::HandleQuestCompleted(FName QuestId)
{
	// Play completion animation then refresh
	if (UMOQuestTrackerEntry* Entry = FindEntryForQuest(QuestId))
	{
		Entry->PlayCompletionAnimation();
	}

	// Refresh after a short delay to show completion state
	// In real implementation, this might be triggered after the animation
	RefreshTrackedQuests();
}

void UMOQuestHUDWidget::HandleQuestAbandoned(FName QuestId)
{
	RefreshTrackedQuests();
}
