#include "MOCharacterInfoEntry.h"
#include "MOCommonButton.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"

void UMOCharacterInfoEntry::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind Change button
	if (ChangeButton)
	{
		ChangeButton->OnClicked().RemoveAll(this);
		ChangeButton->OnClicked().AddUObject(this, &UMOCharacterInfoEntry::HandleChangeClicked);
	}

	// Bind Confirm button
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().RemoveAll(this);
		ConfirmButton->OnClicked().AddUObject(this, &UMOCharacterInfoEntry::HandleConfirmClicked);
	}

	// Bind Cancel button
	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
		CancelButton->OnClicked().AddUObject(this, &UMOCharacterInfoEntry::HandleCancelClicked);
	}

	// Bind text commit (Enter key)
	if (EditTextBox)
	{
		EditTextBox->OnTextCommitted.RemoveAll(this);
		EditTextBox->OnTextCommitted.AddDynamic(this, &UMOCharacterInfoEntry::HandleTextCommitted);
	}

	// Start in display mode
	bIsInEditMode = false;
	UpdateWidgetVisibility();
}

void UMOCharacterInfoEntry::InitializeEntry(FName InFieldId, const FText& Label, const FText& Value, bool bCanChange)
{
	FieldId = InFieldId;
	bIsEditable = bCanChange;
	CurrentValue = Value.ToString();
	bIsInEditMode = false;

	if (LabelText)
	{
		LabelText->SetText(Label);
	}

	if (ValueText)
	{
		ValueText->SetText(Value);
	}

	UpdateWidgetVisibility();

	// Notify Blueprint
	OnEntryInitialized(InFieldId, Label, Value, bCanChange);
}

void UMOCharacterInfoEntry::SetValue(const FText& Value)
{
	CurrentValue = Value.ToString();

	if (ValueText)
	{
		ValueText->SetText(Value);
	}
}

void UMOCharacterInfoEntry::EnterEditMode()
{
	if (!bIsEditable || bIsInEditMode)
	{
		return;
	}

	bIsInEditMode = true;

	// Pre-fill text box with current value
	if (EditTextBox)
	{
		EditTextBox->SetText(FText::FromString(CurrentValue));
	}

	UpdateWidgetVisibility();

	// Focus the text box
	if (EditTextBox)
	{
		EditTextBox->SetKeyboardFocus();
	}

	OnEditModeEntered();
}

void UMOCharacterInfoEntry::CancelEdit()
{
	if (!bIsInEditMode)
	{
		return;
	}

	bIsInEditMode = false;
	UpdateWidgetVisibility();

	OnEditModeExited();
}

void UMOCharacterInfoEntry::ConfirmEdit()
{
	if (!bIsInEditMode)
	{
		return;
	}

	// Get the new value from the text box
	FString NewValue;
	if (EditTextBox)
	{
		NewValue = EditTextBox->GetText().ToString();
	}

	// Update display
	CurrentValue = NewValue;
	if (ValueText)
	{
		ValueText->SetText(FText::FromString(NewValue));
	}

	bIsInEditMode = false;
	UpdateWidgetVisibility();

	OnEditModeExited();

	// Broadcast the change
	if (FieldId != NAME_None)
	{
		OnValueChanged.Broadcast(FieldId, NewValue);
	}
}

void UMOCharacterInfoEntry::HandleChangeClicked()
{
	if (FieldId != NAME_None)
	{
		// Enter edit mode instead of just broadcasting
		EnterEditMode();
		OnChangeRequested.Broadcast(FieldId);
	}
}

void UMOCharacterInfoEntry::HandleConfirmClicked()
{
	ConfirmEdit();
}

void UMOCharacterInfoEntry::HandleCancelClicked()
{
	CancelEdit();
}

void UMOCharacterInfoEntry::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		ConfirmEdit();
	}
	else if (CommitMethod == ETextCommit::OnCleared)
	{
		CancelEdit();
	}
}

void UMOCharacterInfoEntry::UpdateWidgetVisibility()
{
	// Display mode widgets
	if (ValueText)
	{
		ValueText->SetVisibility(bIsInEditMode ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (ChangeButton)
	{
		// Only show if editable AND not in edit mode
		bool bShowChange = bIsEditable && !bIsInEditMode;
		ChangeButton->SetVisibility(bShowChange ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Edit mode widgets
	if (EditTextBox)
	{
		EditTextBox->SetVisibility(bIsInEditMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (ConfirmButton)
	{
		ConfirmButton->SetVisibility(bIsInEditMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (CancelButton)
	{
		CancelButton->SetVisibility(bIsInEditMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}
