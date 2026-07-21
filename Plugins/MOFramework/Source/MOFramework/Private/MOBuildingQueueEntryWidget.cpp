/**
 * MOBuildingQueueEntryWidget.cpp - building compat over the shared queue row
 * (migration Stage 3). See the header for the compatibility contract.
 */

#include "MOBuildingQueueEntryWidget.h"
#include "MOFramework.h"
#include "MOBuildingQueueWidget.h"

UMOBuildingQueueEntryWidget::UMOBuildingQueueEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOBuildingQueueEntryWidget::SetupEntry(const FMOBuildQueueEntryDisplayData& InData)
{
	// Legacy entry point (BP/tests): mirror the legacy struct, translate to the
	// neutral row (3-state mapping preserved), render, fire the legacy event.
	LegacyData = InData;

	FMOQueueDisplayRow NeutralRow;
	NeutralRow.RowId = InData.EntryId;
	NeutralRow.SourceId = InData.RecipeId;
	NeutralRow.Title = InData.RecipeName;
	NeutralRow.Icon = InData.Icon;
	NeutralRow.CountText = InData.CountText;
	NeutralRow.Progress = InData.Progress;
	NeutralRow.TimeRemainingText = InData.TimeRemainingText;
	NeutralRow.State = UMOBuildingQueueWidget::RowStateForBuildState(InData.State);
	SetRow(NeutralRow);

	NotifyVisualsUpdated();
}

void UMOBuildingQueueEntryWidget::UpdateProgress(float NewProgress, const FText& NewTimeRemaining)
{
	UpdateLiveProgress(NewProgress, NewTimeRemaining);
	LegacyData.Progress = NewProgress;
	LegacyData.TimeRemainingText = NewTimeRemaining;
}

void UMOBuildingQueueEntryWidget::NotifyVisualsUpdated()
{
	OnVisualsUpdated(LegacyData);
}

void UMOBuildingQueueEntryWidget::NotifyCancelIntent(const FGuid& InRowId)
{
	// Legacy delegate keeps broadcasting for existing BP listeners.
	OnCancelRequested.Broadcast(InRowId);
}

void UMOBuildingQueueEntryWidget::UpdateVisuals()
{
	UpdateRowVisuals();
	NotifyVisualsUpdated();
}
