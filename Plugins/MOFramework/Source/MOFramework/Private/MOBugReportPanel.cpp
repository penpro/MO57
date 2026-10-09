#include "MOBugReportPanel.h"
#include "MOBugReport.h"
#include "MOCommonButton.h"
#include "MOCommunitySettings.h"
#include "MOFramework.h"
#include "MOUIUtils.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/TextBlock.h"

UMOBugReportPanel::UMOBugReportPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UMOBugReportPanel::NativeConstruct()
{
	Super::NativeConstruct();

	UMOUIUtils::ApplyReadableTextInputStyle(TitleInput);
	UMOUIUtils::ApplyReadableTextInputStyle(ContactInput);
	UMOUIUtils::ApplyReadableMultiLineTextInputStyle(DescriptionInput);
	UMOUIUtils::ApplyReadableMultiLineTextInputStyle(StepsInput);

	if (CategoryCombo && CategoryCombo->GetOptionCount() == 0)
	{
		for (const FString& Category : UMOBugReportSubsystem::GetCategories())
		{
			CategoryCombo->AddOption(Category);
		}
		CategoryCombo->SetSelectedIndex(0);
	}
	if (IncludeLogCheck)
	{
		IncludeLogCheck->SetIsChecked(true);
	}
	if (IncludeScreenshotCheck)
	{
		IncludeScreenshotCheck->SetIsChecked(true);
	}

	if (SendButton)
	{
		SendButton->SetButtonText(NSLOCTEXT("MOBugReportPanel", "Send", "Send Report"));
		SendButton->OnClicked().RemoveAll(this);
		SendButton->OnClicked().AddUObject(this, &UMOBugReportPanel::HandleSendClicked);
	}
	if (BackButton)
	{
		BackButton->SetButtonText(NSLOCTEXT("MOBugReportPanel", "Back", "Back"));
		BackButton->OnClicked().RemoveAll(this);
		BackButton->OnClicked().AddUObject(this, &UMOBugReportPanel::HandleBackClicked);
	}
	if (DiscordButton)
	{
		DiscordButton->SetButtonText(NSLOCTEXT("MOBugReportPanel", "Discord", "Open Discord"));
		DiscordButton->OnClicked().RemoveAll(this);
		DiscordButton->OnClicked().AddUObject(this, &UMOBugReportPanel::HandleDiscordClicked);
	}
	if (PreviewButton)
	{
		PreviewButton->SetButtonText(NSLOCTEXT("MOBugReportPanel", "Preview", "Preview"));
		PreviewButton->OnClicked().RemoveAll(this);
		PreviewButton->OnClicked().AddUObject(this, &UMOBugReportPanel::HandlePreviewClicked);
	}

	if (PreviewText)
	{
		PreviewText->SetText(FText::GetEmpty());
	}
	SetStatus(FString());
}

void UMOBugReportPanel::NativeDestruct()
{
	if (SendButton) SendButton->OnClicked().RemoveAll(this);
	if (BackButton) BackButton->OnClicked().RemoveAll(this);
	if (DiscordButton) DiscordButton->OnClicked().RemoveAll(this);
	if (PreviewButton) PreviewButton->OnClicked().RemoveAll(this);

	Super::NativeDestruct();
}

UWidget* UMOBugReportPanel::NativeGetDesiredFocusTarget() const
{
	if (TitleInput) return TitleInput;
	if (SendButton) return SendButton;
	return nullptr;
}

void UMOBugReportPanel::PrepareForDisplay()
{
	SetBusy(false);
	SetStatus(FString());
	if (PreviewText)
	{
		PreviewText->SetText(FText::GetEmpty());
	}
	if (UMOBugReportSubsystem* Reports = UMOBugReportSubsystem::Get(this))
	{
		Reports->CaptureScreenshot();
	}
}

FMOBugReportDraft UMOBugReportPanel::ReadDraft() const
{
	FMOBugReportDraft Draft;
	Draft.Title = TitleInput ? TitleInput->GetText().ToString() : FString();
	Draft.Description = DescriptionInput ? DescriptionInput->GetText().ToString() : FString();
	Draft.Steps = StepsInput ? StepsInput->GetText().ToString() : FString();
	Draft.Contact = ContactInput ? ContactInput->GetText().ToString() : FString();
	Draft.Category = CategoryCombo ? CategoryCombo->GetSelectedOption() : UMOBugReportSubsystem::GetCategories().Last();
	Draft.bIncludeLog = !IncludeLogCheck || IncludeLogCheck->IsChecked();
	Draft.bIncludeScreenshot = !IncludeScreenshotCheck || IncludeScreenshotCheck->IsChecked();
	return Draft;
}

void UMOBugReportPanel::RefreshPreview()
{
	if (!PreviewText)
	{
		return;
	}
	UMOBugReportSubsystem* Reports = UMOBugReportSubsystem::Get(this);
	if (!Reports)
	{
		PreviewText->SetText(NSLOCTEXT("MOBugReportPanel", "NoSubsystem", "Bug reporting isn't available right now."));
		return;
	}
	PreviewText->SetText(FText::FromString(Reports->BuildReportText(ReadDraft(), FGuid(), /*bPreview=*/true)));
}

void UMOBugReportPanel::SendReport()
{
	UMOBugReportSubsystem* Reports = UMOBugReportSubsystem::Get(this);
	if (!Reports)
	{
		SetStatus(TEXT("Bug reporting isn't available right now."));
		return;
	}

	SetBusy(true);
	SetStatus(TEXT("Sending your report..."));
	Reports->Submit(ReadDraft(), FMOBugReportFinished::CreateWeakLambda(this, [this](const FMOBugReportResult& Result)
	{
		HandleSubmitFinished(Result);
	}));
}

void UMOBugReportPanel::ClearForm()
{
	if (TitleInput) TitleInput->SetText(FText::GetEmpty());
	if (DescriptionInput) DescriptionInput->SetText(FText::GetEmpty());
	if (StepsInput) StepsInput->SetText(FText::GetEmpty());
	if (PreviewText) PreviewText->SetText(FText::GetEmpty());
	// The contact line stays: a player sending a second report should not have to retype it. Category and the two attachment boxes stay too.
}

void UMOBugReportPanel::HandleSubmitFinished(const FMOBugReportResult& Result)
{
	SetBusy(false);
	SetStatus(Result.Message);
	if (Result.bSent)
	{
		ClearForm();
		if (UMOBugReportSubsystem* Reports = UMOBugReportSubsystem::Get(this))
		{
			Reports->DiscardScreenshot(); // that picture belongs to the report that was just sent
		}
	}
}

void UMOBugReportPanel::HandleSendClicked()
{
	SendReport();
}

void UMOBugReportPanel::HandleBackClicked()
{
	OnRequestClose.Broadcast();
}

void UMOBugReportPanel::HandleDiscordClicked()
{
	FText Message;
	UMOCommunitySettings::OpenBugReportLink(Message);
	SetStatus(Message.ToString());
}

void UMOBugReportPanel::HandlePreviewClicked()
{
	RefreshPreview();
}

void UMOBugReportPanel::SetStatus(const FString& Message) const
{
	if (!Message.IsEmpty())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOBugReportPanel] Status: %s"), *Message);
	}
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
	}
}

void UMOBugReportPanel::SetBusy(bool bBusy)
{
	if (SendButton)
	{
		SendButton->SetIsEnabled(!bBusy); // a second click during an upload would start a second report
	}
}
