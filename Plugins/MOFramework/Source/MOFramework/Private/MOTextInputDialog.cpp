#include "MOTextInputDialog.h"
#include "MOFramework.h"

#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"

UMOTextInputDialog::UMOTextInputDialog(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOTextInputDialog::ConfigureWithText(
	const FText& Title,
	const FText& Message,
	const FText& InitialText,
	int32 InMaxLength,
	const FText& HintText,
	const FText& ConfirmText,
	const FText& CancelText)
{
	// Delegate base text setup to UMOConfirmationBase::Configure — sets title,
	// message, button labels, ensures bindings exist.
	Configure(Title, Message, ConfirmText, CancelText);

	MaxLength = FMath::Max(0, InMaxLength);

	if (TextInput)
	{
		TextInput->SetText(InitialText);
		TextInput->SetHintText(HintText);
		// Move caret to end of pre-filled text so the player can immediately
		// type more or backspace from the tail. (UEditableTextBox doesn't
		// expose direct caret API; SetText already triggers OnTextChanged
		// which would clamp if needed.)
	}
}

FText UMOTextInputDialog::GetEnteredText() const
{
	return TextInput ? TextInput->GetText() : FText::GetEmpty();
}

void UMOTextInputDialog::NativeConstruct()
{
	Super::NativeConstruct();

	if (TextInput)
	{
		TextInput->OnTextCommitted.RemoveDynamic(this, &UMOTextInputDialog::HandleTextCommitted);
		TextInput->OnTextCommitted.AddDynamic(this, &UMOTextInputDialog::HandleTextCommitted);

		TextInput->OnTextChanged.RemoveDynamic(this, &UMOTextInputDialog::HandleTextChanged);
		TextInput->OnTextChanged.AddDynamic(this, &UMOTextInputDialog::HandleTextChanged);
	}
}

void UMOTextInputDialog::NativeDestruct()
{
	if (TextInput)
	{
		TextInput->OnTextCommitted.RemoveDynamic(this, &UMOTextInputDialog::HandleTextCommitted);
		TextInput->OnTextChanged.RemoveDynamic(this, &UMOTextInputDialog::HandleTextChanged);
	}

	Super::NativeDestruct();
}

UWidget* UMOTextInputDialog::NativeGetDesiredFocusTarget() const
{
	// Focus the input so the player can type immediately. Falls back to the
	// confirm button if the BP forgot to bind the text input (BindWidget will
	// already have errored at compile time, but be defensive).
	if (TextInput)
	{
		return TextInput;
	}
	return Super::NativeGetDesiredFocusTarget();
}

void UMOTextInputDialog::OnDialogConfirmed_Implementation()
{
	// Broadcast our text-carrying delegate FIRST so consumers that bind only
	// OnTextConfirmed see the entered text. Base OnConfirmed broadcast
	// happens after this returns via the base's HandleConfirmClicked flow.
	OnTextConfirmed.Broadcast(GetEnteredText());
	Super::OnDialogConfirmed_Implementation();
}

void UMOTextInputDialog::ClearResultDelegates()
{
	Super::ClearResultDelegates();
	OnTextConfirmed.Clear();
}

void UMOTextInputDialog::HandleTextCommitted(const FText& InText, ETextCommit::Type CommitMethod)
{
	// Enter key in the input acts as Confirm. Other commit types (lose focus,
	// click elsewhere) shouldn't dismiss — the player likely just tabbed away.
	if (CommitMethod == ETextCommit::OnEnter)
	{
		HandleConfirmClicked();
	}
}

void UMOTextInputDialog::HandleTextChanged(const FText& InText)
{
	// Soft length cap — clamp the visible text if it exceeds MaxLength. Some
	// IMEs send long batches at once (paste, autocomplete) that bypass
	// per-keystroke clamps in the BP. Doing it here covers all paths.
	if (MaxLength > 0 && TextInput)
	{
		const FString S = InText.ToString();
		if (S.Len() > MaxLength)
		{
			TextInput->SetText(FText::FromString(S.Left(MaxLength)));
		}
	}
}
