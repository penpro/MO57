#include "MOBuildingQueueEntryWidget.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Components/Border.h"

UMOBuildingQueueEntryWidget::UMOBuildingQueueEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOBuildingQueueEntryWidget::SetupEntry(const FMOBuildQueueEntryDisplayData& InData)
{
	EntryData = InData;
	UpdateVisuals();
}

void UMOBuildingQueueEntryWidget::UpdateProgress(float NewProgress, const FText& NewTimeRemaining)
{
	EntryData.Progress = NewProgress;
	EntryData.TimeRemainingText = NewTimeRemaining;

	if (ProgressBar)
	{
		ProgressBar->SetPercent(NewProgress);
	}

	if (TimeRemainingText)
	{
		TimeRemainingText->SetText(NewTimeRemaining);
	}
}

void UMOBuildingQueueEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind cancel button
	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
		CancelButton->OnClicked().AddUObject(this, &UMOBuildingQueueEntryWidget::HandleCancelClicked);
	}
	else if (CancelButtonSimple)
	{
		CancelButtonSimple->OnClicked.RemoveDynamic(this, &UMOBuildingQueueEntryWidget::HandleCancelClicked);
		CancelButtonSimple->OnClicked.AddDynamic(this, &UMOBuildingQueueEntryWidget::HandleCancelClicked);
	}
}

void UMOBuildingQueueEntryWidget::NativeDestruct()
{
	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
	}
	if (CancelButtonSimple)
	{
		CancelButtonSimple->OnClicked.RemoveDynamic(this, &UMOBuildingQueueEntryWidget::HandleCancelClicked);
	}

	Super::NativeDestruct();
}

void UMOBuildingQueueEntryWidget::HandleCancelClicked()
{
	OnCancelRequested.Broadcast(EntryData.EntryId);
}

void UMOBuildingQueueEntryWidget::UpdateVisuals()
{
	// Update recipe name
	if (RecipeNameText)
	{
		RecipeNameText->SetText(EntryData.RecipeName);
	}

	// Update icon
	if (RecipeIcon && !EntryData.Icon.IsNull())
	{
		UTexture2D* IconTexture = EntryData.Icon.LoadSynchronous();
		if (IconTexture)
		{
			RecipeIcon->SetBrushFromTexture(IconTexture);
			RecipeIcon->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			RecipeIcon->SetVisibility(ESlateVisibility::Hidden);
		}
	}
	else if (RecipeIcon)
	{
		RecipeIcon->SetVisibility(ESlateVisibility::Hidden);
	}

	// Update count text
	if (CountText)
	{
		CountText->SetText(EntryData.CountText);
	}

	// Update progress bar
	if (ProgressBar)
	{
		ProgressBar->SetPercent(EntryData.Progress);

		// Different color for active vs queued vs paused
		FLinearColor ProgressColor;
		switch (EntryData.State)
		{
		case EMOBuildState::Constructing:
			ProgressColor = ActiveColor;
			break;
		case EMOBuildState::Paused:
			ProgressColor = PausedColor;
			break;
		default:
			ProgressColor = QueuedColor;
			break;
		}
		ProgressBar->SetFillColorAndOpacity(ProgressColor);
	}

	// Update time remaining
	if (TimeRemainingText)
	{
		TimeRemainingText->SetText(EntryData.TimeRemainingText);
	}

	// Notify Blueprint
	OnVisualsUpdated(EntryData);
}
