/**
 * MOQueueRowWidgetBase.cpp - shared queue row rendering + cancel intent (Stage 3)
 * See MOQueueRowWidgetBase.h for the compatibility contract.
 */

#include "MOQueueRowWidgetBase.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"

UMOQueueRowWidgetBase::UMOQueueRowWidgetBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOQueueRowWidgetBase::SetRow(const FMOQueueDisplayRow& InRow)
{
	Row = InRow;
	MOQueueDisplay::FinalizeRowTexts(Row);
	UpdateRowVisuals();
}

void UMOQueueRowWidgetBase::UpdateLiveProgress(float NewProgress, const FText& NewTimeRemaining)
{
	Row.Progress = NewProgress;
	Row.TimeRemainingText = NewTimeRemaining;

	if (ProgressBar)
	{
		ProgressBar->SetPercent(NewProgress);
	}
	if (TimeRemainingText)
	{
		TimeRemainingText->SetText(NewTimeRemaining);
	}
}

void UMOQueueRowWidgetBase::RequestCancel()
{
	HandleCancelClicked();
}

void UMOQueueRowWidgetBase::UpdateRowVisuals()
{
	if (RecipeNameText)
	{
		RecipeNameText->SetText(Row.Title);
	}

	// Icon: sync load with Hidden fallback (legacy behavior, both domains).
	if (RecipeIcon && !Row.Icon.IsNull())
	{
		UTexture2D* IconTexture = Row.Icon.LoadSynchronous();
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

	if (CountText)
	{
		CountText->SetText(Row.CountText);
	}

	if (ProgressBar)
	{
		ProgressBar->SetPercent(Row.Progress);

		// 3-state fill color: Active/Paused/Queued. Crafting rows never present
		// Paused today, so its visuals are unchanged; building's paused tint is
		// preserved.
		FLinearColor FillColor = QueuedColor;
		switch (Row.State)
		{
		case EMOQueueRowState::Active: FillColor = ActiveColor; break;
		case EMOQueueRowState::Paused: FillColor = PausedColor; break;
		default: break;
		}
		ProgressBar->SetFillColorAndOpacity(FillColor);
	}

	if (TimeRemainingText)
	{
		TimeRemainingText->SetText(Row.TimeRemainingText);
	}

	// Cancel affordance follows the row's cancellability (all current sources
	// mark rows cancellable, so this is visually inert today).
	const ESlateVisibility CancelVis = Row.bCancellable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (CancelButton)
	{
		CancelButton->SetVisibility(CancelVis);
	}
	if (CancelButtonSimple)
	{
		CancelButtonSimple->SetVisibility(CancelVis);
	}
}

void UMOQueueRowWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	// F18: idempotent bindings — remove-before-add, prefer the CommonUI button.
	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
		CancelButton->OnClicked().AddUObject(this, &UMOQueueRowWidgetBase::HandleCancelClicked);
	}
	else if (CancelButtonSimple)
	{
		CancelButtonSimple->OnClicked.RemoveDynamic(this, &UMOQueueRowWidgetBase::HandleCancelClicked);
		CancelButtonSimple->OnClicked.AddDynamic(this, &UMOQueueRowWidgetBase::HandleCancelClicked);
	}
}

void UMOQueueRowWidgetBase::NativeDestruct()
{
	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
	}
	if (CancelButtonSimple)
	{
		CancelButtonSimple->OnClicked.RemoveDynamic(this, &UMOQueueRowWidgetBase::HandleCancelClicked);
	}

	Super::NativeDestruct();
}

void UMOQueueRowWidgetBase::HandleCancelClicked()
{
	if (!Row.bCancellable)
	{
		return;
	}
	// Intent only — execution belongs to the renderer's domain hook.
	NotifyCancelIntent(Row.RowId);
	OnCancelIntent.Broadcast(Row.RowId);
}
