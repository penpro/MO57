#include "MOHostGamePanel.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOSessionSubsystem.h"
#include "MOUIUtils.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"

UMOHostGamePanel::UMOHostGamePanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UMOHostGamePanel::NativeConstruct()
{
	Super::NativeConstruct();

	UMOUIUtils::ApplyReadableTextInputStyle(SessionNameInputBox);
	UMOUIUtils::ApplyReadableTextInputStyle(MaxPlayersInputBox);

	if (HostButton)
	{
		HostButton->OnClicked().RemoveAll(this);
		HostButton->OnClicked().AddUObject(this, &UMOHostGamePanel::HandleHostButtonClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked().RemoveAll(this);
		BackButton->OnClicked().AddUObject(this, &UMOHostGamePanel::HandleBackClicked);
	}

	PrepareForDisplay();
}

void UMOHostGamePanel::NativeDestruct()
{
	if (HostButton) HostButton->OnClicked().RemoveAll(this);
	if (BackButton) BackButton->OnClicked().RemoveAll(this);

	Super::NativeDestruct();
}

UWidget* UMOHostGamePanel::NativeGetDesiredFocusTarget() const
{
	if (SessionNameInputBox) return SessionNameInputBox;
	if (HostButton) return HostButton;
	return nullptr;
}

void UMOHostGamePanel::PrepareForDisplay()
{
	bHostInFlight = false;

	if (HostButton)
	{
		HostButton->SetIsEnabled(true);
	}

	if (HostStatusText)
	{
		HostStatusText->SetText(FText::GetEmpty());
	}

	if (MaxPlayersInputBox && MaxPlayersInputBox->GetText().IsEmpty())
	{
		MaxPlayersInputBox->SetText(FText::AsNumber(UMOSessionSubsystem::DefaultMaxPlayers));
	}
}

void UMOHostGamePanel::NotifyHostResult(bool bSuccess, const FString& ErrorMessage)
{
	bHostInFlight = false;

	if (bSuccess)
	{
		// The subsystem travels into the world next; leave the button disabled
		// so a second click can't start a second session during the transition.
		SetStatus(TEXT("Session created. Entering world..."));
		return;
	}

	SetStatus(FString::Printf(TEXT("Failed to host: %s"), *ErrorMessage));
	if (HostButton)
	{
		HostButton->SetIsEnabled(true);
	}
}

void UMOHostGamePanel::HandleHostButtonClicked()
{
	if (bHostInFlight)
	{
		return;
	}

	FString DisplayName = SessionNameInputBox ? SessionNameInputBox->GetText().ToString() : FString();
	DisplayName.TrimStartAndEndInline();
	if (DisplayName.IsEmpty())
	{
		DisplayName = TEXT("Untitled Camp");
	}

	const int32 MaxPlayers = ReadAndEchoMaxPlayers();

	UE_LOG(LogMOFramework, Log, TEXT("[MOHostGamePanel] Host requested: '%s' (max %d)"), *DisplayName, MaxPlayers);

	bHostInFlight = true;
	SetStatus(TEXT("Creating session..."));
	if (HostButton)
	{
		HostButton->SetIsEnabled(false);
	}

	OnHostRequested.Broadcast(DisplayName, MaxPlayers);
}

void UMOHostGamePanel::HandleBackClicked()
{
	OnRequestClose.Broadcast();
}

int32 UMOHostGamePanel::ReadAndEchoMaxPlayers() const
{
	int32 MaxPlayers = UMOSessionSubsystem::DefaultMaxPlayers;
	if (MaxPlayersInputBox && !MaxPlayersInputBox->GetText().IsEmpty())
	{
		MaxPlayers = FCString::Atoi(*MaxPlayersInputBox->GetText().ToString());
	}

	MaxPlayers = UMOSessionSubsystem::ClampMaxPlayers(MaxPlayers);

	if (MaxPlayersInputBox)
	{
		MaxPlayersInputBox->SetText(FText::AsNumber(MaxPlayers));
	}
	return MaxPlayers;
}

void UMOHostGamePanel::SetStatus(const FString& Message) const
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOHostGamePanel] Status: %s"), *Message);
	if (HostStatusText)
	{
		HostStatusText->SetText(FText::FromString(Message));
	}
}
