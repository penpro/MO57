#include "MOSurvivorTaskMenu.h"
#include "MOSurvivorCommands.h"
#include "MOFramework.h"
#include "CommonInputModeTypes.h"
#include "MOSurvivorJobQueueComponent.h"
#include "MOSurvivorJobDatabaseSettings.h"
#include "MOSurvivorTaskEntryWidget.h"
#include "MOSurvivorJobEntryWidget.h"
#include "MOIdentityComponent.h"
#include "MOCommonButton.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "GameFramework/Pawn.h"

UMOSurvivorTaskMenu::UMOSurvivorTaskMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Enable keyboard input for this widget (needed for Tab/Escape to close)
	SetIsFocusable(true);
}

void UMOSurvivorTaskMenu::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind close button
	if (CloseButton)
	{
		CloseButton->OnClicked().RemoveAll(this);
		CloseButton->OnClicked().AddUObject(this, &UMOSurvivorTaskMenu::HandleCloseClicked);
	}

	// Set keyboard focus to this widget so Tab/Escape work
	SetFocus();
}

void UMOSurvivorTaskMenu::NativeDestruct()
{
	// Unbind from job queue
	if (UMOSurvivorJobQueueComponent* JobQueue = CachedJobQueue.Get())
	{
		JobQueue->OnQueueChanged.RemoveDynamic(this, &UMOSurvivorTaskMenu::OnQueueChanged);
	}

	TargetSurvivor.Reset();
	CachedJobQueue.Reset();

	Super::NativeDestruct();
}

TOptional<FUIInputConfig> UMOSurvivorTaskMenu::GetDesiredInputConfig() const
{
	return FUIInputConfig(
		ECommonInputMode::Menu,
		EMouseCaptureMode::NoCapture,
		EMouseLockMode::DoNotLock,
		false
	);
}

bool UMOSurvivorTaskMenu::NativeOnCloseKeyRequested(const FKeyEvent& InKeyEvent)
{
	// Broadcast OnRequestClose so the spawning controller's cleanup runs; the
	// base default is a bare DeactivateWidget that would skip it.
	OnRequestClose.Broadcast();
	return true;
}

void UMOSurvivorTaskMenu::InitializeForSurvivor(APawn* Survivor)
{
	if (!IsValid(Survivor))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorTaskMenu] InitializeForSurvivor called with invalid survivor"));
		return;
	}

	TargetSurvivor = Survivor;

	// Cache job queue and bind to changes
	CachedJobQueue = Survivor->FindComponentByClass<UMOSurvivorJobQueueComponent>();
	if (UMOSurvivorJobQueueComponent* JobQueue = CachedJobQueue.Get())
	{
		JobQueue->OnQueueChanged.RemoveDynamic(this, &UMOSurvivorTaskMenu::OnQueueChanged);
		JobQueue->OnQueueChanged.AddDynamic(this, &UMOSurvivorTaskMenu::OnQueueChanged);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorTaskMenu] Initialized for survivor %s (JobQueue: %s)"),
		*Survivor->GetName(),
		CachedJobQueue.IsValid() ? TEXT("Yes") : TEXT("No"));

	// Update UI
	UpdateDisplayTexts();
	PopulateTaskList();
	RefreshJobQueue();
}

void UMOSurvivorTaskMenu::HandleCloseClicked()
{
	OnRequestClose.Broadcast();
}

void UMOSurvivorTaskMenu::OnQueueChanged()
{
	RefreshJobQueue();
	UpdateDisplayTexts();
}

void UMOSurvivorTaskMenu::UpdateDisplayTexts()
{
	APawn* Survivor = TargetSurvivor.Get();
	if (!Survivor)
	{
		return;
	}

	// Update survivor name
	if (SurvivorNameText)
	{
		FText SurvivorName = FText::FromString(TEXT("Survivor"));

		if (UMOIdentityComponent* IdentityComp = Survivor->FindComponentByClass<UMOIdentityComponent>())
		{
			if (!IdentityComp->DisplayName.IsEmpty())
			{
				SurvivorName = IdentityComp->DisplayName;
			}
		}

		SurvivorNameText->SetText(SurvivorName);
	}

	// Update current job text
	if (CurrentJobText)
	{
		FText JobText = FText::FromString(TEXT("Idle"));

		if (UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue())
		{
			FMOSurvivorJobEntry CurrentJob = JobQueue->GetCurrentJob();
			if (CurrentJob.IsValid())
			{
				const FMOSurvivorJobDefinitionRow* JobDef = JobQueue->GetJobDefinitionPtr(CurrentJob.JobType);
				if (JobDef)
				{
					JobText = JobDef->DisplayName;
				}
			}
		}

		CurrentJobText->SetText(JobText);
	}
}

void UMOSurvivorTaskMenu::PopulateTaskList()
{
	if (!TaskListScrollBox)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorTaskMenu] PopulateTaskList: No TaskListScrollBox bound"));
		return;
	}

	TaskListScrollBox->ClearChildren();

	// Check for widget class
	if (!TaskEntryWidgetClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorTaskMenu] PopulateTaskList: No TaskEntryWidgetClass configured"));
		return;
	}

	// Get all work tasks from centralized settings (excludes commands)
	TArray<FMOSurvivorJobDefinitionRow> JobDefs;
	UMOSurvivorJobDatabaseSettings::GetAllWorkTasks(JobDefs);

	if (JobDefs.Num() == 0)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorTaskMenu] No job definitions found! Check Project Settings > Plugins > MO Survivor Job Database"));
	}
	else
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorTaskMenu] Found %d work task definitions"), JobDefs.Num());
	}

	// Create entry widgets for each job definition
	for (const FMOSurvivorJobDefinitionRow& JobDef : JobDefs)
	{
		// Skip "None" type
		if (JobDef.JobType == EMOSurvivorJobType::None)
		{
			continue;
		}

		UMOSurvivorTaskEntryWidget* EntryWidget = CreateWidget<UMOSurvivorTaskEntryWidget>(this, TaskEntryWidgetClass);
		if (!EntryWidget)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorTaskMenu] Failed to create task entry widget for job type %d"),
				static_cast<int32>(JobDef.JobType));
			continue;
		}

		// Initialize with job definition data
		EntryWidget->InitializeFromJobDefinition(JobDef);

		// Bind to task clicked delegate
		EntryWidget->OnTaskClicked.AddDynamic(this, &UMOSurvivorTaskMenu::HandleTaskClicked);

		// Add to scroll box
		TaskListScrollBox->AddChild(EntryWidget);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorTaskMenu] Created %d task entry widgets"),
		TaskListScrollBox->GetChildrenCount());
}

void UMOSurvivorTaskMenu::RefreshJobQueue()
{
	if (!JobQueueScrollBox)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorTaskMenu] RefreshJobQueue: No JobQueueScrollBox bound"));
		return;
	}

	JobQueueScrollBox->ClearChildren();

	// Check for widget class
	if (!JobEntryWidgetClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorTaskMenu] RefreshJobQueue: No JobEntryWidgetClass configured"));
		return;
	}

	UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue();
	if (!JobQueue)
	{
		return;
	}

	TArray<FMOSurvivorJobEntry> Jobs = JobQueue->GetAllJobs();
	// DEBUG (MOSurvivorTaskMenu, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOSurvivorTaskMenu] Displaying %d jobs in queue"), Jobs.Num());   // every replicated job change: verbose

	// Create entry widgets for each job in queue
	for (const FMOSurvivorJobEntry& Job : Jobs)
	{
		// Skip invalid or completed jobs
		if (!Job.IsValid() || Job.State == EMOSurvivorJobState::Completed ||
			Job.State == EMOSurvivorJobState::Cancelled)
		{
			continue;
		}

		// Get job definition for display info
		const FMOSurvivorJobDefinitionRow* JobDef = JobQueue->GetJobDefinitionPtr(Job.JobType);
		if (!JobDef)
		{
			continue;
		}

		UMOSurvivorJobEntryWidget* EntryWidget = CreateWidget<UMOSurvivorJobEntryWidget>(this, JobEntryWidgetClass);
		if (!EntryWidget)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorTaskMenu] Failed to create job entry widget for job %s"),
				*Job.JobId.ToString());
			continue;
		}

		// Initialize with job data
		EntryWidget->InitializeFromJobEntry(Job, *JobDef);

		// Bind to cancel delegate
		EntryWidget->OnJobCancelled.AddDynamic(this, &UMOSurvivorTaskMenu::HandleJobCancelled);

		// Add to scroll box
		JobQueueScrollBox->AddChild(EntryWidget);
	}
}

UMOSurvivorJobQueueComponent* UMOSurvivorTaskMenu::GetJobQueue() const
{
	if (CachedJobQueue.IsValid())
	{
		return CachedJobQueue.Get();
	}

	APawn* Survivor = TargetSurvivor.Get();
	if (Survivor)
	{
		return Survivor->FindComponentByClass<UMOSurvivorJobQueueComponent>();
	}

	return nullptr;
}

void UMOSurvivorTaskMenu::HandleTaskClicked(EMOSurvivorJobType JobType)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorTaskMenu] HandleTaskClicked received JobType: %d"),
		static_cast<int32>(JobType));

	if (JobType == EMOSurvivorJobType::None)
	{
		return;
	}

	// The order goes to the server (FMOSurvivorCommands): the job queue changes only there, and the new job arrives here by replication
	// (OnQueueChanged). A joiner's direct EnqueueJob was refused ("called on non-authority").
	FMOSurvivorCommand Command = FMOSurvivorCommand::Make(EMOSurvivorCommandKind::EnqueueJob);
	Command.JobType = JobType;
	Command.RepeatCount = 1;   // (can be adjusted later)
	const bool bSent = FMOSurvivorCommands::Submit(GetOwningPlayer(), TargetSurvivor.Get(), Command);
	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorTaskMenu] Job type %d %s"), static_cast<int32>(JobType), bSent ? TEXT("ordered") : TEXT("NOT ordered (no survivor)"));
}

void UMOSurvivorTaskMenu::HandleJobCancelled(FGuid JobId)
{
	if (!JobId.IsValid())
	{
		return;
	}

	FMOSurvivorCommand Command = FMOSurvivorCommand::Make(EMOSurvivorCommandKind::CancelJob);
	Command.JobId = JobId;
	const bool bSent = FMOSurvivorCommands::Submit(GetOwningPlayer(), TargetSurvivor.Get(), Command);
	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorTaskMenu] Cancel of job %s %s"), *JobId.ToString(), bSent ? TEXT("ordered") : TEXT("NOT ordered (no survivor)"));
}
