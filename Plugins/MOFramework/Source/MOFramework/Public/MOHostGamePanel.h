/**
 * =============================================================================
 * MOHostGamePanel.h - Host Co-op Game Panel (Main Menu)
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Focus panel behind the main menu's "Host Game" button (HostGameButton ->
 * HostGamePanel, the same pairing as NewGameButton -> NewGamePanel). The player
 * names the camp and picks a player limit; this panel broadcasts the request and
 * shows progress. Joining is a separate panel (UMOJoinGamePanel).
 *
 * DIVISION OF RESPONSIBILITY (mirrors New Game panel -> Controller):
 * - This panel only collects input and shows status. It broadcasts
 *   OnHostRequested(DisplayName, MaxPlayers) up through UMOMainMenuWidget to
 *   AMOMainMenuPlayerController::HostSession(), because only the controller
 *   knows GameplayLevelPath. The controller hands off to UMOSessionSubsystem,
 *   which creates the session AND travels as a listen server on success.
 * - The controller reports the outcome back through
 *   UMOMainMenuWidget::NotifyHostSessionResult -> NotifyHostResult here, so the
 *   Host button never stays disabled after a failure.
 *
 * BLUEPRINT SETUP (create WBP_HostGamePanel with this class as parent and add it
 * as a child of the main menu's FocusWindowSwitcher, in any position after the
 * four existing panels; the easiest path is to copy the widgets from
 * WBP_NewGamePanel and rename them):
 *
 *   Widget name           Type                 Required?
 *   -------------------   ------------------   ---------
 *   SessionNameInputBox   UEditableTextBox     YES   camp / session name
 *   HostButton            UMOCommonButton      YES   starts hosting
 *   MaxPlayersInputBox    UEditableTextBox     no    clamped to the subsystem's range
 *   HostStatusText        UTextBlock           no    "Creating session..." etc.
 *   BackButton            UMOCommonButton      no    closes the panel
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES ARISE
 * =============================================================================
 *
 * [2026-10] PLAYER LIMIT POLICY LIVES IN THE SUBSYSTEM: min / max / default are
 *   UMOSessionSubsystem::MinPlayers / MaxPlayersLimit / DefaultMaxPlayers and
 *   UMOSessionSubsystem::ClampMaxPlayers. This panel only echoes the clamped
 *   value back to the text box; HostSession clamps again, so the console command
 *   and the UI can never disagree.
 *
 * [2026-10] HOST BUTTON RE-ENABLE: CreateSession is async and can fail (Steam
 *   unavailable, already in a session). The button is disabled while a request is
 *   in flight and re-enabled ONLY by NotifyHostResult(false, ...) or
 *   PrepareForDisplay(), so a failed attempt can be retried without reopening
 *   the menu.
 *
 * =============================================================================
 * RELATED FILES: MOSessionSubsystem.h, MOMainMenuWidget.h,
 *                MOMainMenuPlayerController.h, MOJoinGamePanel.h
 * LAST UPDATED: 2026-10-05
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOActivatableWidget.h"
#include "MOUIDelegates.h"
#include "MOHostGamePanel.generated.h"

class UMOCommonButton;
class UEditableTextBox;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOHostGameRequestedSignature, const FString&, DisplayName, int32, MaxPlayers);

UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOHostGamePanel : public UMOActivatableWidget
{
	GENERATED_BODY()

public:
	UMOHostGamePanel(const FObjectInitializer& ObjectInitializer);

	/** Fired when the Host button is clicked. The caller (UMOMainMenuWidget ->
	 *  AMOMainMenuPlayerController) owns the gameplay level path. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|Multiplayer")
	FMOHostGameRequestedSignature OnHostRequested;

	/** Fired when the panel wants to close (Back button / close key). */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|Multiplayer")
	FMOUIRequestClose OnRequestClose;

	/** Reset to a fresh, usable state (button enabled, status cleared, default
	 *  player count). Called by UMOMainMenuWidget every time the panel is opened. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Multiplayer")
	void PrepareForDisplay();

	/**
	 * Called once the host attempt this panel started resolves, so the panel can
	 * show success or failure instead of leaving the Host button spinning.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Multiplayer")
	void NotifyHostResult(bool bSuccess, const FString& ErrorMessage);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
	UFUNCTION() void HandleHostButtonClicked();
	UFUNCTION() void HandleBackClicked();

	/** Parse MaxPlayersInputBox, clamp via the subsystem policy, echo it back. */
	int32 ReadAndEchoMaxPlayers() const;
	void SetStatus(const FString& Message) const;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UEditableTextBox> SessionNameInputBox;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> HostButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEditableTextBox> MaxPlayersInputBox;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> HostStatusText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> BackButton;

	/** True while a host attempt this panel started is awaiting NotifyHostResult. */
	bool bHostInFlight = false;
};
