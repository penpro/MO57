/**
 * MOCraftingQueueEntryWidget.cpp - crafting compat over the shared queue row
 * (migration Stage 3). See the header for the compatibility contract.
 */

#include "MOCraftingQueueEntryWidget.h"
#include "MOFramework.h"

UMOCraftingQueueEntryWidget::UMOCraftingQueueEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOCraftingQueueEntryWidget::SetupEntry(const FMOQueueEntryDisplayData& InData)
{
	// Legacy entry point (BP/tests): mirror the legacy struct, translate to the
	// neutral row, render via the base, then fire the legacy visual event —
	// the same order the shared renderer uses.
	LegacyData = InData;

	FMOQueueDisplayRow NeutralRow;
	NeutralRow.RowId = InData.EntryId;
	NeutralRow.SourceId = InData.RecipeId;
	NeutralRow.Title = InData.RecipeName;
	NeutralRow.Icon = InData.Icon;
	NeutralRow.CountText = InData.CountText;
	NeutralRow.Progress = InData.Progress;
	NeutralRow.TimeRemainingText = InData.TimeRemainingText;
	NeutralRow.State = InData.bIsActive ? EMOQueueRowState::Active : EMOQueueRowState::Queued;
	SetRow(NeutralRow);

	NotifyVisualsUpdated();
}

void UMOCraftingQueueEntryWidget::UpdateProgress(float NewProgress, const FText& NewTimeRemaining)
{
	UpdateLiveProgress(NewProgress, NewTimeRemaining);
	LegacyData.Progress = NewProgress;
	LegacyData.TimeRemainingText = NewTimeRemaining;
}

void UMOCraftingQueueEntryWidget::NotifyVisualsUpdated()
{
	OnVisualsUpdated(LegacyData);
}

void UMOCraftingQueueEntryWidget::NotifyCancelIntent(const FGuid& InRowId)
{
	// Legacy delegate keeps broadcasting for existing BP listeners.
	OnCancelRequested.Broadcast(InRowId);
}

void UMOCraftingQueueEntryWidget::UpdateVisuals()
{
	UpdateRowVisuals();
	NotifyVisualsUpdated();
}
