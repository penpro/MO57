#include "MOKeyBindingEntryWidget.h"
#include "MOFramework.h"
#include "Components/TextBlock.h"
#include "Components/InputKeySelector.h"
#include "CommonButtonBase.h"

void UMOKeyBindingEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind key selector
	if (KeySelector)
	{
		KeySelector->OnKeySelected.RemoveDynamic(this, &UMOKeyBindingEntryWidget::HandleKeySelected);
		KeySelector->OnKeySelected.AddDynamic(this, &UMOKeyBindingEntryWidget::HandleKeySelected);
	}

	// Bind reset button
	if (ResetButton)
	{
		ResetButton->OnClicked().RemoveAll(this);
		ResetButton->OnClicked().AddUObject(this, &UMOKeyBindingEntryWidget::HandleResetClicked);
	}

	UpdateVisuals();
}

void UMOKeyBindingEntryWidget::NativeDestruct()
{
	// Unbind delegates
	if (KeySelector)
	{
		KeySelector->OnKeySelected.RemoveDynamic(this, &UMOKeyBindingEntryWidget::HandleKeySelected);
	}

	if (ResetButton)
	{
		ResetButton->OnClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UMOKeyBindingEntryWidget::SetupEntry(const FMOKeyBindingVisualData& InData)
{
	EntryData = InData;
	UpdateVisuals();
}

void UMOKeyBindingEntryWidget::UpdateVisuals()
{
	// Update action name
	if (ActionNameText)
	{
		ActionNameText->SetText(EntryData.DisplayName);
	}

	// Update key selector with current key
	if (KeySelector)
	{
		KeySelector->SetSelectedKey(FInputChord(EntryData.CurrentKey));
	}

	// Enable/disable reset button based on whether we're using a custom binding
	if (ResetButton)
	{
		const bool bIsCustom = EntryData.CurrentKey != EntryData.DefaultKey;
		ResetButton->SetIsEnabled(bIsCustom);
	}

	// Notify Blueprint
	OnVisualsUpdated(EntryData);
}

void UMOKeyBindingEntryWidget::ResetToDefault()
{
	if (EntryData.DefaultKey.IsValid() && EntryData.CurrentKey != EntryData.DefaultKey)
	{
		EntryData.CurrentKey = EntryData.DefaultKey;
		UpdateVisuals();
		OnKeyChanged.Broadcast(EntryData.ActionId, EntryData.DefaultKey);

		UE_LOG(LogMOFramework, Log, TEXT("[KeyBindingEntry] Reset %s to default: %s"),
			*EntryData.ActionId.ToString(), *EntryData.DefaultKey.ToString());
	}
}

void UMOKeyBindingEntryWidget::HandleKeySelected(FInputChord SelectedKey)
{
	// Get the primary key from the chord
	FKey NewKey = SelectedKey.Key;

	// Ignore invalid keys
	if (!NewKey.IsValid())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[KeyBindingEntry] Invalid key selected for %s"), *EntryData.ActionId.ToString());
		return;
	}

	// Ignore if same as current
	if (NewKey == EntryData.CurrentKey)
	{
		return;
	}

	// Update entry data
	EntryData.CurrentKey = NewKey;
	UpdateVisuals();

	// Broadcast change
	OnKeyChanged.Broadcast(EntryData.ActionId, NewKey);

	UE_LOG(LogMOFramework, Log, TEXT("[KeyBindingEntry] Key changed for %s: %s"),
		*EntryData.ActionId.ToString(), *NewKey.ToString());
}

void UMOKeyBindingEntryWidget::HandleResetClicked()
{
	ResetToDefault();
}
