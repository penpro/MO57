#include "MOMultiplayerPanel.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOSessionListWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"

UMOMultiplayerPanel::UMOMultiplayerPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UMOMultiplayerPanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (HostTabButton)
	{
		HostTabButton->OnClicked().RemoveAll(this);
		HostTabButton->OnClicked().AddUObject(this, &UMOMultiplayerPanel::HandleHostTabClicked);
	}
	if (JoinTabButton)
	{
		JoinTabButton->OnClicked().RemoveAll(this);
		JoinTabButton->OnClicked().AddUObject(this, &UMOMultiplayerPanel::HandleJoinTabClicked);
	}
	if (HostButton)
	{
		HostButton->OnClicked().RemoveAll(this);
		HostButton->OnClicked().AddUObject(this, &UMOMultiplayerPanel::HandleHostButtonClicked);
	}
	if (RefreshButton)
	{
		RefreshButton->OnClicked().RemoveAll(this);
		RefreshButton->OnClicked().AddUObject(this, &UMOMultiplayerPanel::HandleRefreshButtonClicked);
	}
	if (JoinButton)
	{
		JoinButton->OnClicked().RemoveAll(this);
		JoinButton->OnClicked().AddUObject(this, &UMOMultiplayerPanel::HandleJoinButtonClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked().RemoveAll(this);
		BackButton->OnClicked().AddUObject(this, &UMOMultiplayerPanel::HandleBackClicked);
	}

	if (UMOSessionSubsystem* Sessions = UMOSessionSubsystem::Get(this))
	{
		Sessions->OnSessionSearchComplete.RemoveDynamic(this, &UMOMultiplayerPanel::HandleSearchComplete);
		Sessions->OnSessionSearchComplete.AddDynamic(this, &UMOMultiplayerPanel::HandleSearchComplete);
		Sessions->OnJoinComplete.RemoveDynamic(this, &UMOMultiplayerPanel::HandleJoinComplete);
		Sessions->OnJoinComplete.AddDynamic(this, &UMOMultiplayerPanel::HandleJoinComplete);
	}

	if (MaxPlayersInputBox && MaxPlayersInputBox->GetText().IsEmpty())
	{
		MaxPlayersInputBox->SetText(FText::AsNumber(4));
	}

	ShowHostTab();
}

void UMOMultiplayerPanel::NativeDestruct()
{
	if (HostTabButton) HostTabButton->OnClicked().RemoveAll(this);
	if (JoinTabButton) JoinTabButton->OnClicked().RemoveAll(this);
	if (HostButton) HostButton->OnClicked().RemoveAll(this);
	if (RefreshButton) RefreshButton->OnClicked().RemoveAll(this);
	if (JoinButton) JoinButton->OnClicked().RemoveAll(this);
	if (BackButton) BackButton->OnClicked().RemoveAll(this);

	if (UMOSessionSubsystem* Sessions = UMOSessionSubsystem::Get(this))
	{
		Sessions->OnSessionSearchComplete.RemoveDynamic(this, &UMOMultiplayerPanel::HandleSearchComplete);
		Sessions->OnJoinComplete.RemoveDynamic(this, &UMOMultiplayerPanel::HandleJoinComplete);
	}

	Super::NativeDestruct();
}

UWidget* UMOMultiplayerPanel::NativeGetDesiredFocusTarget() const
{
	if (HostJoinSwitcher && HostJoinSwitcher->GetActiveWidgetIndex() == HostJoinIndex_Join)
	{
		if (RefreshButton) return RefreshButton;
	}
	if (SessionNameInputBox) return SessionNameInputBox;
	if (HostButton) return HostButton;
	return nullptr;
}

void UMOMultiplayerPanel::ShowHostTab()
{
	if (HostJoinSwitcher)
	{
		HostJoinSwitcher->SetActiveWidgetIndex(HostJoinIndex_Host);
	}
}

void UMOMultiplayerPanel::ShowJoinTab()
{
	if (HostJoinSwitcher)
	{
		HostJoinSwitcher->SetActiveWidgetIndex(HostJoinIndex_Join);
	}
	HandleRefreshButtonClicked();
}

void UMOMultiplayerPanel::NotifyHostResult(bool bSuccess, const FString& ErrorMessage)
{
	bHostInFlight = false;

	if (bSuccess)
	{
		SetHostStatus(TEXT("Session created — entering world..."));
		return;
	}

	SetHostStatus(FString::Printf(TEXT("Failed to host: %s"), *ErrorMessage));
	if (HostButton)
	{
		HostButton->SetIsEnabled(true);
	}
}

void UMOMultiplayerPanel::HandleHostTabClicked()
{
	ShowHostTab();
}

void UMOMultiplayerPanel::HandleJoinTabClicked()
{
	ShowJoinTab();
}

void UMOMultiplayerPanel::HandleHostButtonClicked()
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

	const int32 MaxPlayers = GetMaxPlayersFromInput();

	UE_LOG(LogMOFramework, Log, TEXT("[MOMultiplayerPanel] Host requested: '%s' (max %d)"), *DisplayName, MaxPlayers);

	bHostInFlight = true;
	SetHostStatus(TEXT("Creating session..."));
	if (HostButton)
	{
		HostButton->SetIsEnabled(false);
	}

	OnHostRequested.Broadcast(DisplayName, MaxPlayers);
}

void UMOMultiplayerPanel::HandleRefreshButtonClicked()
{
	UMOSessionSubsystem* Sessions = UMOSessionSubsystem::Get(this);
	if (!Sessions)
	{
		SetJoinStatus(TEXT("No online subsystem available."));
		return;
	}

	if (RefreshButton)
	{
		RefreshButton->SetIsEnabled(false);
	}
	if (JoinButton)
	{
		JoinButton->SetIsEnabled(false);
	}

	SetJoinStatus(TEXT("Searching..."));
	Sessions->FindSessions();
}

void UMOMultiplayerPanel::HandleJoinButtonClicked()
{
	UMOSessionSubsystem* Sessions = UMOSessionSubsystem::Get(this);
	FMOFoundSessionInfo Selected;
	if (!Sessions || !SessionListWidget || !SessionListWidget->GetSelectedSessionInfo(Selected))
	{
		SetJoinStatus(TEXT("Select a session first."));
		return;
	}

	if (JoinButton)
	{
		JoinButton->SetIsEnabled(false);
	}
	SetJoinStatus(FString::Printf(TEXT("Joining '%s'..."), *Selected.DisplayName));
	Sessions->JoinSessionByIndex(Selected.ResultIndex);
}

void UMOMultiplayerPanel::HandleBackClicked()
{
	OnRequestClose.Broadcast();
}

void UMOMultiplayerPanel::HandleSearchComplete(bool bSuccess, const TArray<FMOFoundSessionInfo>& Results)
{
	if (RefreshButton)
	{
		RefreshButton->SetIsEnabled(true);
	}
	if (JoinButton)
	{
		JoinButton->SetIsEnabled(true);
	}

	if (SessionListWidget)
	{
		SessionListWidget->SetSessionResults(Results);
	}

	if (!bSuccess)
	{
		SetJoinStatus(TEXT("Search failed."));
	}
	else if (Results.Num() == 0)
	{
		SetJoinStatus(TEXT("No sessions found."));
	}
	else
	{
		SetJoinStatus(FString::Printf(TEXT("%d session(s) found."), Results.Num()));
	}
}

void UMOMultiplayerPanel::HandleJoinComplete(bool bSuccess, const FString& ErrorMessage)
{
	if (JoinButton)
	{
		JoinButton->SetIsEnabled(true);
	}

	SetJoinStatus(bSuccess ? TEXT("Joined — entering world...") : FString::Printf(TEXT("Failed to join: %s"), *ErrorMessage));
}

int32 UMOMultiplayerPanel::GetMaxPlayersFromInput() const
{
	int32 MaxPlayers = 4;
	if (MaxPlayersInputBox && !MaxPlayersInputBox->GetText().IsEmpty())
	{
		MaxPlayers = FCString::Atoi(*MaxPlayersInputBox->GetText().ToString());
	}
	return FMath::Clamp(MaxPlayers, 1, 8);
}

void UMOMultiplayerPanel::SetHostStatus(const FString& Message) const
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMultiplayerPanel] Host status: %s"), *Message);
	if (HostStatusText)
	{
		HostStatusText->SetText(FText::FromString(Message));
	}
}

void UMOMultiplayerPanel::SetJoinStatus(const FString& Message) const
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMultiplayerPanel] Join status: %s"), *Message);
	if (JoinStatusText)
	{
		JoinStatusText->SetText(FText::FromString(Message));
	}
}
