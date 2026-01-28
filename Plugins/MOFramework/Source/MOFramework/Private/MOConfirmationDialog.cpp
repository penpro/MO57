#include "MOConfirmationDialog.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "Components/TextBlock.h"

void UMOConfirmationDialog::NativeConstruct()
{
	Super::NativeConstruct();

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().AddUObject(this, &UMOConfirmationDialog::HandleConfirmClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().AddUObject(this, &UMOConfirmationDialog::HandleCancelClicked);
	}
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
