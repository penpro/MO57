#include "MOJoinGamePanel.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOSessionListWidget.h"
#include "Components/TextBlock.h"

UMOJoinGamePanel::UMOJoinGamePanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UMOJoinGamePanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (RefreshButton)
	{
		RefreshButton->OnClicked().RemoveAll(this);
		RefreshButton->OnClicked().AddUObject(this, &UMOJoinGamePanel::HandleRefreshClicked);
	}
	if (JoinButton)
	{
		JoinButton->OnClicked().RemoveAll(this);
		JoinButton->OnClicked().AddUObject(this, &UMOJoinGamePanel::HandleJoinClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked().RemoveAll(this);
		BackButton->OnClicked().AddUObject(this, &UMOJoinGamePanel::HandleBackClicked);
	}

	if (SessionListWidget)
	{
		SessionListWidget->OnEntrySelected.RemoveDynamic(this, &UMOJoinGamePanel::HandleSessionSelected);
		SessionListWidget->OnEntrySelected.AddDynamic(this, &UMOJoinGamePanel::HandleSessionSelected);
		SessionListWidget->OnSelectionCleared.RemoveDynamic(this, &UMOJoinGamePanel::HandleSessionSelectionCleared);
		SessionListWidget->OnSelectionCleared.AddDynamic(this, &UMOJoinGamePanel::HandleSessionSelectionCleared);
	}

	if (UMOSessionSubsystem* Sessions = UMOSessionSubsystem::Get(this))
	{
		Sessions->OnSessionSearchComplete.RemoveDynamic(this, &UMOJoinGamePanel::HandleSearchComplete);
		Sessions->OnSessionSearchComplete.AddDynamic(this, &UMOJoinGamePanel::HandleSearchComplete);
		Sessions->OnJoinComplete.RemoveDynamic(this, &UMOJoinGamePanel::HandleJoinComplete);
		Sessions->OnJoinComplete.AddDynamic(this, &UMOJoinGamePanel::HandleJoinComplete);
	}

	UpdateButtonStates();
}

void UMOJoinGamePanel::NativeDestruct()
{
	if (RefreshButton) RefreshButton->OnClicked().RemoveAll(this);
	if (JoinButton) JoinButton->OnClicked().RemoveAll(this);
	if (BackButton) BackButton->OnClicked().RemoveAll(this);

	if (SessionListWidget)
	{
		SessionListWidget->OnEntrySelected.RemoveDynamic(this, &UMOJoinGamePanel::HandleSessionSelected);
		SessionListWidget->OnSelectionCleared.RemoveDynamic(this, &UMOJoinGamePanel::HandleSessionSelectionCleared);
	}

	if (UMOSessionSubsystem* Sessions = UMOSessionSubsystem::Get(this))
	{
		Sessions->OnSessionSearchComplete.RemoveDynamic(this, &UMOJoinGamePanel::HandleSearchComplete);
		Sessions->OnJoinComplete.RemoveDynamic(this, &UMOJoinGamePanel::HandleJoinComplete);
	}

	Super::NativeDestruct();
}

UWidget* UMOJoinGamePanel::NativeGetDesiredFocusTarget() const
{
	if (RefreshButton) return RefreshButton;
	if (JoinButton) return JoinButton;
	return nullptr;
}

void UMOJoinGamePanel::RefreshSessions()
{
	UMOSessionSubsystem* Sessions = UMOSessionSubsystem::Get(this);
	if (!Sessions)
	{
		SetStatus(TEXT("No online subsystem available."));
		return;
	}

	if (bJoinInFlight)
	{
		return;
	}

	if (Sessions->IsSearchInProgress())
	{
		// A search is already running (opened twice, or started from the console).
		// Its completion will repopulate the list, so don't start another.
		UpdateButtonStates();
		return;
	}

	SetStatus(TEXT("Searching..."));
	Sessions->FindSessions();
	// FindSessions broadcasts on every failure path except "already running",
	// which is handled above. Update after the call so IsSearchInProgress() is current.
	UpdateButtonStates();
}

void UMOJoinGamePanel::HandleRefreshClicked()
{
	RefreshSessions();
}

void UMOJoinGamePanel::HandleJoinClicked()
{
	UMOSessionSubsystem* Sessions = UMOSessionSubsystem::Get(this);
	FMOFoundSessionInfo Selected;
	if (!Sessions || !SessionListWidget || !SessionListWidget->GetSelectedSessionInfo(Selected))
	{
		SetStatus(TEXT("Select a session first."));
		return;
	}

	if (bJoinInFlight || Selected.bIsFull)
	{
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOJoinGamePanel] Join requested: '%s' (result %d)"),
		*Selected.DisplayName, Selected.ResultIndex);

	bJoinInFlight = true;
	SetStatus(FString::Printf(TEXT("Joining '%s'..."), *Selected.DisplayName));
	UpdateButtonStates();

	// Every failure path inside JoinSessionByIndex broadcasts OnJoinComplete(false),
	// so the in-flight flag is cleared there, not from the return value.
	Sessions->JoinSessionByIndex(Selected.ResultIndex);
}

void UMOJoinGamePanel::HandleBackClicked()
{
	OnRequestClose.Broadcast();
}

void UMOJoinGamePanel::HandleSessionSelected(FName /*SelectedId*/)
{
	UpdateButtonStates();
}

void UMOJoinGamePanel::HandleSessionSelectionCleared()
{
	UpdateButtonStates();
}

void UMOJoinGamePanel::HandleSearchComplete(bool bSuccess, const TArray<FMOFoundSessionInfo>& Results)
{
	if (SessionListWidget)
	{
		SessionListWidget->SetSessionResults(Results);
	}

	if (!bSuccess)
	{
		SetStatus(TEXT("Search failed."));
	}
	else if (Results.Num() == 0)
	{
		SetStatus(TEXT("No sessions found."));
	}
	else
	{
		SetStatus(FString::Printf(TEXT("%d session(s) found."), Results.Num()));
	}

	UpdateButtonStates();
}

void UMOJoinGamePanel::HandleJoinComplete(bool bSuccess, const FString& ErrorMessage)
{
	if (bSuccess)
	{
		// The subsystem is travelling into the host's world. Stay in the in-flight
		// state so nothing can be clicked during the transition.
		SetStatus(TEXT("Joined. Entering world..."));
		return;
	}

	bJoinInFlight = false;
	SetStatus(FString::Printf(TEXT("Failed to join: %s"), *ErrorMessage));
	UpdateButtonStates();
}

void UMOJoinGamePanel::UpdateButtonStates()
{
	const UMOSessionSubsystem* Sessions = UMOSessionSubsystem::Get(this);
	const bool bSearching = Sessions && Sessions->IsSearchInProgress();

	FMOFoundSessionInfo Selected;
	const bool bJoinableSelection = SessionListWidget
		&& SessionListWidget->GetSelectedSessionInfo(Selected)
		&& !Selected.bIsFull;

	if (RefreshButton)
	{
		RefreshButton->SetIsEnabled(!bSearching && !bJoinInFlight);
	}
	if (JoinButton)
	{
		JoinButton->SetIsEnabled(bJoinableSelection && !bSearching && !bJoinInFlight);
	}
}

void UMOJoinGamePanel::SetStatus(const FString& Message) const
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOJoinGamePanel] Status: %s"), *Message);
	if (JoinStatusText)
	{
		JoinStatusText->SetText(FText::FromString(Message));
	}
}
