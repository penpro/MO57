#include "MOConfirmationDialog.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "CommonInputModeTypes.h"
#include "Components/TextBlock.h"

TOptional<FUIInputConfig> UMOConfirmationDialog::GetDesiredInputConfig() const
{
	return FUIInputConfig(
		ECommonInputMode::Menu,
		EMouseCaptureMode::NoCapture,
		EMouseLockMode::DoNotLock,
		false
	);
}

void UMOConfirmationDialog::NativeConstruct()
{
	Super::NativeConstruct();

	// Confirmation dialogs must be acknowledged — don't dismiss on outside click
	bClosesOnOutsideClick = false;

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().RemoveAll(this);
		ConfirmButton->OnClicked().AddUObject(this, &UMOConfirmationDialog::HandleConfirmClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
		CancelButton->OnClicked().AddUObject(this, &UMOConfirmationDialog::HandleCancelClicked);
	}
}

void UMOConfirmationDialog::NativeDestruct()
{
	// Clean up button bindings
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

UWidget* UMOConfirmationDialog::NativeGetDesiredFocusTarget() const
{
	// Default focus to Cancel (safer option)
	if (CancelButton)
	{
		return CancelButton;
	}
	if (ConfirmButton)
	{
		return ConfirmButton;
	}
	return nullptr;
}

FReply UMOConfirmationDialog::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Escape cancels
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		HandleCancelClicked();
		return FReply::Handled();
	}

	// Enter confirms (only if confirm button is focused)
	if (InKeyEvent.GetKey() == EKeys::Enter)
	{
		if (ConfirmButton && ConfirmButton->HasKeyboardFocus())
		{
			HandleConfirmClicked();
			return FReply::Handled();
		}
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMOConfirmationDialog::Setup(const FText& Title, const FText& Message, const FText& ConfirmText, const FText& CancelText)
{
	if (TitleText)
	{
		TitleText->SetText(Title);
	}

	if (MessageText)
	{
		MessageText->SetText(Message);
	}

	if (ConfirmButton)
	{
		ConfirmButton->SetButtonText(ConfirmText);
	}

	if (CancelButton)
	{
		CancelButton->SetButtonText(CancelText);
	}
}

void UMOConfirmationDialog::HandleConfirmClicked()
{
	OnConfirmed.Broadcast();
	DeactivateWidget();
}

void UMOConfirmationDialog::HandleCancelClicked()
{
	OnCancelled.Broadcast();
	DeactivateWidget();
}
