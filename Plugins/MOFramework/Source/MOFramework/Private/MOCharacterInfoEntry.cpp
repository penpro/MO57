#include "MOCharacterInfoEntry.h"
#include "MOCommonButton.h"
#include "Components/TextBlock.h"

void UMOCharacterInfoEntry::NativeConstruct()
{
	Super::NativeConstruct();

	if (ChangeButton)
	{
		ChangeButton->OnClicked().RemoveAll(this);
		ChangeButton->OnClicked().AddUObject(this, &UMOCharacterInfoEntry::HandleChangeClicked);
	}
}

void UMOCharacterInfoEntry::InitializeEntry(FName InFieldId, const FText& Label, const FText& Value, bool bCanChange)
{
	FieldId = InFieldId;
	bIsEditable = bCanChange;

	if (LabelText)
	{
		LabelText->SetText(Label);
	}

	if (ValueText)
	{
		ValueText->SetText(Value);
	}

	if (ChangeButton)
	{
		ChangeButton->SetVisibility(bCanChange ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Notify Blueprint
	OnEntryInitialized(InFieldId, Label, Value, bCanChange);
}

void UMOCharacterInfoEntry::SetValue(const FText& Value)
{
	if (ValueText)
	{
		ValueText->SetText(Value);
	}
}

void UMOCharacterInfoEntry::HandleChangeClicked()
{
	if (FieldId != NAME_None)
	{
		OnChangeRequested.Broadcast(FieldId);
	}
}
