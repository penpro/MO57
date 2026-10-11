/**
 * MOQuickPickDialog.cpp - A small modal that offers a handful of choices as buttons
 */

#include "MOQuickPickDialog.h"
#include "MOCommonButton.h"
#include "MOFramework.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "CommonInputModeTypes.h"

UMOQuickPickDialog::UMOQuickPickDialog(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A question must be answered or cancelled explicitly, not dismissed by a stray click outside.
	bClosesOnOutsideClick = false;
}

void UMOQuickPickDialog::NativeConstruct()
{
	Super::NativeConstruct();

	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
		CancelButton->OnClicked().AddUObject(this, &UMOQuickPickDialog::HandleCancelClicked);
	}
}

void UMOQuickPickDialog::NativeDestruct()
{
	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
	}
	ClearOptions();
	OnChosen.Clear();
	OnCancelled.Clear();
	Super::NativeDestruct();
}

void UMOQuickPickDialog::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
	// One-shot results: a pooled instance must not carry a past consumer's bindings into the next open.
	OnChosen.Clear();
	OnCancelled.Clear();
}

UWidget* UMOQuickPickDialog::NativeGetDesiredFocusTarget() const
{
	return OptionButtons.Num() > 0 ? Cast<UWidget>(OptionButtons[0]) : Cast<UWidget>(CancelButton);
}

bool UMOQuickPickDialog::NativeOnCloseKeyRequested(const FKeyEvent& InKeyEvent)
{
	HandleCancelClicked();
	return true;
}

TOptional<FUIInputConfig> UMOQuickPickDialog::GetDesiredInputConfig() const
{
	return FUIInputConfig(
		ECommonInputMode::Menu,
		EMouseCaptureMode::NoCapture,
		EMouseLockMode::DoNotLock,
		false /* bHideCursorDuringViewportCapture */);
}

void UMOQuickPickDialog::ClearOptions()
{
	for (UMOCommonButton* Button : OptionButtons)
	{
		if (Button)
		{
			Button->OnClicked().RemoveAll(this);
		}
	}
	OptionButtons.Reset();
	if (OptionsGrid)
	{
		OptionsGrid->ClearChildren();
	}
}

void UMOQuickPickDialog::Configure(const FText& Title, const FText& Message, const TArray<FMOQuickPickOption>& Options, int32 Columns)
{
	if (TitleText)
	{
		TitleText->SetText(Title);
	}
	if (MessageText)
	{
		MessageText->SetText(Message);
		MessageText->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	ClearOptions();
	if (!OptionsGrid)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOQuickPick] No OptionsGrid bound: the widget blueprint is missing it"));
		return;
	}
	if (!OptionButtonClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOQuickPick] No OptionButtonClass set: the widget blueprint's class defaults are missing it"));
		return;
	}

	Columns = FMath::Max(Columns, 1);
	for (int32 I = 0; I < Options.Num(); ++I)
	{
		UMOCommonButton* Button = CreateWidget<UMOCommonButton>(GetOwningPlayer(), OptionButtonClass);
		if (!Button)
		{
			continue;
		}
		Button->SetButtonText(Options[I].Label);
		const FName Id = Options[I].Id;
		Button->OnClicked().AddUObject(this, &UMOQuickPickDialog::HandleOptionClicked, Id);
		if (UUniformGridSlot* GridSlot = OptionsGrid->AddChildToUniformGrid(Button, I / Columns, I % Columns))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Fill);
		}
		OptionButtons.Add(Button);
	}
}

void UMOQuickPickDialog::HandleOptionClicked(FName OptionId)
{
	OnChosen.Broadcast(OptionId);
	DeactivateWidget();
}

void UMOQuickPickDialog::HandleCancelClicked()
{
	OnCancelled.Broadcast();
	DeactivateWidget();
}
