/**
 * MOListEntryBase.cpp - Generic List Entry Widget Base Implementation
 */

#include "MOListEntryBase.h"
#include "MOUITokens.h"
#include "MOCommonButton.h"
#include "Components/Border.h"

UMOListEntryBase::UMOListEntryBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOListEntryBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (EntryButton)
	{
		EntryButton->OnClicked().RemoveAll(this);
		EntryButton->OnClicked().AddUObject(this, &UMOListEntryBase::HandleButtonClicked);
	}

	UpdateVisuals();
}

void UMOListEntryBase::NativeDestruct()
{
	if (EntryButton)
	{
		EntryButton->OnClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

// ============================================================================
// DATA BINDING
// ============================================================================

void UMOListEntryBase::SetEntryId(FName InEntryId)
{
	if (EntryId == InEntryId)
	{
		return;
	}

	EntryId = InEntryId;
	OnDataBound(InEntryId);
}

void UMOListEntryBase::OnDataBound_Implementation(FName InEntryId)
{
	// Base implementation does nothing - subclasses override to update visuals
	UpdateVisuals();
}

// ============================================================================
// SELECTION
// ============================================================================

void UMOListEntryBase::SetSelected(bool bInSelected)
{
	if (bIsSelected != bInSelected)
	{
		bIsSelected = bInSelected;
		UpdateVisuals();
	}
}

// ============================================================================
// ENABLED STATE
// ============================================================================

void UMOListEntryBase::SetEntryEnabled(bool bInEnabled)
{
	if (bIsEnabled != bInEnabled)
	{
		bIsEnabled = bInEnabled;

		if (EntryButton)
		{
			EntryButton->SetIsEnabled(bIsEnabled);
		}

		UpdateVisuals();
	}
}

// ============================================================================
// VISUALS
// ============================================================================

void UMOListEntryBase::UpdateVisuals_Implementation()
{
	if (BackgroundBorder)
	{
		BackgroundBorder->SetBrushColor(ResolveRowColor(bIsSelected, !bIsEnabled));
	}
}

FLinearColor UMOListEntryBase::ResolveRowColor(bool bSelected, bool bDimmed) const
{
	// Selection outranks dimming: the recipe you picked and cannot make yet still has to read as picked. Dimmed first used to
	// win, so selecting an uncraftable recipe changed nothing in the list (first-ten-minutes pass, Oct 15 2026).
	if (!bUseThemeColors)
	{
		return bSelected ? SelectedColor : bDimmed ? DisabledColor : NormalColor;
	}
	const FLinearColor Row = UMOUITokens::Color(bDimmed ? "color.surface.1" : "color.surface.2");   // a dimmed row sits one surface lower
	if (bSelected)
	{
		return FMath::Lerp(Row, UMOUITokens::Color("color.accent"), 0.18f);   // kit: accent muted over the row
	}
	if (bIsHovered)
	{
		return UMOUITokens::Color(bDimmed ? "color.surface.2" : "color.surface.3");   // one surface up, dimmed or not
	}
	return Row;
}

void UMOListEntryBase::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bIsHovered = true;
	UpdateVisuals();
}

void UMOListEntryBase::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bIsHovered = false;
	UpdateVisuals();
}

// ============================================================================
// CLICK HANDLING
// ============================================================================

void UMOListEntryBase::HandleButtonClicked()
{
	if (bIsEnabled)
	{
		OnEntrySelected.Broadcast(EntryId);
	}
}
