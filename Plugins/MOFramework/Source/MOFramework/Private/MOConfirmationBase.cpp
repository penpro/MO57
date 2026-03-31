/**
 * MOConfirmationBase.cpp - Generic Confirmation Dialog Base Implementation
 */

#include "MOConfirmationBase.h"
#include "MOCommonButton.h"
#include "Components/TextBlock.h"

UMOConfirmationBase::UMOConfirmationBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOConfirmationBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().RemoveAll(this);
		ConfirmButton->OnClicked().AddUObject(this, &UMOConfirmationBase::HandleConfirmClicked);
	}

	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
		CancelButton->OnClicked().AddUObject(this, &UMOConfirmationBase::HandleCancelClicked);
	}
}

void UMOConfirmationBase::NativeDestruct()
{
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().RemoveAll(this);
	}

	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

UWidget* UMOConfirmationBase::NativeGetDesiredFocusTarget() const
{
	// Default focus to cancel button (safer default)
	return CancelButton;
}

FReply UMOConfirmationBase::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		HandleCancelClicked();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void UMOConfirmationBase::Configure(
	const FText& Title,
	const FText& Message,
	const FText& ConfirmText,
	const FText& CancelText)
{
	if (TitleText)
	{
		TitleText->SetText(Title);
	}

	if (MessageText)
	{
		MessageText->SetText(Message);
	}

	// Note: Button text would need UMOCommonButton to expose SetButtonText
	// For now, Blueprint handles button text via BindWidget text blocks
}

// ============================================================================
// BUTTON HANDLERS
// ============================================================================

void UMOConfirmationBase::HandleConfirmClicked()
{
	OnDialogConfirmed();
	OnConfirmed.Broadcast();
	DeactivateWidget();
}

void UMOConfirmationBase::HandleCancelClicked()
{
	OnDialogCancelled();
	OnCancelled.Broadcast();
	DeactivateWidget();
}

void UMOConfirmationBase::OnDialogConfirmed_Implementation()
{
	// Base implementation does nothing - subclasses can override
}

void UMOConfirmationBase::OnDialogCancelled_Implementation()
{
	// Base implementation does nothing - subclasses can override
}
