#include "MOSessionSubsystem.h"
#include "MOFramework.h"
#include "MOTravelUtils.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void UMOSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	UE_LOG(LogMOFramework, Log, TEXT("[MOSession] Initialize: active online subsystem = %s"),
		OnlineSub ? *OnlineSub->GetSubsystemName().ToString() : TEXT("none"));
}

void UMOSessionSubsystem::Deinitialize()
{
	if (const IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);

		// The online subsystem outlives this game instance (always in the editor, where it persists across PIE
		// runs; and Steam keeps a lobby listed until it is left). Fire-and-forget: nothing is left to listen.
		if (Sessions->GetNamedSession(NAME_GameSession))
		{
			Sessions->DestroySession(NAME_GameSession);
		}
	}
	AfterDestroy = nullptr;

	Super::Deinitialize();
}

UMOSessionSubsystem* UMOSessionSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UMOSessionSubsystem>() : nullptr;
}

int32 UMOSessionSubsystem::ClampMaxPlayers(int32 Requested)
{
	return FMath::Clamp(Requested, MinPlayers, MaxPlayersLimit);
}

IOnlineSessionPtr UMOSessionSubsystem::GetSessionInterface() const
{
	const IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	return OnlineSub ? OnlineSub->GetSessionInterface() : nullptr;
}

bool UMOSessionSubsystem::IsUsingRealOnlineSubsystem() const
{
	const IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	return OnlineSub && OnlineSub->GetSubsystemName() != NULL_SUBSYSTEM;
}

bool UMOSessionSubsystem::IsLanMode() const
{
	return !IsUsingRealOnlineSubsystem();
}

bool UMOSessionSubsystem::HasActiveSession() const
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	return Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
}

// ============================================================================
// HOST
// ============================================================================

bool UMOSessionSubsystem::HostSession(const FString& DisplayName, int32 MaxPlayers, const FString& GameplayLevelPath)
{
	if (!GetSessionInterface().IsValid())
	{
		return HostSessionNow(DisplayName, MaxPlayers, GameplayLevelPath);  // reports the missing subsystem
	}

	if (bDestroyInFlight || HasActiveSession())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSession] HostSession: a stale session is registered -- destroying it first"));
		DestroyThen([this, DisplayName, MaxPlayers, GameplayLevelPath]()
		{
			HostSessionNow(DisplayName, MaxPlayers, GameplayLevelPath);
		});
		return true;
	}

	return HostSessionNow(DisplayName, MaxPlayers, GameplayLevelPath);
}

bool UMOSessionSubsystem::HostSessionNow(const FString& DisplayName, int32 MaxPlayers, const FString& GameplayLevelPath)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOSession] HostSession: no session interface (no online subsystem active)"));
		OnHostComplete.Broadcast(false, TEXT("No online subsystem available."));
		return false;
	}

	if (HasActiveSession())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSession] HostSession: a session is already active — call LeaveSession first"));
		OnHostComplete.Broadcast(false, TEXT("Already in a session."));
		return false;
	}

	const ULocalPlayer* LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	if (!LocalPlayer)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOSession] HostSession: no local player"));
		OnHostComplete.Broadcast(false, TEXT("No local player."));
		return false;
	}

	PendingHostLevelPath = GameplayLevelPath;

	const int32 EffectiveMaxPlayers = ClampMaxPlayers(MaxPlayers);

	FOnlineSessionSettings SessionSettings;
	SessionSettings.NumPublicConnections = EffectiveMaxPlayers;
	SessionSettings.bShouldAdvertise = true;
	SessionSettings.bIsLANMatch = IsLanMode();
	SessionSettings.bIsDedicated = false;
	SessionSettings.bAllowJoinInProgress = true;
	SessionSettings.bUsesPresence = IsUsingRealOnlineSubsystem();
	SessionSettings.bAllowJoinViaPresence = IsUsingRealOnlineSubsystem();
	SessionSettings.bUseLobbiesIfAvailable = IsUsingRealOnlineSubsystem();
	SessionSettings.Set(FName(TEXT("MOSESSIONNAME")), DisplayName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	CreateSessionCompleteHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UMOSessionSubsystem::HandleCreateSessionComplete));

	UE_LOG(LogMOFramework, Warning, TEXT("[MOSession] HostSession: creating '%s' (max %d, requested %d, %s)"),
		*DisplayName, EffectiveMaxPlayers, MaxPlayers, IsUsingRealOnlineSubsystem() ? TEXT("Steam") : TEXT("Null/LAN"));

	if (!Sessions->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionSettings))
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
		UE_LOG(LogMOFramework, Error, TEXT("[MOSession] HostSession: CreateSession call failed to start"));
		OnHostComplete.Broadcast(false, TEXT("Failed to start session creation."));
		return false;
	}

	return true;
}

void UMOSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (const IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
	}

	if (!bWasSuccessful)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOSession] CreateSession failed for '%s'"), *SessionName.ToString());
		OnHostComplete.Broadcast(false, TEXT("Failed to create session."));
		return;
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOSession] CreateSession succeeded — traveling to '%s' as listen server"), *PendingHostLevelPath);
	OnHostComplete.Broadcast(true, FString());

	if (!UMOTravelUtils::TravelToGameplayLevel(this, PendingHostLevelPath, /*bAsListenServer=*/true))
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOSession] Post-host travel to '%s' failed"), *PendingHostLevelPath);
	}
}

// ============================================================================
// FIND
// ============================================================================

bool UMOSessionSubsystem::IsSearchInProgress() const
{
	return SessionSearch.IsValid() && SessionSearch->SearchState == EOnlineAsyncTaskState::InProgress;
}

void UMOSessionSubsystem::FindSessions()
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOSession] FindSessions: no session interface"));
		OnSessionSearchComplete.Broadcast(false, {});
		return;
	}

	if (IsSearchInProgress())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSession] FindSessions: search already in progress"));
		return;
	}

	const ULocalPlayer* LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	if (!LocalPlayer)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOSession] FindSessions: no local player"));
		OnSessionSearchComplete.Broadcast(false, {});
		return;
	}

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->MaxSearchResults = 50;
	SessionSearch->bIsLanQuery = IsLanMode();
	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, IsUsingRealOnlineSubsystem(), EOnlineComparisonOp::Equals);

	FindSessionsCompleteHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UMOSessionSubsystem::HandleFindSessionsComplete));

	UE_LOG(LogMOFramework, Log, TEXT("[MOSession] FindSessions: searching (%s)"),
		IsUsingRealOnlineSubsystem() ? TEXT("Steam") : TEXT("Null/LAN"));

	if (!Sessions->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), SessionSearch.ToSharedRef()))
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		UE_LOG(LogMOFramework, Error, TEXT("[MOSession] FindSessions: call failed to start"));
		OnSessionSearchComplete.Broadcast(false, {});
	}
}

void UMOSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (const IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
	}

	TArray<FMOFoundSessionInfo> Results;

	if (!bWasSuccessful || !SessionSearch.IsValid())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSession] FindSessions failed or no search object"));
		OnSessionSearchComplete.Broadcast(false, Results);
		return;
	}

	Results.Reserve(SessionSearch->SearchResults.Num());
	for (int32 Index = 0; Index < SessionSearch->SearchResults.Num(); ++Index)
	{
		const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[Index];

		FString FoundDisplayName;
		if (!Result.Session.SessionSettings.Get(FName(TEXT("MOSESSIONNAME")), FoundDisplayName) || FoundDisplayName.IsEmpty())
		{
			FoundDisplayName = Result.Session.OwningUserName.IsEmpty() ? TEXT("Unnamed Game") : Result.Session.OwningUserName;
		}

		FMOFoundSessionInfo Info;
		Info.ResultIndex = Index;
		Info.DisplayName = FoundDisplayName;
		Info.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
		Info.CurrentPlayers = Info.MaxPlayers - Result.Session.NumOpenPublicConnections;
		Info.PingMs = Result.PingInMs;
		Info.bIsFull = Result.Session.NumOpenPublicConnections <= 0;
		Results.Add(Info);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOSession] FindSessions complete: %d result(s)"), Results.Num());
	for (const FMOFoundSessionInfo& Info : Results)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOSession]   [%d] '%s' %d/%d players, %dms%s"),
			Info.ResultIndex, *Info.DisplayName, Info.CurrentPlayers, Info.MaxPlayers, Info.PingMs,
			Info.bIsFull ? TEXT(" (FULL)") : TEXT(""));
	}
	OnSessionSearchComplete.Broadcast(true, Results);
}

// ============================================================================
// JOIN
// ============================================================================

bool UMOSessionSubsystem::JoinSessionByIndex(int32 ResultIndex)
{
	if (GetSessionInterface().IsValid() && (bDestroyInFlight || HasActiveSession()))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSession] JoinSessionByIndex: a stale session is registered -- destroying it first"));
		DestroyThen([this, ResultIndex]()
		{
			JoinSessionNow(ResultIndex);
		});
		return true;
	}

	return JoinSessionNow(ResultIndex);
}

bool UMOSessionSubsystem::JoinSessionNow(int32 ResultIndex)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOSession] JoinSessionByIndex: no session interface"));
		OnJoinComplete.Broadcast(false, TEXT("No online subsystem available."));
		return false;
	}

	if (!SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(ResultIndex))
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOSession] JoinSessionByIndex: invalid index %d"), ResultIndex);
		OnJoinComplete.Broadcast(false, TEXT("Invalid session selection."));
		return false;
	}

	const ULocalPlayer* LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	if (!LocalPlayer)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOSession] JoinSessionByIndex: no local player"));
		OnJoinComplete.Broadcast(false, TEXT("No local player."));
		return false;
	}

	JoinSessionCompleteHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UMOSessionSubsystem::HandleJoinSessionComplete));

	UE_LOG(LogMOFramework, Warning, TEXT("[MOSession] JoinSessionByIndex: joining result %d"), ResultIndex);

	if (!Sessions->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionSearch->SearchResults[ResultIndex]))
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		UE_LOG(LogMOFramework, Error, TEXT("[MOSession] JoinSessionByIndex: call failed to start"));
		OnJoinComplete.Broadcast(false, TEXT("Failed to start join."));
		return false;
	}

	return true;
}

void UMOSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid())
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
	}

	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOSession] JoinSession failed: %d"), static_cast<int32>(Result));
		OnJoinComplete.Broadcast(false, TEXT("Failed to join session."));
		return;
	}

	FString ConnectString;
	if (!Sessions.IsValid() || !Sessions->GetResolvedConnectString(SessionName, ConnectString) || ConnectString.IsEmpty())
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOSession] JoinSession: could not resolve connect string"));
		OnJoinComplete.Broadcast(false, TEXT("Could not resolve host address."));
		return;
	}

	APlayerController* PC = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	if (!PC)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOSession] JoinSession: no local player controller to travel"));
		OnJoinComplete.Broadcast(false, TEXT("No player controller."));
		return;
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOSession] JoinSession succeeded — ClientTravel to '%s'"), *ConnectString);
	OnJoinComplete.Broadcast(true, FString());
	PC->ClientTravel(ConnectString, TRAVEL_Absolute);
}

// ============================================================================
// LEAVE
// ============================================================================

void UMOSessionSubsystem::LeaveSession()
{
	DestroyThen(nullptr);
}

void UMOSessionSubsystem::ReleaseStaleSession()
{
	if (HasActiveSession() && !bDestroyInFlight)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSession] ReleaseStaleSession: leaving a session that outlived its world"));
		LeaveSession();
	}
}

void UMOSessionSubsystem::DestroyThen(TFunction<void()> Continuation)
{
	if (bDestroyInFlight)
	{
		if (Continuation)
		{
			AfterDestroy = MoveTemp(Continuation);
		}
		return;
	}

	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || !Sessions->GetNamedSession(NAME_GameSession))
	{
		if (Continuation)
		{
			Continuation();
		}
		return;
	}

	bDestroyInFlight = true;
	AfterDestroy = MoveTemp(Continuation);
	DestroySessionCompleteHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UMOSessionSubsystem::HandleDestroySessionComplete));

	UE_LOG(LogMOFramework, Log, TEXT("[MOSession] LeaveSession: destroying session"));

	// Some subsystems complete synchronously (the delegate fires inside the call), others later. If the call itself
	// fails to start, no delegate will ever fire, so complete it ourselves -- unless it already did.
	if (!Sessions->DestroySession(NAME_GameSession) && bDestroyInFlight)
	{
		HandleDestroySessionComplete(NAME_GameSession, false);
	}
}

void UMOSessionSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (const IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOSession] DestroySession '%s' complete: %s"),
		*SessionName.ToString(), bWasSuccessful ? TEXT("success") : TEXT("failed"));

	bDestroyInFlight = false;
	if (TFunction<void()> Next = MoveTemp(AfterDestroy))
	{
		AfterDestroy = nullptr;
		Next();
	}
}
