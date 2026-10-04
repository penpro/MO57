/**
 * =============================================================================
 * MOMainMenuWidget.h - Title Screen / Main Menu
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Main menu displayed on game launch. Provides New Game, Load Game, Options,
 * and Exit buttons. Layout mirrors MOInGameMenu for consistency.
 *
 * LAYOUT:
 * +------------------+------------------------+
 * | New Game         |                        |
 * | Load Game        |     Focus Window       |
 * | Options          |   (contextual panel)   |
 * | Exit Game        |                        |
 * +------------------+------------------------+
 *
 * CONTENT PANELS:
 * - NewGamePanel: World seed, game settings
 * - LoadPanel: Save slot selection
 * - OptionsPanel: Settings (shared with in-game)
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
 *
 * =============================================================================
 * RELATED FILES: MOMainMenuGameMode.h, MONewGamePanel.h, MOLoadPanel.h
 * LAST UPDATED: 2026-02-25
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
class UMOMultiplayerPanel;
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

	/** Called when a co-op session host is requested from the Multiplayer panel. */
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

	/** Show the multiplayer host/join panel in the focus window. */
	UFUNCTION(BlueprintCallable, Category="MO|MainMenu")
	void ShowMultiplayerPanel();

	/**
	 * Forward a host-attempt result from the controller back to the open
	 * Multiplayer panel (so it can un-stick its Host button on failure).
	 * No-op if the panel isn't currently the open one.
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
	UFUNCTION() void HandleMultiplayerClicked();
	UFUNCTION() void HandleExitGameClicked();

	// ============================================================================
	// PANEL HANDLERS
	// ============================================================================

	UFUNCTION() void HandlePanelRequestClose();
	UFUNCTION() void HandleLoadPanelLoadRequested(const FString& SlotName);
	UFUNCTION() void HandleNewGamePanelStartRequested();
	UFUNCTION() void HandleMultiplayerHostRequested(const FString& DisplayName, int32 MaxPlayers);

	// ============================================================================
	// INTERNAL
	// ============================================================================

	void BindButtonEvents();
	void SwitchToPanel(int32 PanelIndex);

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

	/** Multiplayer button - opens Host/Join panel. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> MultiplayerButton;

	/** Exit Game button - quits application. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> ExitGameButton;

	/**
	 * Widget switcher for the focus window on the right side.
	 * Index 0: Empty/None (shows nothing or placeholder)
	 * Index 1: New Game panel (seed configuration)
	 * Index 2: Load panel
	 * Index 3: Options panel
	 * Index 4: Multiplayer panel (host/join)
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

	/** Multiplayer host/join panel (optional - can be added directly to switcher in WBP). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOMultiplayerPanel> MultiplayerPanel;

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
	static constexpr int32 PanelIndex_Multiplayer = 4;
};
