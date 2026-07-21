/**
 * MOCraftingQueueWidget.cpp - crafting adapter over the shared queue renderer
 * (migration Stage 3). See the header for the compatibility contract.
 */

#include "MOCraftingQueueWidget.h"
#include "MOFramework.h"
#include "MOCraftingQueueComponent.h"
#include "MOCraftingQueueEntryWidget.h"
#include "MOQueueRowWidgetBase.h"
#include "MORecipeDatabaseSettings.h"
#include "MORecipeDefinitionRow.h"
#include "MOUIUtils.h"

UMOCraftingQueueWidget::UMOCraftingQueueWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOCraftingQueueWidget::InitializeQueue(UMOCraftingQueueComponent* InQueueComponent)
{
	// Unbind from previous component (source-swap safe, legacy discipline).
	if (UMOCraftingQueueComponent* OldQueue = QueueComponent.Get())
	{
		OldQueue->OnQueueChanged.RemoveDynamic(this, &UMOCraftingQueueWidget::HandleQueueChanged);
		OldQueue->OnCraftProgress.RemoveDynamic(this, &UMOCraftingQueueWidget::HandleCraftProgress);
		OldQueue->OnCraftCompleted.RemoveDynamic(this, &UMOCraftingQueueWidget::HandleCraftCompleted);
	}

	QueueComponent = InQueueComponent;

	if (InQueueComponent)
	{
		InQueueComponent->OnQueueChanged.AddDynamic(this, &UMOCraftingQueueWidget::HandleQueueChanged);
		InQueueComponent->OnCraftProgress.AddDynamic(this, &UMOCraftingQueueWidget::HandleCraftProgress);
		InQueueComponent->OnCraftCompleted.AddDynamic(this, &UMOCraftingQueueWidget::HandleCraftCompleted);
	}

	SyncRowWidgetClass();
	RefreshRows();
}

void UMOCraftingQueueWidget::RefreshQueue()
{
	SyncRowWidgetClass();
	RefreshRows();
}

void UMOCraftingQueueWidget::UpdateProgress()
{
	UpdateProgressDisplay();
}

bool UMOCraftingQueueWidget::IsQueueEmpty() const
{
	UMOCraftingQueueComponent* Queue = QueueComponent.Get();
	return !Queue || Queue->IsQueueEmpty();
}

int32 UMOCraftingQueueWidget::GetQueueLength() const
{
	UMOCraftingQueueComponent* Queue = QueueComponent.Get();
	return Queue ? Queue->GetQueueLength() : 0;
}

float UMOCraftingQueueWidget::GetCurrentProgress() const
{
	UMOCraftingQueueComponent* Queue = QueueComponent.Get();
	return Queue ? Queue->GetOverallQueueProgress() : 0.0f;
}

FText UMOCraftingQueueWidget::GetTimeRemainingText() const
{
	UMOCraftingQueueComponent* Queue = QueueComponent.Get();
	if (!Queue)
	{
		return FText::GetEmpty();
	}
	return UMOUIUtils::FormatDurationAsText(Queue->GetTotalTimeRemaining());
}

FMOQueueDisplayRow UMOCraftingQueueWidget::BuildCraftingDisplayRow(
	const FMOCraftingQueueEntry& Entry,
	const FMORecipeDefinitionRow* Recipe,
	bool bIsActive,
	float ActiveRemainingSeconds)
{
	FMOQueueDisplayRow Row;
	Row.RowId = Entry.EntryId;
	Row.SourceId = Entry.RecipeId;
	Row.Progress = Entry.Progress;
	Row.State = bIsActive ? EMOQueueRowState::Active : EMOQueueRowState::Queued;
	Row.bCancellable = true;

	if (Recipe)
	{
		Row.Title = Recipe->DisplayName;
		Row.Icon = Recipe->Icon;
	}
	else
	{
		Row.Title = FText::FromName(Entry.RecipeId);
	}

	// Repeat display: "current iteration / total" (legacy format).
	Row.CountCurrent = Entry.CompletedCount + 1;
	Row.CountTotal = Entry.Count;

	// Active row: authoritative remaining from the component. Queued rows:
	// base CraftTime * Count estimate (legacy parity; tool bonuses not applied).
	if (bIsActive)
	{
		Row.RemainingSeconds = FMath::Max(0.0f, ActiveRemainingSeconds);
	}
	else
	{
		const float CraftDuration = Recipe ? Recipe->CraftTime : 0.0f;
		Row.RemainingSeconds = CraftDuration * Entry.Count;
	}

	return Row;
}

bool UMOCraftingQueueWidget::HasQueueSource_Implementation() const
{
	return QueueComponent.IsValid();
}

void UMOCraftingQueueWidget::BuildDisplayRows_Implementation(TArray<FMOQueueDisplayRow>& OutRows) const
{
	UMOCraftingQueueComponent* Queue = QueueComponent.Get();
	if (!Queue)
	{
		return;
	}

	TArray<FMOCraftingQueueEntry> QueueEntries;
	Queue->GetAllQueueEntries(QueueEntries);

	const float ActiveRemaining = Queue->GetCurrentCraftTimeRemaining();
	for (int32 i = 0; i < QueueEntries.Num(); ++i)
	{
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(QueueEntries[i].RecipeId);
		OutRows.Add(BuildCraftingDisplayRow(QueueEntries[i], Recipe, i == 0, ActiveRemaining));
	}
}

void UMOCraftingQueueWidget::GetHeaderDisplay_Implementation(FMOQueueHeaderDisplay& OutHeader) const
{
	// Crafting header semantics: OVERALL queue progress + TOTAL remaining
	// (row 0 shows the per-craft values via the live-progress hook).
	OutHeader = FMOQueueHeaderDisplay();
	UMOCraftingQueueComponent* Queue = QueueComponent.Get();
	if (!Queue || Queue->IsQueueEmpty())
	{
		return;
	}

	OutHeader.bHasRows = true;
	OutHeader.Progress = Queue->GetOverallQueueProgress();
	OutHeader.RemainingSeconds = FMath::Max(0.0f, Queue->GetTotalTimeRemaining());
	const TArray<FMOQueueDisplayRow>& Rows = GetLastBuiltRows();
	if (Rows.Num() > 0)
	{
		OutHeader.ActiveTitle = Rows[0].Title;
	}
}

bool UMOCraftingQueueWidget::GetActiveRowLiveProgress_Implementation(float& OutProgress, float& OutRemainingSeconds) const
{
	UMOCraftingQueueComponent* Queue = QueueComponent.Get();
	if (!Queue || Queue->IsQueueEmpty())
	{
		return false;
	}
	OutProgress = Queue->GetCurrentCraftProgress();
	OutRemainingSeconds = FMath::Max(0.0f, Queue->GetCurrentCraftTimeRemaining());
	return true;
}

void UMOCraftingQueueWidget::ExecuteCancelRow_Implementation(const FGuid& RowId)
{
	// Domain-owned cancellation: authority + refund policy live here (the
	// component rejects non-authority calls). Refund remaining ingredients —
	// the legacy policy, now expressed at the adapter boundary.
	if (UMOCraftingQueueComponent* Queue = QueueComponent.Get())
	{
		Queue->CancelCraft(RowId, /*bRefundIngredients=*/true);
	}
}

void UMOCraftingQueueWidget::ExecuteCancelAll_Implementation()
{
	if (UMOCraftingQueueComponent* Queue = QueueComponent.Get())
	{
		Queue->CancelAllCrafts(/*bRefundIngredients=*/true);
	}
}

void UMOCraftingQueueWidget::NativeDestruct()
{
	if (UMOCraftingQueueComponent* Queue = QueueComponent.Get())
	{
		Queue->OnQueueChanged.RemoveDynamic(this, &UMOCraftingQueueWidget::HandleQueueChanged);
		Queue->OnCraftProgress.RemoveDynamic(this, &UMOCraftingQueueWidget::HandleCraftProgress);
		Queue->OnCraftCompleted.RemoveDynamic(this, &UMOCraftingQueueWidget::HandleCraftCompleted);
	}

	Super::NativeDestruct();
}

void UMOCraftingQueueWidget::NotifyRowsRefreshed(int32 RowCount)
{
	OnQueueUpdated(RowCount);
}

void UMOCraftingQueueWidget::NotifyProgressUpdated(float Progress, const FText& TimeRemaining)
{
	OnProgressUpdated(Progress, TimeRemaining);
}

void UMOCraftingQueueWidget::OnRowWidgetBound(UMOQueueRowWidgetBase* RowWidget, const FMOQueueDisplayRow& InRow)
{
	// Keep the legacy BP-visible display struct in sync on the compat entry so
	// OnVisualsUpdated/GetEntryData see the same values the base rendered.
	if (UMOCraftingQueueEntryWidget* LegacyEntry = Cast<UMOCraftingQueueEntryWidget>(RowWidget))
	{
		FMOQueueEntryDisplayData LegacyData;
		LegacyData.EntryId = InRow.RowId;
		LegacyData.RecipeId = InRow.SourceId;
		LegacyData.RecipeName = InRow.Title;
		LegacyData.Icon = InRow.Icon;
		LegacyData.CountText = InRow.CountText;
		LegacyData.Progress = InRow.Progress;
		LegacyData.TimeRemainingText = InRow.TimeRemainingText;
		LegacyData.bIsActive = (InRow.State == EMOQueueRowState::Active);
		LegacyEntry->SetLegacyEntryData(LegacyData);
	}
}

void UMOCraftingQueueWidget::HandleQueueChanged()
{
	RefreshRows();
}

void UMOCraftingQueueWidget::HandleCraftProgress(const FGuid& EntryId, float Progress)
{
	// Progress is handled by tick-based updates for smoother display.
}

void UMOCraftingQueueWidget::HandleCraftCompleted(const FGuid& EntryId, const FMOCraftResult& Result)
{
	RefreshRows();
}

void UMOCraftingQueueWidget::SyncRowWidgetClass()
{
	if (QueueEntryWidgetClass)
	{
		RowWidgetClass = QueueEntryWidgetClass;
	}
}
