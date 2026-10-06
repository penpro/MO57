/**
 * =============================================================================
 * MOJoinGamePanel.h - Join Co-op Game Panel (Main Menu)
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Focus panel behind the main menu's "Join Game" button (JoinGameButton ->
 * JoinGamePanel, the same pairing as LoadGameButton -> LoadPanel). It searches for
 * sessions other players are hosting, lists them, and joins the selected one.
 * Hosting is a separate panel (UMOHostGamePanel).
 *
 * DIVISION OF RESPONSIBILITY:
 * Unlike hosting, joining needs no level-path knowledge, so this panel talks to
 * UMOSessionSubsystem DIRECTLY (FindSessions / JoinSessionByIndex) with no
 * controller round trip. The subsystem travels the client into the host's world
 * on success.
 *
 * FLOW:
 * UMOMainMenuWidget::ShowJoinGamePanel -> RefreshSessions() (search starts as
 * soon as the panel opens) -> OnSessionSearchComplete fills SessionListWidget ->
 * player selects a row -> JoinButton enables -> JoinSessionByIndex ->
 * OnJoinComplete (failure re-enables the panel; success leaves it disabled while
 * travel happens).
 *
 * BLUEPRINT SETUP (create WBP_JoinGamePanel with this class as parent and add it
 * as a child of the main menu's FocusWindowSwitcher, in any position after the
 * four existing panels; the easiest path is to copy the list + buttons from
 * WBP_LoadPanel and rename them):
 *
 *   Widget name          Type                  Required?
 *   ------------------   -------------------   ---------
 *   SessionListWidget    UMOSessionListWidget  YES   the browser list
 *   RefreshButton        UMOCommonButton       YES   search again
 *   JoinButton           UMOCommonButton       YES   join the selected row
 *   JoinStatusText       UTextBlock            no    "Searching...", "2 found" etc.
 *   BackButton           UMOCommonButton       no    closes the panel
 *
 * The list needs two more Blueprints:
 *   - WBP_SessionListEntry (parent UMOSessionListEntry): optional text blocks
 *     SessionNameText and SessionDetailText, plus the base class's optional
 *     EntryButton and BackgroundBorder.
 *   - On the SessionListWidget instance, set EntryWidgetClass to
 *     WBP_SessionListEntry and put a ScrollBox named ContentScrollBox (or a
 *     VerticalBox named ContentContainer) inside the list widget.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES ARISE
 * =============================================================================
 *
 * [2026-10] SUBSYSTEM DELEGATES ARE SHARED: UMOSessionSubsystem lives as long as
 *   the GameInstance and broadcasts OnSessionSearchComplete / OnJoinComplete to
 *   everyone bound, including searches started by the MO.Session.Find console
 *   command. NativeDestruct unbinds this panel so a destroyed panel never reacts
 *   to a search started by whatever replaces it.
 *
 * [2026-10] ONE FAILURE CHANNEL: every failed JoinSessionByIndex path broadcasts
 *   OnJoinComplete(false, ...) before returning, so the panel clears its
 *   in-flight flag in HandleJoinComplete only; do not also react to the bool
 *   return value or the status text would be overwritten.
 *
 * [2026-10] FINDSESSIONS IS SILENT WHEN BUSY: a second FindSessions call during a
 *   search returns without broadcasting, so RefreshSessions() checks
 *   IsSearchInProgress() first instead of waiting for a callback that never comes.
 *
 * =============================================================================
 * RELATED FILES: MOSessionSubsystem.h, MOSessionListWidget.h, MOSessionListEntry.h,
 *                MOMainMenuWidget.h, MOHostGamePanel.h
 * LAST UPDATED: 2026-10-05
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOActivatableWidget.h"
#include "MOUIDelegates.h"
#include "MOSessionSubsystem.h"
#include "MOJoinGamePanel.generated.h"

class UMOCommonButton;
class UTextBlock;
class UMOSessionListWidget;

UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOJoinGamePanel : public UMOActivatableWidget
{
	GENERATED_BODY()

public:
	UMOJoinGamePanel(const FObjectInitializer& ObjectInitializer);

	/** Fired when the panel wants to close (Back button / close key). */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|Multiplayer")
	FMOUIRequestClose OnRequestClose;

	/**
	 * Start a session search and repopulate the list from the results. Called by
	 * UMOMainMenuWidget whenever the panel is opened and by the Refresh button.
	 * No-op while a search is already running.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Multiplayer")
	void RefreshSessions();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
	UFUNCTION() void HandleRefreshClicked();
	UFUNCTION() void HandleJoinClicked();
	UFUNCTION() void HandleBackClicked();

	/** UMOScrollListBase::OnEntrySelected / OnSelectionCleared. */
	UFUNCTION() void HandleSessionSelected(FName SelectedId);
	UFUNCTION() void HandleSessionSelectionCleared();

	/** UMOSessionSubsystem::OnSessionSearchComplete. */
	UFUNCTION() void HandleSearchComplete(bool bSuccess, const TArray<FMOFoundSessionInfo>& Results);

	/** UMOSessionSubsystem::OnJoinComplete. */
	UFUNCTION() void HandleJoinComplete(bool bSuccess, const FString& ErrorMessage);

	/** Single place that decides which buttons are usable right now. */
	void UpdateButtonStates();
	void SetStatus(const FString& Message) const;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOSessionListWidget> SessionListWidget;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> RefreshButton;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> JoinButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> JoinStatusText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> BackButton;

	/** True from the Join click until OnJoinComplete reports a failure. On success
	 *  it stays true: the subsystem is travelling and the panel must not react. */
	bool bJoinInFlight = false;
};
