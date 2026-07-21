/**
 * MOQueueRendererBase.cpp - shared queue container lifecycle (Stage 3)
 * See MOQueueRendererBase.h for the adapter seam and compatibility contract.
 */

#include "MOQueueRendererBase.h"
#include "MOFramework.h"
#include "MOQueueRowWidgetBase.h"
#include "MOCommonButton.h"
#include "MOUIUtils.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/PanelWidget.h"

UMOQueueRendererBase::UMOQueueRendererBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOQueueRendererBase::RefreshRows()
{
	// Teardown: destroy and recreate every row (legacy parity — keeps the
	// per-fresh-widget intent binding trivially idempotent).
	for (UMOQueueRowWidgetBase* RowWidget : RowWidgets)
	{
		if (RowWidget)
		{
			RowWidget->OnCancelIntent.RemoveAll(this);
			RowWidget->RemoveFromParent();
		}
	}
	RowWidgets.Empty();
	LastBuiltRows.Empty();

	// Unbound source: leave visuals untouched (legacy early-return parity).
	if (!HasQueueSource())
	{
		return;
	}

	TArray<FMOQueueDisplayRow> Rows;
	BuildDisplayRows(Rows);
	for (FMOQueueDisplayRow& Row : Rows)
	{
		MOQueueDisplay::FinalizeRowTexts(Row);
	}
	LastBuiltRows = Rows;

	// Empty state (before the container/class early-out, legacy parity).
	if (EmptyQueueText)
	{
		EmptyQueueText->SetVisibility(Rows.Num() == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	UPanelWidget* Container = GetRowContainer();
	if (Container && RowWidgetClass)
	{
		for (const FMOQueueDisplayRow& Row : Rows)
		{
			UMOQueueRowWidgetBase* RowWidget = CreateWidget<UMOQueueRowWidgetBase>(this, RowWidgetClass);
			if (!RowWidget)
			{
				continue;
			}

			RowWidget->SetRow(Row);
			RowWidget->OnCancelIntent.AddDynamic(this, &UMOQueueRendererBase::HandleRowCancelIntent);

			// Let the domain synchronize legacy display data BEFORE the visual
			// notification, so legacy BP events see consistent values.
			OnRowWidgetBound(RowWidget, Row);
			RowWidget->NotifyVisualsUpdated();

			Container->AddChild(RowWidget);
			RowWidgets.Add(RowWidget);
		}
	}

	// Header block (rebuild path updates the title; the tick path does not —
	// legacy parity).
	FMOQueueHeaderDisplay Header;
	GetHeaderDisplay(Header);
	if (Header.bHasRows)
	{
		if (CurrentCraftNameText)
		{
			CurrentCraftNameText->SetText(Header.ActiveTitle);
		}
		if (CurrentProgressBar)
		{
			CurrentProgressBar->SetPercent(Header.Progress);
		}
		if (ProgressText)
		{
			ProgressText->SetText(MOQueueDisplay::FormatPercent(Header.Progress));
		}
		const FText RemainingText = Header.RemainingSeconds >= 0.0f
			? UMOUIUtils::FormatDurationAsText(Header.RemainingSeconds)
			: FText::GetEmpty();
		if (TimeRemainingText)
		{
			TimeRemainingText->SetText(RemainingText);
		}
		if (TotalTimeRemainingText)
		{
			TotalTimeRemainingText->SetText(RemainingText);
		}
	}
	else
	{
		if (CurrentCraftNameText)
		{
			CurrentCraftNameText->SetText(FText::GetEmpty());
		}
		ClearHeaderDisplay();
	}

	NotifyRowsRefreshed(Rows.Num());
}

void UMOQueueRendererBase::UpdateProgressDisplay()
{
	FMOQueueHeaderDisplay Header;
	const bool bSourced = HasQueueSource();
	if (bSourced)
	{
		GetHeaderDisplay(Header);
	}

	if (!bSourced || !Header.bHasRows)
	{
		// Empty / lost source: clear the numeric header (name is owned by the
		// rebuild path) and tell the domain so legacy BP events fire.
		ClearHeaderDisplay();
		NotifyProgressUpdated(0.0f, FText::GetEmpty());
		return;
	}

	const FText RemainingText = Header.RemainingSeconds >= 0.0f
		? UMOUIUtils::FormatDurationAsText(Header.RemainingSeconds)
		: FText::GetEmpty();

	if (CurrentProgressBar)
	{
		CurrentProgressBar->SetPercent(Header.Progress);
	}
	if (ProgressText)
	{
		ProgressText->SetText(MOQueueDisplay::FormatPercent(Header.Progress));
	}
	if (TimeRemainingText)
	{
		TimeRemainingText->SetText(RemainingText);
	}
	if (TotalTimeRemainingText)
	{
		TotalTimeRemainingText->SetText(RemainingText);
	}

	// Row 0 live progress (the only row that ticks — legacy parity).
	float ActiveProgress = 0.0f;
	float ActiveRemaining = -1.0f;
	if (RowWidgets.Num() > 0 && RowWidgets[0] && GetActiveRowLiveProgress(ActiveProgress, ActiveRemaining))
	{
		const FText ActiveRemainingText = ActiveRemaining >= 0.0f
			? UMOUIUtils::FormatDurationAsText(ActiveRemaining)
			: FText::GetEmpty();
		RowWidgets[0]->UpdateLiveProgress(ActiveProgress, ActiveRemainingText);
	}

	NotifyProgressUpdated(Header.Progress, RemainingText);
}

void UMOQueueRendererBase::RequestCancelAll()
{
	HandleCancelAllClicked();
}

UMOQueueRowWidgetBase* UMOQueueRendererBase::GetRowWidgetAt(int32 Index) const
{
	return RowWidgets.IsValidIndex(Index) ? RowWidgets[Index].Get() : nullptr;
}

bool UMOQueueRendererBase::IsShowingEmptyState() const
{
	return EmptyQueueText && EmptyQueueText->GetVisibility() == ESlateVisibility::Visible;
}

void UMOQueueRendererBase::GetHeaderDisplay_Implementation(FMOQueueHeaderDisplay& OutHeader) const
{
	// Default: mirror row 0 (fits single-operation domains like building).
	// Multi-row domains override for overall/total semantics.
	OutHeader = FMOQueueHeaderDisplay();
	if (LastBuiltRows.Num() > 0)
	{
		const FMOQueueDisplayRow& Active = LastBuiltRows[0];
		OutHeader.ActiveTitle = Active.Title;
		OutHeader.Progress = Active.Progress;
		OutHeader.RemainingSeconds = Active.RemainingSeconds;
		OutHeader.bHasRows = true;
	}
}

void UMOQueueRendererBase::NativeConstruct()
{
	Super::NativeConstruct();

	// F18: idempotent CancelAll binding.
	if (CancelAllButton)
	{
		CancelAllButton->OnClicked().RemoveAll(this);
		CancelAllButton->OnClicked().AddUObject(this, &UMOQueueRendererBase::HandleCancelAllClicked);
	}
}

void UMOQueueRendererBase::NativeDestruct()
{
	if (CancelAllButton)
	{
		CancelAllButton->OnClicked().RemoveAll(this);
	}

	for (UMOQueueRowWidgetBase* RowWidget : RowWidgets)
	{
		if (RowWidget)
		{
			RowWidget->OnCancelIntent.RemoveAll(this);
		}
	}

	Super::NativeDestruct();
}

void UMOQueueRendererBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	TimeSinceLastUpdate += InDeltaTime;
	if (TimeSinceLastUpdate >= ProgressUpdateInterval)
	{
		TimeSinceLastUpdate = 0.0f;
		UpdateProgressDisplay();
	}
}

void UMOQueueRendererBase::HandleRowCancelIntent(const FGuid& RowId)
{
	// Observers first, then the domain executes. The shared layer never mutates
	// gameplay; row removal flows back via the domain's source events.
	OnCancelRowIntent.Broadcast(RowId);
	ExecuteCancelRow(RowId);
}

void UMOQueueRendererBase::HandleCancelAllClicked()
{
	OnCancelAllIntent.Broadcast();
	ExecuteCancelAll();
}

void UMOQueueRendererBase::ClearHeaderDisplay()
{
	if (CurrentProgressBar)
	{
		CurrentProgressBar->SetPercent(0.0f);
	}
	if (ProgressText)
	{
		ProgressText->SetText(FText::GetEmpty());
	}
	if (TimeRemainingText)
	{
		TimeRemainingText->SetText(FText::GetEmpty());
	}
	if (TotalTimeRemainingText)
	{
		TotalTimeRemainingText->SetText(FText::GetEmpty());
	}
}

UPanelWidget* UMOQueueRendererBase::GetRowContainer() const
{
	return QueueScrollBox ? Cast<UPanelWidget>(QueueScrollBox) : Cast<UPanelWidget>(QueueContainer);
}
