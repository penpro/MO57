/**
 * =============================================================================
 * MOSessionSubsystem.h - Steam Session Hosting / Browsing / Joining
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * The player-facing entry point for real co-op: create a session (become a
 * host other players can find/join) and search for + join sessions other
 * players are hosting. Wraps IOnlineSession (resolved via IOnlineSubsystem::Get,
 * whichever platform subsystem is active — Steam per Config/DefaultEngine.ini's
 * DefaultPlatformService, or Null if Steam can't init, e.g. dev machine without
 * Steam running). GameInstanceSubsystem because the online subsystem interfaces
 * are inherently GameInstance-lifetime, matching UE's own reference pattern.
 *
 * This is the missing piece the MP-authority hardening campaign (H17-H21, the
 * 2-client PIE test harness) sat on top of: that work made an EXISTING
 * connection replicate correctly; this subsystem is how two players actually
 * find and connect to each other in the first place.
 *
 * FLOW:
 * - Host: HostSession(DisplayName, MaxPlayers) -> OnHostComplete -> on success,
 *   the subsystem itself travels to gameplay as a listen server
 *   (MOTravelUtils::TravelToGameplayLevel with bAsListenServer=true).
 * - Join: FindSessions() -> OnSessionSearchComplete(Results) -> UI picks one ->
 *   JoinSessionByIndex(Index) -> OnJoinComplete -> on success, ClientTravel to
 *   the resolved connect string.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES ARISE
 * =============================================================================
 *
 * [2026-09] STEAM INIT CAN SILENTLY FALL BACK TO NULL: if SteamAPI_Init fails
 *   (Steam not running/logged in, or the App ID isn't fully configured on the
 *   Steamworks partner site yet), OnlineSubsystemSteam logs "Unable to create
 *   OnlineSubsystem instance Steam" and the engine falls back to the Null
 *   subsystem per Config/DefaultEngine.ini's fallback NetDriverDefinitions.
 *   Sessions still "work" against Null (useful for solo dev testing of this
 *   subsystem's plumbing) but won't be visible to real Steam friends/players.
 *   IsUsingRealOnlineSubsystem() reports which one is actually active.
 *
 * [2026-09] SESSION NAME IS FIXED: uses the engine's NAME_GameSession constant
 *   (the same session name UE's own templates/samples use) — one active
 *   session per local player, which matches this project's co-op model
 *   (small group, not simultaneous multi-session).
 *
 * [2026-09] HOST TRAVELS ITSELF: HostSession, on success, calls
 *   MOTravelUtils::TravelToGameplayLevel(..., bAsListenServer=true) directly —
 *   callers don't need to (and shouldn't) also call TravelToGameplayLevel.
 *
 * =============================================================================
 * RELATED FILES: MOTravelUtils.h, MOMainMenuPlayerController.h,
 *                MOMultiplayerPanel.h
 * LAST UPDATED: 2026-09-22
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "MOSessionSubsystem.generated.h"

/** One entry in a session search result list — BlueprintType-friendly summary
 *  of an FOnlineSessionSearchResult. ResultIndex is opaque; pass it back to
 *  JoinSessionByIndex, don't try to re-derive it. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOFoundSessionInfo
{
	GENERATED_BODY()

	/** Index into the subsystem's current SessionSearch results — pass to JoinSessionByIndex. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Session")
	int32 ResultIndex = INDEX_NONE;

	/** Host's display name (the string HostSession was given). Falls back to the
	 *  Steam owning-user's name if the host didn't set one. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Session")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="MO|Session")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category="MO|Session")
	int32 MaxPlayers = 0;

	/** Ping in ms, -1 if unknown. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Session")
	int32 PingMs = -1;

	/** True if this session is already full (UI should disable its Join button). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Session")
	bool bIsFull = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOSessionHostCompleteSignature, bool, bSuccess, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOSessionSearchCompleteSignature, bool, bSuccess, const TArray<FMOFoundSessionInfo>&, Results);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOSessionJoinCompleteSignature, bool, bSuccess, const FString&, ErrorMessage);

UCLASS()
class MOFRAMEWORK_API UMOSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Convenience accessor — same pattern as the other subsystems. */
	UFUNCTION(BlueprintPure, Category="MO|Session", meta=(WorldContext="WorldContextObject"))
	static UMOSessionSubsystem* Get(const UObject* WorldContextObject);

	// ============================================================================
	// HOST
	// ============================================================================

	/**
	 * Create a session and, on success, travel to GameplayLevelPath as a listen
	 * server. DisplayName is shown to other players browsing sessions (e.g. via
	 * FindSessions); MaxPlayers bounds NumPublicConnections.
	 * @return false immediately if no online subsystem/session interface is
	 *         available, or a session is already active (call LeaveSession first).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Session")
	bool HostSession(const FString& DisplayName, int32 MaxPlayers, const FString& GameplayLevelPath);

	UPROPERTY(BlueprintAssignable, Category="MO|Session")
	FMOSessionHostCompleteSignature OnHostComplete;

	// ============================================================================
	// FIND
	// ============================================================================

	/** Search for joinable sessions. Results arrive via OnSessionSearchComplete. */
	UFUNCTION(BlueprintCallable, Category="MO|Session")
	void FindSessions();

	UPROPERTY(BlueprintAssignable, Category="MO|Session")
	FMOSessionSearchCompleteSignature OnSessionSearchComplete;

	/** True while a FindSessions search is in flight (UI should disable Refresh). */
	UFUNCTION(BlueprintPure, Category="MO|Session")
	bool IsSearchInProgress() const;

	// ============================================================================
	// JOIN
	// ============================================================================

	/**
	 * Join the session at ResultIndex from the most recent FindSessions results.
	 * On success, ClientTravels the first local player controller to the
	 * resolved connect string.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Session")
	bool JoinSessionByIndex(int32 ResultIndex);

	UPROPERTY(BlueprintAssignable, Category="MO|Session")
	FMOSessionJoinCompleteSignature OnJoinComplete;

	// ============================================================================
	// LEAVE / STATUS
	// ============================================================================

	/** Destroy the current session (host) or leave it (client). Safe to call with none active. */
	UFUNCTION(BlueprintCallable, Category="MO|Session")
	void LeaveSession();

	UFUNCTION(BlueprintPure, Category="MO|Session")
	bool HasActiveSession() const;

	/** True if the ACTIVE online subsystem is Steam (not the Null fallback) —
	 *  see the class header's boot-fallback pitfall note. UI can use this to
	 *  show a "not connected to Steam" hint instead of silently degrading. */
	UFUNCTION(BlueprintPure, Category="MO|Session")
	bool IsUsingRealOnlineSubsystem() const;

private:
	IOnlineSessionPtr GetSessionInterface() const;

	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	/** Gameplay level path captured at HostSession time — consumed once the
	 *  create-session delegate fires (async, so can't be a local variable). */
	FString PendingHostLevelPath;

	TSharedPtr<FOnlineSessionSearch> SessionSearch;

	FDelegateHandle CreateSessionCompleteHandle;
	FDelegateHandle FindSessionsCompleteHandle;
	FDelegateHandle JoinSessionCompleteHandle;
	FDelegateHandle DestroySessionCompleteHandle;
};
