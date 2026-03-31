/**
 * MOListEntryBase.cpp - Generic List Entry Widget Base Implementation
 */

#include "MOListEntryBase.h"
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
		FLinearColor Color;
		if (!bIsEnabled)
		{
			Color = DisabledColor;
		}
		else if (bIsSelected)
		{
			Color = SelectedColor;
		}
		else
		{
			Color = NormalColor;
		}

		BackgroundBorder->SetBrushColor(Color);
	}
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
