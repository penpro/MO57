#include "MOSurvivorJobEntryWidget.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Engine/Texture2D.h"

void UMOSurvivorJobEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
		CancelButton->OnClicked().AddUObject(this, &UMOSurvivorJobEntryWidget::HandleCancelClicked);
	}
}

void UMOSurvivorJobEntryWidget::InitializeFromJobEntry(const FMOSurvivorJobEntry& JobEntry, const FMOSurvivorJobDefinitionRow& JobDef)
{
	JobId = JobEntry.JobId;
	CachedJobEntry = JobEntry;
	CachedJobDef = JobDef;

	// Update display
	if (JobNameText)
	{
		JobNameText->SetText(JobDef.DisplayName);
	}

	if (JobStateText)
	{
		JobStateText->SetText(GetStateDisplayText(JobEntry.State));
	}

	if (RepeatCountText)
	{
		if (JobEntry.RepeatCount > 1)
		{
			RepeatCountText->SetText(FText::Format(
				NSLOCTEXT("MOSurvivor", "RepeatCount", "{0}/{1}"),
				FText::AsNumber(JobEntry.CompletedCount),
				FText::AsNumber(JobEntry.RepeatCount)));
			RepeatCountText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			RepeatCountText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (JobIcon && !JobDef.Icon.IsNull())
	{
		if (UTexture2D* IconTexture = JobDef.Icon.LoadSynchronous())
		{
			JobIcon->SetBrushFromTexture(IconTexture);
			JobIcon->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			JobIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else if (JobIcon)
	{
		JobIcon->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (ProgressBar)
	{
		ProgressBar->SetPercent(JobEntry.Progress);
		// Show progress bar only for active jobs
		ProgressBar->SetVisibility(JobEntry.State == EMOSurvivorJobState::Active ||
			JobEntry.State == EMOSurvivorJobState::Performing
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOSurvivorJobEntry] Initialized for job %s: %s (State: %d)"),
		*JobId.ToString(), *JobDef.DisplayName.ToString(), static_cast<int32>(JobEntry.State));
}

void UMOSurvivorJobEntryWidget::UpdateProgress(float Progress)
{
	CachedJobEntry.Progress = Progress;

	if (ProgressBar)
	{
		ProgressBar->SetPercent(Progress);
	}
}

void UMOSurvivorJobEntryWidget::UpdateState(EMOSurvivorJobState NewState)
{
	CachedJobEntry.State = NewState;

	if (JobStateText)
	{
		JobStateText->SetText(GetStateDisplayText(NewState));
	}

	if (ProgressBar)
	{
		// Show progress bar only for active jobs
		ProgressBar->SetVisibility(NewState == EMOSurvivorJobState::Active ||
			NewState == EMOSurvivorJobState::Performing
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}
}

void UMOSurvivorJobEntryWidget::HandleCancelClicked()
{
	if (JobId.IsValid())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorJobEntry] Cancel clicked for job %s"),
			*JobId.ToString());
		OnJobCancelled.Broadcast(JobId);
	}
}

FText UMOSurvivorJobEntryWidget::GetStateDisplayText(EMOSurvivorJobState State) const
{
	switch (State)
	{
	case EMOSurvivorJobState::Queued:
		return NSLOCTEXT("MOSurvivor", "StateQueued", "Queued");
	case EMOSurvivorJobState::Active:
		return NSLOCTEXT("MOSurvivor", "StateActive", "Active");
	case EMOSurvivorJobState::MovingToTarget:
		return NSLOCTEXT("MOSurvivor", "StateMoving", "Moving...");
	case EMOSurvivorJobState::Performing:
		return NSLOCTEXT("MOSurvivor", "StatePerforming", "Working...");
	case EMOSurvivorJobState::Returning:
		return NSLOCTEXT("MOSurvivor", "StateReturning", "Returning");
	case EMOSurvivorJobState::Completed:
		return NSLOCTEXT("MOSurvivor", "StateCompleted", "Done");
	case EMOSurvivorJobState::Failed:
		return NSLOCTEXT("MOSurvivor", "StateFailed", "Failed");
	case EMOSurvivorJobState::Cancelled:
		return NSLOCTEXT("MOSurvivor", "StateCancelled", "Cancelled");
	default:
		return NSLOCTEXT("MOSurvivor", "StateUnknown", "Unknown");
	}
}
