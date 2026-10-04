/**
 * =============================================================================
 * MOMultiplayerPanel.h - Host / Join Co-op Session Panel
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Main-menu panel with two tabs: Host (create a session, then travel in as
 * listen server) and Join (search + pick from a browser list). This is the
 * player-facing surface for UMOSessionSubsystem — the backend existed
 * (net-mode-aware travel, MP-authority hardening) but nothing let a player
 * actually start or find a co-op game before this.
 *
 * DIVISION OF RESPONSIBILITY (mirrors New Game panel -> Controller):
 * - Host tab: broadcasts OnHostRequested(DisplayName, MaxPlayers) up through
 *   MOMainMenuWidget to AMOMainMenuPlayerController::HostSession(), because
 *   only the controller knows GameplayLevelPath. The controller then hands
 *   off to UMOSessionSubsystem, which creates the session AND travels once it
 *   succeeds.
 * - Join tab: talks to UMOSessionSubsystem DIRECTLY (FindSessions /
 *   JoinSessionByIndex need no level-path knowledge) — no controller
 *   round-trip needed.
 *
 * BLUEPRINT SETUP:
 * 1. Create WBP_MultiplayerPanel based on this class
 * 2. HostJoinSwitcher (WidgetSwitcher): index 0 = Host view, index 1 = Join view
 * 3. HostTabButton / JoinTabButton (UMOCommonButton) switch HostJoinSwitcher
 * 4. Host view: SessionNameInputBox, MaxPlayersInputBox (EditableTextBox),
 *    HostButton, optional HostStatusText
 * 5. Join view: RefreshButton, SessionListWidget (UMOSessionListWidget),
 *    JoinButton, optional JoinStatusText
 * 6. Optional BackButton -> OnRequestClose
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES ARISE
 * =============================================================================
 *
 * [2026-09] MAX PLAYERS CLAMP: MaxPlayersInputBox is free-text; parsed and
 *   clamped to [1, 8] (this project's co-op model is a small group, not an
 *   MMO — see MO57 project vision). Empty/invalid input defaults to 4.
 *
 * [2026-09] SUBSYSTEM DELEGATES ARE SHARED: UMOSessionSubsystem is
 *   GameInstance-lifetime and its OnSessionSearchComplete/OnJoinComplete
 *   delegates are broadcast to everyone bound. NativeDestruct unbinds this
 *   panel's handlers so a destroyed panel never reacts to a search kicked off
 *   by whatever replaces it.
 *
 * =============================================================================
 * RELATED FILES: MOSessionSubsystem.h, MOSessionListWidget.h, MOMainMenuWidget.h,
 *                MOMainMenuPlayerController.h
 * LAST UPDATED: 2026-09-22
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOActivatableWidget.h"
#include "MOUIDelegates.h"
#include "MOSessionSubsystem.h"
#include "MOMultiplayerPanel.generated.h"

class UMOCommonButton;
class UWidgetSwitcher;
class UEditableTextBox;
class UTextBlock;
class UMOSessionListWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOMultiplayerHostRequestedSignature, const FString&, DisplayName, int32, MaxPlayers);

UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOMultiplayerPanel : public UMOActivatableWidget
{
	GENERATED_BODY()

public:
	UMOMultiplayerPanel(const FObjectInitializer& ObjectInitializer);

	/** Fired when the Host button is clicked with a valid display name. Caller
	 *  (MOMainMenuWidget -> AMOMainMenuPlayerController) owns the level path. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|Multiplayer")
	FMOMultiplayerHostRequestedSignature OnHostRequested;

	/** Fired when the panel wants to close (Back button / close key). */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|Multiplayer")
	FMOUIRequestClose OnRequestClose;

	/** Show the Host tab (called by MOMainMenuWidget when opening this panel). */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Multiplayer")
	void ShowHostTab();

	/** Show the Join tab and kick off an immediate search. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Multiplayer")
	void ShowJoinTab();

	/**
	 * Called by the controller once a host attempt this panel started
	 * resolves, so the panel can surface success/failure instead of leaving
	 * the Host button silently spinning forever.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Multiplayer")
	void NotifyHostResult(bool bSuccess, const FString& ErrorMessage);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
	// ============================================================================
	// HANDLERS
	// ============================================================================

	UFUNCTION() void HandleHostTabClicked();
	UFUNCTION() void HandleJoinTabClicked();
	UFUNCTION() void HandleHostButtonClicked();
	UFUNCTION() void HandleRefreshButtonClicked();
	UFUNCTION() void HandleJoinButtonClicked();
	UFUNCTION() void HandleBackClicked();

	/** UMOSessionSubsystem::OnSessionSearchComplete handler. */
	UFUNCTION() void HandleSearchComplete(bool bSuccess, const TArray<FMOFoundSessionInfo>& Results);

	/** UMOSessionSubsystem::OnJoinComplete handler. */
	UFUNCTION() void HandleJoinComplete(bool bSuccess, const FString& ErrorMessage);

	int32 GetMaxPlayersFromInput() const;
	void SetHostStatus(const FString& Message) const;
	void SetJoinStatus(const FString& Message) const;

	// ============================================================================
	// BIND WIDGETS
	// ============================================================================

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UWidgetSwitcher> HostJoinSwitcher;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> HostTabButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> JoinTabButton;

	/** Host tab. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UEditableTextBox> SessionNameInputBox;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEditableTextBox> MaxPlayersInputBox;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> HostButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> HostStatusText;

	/** Join tab. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> RefreshButton;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOSessionListWidget> SessionListWidget;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> JoinButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> JoinStatusText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> BackButton;

	static constexpr int32 HostJoinIndex_Host = 0;
	static constexpr int32 HostJoinIndex_Join = 1;

	/** True while a host attempt this panel started is awaiting NotifyHostResult. */
	bool bHostInFlight = false;
};
