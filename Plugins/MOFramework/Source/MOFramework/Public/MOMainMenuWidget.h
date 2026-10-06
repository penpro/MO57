/**
 * =============================================================================
 * MOMainMenuWidget.h - Title Screen / Main Menu
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Main menu displayed on game launch. Provides New Game, Load Game, Host Game,
 * Join Game, Options, and Exit buttons. Layout mirrors MOInGameMenu for
 * consistency. Host Game and Join Game are optional: omit their buttons from the
 * WBP and the menu behaves exactly as it did before co-op existed.
 *
 * LAYOUT:
 * +------------------+------------------------+
 * | New Game         |                        |
 * | Load Game        |     Focus Window       |
 * | Host Game        |   (contextual panel)   |
 * | Join Game        |                        |
 * | Options          |                        |
 * | Exit Game        |                        |
 * +------------------+------------------------+
 *
 * CONTENT PANELS (button -> panel; panels are found by TYPE inside
 * FocusWindowSwitcher, so only the BUTTONS need exact widget names):
 * - NewGameButton  -> NewGamePanel : World seed, game settings
 * - LoadGameButton -> LoadPanel    : Save slot selection
 * - HostGameButton -> HostGamePanel: Name a co-op camp and start hosting
 * - JoinGameButton -> JoinGamePanel: Browse and join a hosted camp
 * - OptionsButton  -> OptionsPanel : Settings (shared with in-game)
 *
 * ADDING THE CO-OP BUTTONS TO WBP_MOInGameMenu1 (the main menu blueprint):
 * 1. Duplicate an existing button (e.g. LoadGameButton) and rename it EXACTLY
 *    HostGameButton / JoinGameButton. Keep it a UMOCommonButton.
 * 2. Add WBP_HostGamePanel / WBP_JoinGamePanel (see MOHostGamePanel.h and
 *    MOJoinGamePanel.h for their widget lists) to FocusWindowSwitcher. Order does
 *    not matter: the switcher index is looked up from the panel widget itself.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] GAME MODE: Main menu uses MOMainMenuGameMode, not the play mode.
 *   Different PlayerController and HUD setup.
 *
 * [2024-02] LEVEL TRANSITION: New Game and Load transition to play level.
 *   Handle cleanup before OpenLevel.
 *
 * [2024-02] PANEL INDICES: Widget switcher indices must match button order.
 *   See CLAUDE.md "New Game Panel Blueprint Setup" for index mapping.
 *   (None=0, NewGame=1, Load=2, Options=3 are fixed by convention.)
 *
 * [2026-10] CO-OP PANELS RESOLVE THEIR OWN INDEX: Host and Join panels use
 *   FocusWindowSwitcher->GetChildIndex(Panel) and have no fixed index. A
 *   hard-coded index would silently show the wrong panel (or nothing) when a
 *   designer adds only one of the two or in a different order. A co-op button
 *   whose panel is missing from the switcher logs a warning and does nothing.
 *
 * [2026-10] HOST RESULT ROUTING: the Host panel does NOT close itself on request
 *   (CreateSession is async and can fail). The controller reports back through
 *   NotifyHostSessionResult, which forwards to the Host panel.
 *
 * =============================================================================
 * RELATED FILES: MOMainMenuGameMode.h, MONewGamePanel.h, MOLoadPanel.h,
 *                MOHostGamePanel.h, MOJoinGamePanel.h, MOMainMenuPlayerController.h
 * LAST UPDATED: 2026-10-05
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOActivatableWidget.h"
#include "MOMainMenuWidget.generated.h"

class UMOCommonButton;
class UWidgetSwitcher;
class UMOLoadPanel;
class UMOOptionsPanel;
class UMONewGamePanel;
class UMOHostGamePanel;
class UMOJoinGamePanel;
class UPanelWidget;
class UTextBlock;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOMainMenuNewGameSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOMainMenuLoadGameSignature, const FString&, SlotName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOMainMenuExitGameSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOMainMenuHostSessionSignature, const FString&, DisplayName, int32, MaxPlayers);

UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOMainMenuWidget : public UMOActivatableWidget
{
	GENERATED_BODY()

public:
	UMOMainMenuWidget(const FObjectInitializer& ObjectInitializer);

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Called when New Game button is clicked. */
	UPROPERTY(BlueprintAssignable, Category="MO|MainMenu")
	FMOMainMenuNewGameSignature OnNewGameRequested;

	/** Called when a save slot is selected for loading. */
	UPROPERTY(BlueprintAssignable, Category="MO|MainMenu")
	FMOMainMenuLoadGameSignature OnLoadGameRequested;

	/** Called when Exit Game is confirmed. */
	UPROPERTY(BlueprintAssignable, Category="MO|MainMenu")
	FMOMainMenuExitGameSignature OnExitGameRequested;

	/** Called when a co-op session host is requested from the Host Game panel. */
	UPROPERTY(BlueprintAssignable, Category="MO|MainMenu")
	FMOMainMenuHostSessionSignature OnHostSessionRequested;

	// ============================================================================
	// PANEL CONTROL
	// ============================================================================

	/** Show the new game panel in the focus window. */
	UFUNCTION(BlueprintCallable, Category="MO|MainMenu")
	void ShowNewGamePanel();

	/** Show the options panel in the focus window. */
	UFUNCTION(BlueprintCallable, Category="MO|MainMenu")
	void ShowOptionsPanel();

	/** Show the load panel in the focus window. */
	UFUNCTION(BlueprintCallable, Category="MO|MainMenu")
	void ShowLoadPanel();

	/** Show the Host Game panel in the focus window. No-op (with a warning) if the
	 *  WBP has no UMOHostGamePanel in FocusWindowSwitcher. */
	UFUNCTION(BlueprintCallable, Category="MO|MainMenu")
	void ShowHostGamePanel();

	/** Show the Join Game panel in the focus window and start a session search.
	 *  No-op (with a warning) if the WBP has no UMOJoinGamePanel in FocusWindowSwitcher. */
	UFUNCTION(BlueprintCallable, Category="MO|MainMenu")
	void ShowJoinGamePanel();

	/**
	 * Forward a host-attempt result from the controller back to the Host Game
	 * panel (so it can un-stick its Host button on failure).
	 * No-op if the WBP has no Host Game panel.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|MainMenu")
	void NotifyHostSessionResult(bool bSuccess, const FString& ErrorMessage);

	/** Close the current focus panel (return to none). */
	UFUNCTION(BlueprintCallable, Category="MO|MainMenu")
	void CloseFocusPanel();

	/** Check if any focus panel is currently open. */
	UFUNCTION(BlueprintPure, Category="MO|MainMenu")
	bool IsFocusPanelOpen() const;

	/** Refresh the load panel's list of saves. */
	UFUNCTION(BlueprintCallable, Category="MO|MainMenu")
	void RefreshLoadPanelList();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/**
	 * Close keys back out of an open focus panel; at the bare title screen they
	 * do nothing — there is nothing behind the main menu, and deactivating it
	 * soft-locks the pawn-less level (hidden cursor, no input surface).
	 */
	virtual bool NativeOnCloseKeyRequested(const FKeyEvent& InKeyEvent) override;

private:
	// ============================================================================
	// BUTTON HANDLERS
	// ============================================================================

	UFUNCTION() void HandleNewGameClicked();
	UFUNCTION() void HandleLoadGameClicked();
	UFUNCTION() void HandleOptionsClicked();
	UFUNCTION() void HandleHostGameClicked();
	UFUNCTION() void HandleJoinGameClicked();
	UFUNCTION() void HandleExitGameClicked();

	// ============================================================================
	// PANEL HANDLERS
	// ============================================================================

	UFUNCTION() void HandlePanelRequestClose();
	UFUNCTION() void HandleLoadPanelLoadRequested(const FString& SlotName);
	UFUNCTION() void HandleNewGamePanelStartRequested();
	UFUNCTION() void HandleHostPanelHostRequested(const FString& DisplayName, int32 MaxPlayers);

	// ============================================================================
	// INTERNAL
	// ============================================================================

	void BindButtonEvents();
	void SwitchToPanel(int32 PanelIndex);

	/**
	 * Switcher index of a co-op panel, looked up from the widget itself so the
	 * designer's child order doesn't matter. Returns INDEX_NONE if the panel is
	 * missing or not a child of FocusWindowSwitcher.
	 */
	int32 FindPanelIndex(const UWidget* Panel) const;

	// ============================================================================
	// BIND WIDGETS
	// ============================================================================

	/** Container for the menu buttons on the left side. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> ButtonsBox;

	/** New Game button - starts a fresh world. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> NewGameButton;

	/** Load Game button - opens load panel. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> LoadGameButton;

	/** Options button - opens options panel. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> OptionsButton;

	/** Host Game button - opens the Host Game panel (optional: co-op UI). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> HostGameButton;

	/** Join Game button - opens the Join Game panel and starts a search (optional: co-op UI). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> JoinGameButton;

	/** Exit Game button - quits application. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> ExitGameButton;

	/**
	 * Widget switcher for the focus window on the right side.
	 * Index 0: Empty/None (shows nothing or placeholder)
	 * Index 1: New Game panel (seed configuration)
	 * Index 2: Load panel
	 * Index 3: Options panel
	 * Index 4+: Host Game and Join Game panels, in any order (looked up by widget, see
	 *              FindPanelIndex, so no fixed index is needed)
	 */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UWidgetSwitcher> FocusWindowSwitcher;

	// ============================================================================
	// OPTIONAL BIND WIDGETS
	// ============================================================================

	/** New game panel (optional - can be added directly to switcher in WBP). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMONewGamePanel> NewGamePanel;

	/** Load panel (optional - can be added directly to switcher in WBP). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOLoadPanel> LoadPanel;

	/** Options panel (optional - can be added directly to switcher in WBP). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOOptionsPanel> OptionsPanel;

	/** Host Game panel (optional - found by type if not named; see MOHostGamePanel.h). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOHostGamePanel> HostGamePanel;

	/** Join Game panel (optional - found by type if not named; see MOJoinGamePanel.h). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOJoinGamePanel> JoinGamePanel;

	/**
	 * Optional text block showing project version + commit hash + branch.
	 * Add a UTextBlock named "BuildInfoLabel" to WBP_MainMenu (small font,
	 * corner of the screen). NativeConstruct populates it from
	 * UMOBuildInfo::GetDisplayLabel(). Omit it from the WBP if you don't
	 * want the label visible.
	 */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> BuildInfoLabel;

	// ============================================================================
	// STATE
	// ============================================================================

	/** Currently active panel index. */
	int32 CurrentPanelIndex = 0;

	/** Panel indices. */
	static constexpr int32 PanelIndex_None = 0;
	static constexpr int32 PanelIndex_NewGame = 1;
	static constexpr int32 PanelIndex_Load = 2;
	static constexpr int32 PanelIndex_Options = 3;
	// Host Game / Join Game panels have no constant: see FindPanelIndex.
};
