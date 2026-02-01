#include "MOCraftingQueueEntryWidget.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Components/Border.h"

UMOCraftingQueueEntryWidget::UMOCraftingQueueEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOCraftingQueueEntryWidget::SetupEntry(const FMOQueueEntryDisplayData& InData)
{
	EntryData = InData;
	UpdateVisuals();
}

void UMOCraftingQueueEntryWidget::UpdateProgress(float NewProgress, const FText& NewTimeRemaining)
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

void UMOCraftingQueueEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind cancel button
	if (CancelButton)
	{
		CancelButton->OnClicked().AddUObject(this, &UMOCraftingQueueEntryWidget::HandleCancelClicked);
	}
	else if (CancelButtonSimple)
	{
		CancelButtonSimple->OnClicked.AddDynamic(this, &UMOCraftingQueueEntryWidget::HandleCancelClicked);
	}
}

void UMOCraftingQueueEntryWidget::HandleCancelClicked()
{
	OnCancelRequested.Broadcast(EntryData.EntryId);
}

void UMOCraftingQueueEntryWidget::UpdateVisuals()
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

		// Different color for active vs queued
		FLinearColor ProgressColor = EntryData.bIsActive ? ActiveColor : QueuedColor;
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
