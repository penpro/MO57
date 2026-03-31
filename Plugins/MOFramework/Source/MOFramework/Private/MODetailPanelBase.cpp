/**
 * MODetailPanelBase.cpp - Generic Detail Panel Widget Base Implementation
 */

#include "MODetailPanelBase.h"
#include "MOCommonButton.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

UMODetailPanelBase::UMODetailPanelBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMODetailPanelBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (ActionButton)
	{
		ActionButton->OnClicked().RemoveAll(this);
		ActionButton->OnClicked().AddUObject(this, &UMODetailPanelBase::HandleActionClicked);
	}
}

void UMODetailPanelBase::NativeDestruct()
{
	if (ActionButton)
	{
		ActionButton->OnClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

// ============================================================================
// DATA BINDING
// ============================================================================

void UMODetailPanelBase::SetItemId(FName InItemId)
{
	ItemId = InItemId;
	OnItemBound(InItemId);
}

void UMODetailPanelBase::OnItemBound_Implementation(FName InItemId)
{
	// Base implementation does nothing - subclasses override to load data
}

void UMODetailPanelBase::ClearPanel()
{
	ItemId = NAME_None;
	SetTitle(FText::GetEmpty());
	SetDescription(FText::GetEmpty());
	SetIcon(nullptr);
	SetActionVisible(false);
}

// ============================================================================
// CONTENT SETTERS
// ============================================================================

void UMODetailPanelBase::SetTitle(const FText& Title)
{
	if (TitleText)
	{
		TitleText->SetText(Title);
	}
}

void UMODetailPanelBase::SetDescription(const FText& Description)
{
	if (DescriptionText)
	{
		DescriptionText->SetText(Description);
	}
}

void UMODetailPanelBase::SetIcon(UTexture2D* Icon)
{
	if (IconImage)
	{
		if (Icon)
		{
			IconImage->SetBrushFromTexture(Icon);
			IconImage->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			IconImage->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UMODetailPanelBase::SetIconFromSoftPtr(TSoftObjectPtr<UTexture2D> IconPtr)
{
	if (IconPtr.IsNull())
	{
		SetIcon(nullptr);
	}
	else
	{
		// Synchronous load - consider async for large lists
		UTexture2D* LoadedIcon = IconPtr.LoadSynchronous();
		SetIcon(LoadedIcon);
	}
}

// ============================================================================
// ACTION BUTTON
// ============================================================================

void UMODetailPanelBase::SetActionText(const FText& ActionText)
{
	if (ActionButton)
	{
		// UMOCommonButton should have a SetButtonText method
		// For now, we'll rely on Blueprint to handle button text
	}
}

void UMODetailPanelBase::SetActionEnabled(bool bEnabled)
{
	if (ActionButton)
	{
		ActionButton->SetIsEnabled(bEnabled);
	}
}

void UMODetailPanelBase::SetActionVisible(bool bVisible)
{
	if (ActionButton)
	{
		ActionButton->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UMODetailPanelBase::HandleActionClicked()
{
	if (ItemId != NAME_None)
	{
		OnActionRequested.Broadcast(ItemId);
	}
}
