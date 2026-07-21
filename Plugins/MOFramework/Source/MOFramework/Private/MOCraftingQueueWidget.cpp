#include "MOCraftingQueueWidget.h"
#include "MOFramework.h"
#include "MOCraftingQueueComponent.h"
#include "MOCraftingQueueEntryWidget.h"
#include "MORecipeDatabaseSettings.h"
#include "MOCommonButton.h"
#include "MOUIUtils.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

UMOCraftingQueueWidget::UMOCraftingQueueWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOCraftingQueueWidget::InitializeQueue(UMOCraftingQueueComponent* InQueueComponent)
{
	// Unbind from previous component
	if (UMOCraftingQueueComponent* OldQueue = QueueComponent.Get())
	{
		OldQueue->OnQueueChanged.RemoveDynamic(this, &UMOCraftingQueueWidget::HandleQueueChanged);
		OldQueue->OnCraftProgress.RemoveDynamic(this, &UMOCraftingQueueWidget::HandleCraftProgress);
		OldQueue->OnCraftCompleted.RemoveDynamic(this, &UMOCraftingQueueWidget::HandleCraftCompleted);
	}

	QueueComponent = InQueueComponent;

	// Bind to new component
	if (InQueueComponent)
	{
		InQueueComponent->OnQueueChanged.AddDynamic(this, &UMOCraftingQueueWidget::HandleQueueChanged);
		InQueueComponent->OnCraftProgress.AddDynamic(this, &UMOCraftingQueueWidget::HandleCraftProgress);
		InQueueComponent->OnCraftCompleted.AddDynamic(this, &UMOCraftingQueueWidget::HandleCraftCompleted);
	}

	RefreshQueue();
}

void UMOCraftingQueueWidget::RefreshQueue()
{
	// Clear existing entries
	for (UMOCraftingQueueEntryWidget* Entry : EntryWidgets)
	{
		if (Entry)
		{
			Entry->RemoveFromParent();
		}
	}
	EntryWidgets.Empty();

	UMOCraftingQueueComponent* Queue = QueueComponent.Get();
	if (!Queue)
	{
		return;
	}

	// Get container
	UPanelWidget* Container = QueueScrollBox ? Cast<UPanelWidget>(QueueScrollBox) : Cast<UPanelWidget>(QueueContainer);

	// Get queue entries
	TArray<FMOCraftingQueueEntry> QueueEntries;
	Queue->GetAllQueueEntries(QueueEntries);

	// Update empty state visibility
	if (EmptyQueueText)
	{
		EmptyQueueText->SetVisibility(QueueEntries.Num() == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (!Container || !QueueEntryWidgetClass)
	{
		OnQueueUpdated(QueueEntries.Num());
		return;
	}

	// Create entry widgets
	for (int32 i = 0; i < QueueEntries.Num(); ++i)
	{
		const FMOCraftingQueueEntry& Entry = QueueEntries[i];

		UMOCraftingQueueEntryWidget* EntryWidget = CreateWidget<UMOCraftingQueueEntryWidget>(this, QueueEntryWidgetClass);
		if (!EntryWidget)
		{
			continue;
		}

		// Build display data
		FMOQueueEntryDisplayData DisplayData;
		DisplayData.EntryId = Entry.EntryId;
		DisplayData.RecipeId = Entry.RecipeId;
		DisplayData.Progress = Entry.Progress;
		DisplayData.bIsActive = (i == 0);

		// Get recipe info
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(Entry.RecipeId);
		if (Recipe)
		{
			DisplayData.RecipeName = Recipe->DisplayName;
			DisplayData.Icon = Recipe->Icon;
		}
		else
		{
			DisplayData.RecipeName = FText::FromName(Entry.RecipeId);
		}

		// Format count
		DisplayData.CountText = FText::Format(
			NSLOCTEXT("MOCrafting", "QueueCount", "{0}/{1}"),
			FText::AsNumber(Entry.CompletedCount + 1),
			FText::AsNumber(Entry.Count)
		);

		// Calculate time remaining
		if (i == 0)
		{
			float TimeRemaining = Queue->GetCurrentCraftTimeRemaining();
			DisplayData.TimeRemainingText = UMOUIUtils::FormatDurationAsText(TimeRemaining);
		}
		else
		{
			// Calculate time for queued entries
			float CraftDuration = Recipe ? Recipe->CraftTime : 0.0f;
			float TimeRemaining = CraftDuration * Entry.Count;
			DisplayData.TimeRemainingText = UMOUIUtils::FormatDurationAsText(TimeRemaining);
		}

		EntryWidget->SetupEntry(DisplayData);
		EntryWidget->OnCancelRequested.AddDynamic(this, &UMOCraftingQueueWidget::HandleEntryCancelRequested);

		Container->AddChild(EntryWidget);
		EntryWidgets.Add(EntryWidget);
	}

	// Update current craft display
	if (QueueEntries.Num() > 0)
	{
		const FMOCraftingQueueEntry& CurrentEntry = QueueEntries[0];
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(CurrentEntry.RecipeId);

		if (CurrentCraftNameText && Recipe)
		{
			CurrentCraftNameText->SetText(Recipe->DisplayName);
		}

		// Use overall queue progress for the main progress bar
		float OverallProgress = Queue->GetOverallQueueProgress();

		if (CurrentProgressBar)
		{
			CurrentProgressBar->SetPercent(OverallProgress);
		}

		if (ProgressText)
		{
			ProgressText->SetText(FText::Format(
				NSLOCTEXT("MOCrafting", "ProgressPercent", "{0}%"),
				FText::AsNumber(FMath::RoundToInt(OverallProgress * 100))
			));
		}

		if (TimeRemainingText)
		{
			TimeRemainingText->SetText(UMOUIUtils::FormatDurationAsText(Queue->GetTotalTimeRemaining()));
		}

		if (TotalTimeRemainingText)
		{
			TotalTimeRemainingText->SetText(UMOUIUtils::FormatDurationAsText(Queue->GetTotalTimeRemaining()));
		}
	}
	else
	{
		// Queue is empty - clear the display
		if (CurrentCraftNameText)
		{
			CurrentCraftNameText->SetText(FText::GetEmpty());
		}

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

	OnQueueUpdated(QueueEntries.Num());
}

void UMOCraftingQueueWidget::UpdateProgress()
{
	UMOCraftingQueueComponent* Queue = QueueComponent.Get();
	if (!Queue || Queue->IsQueueEmpty())
	{
		// Queue is empty, ensure display is cleared
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
		OnProgressUpdated(0.0f, FText::GetEmpty());
		return;
	}

	// Use overall progress for the main display
	float OverallProgress = Queue->GetOverallQueueProgress();
	FText TotalTimeRemaining = UMOUIUtils::FormatDurationAsText(Queue->GetTotalTimeRemaining());

	if (CurrentProgressBar)
	{
		CurrentProgressBar->SetPercent(OverallProgress);
	}

	if (ProgressText)
	{
		ProgressText->SetText(FText::Format(
			NSLOCTEXT("MOCrafting", "ProgressPercent", "{0}%"),
			FText::AsNumber(FMath::RoundToInt(OverallProgress * 100))
		));
	}

	if (TimeRemainingText)
	{
		TimeRemainingText->SetText(TotalTimeRemaining);
	}

	if (TotalTimeRemainingText)
	{
		TotalTimeRemainingText->SetText(TotalTimeRemaining);
	}

	// Update first entry widget with current craft progress (not overall)
	if (EntryWidgets.Num() > 0 && EntryWidgets[0])
	{
		float CurrentProgress = Queue->GetCurrentCraftProgress();
		FText CurrentTimeRemaining = UMOUIUtils::FormatDurationAsText(Queue->GetCurrentCraftTimeRemaining());
		EntryWidgets[0]->UpdateProgress(CurrentProgress, CurrentTimeRemaining);
	}

	OnProgressUpdated(OverallProgress, TotalTimeRemaining);
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

void UMOCraftingQueueWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CancelAllButton)
	{
		CancelAllButton->OnClicked().RemoveAll(this);
		CancelAllButton->OnClicked().AddUObject(this, &UMOCraftingQueueWidget::HandleCancelAllClicked);
	}
}

void UMOCraftingQueueWidget::NativeDestruct()
{
	if (CancelAllButton)
	{
		CancelAllButton->OnClicked().RemoveAll(this);
	}

	// Unbind from queue component
	if (UMOCraftingQueueComponent* Queue = QueueComponent.Get())
	{
		Queue->OnQueueChanged.RemoveDynamic(this, &UMOCraftingQueueWidget::HandleQueueChanged);
		Queue->OnCraftProgress.RemoveDynamic(this, &UMOCraftingQueueWidget::HandleCraftProgress);
		Queue->OnCraftCompleted.RemoveDynamic(this, &UMOCraftingQueueWidget::HandleCraftCompleted);
	}

	Super::NativeDestruct();
}

void UMOCraftingQueueWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Periodic progress update
	TimeSinceLastUpdate += InDeltaTime;
	if (TimeSinceLastUpdate >= ProgressUpdateInterval)
	{
		TimeSinceLastUpdate = 0.0f;
		UpdateProgress();
	}
}

void UMOCraftingQueueWidget::HandleQueueChanged()
{
	RefreshQueue();
}

void UMOCraftingQueueWidget::HandleCraftProgress(const FGuid& EntryId, float Progress)
{
	// Progress is handled by tick-based updates for smoother display
}

void UMOCraftingQueueWidget::HandleCraftCompleted(const FGuid& EntryId, const FMOCraftResult& Result)
{
	RefreshQueue();
}

void UMOCraftingQueueWidget::HandleEntryCancelRequested(const FGuid& EntryId)
{
	if (UMOCraftingQueueComponent* Queue = QueueComponent.Get())
	{
		Queue->CancelCraft(EntryId, true); // true = refund ingredients
	}
}

void UMOCraftingQueueWidget::HandleCancelAllClicked()
{
	if (UMOCraftingQueueComponent* Queue = QueueComponent.Get())
	{
		Queue->CancelAllCrafts(true); // true = refund ingredients
	}
}
