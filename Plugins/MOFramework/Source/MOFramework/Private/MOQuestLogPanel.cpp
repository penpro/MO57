#include "MOQuestLogPanel.h"
#include "CommonInputModeTypes.h"
#include "MOQuestSubsystem.h"
#include "MOQuestLogEntry.h"
#include "MOCommonButton.h"
#include "MOFramework.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"

UMOQuestLogPanel::UMOQuestLogPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOQuestLogPanel::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind buttons
	if (CloseButton)
	{
		CloseButton->OnClicked().RemoveAll(this);
		CloseButton->OnClicked().AddUObject(this, &UMOQuestLogPanel::HandleCloseClicked);
	}

	if (TrackButton)
	{
		TrackButton->OnClicked().RemoveAll(this);
		TrackButton->OnClicked().AddUObject(this, &UMOQuestLogPanel::HandleTrackClicked);
	}

	if (AbandonButton)
	{
		AbandonButton->OnClicked().RemoveAll(this);
		AbandonButton->OnClicked().AddUObject(this, &UMOQuestLogPanel::HandleAbandonClicked);
	}

	BindToQuestSubsystem();
	RefreshQuestList();
}

void UMOQuestLogPanel::NativeDestruct()
{
	UnbindFromQuestSubsystem();
	Super::NativeDestruct();
}

TOptional<FUIInputConfig> UMOQuestLogPanel::GetDesiredInputConfig() const
{
	return FUIInputConfig(
		ECommonInputMode::Menu,
		EMouseCaptureMode::NoCapture,
		EMouseLockMode::DoNotLock,
		false
	);
}

UWidget* UMOQuestLogPanel::NativeGetDesiredFocusTarget() const
{
	// Focus on first quest entry or close button
	if (QuestEntries.Num() > 0 && QuestEntries[0])
	{
		return QuestEntries[0];
	}
	return CloseButton;
}

void UMOQuestLogPanel::BindToQuestSubsystem()
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

	Subsystem->OnQuestStarted.AddDynamic(this, &UMOQuestLogPanel::HandleQuestStateChanged);
	Subsystem->OnQuestCompleted.AddDynamic(this, &UMOQuestLogPanel::HandleQuestStateChanged);
	Subsystem->OnQuestAbandoned.AddDynamic(this, &UMOQuestLogPanel::HandleQuestStateChanged);
}

void UMOQuestLogPanel::UnbindFromQuestSubsystem()
{
	if (UMOQuestSubsystem* Subsystem = QuestSubsystem.Get())
	{
		Subsystem->OnQuestStarted.RemoveDynamic(this, &UMOQuestLogPanel::HandleQuestStateChanged);
		Subsystem->OnQuestCompleted.RemoveDynamic(this, &UMOQuestLogPanel::HandleQuestStateChanged);
		Subsystem->OnQuestAbandoned.RemoveDynamic(this, &UMOQuestLogPanel::HandleQuestStateChanged);
	}
}

void UMOQuestLogPanel::HandleQuestStateChanged(FName QuestId)
{
	RefreshQuestList();
}

void UMOQuestLogPanel::RefreshQuestList()
{
	if (!QuestListScrollBox)
	{
		return;
	}

	UMOQuestSubsystem* Subsystem = QuestSubsystem.Get();
	if (!Subsystem)
	{
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
	QuestListScrollBox->ClearChildren();
	QuestEntries.Empty();

	if (!QuestEntryClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOQuestLogPanel] QuestEntryClass not set"));
		return;
	}

	// Get active quests
	TArray<FMOQuestState> ActiveQuests = Subsystem->GetActiveQuests();

	// Sort by sort order from definition
	ActiveQuests.Sort([Subsystem](const FMOQuestState& A, const FMOQuestState& B)
	{
		const FMOQuestDefinitionRow* DefA = Subsystem->GetQuestDefinitionPtr(A.QuestId);
		const FMOQuestDefinitionRow* DefB = Subsystem->GetQuestDefinitionPtr(B.QuestId);
		const int32 OrderA = DefA ? DefA->SortOrder : 0;
		const int32 OrderB = DefB ? DefB->SortOrder : 0;
		return OrderA < OrderB;
	});

	// Create entries for active quests
	for (const FMOQuestState& State : ActiveQuests)
	{
		const FMOQuestDefinitionRow* Definition = Subsystem->GetQuestDefinitionPtr(State.QuestId);
		if (!Definition)
		{
			continue;
		}

		UMOQuestLogEntry* Entry = CreateWidget<UMOQuestLogEntry>(this, QuestEntryClass);
		if (!Entry)
		{
			continue;
		}

		FMOQuestLogDisplayData DisplayData = CreateQuestDisplayData(State, *Definition);
		Entry->SetupEntry(DisplayData);
		Entry->OnQuestSelected.BindUObject(this, &UMOQuestLogPanel::HandleQuestEntrySelected);

		QuestListScrollBox->AddChild(Entry);
		QuestEntries.Add(Entry);
	}

	// Completed quests section (could add a separator here)
	TArray<FName> CompletedIds = Subsystem->GetCompletedQuestIds();
	for (const FName& QuestId : CompletedIds)
	{
		const FMOQuestDefinitionRow* Definition = Subsystem->GetQuestDefinitionPtr(QuestId);
		if (!Definition)
		{
			continue;
		}

		UMOQuestLogEntry* Entry = CreateWidget<UMOQuestLogEntry>(this, QuestEntryClass);
		if (!Entry)
		{
			continue;
		}

		// Create display data for completed quest
		FMOQuestLogDisplayData DisplayData;
		DisplayData.QuestId = QuestId;
		DisplayData.DisplayName = Definition->DisplayName;
		DisplayData.Category = Definition->Category;
		DisplayData.bIsComplete = true;
		DisplayData.bIsTracked = false;
		DisplayData.bIsTutorial = Definition->bIsTutorial;
		DisplayData.CompletedObjectives = Definition->Objectives.Num();
		DisplayData.TotalObjectives = Definition->Objectives.Num();

		Entry->SetupEntry(DisplayData);
		Entry->OnQuestSelected.BindUObject(this, &UMOQuestLogPanel::HandleQuestEntrySelected);

		QuestListScrollBox->AddChild(Entry);
		QuestEntries.Add(Entry);
	}

	// Select first quest if none selected
	if (SelectedQuestId.IsNone() && QuestEntries.Num() > 0)
	{
		SelectQuest(QuestEntries[0]->GetQuestId());
	}
	else
	{
		RefreshDetailsPanel();
	}
}

void UMOQuestLogPanel::SelectQuest(FName QuestId)
{
	SelectedQuestId = QuestId;

	// Update selection state on entries
	for (UMOQuestLogEntry* Entry : QuestEntries)
	{
		if (Entry)
		{
			Entry->SetSelected(Entry->GetQuestId() == QuestId);
		}
	}

	RefreshDetailsPanel();
}

void UMOQuestLogPanel::RefreshDetailsPanel()
{
	UMOQuestSubsystem* Subsystem = QuestSubsystem.Get();
	if (!Subsystem)
	{
		return;
	}

	const FMOQuestDefinitionRow* Definition = Subsystem->GetQuestDefinitionPtr(SelectedQuestId);
	if (!Definition)
	{
		// Clear details panel
		if (SelectedQuestTitle) SelectedQuestTitle->SetText(FText::GetEmpty());
		if (SelectedQuestDescription) SelectedQuestDescription->SetText(FText::GetEmpty());
		if (ObjectivesContainer) ObjectivesContainer->ClearChildren();
		if (TrackButton) TrackButton->SetIsEnabled(false);
		if (AbandonButton) AbandonButton->SetIsEnabled(false);
		return;
	}

	// Set title and description
	if (SelectedQuestTitle)
	{
		SelectedQuestTitle->SetText(Definition->DisplayName);
	}

	if (SelectedQuestDescription)
	{
		SelectedQuestDescription->SetText(Definition->Description);
	}

	// Get state (may be complete)
	FMOQuestState State;
	const bool bQuestIsActive = Subsystem->GetQuestState(SelectedQuestId, State);
	const bool bIsComplete = Subsystem->IsQuestComplete(SelectedQuestId);

	// Populate objectives
	if (bQuestIsActive)
	{
		PopulateObjectives(State, *Definition);
	}
	else if (bIsComplete)
	{
		// Show all objectives as complete
		FMOQuestState CompletedState;
		CompletedState.QuestId = SelectedQuestId;
		CompletedState.bIsComplete = true;
		for (const FMOQuestObjective& Obj : Definition->Objectives)
		{
			CompletedState.ObjectiveProgress.Add(Obj.ObjectiveId, Obj.RequiredCount);
			CompletedState.CompletedObjectives.Add(Obj.ObjectiveId);
		}
		PopulateObjectives(CompletedState, *Definition);
	}

	// Update button states
	if (TrackButton)
	{
		TrackButton->SetIsEnabled(bQuestIsActive);
		if (TrackButtonText)
		{
			const FText Label = State.bIsTracked
				? NSLOCTEXT("MOQuest", "Untrack", "Untrack")
				: NSLOCTEXT("MOQuest", "Track", "Track");
			TrackButtonText->SetText(Label);
		}
	}

	if (AbandonButton)
	{
		// Can only abandon active, non-tutorial quests
		const bool bCanAbandon = bQuestIsActive && !Definition->bIsTutorial;
		AbandonButton->SetIsEnabled(bCanAbandon);
	}
}

void UMOQuestLogPanel::PopulateObjectives(const FMOQuestState& State, const FMOQuestDefinitionRow& Definition)
{
	if (!ObjectivesContainer)
	{
		return;
	}

	ObjectivesContainer->ClearChildren();

	for (const FMOQuestObjective& Objective : Definition.Objectives)
	{
		// Create a simple text block for each objective
		// In a full implementation, you'd create a dedicated objective entry widget
		UTextBlock* ObjectiveText = NewObject<UTextBlock>(this);
		if (!ObjectiveText)
		{
			continue;
		}

		const int32 Progress = State.GetObjectiveProgress(Objective.ObjectiveId);
		const bool bIsComplete = State.IsObjectiveComplete(Objective.ObjectiveId);

		FString ObjectiveString;
		if (bIsComplete)
		{
			ObjectiveString = FString::Printf(TEXT("[X] %s"), *Objective.Description.ToString());
		}
		else if (Objective.RequiredCount > 1)
		{
			ObjectiveString = FString::Printf(TEXT("[ ] %s (%d/%d)"),
				*Objective.Description.ToString(), Progress, Objective.RequiredCount);
		}
		else
		{
			ObjectiveString = FString::Printf(TEXT("[ ] %s"), *Objective.Description.ToString());
		}

		if (Objective.bOptional)
		{
			ObjectiveString += TEXT(" (Optional)");
		}

		ObjectiveText->SetText(FText::FromString(ObjectiveString));
		ObjectivesContainer->AddChild(ObjectiveText);
	}
}

FMOQuestLogDisplayData UMOQuestLogPanel::CreateQuestDisplayData(
	const FMOQuestState& State,
	const FMOQuestDefinitionRow& Definition) const
{
	FMOQuestLogDisplayData Data;
	Data.QuestId = State.QuestId;
	Data.DisplayName = Definition.DisplayName;
	Data.Category = Definition.Category;
	Data.bIsComplete = State.bIsComplete;
	Data.bIsTracked = State.bIsTracked;
	Data.bIsTutorial = Definition.bIsTutorial;

	// Count completed objectives
	Data.TotalObjectives = Definition.Objectives.Num();
	Data.CompletedObjectives = State.CompletedObjectives.Num();

	return Data;
}

void UMOQuestLogPanel::HandleQuestEntrySelected(FName QuestId)
{
	SelectQuest(QuestId);
}

void UMOQuestLogPanel::HandleCloseClicked()
{
	OnCloseRequested.Broadcast();
	DeactivateWidget();
}

void UMOQuestLogPanel::HandleTrackClicked()
{
	ToggleTrackSelectedQuest();
}

void UMOQuestLogPanel::HandleAbandonClicked()
{
	AbandonSelectedQuest();
}

void UMOQuestLogPanel::ToggleTrackSelectedQuest()
{
	if (SelectedQuestId.IsNone())
	{
		return;
	}

	UMOQuestSubsystem* Subsystem = QuestSubsystem.Get();
	if (!Subsystem)
	{
		return;
	}

	FMOQuestState State;
	if (Subsystem->GetQuestState(SelectedQuestId, State))
	{
		Subsystem->SetQuestTracked(SelectedQuestId, !State.bIsTracked);
		RefreshQuestList();
	}
}

void UMOQuestLogPanel::AbandonSelectedQuest()
{
	if (SelectedQuestId.IsNone())
	{
		return;
	}

	UMOQuestSubsystem* Subsystem = QuestSubsystem.Get();
	if (!Subsystem)
	{
		return;
	}

	Subsystem->AbandonQuest(SelectedQuestId);
	SelectedQuestId = NAME_None;
	RefreshQuestList();
}

void UMOQuestLogPanel::SetQuestEntryClass(TSubclassOf<UMOQuestLogEntry> InClass)
{
	QuestEntryClass = InClass;
}
