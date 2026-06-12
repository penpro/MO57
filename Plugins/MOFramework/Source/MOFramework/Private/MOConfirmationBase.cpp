/**
 * MOConfirmationBase.cpp - Generic Confirmation Dialog Base Implementation
 */

#include "MOConfirmationBase.h"
#include "MOCommonButton.h"
#include "Components/TextBlock.h"
#include "CommonInputModeTypes.h"

UMOConfirmationBase::UMOConfirmationBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Confirmations must be acknowledged — don't dismiss on outside click
	bClosesOnOutsideClick = false;
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

	ClearResultDelegates();

	Super::NativeDestruct();
}

void UMOConfirmationBase::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	// Results are one-shot per open: every close path (confirm/cancel click,
	// Escape/Tab via NativeOnCloseKeyRequested) broadcasts its definitive result
	// before deactivating. PushModalWidget recycles pooled instances across
	// unrelated consumers, so anything still bound here belongs to a past
	// consumer and must not survive into the next open.
	ClearResultDelegates();
}

void UMOConfirmationBase::ClearResultDelegates()
{
	OnConfirmed.Clear();
	OnCancelled.Clear();
}

bool UMOConfirmationBase::NativeOnCloseKeyRequested(const FKeyEvent& InKeyEvent)
{
	// Close keys are a cancel, not a silent dismiss — consumers waiting on a
	// result (e.g. an armed pending-action slot) must hear OnCancelled.
	HandleCancelClicked();
	return true;
}

UWidget* UMOConfirmationBase::NativeGetDesiredFocusTarget() const
{
	// Default focus to cancel button (safer default)
	return CancelButton;
}

FReply UMOConfirmationBase::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Escape never reaches this bubble-phase handler — the base preview handler
	// consumes it and routes through NativeOnCloseKeyRequested (cancel) above.

	// Enter confirms only when the confirm button has explicit keyboard focus.
	// Without the focus guard, Enter would commit any dialog the moment it
	// opened — surprising for destructive prompts like "Delete save".
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

TOptional<FUIInputConfig> UMOConfirmationBase::GetDesiredInputConfig() const
{
	return FUIInputConfig(
		ECommonInputMode::Menu,
		EMouseCaptureMode::NoCapture,
		EMouseLockMode::DoNotLock,
		false /* bHideCursorDuringViewportCapture */);
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

	// Propagate button labels to the actual buttons. UMOCommonButton::SetButtonText
	// updates the stored ButtonLabel AND fires UpdateButtonText so the visible
	// text refreshes — required because otherwise the dialog shows whatever
	// design-time defaults the WBP had (e.g. "Confirm" / "Cancel"), which
	// caused the swapped-label bug between exit-to-menu and overwrite-save.
	if (ConfirmButton)
	{
		ConfirmButton->SetButtonText(ConfirmText);
	}
	if (CancelButton)
	{
		CancelButton->SetButtonText(CancelText);
	}
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
