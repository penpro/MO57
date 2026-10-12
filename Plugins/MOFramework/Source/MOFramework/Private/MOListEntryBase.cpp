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
	if (!bUseThemeColors)
	{
		return bDimmed ? DisabledColor : bSelected ? SelectedColor : NormalColor;
	}
	const FLinearColor Row = UMOUITokens::Color("color.surface.2");
	if (bDimmed)
	{
		return UMOUITokens::Color("color.surface.1");
	}
	if (bSelected)
	{
		return FMath::Lerp(Row, UMOUITokens::Color("color.accent"), 0.18f);   // kit: accent muted over the row
	}
	return bIsHovered ? UMOUITokens::Color("color.surface.3") : Row;
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
