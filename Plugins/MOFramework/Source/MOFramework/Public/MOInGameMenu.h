/**
 * =============================================================================
 * MOInGameMenu.h - Pause/System Menu Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Main pause menu with navigation buttons on left and content panel on right.
 * Provides access to options, save, load, and exit functions.
 *
 * LAYOUT:
 * +------------------+------------------------+
 * | Resume           |                        |
 * | Options          |     Focus Window       |
 * | Save             |   (contextual panel)   |
 * | Load             |                        |
 * | Exit to Main     |                        |
 * | Exit Game        |                        |
 * +------------------+------------------------+
 *
 * CONTENT PANELS:
 * - Options: Graphics, audio, keybinds
 * - Save: Save slot selection
 * - Load: Load slot selection
 * - Exit dialogs: Confirmation before exit
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] WIDGET SWITCHER: FocusWindowSwitcher manages panel visibility.
 *   Index must match button order. Update both when adding panels.
 *
 * [2024-02] EXIT CONFIRMATION: Exit actions show confirmation dialog first.
 *   Don't execute exit immediately on button click.
 *
 * [2024-02] ACTIVATABLE: Extends UCommonActivatableWidget. Menu pauses game
 *   and captures input. Resume closes menu.
 *
 * =============================================================================
 * RELATED FILES: MOSavePanel.h, MOLoadPanel.h, MOOptionsPanel.h, MOConfirmationDialog.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOMenuWidgetBase.h"
#include "MOInGameMenu.generated.h"

class UMOCommonButton;
class UCommonActivatableWidgetSwitcher;
class UWidgetSwitcher;
class UMOSavePanel;
class UMOLoadPanel;
class UMOOptionsPanel;
class UPanelWidget;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOInGameMenuRequestCloseSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOInGameMenuExitToMainMenuSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOInGameMenuExitGameSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOInGameMenuSaveRequestedSignature, const FString&, SlotName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOInGameMenuLoadRequestedSignature, const FString&, SlotName);

/**
 * Inherits from UMOMenuWidgetBase for standardized CommonUI input handling.
 * Tab/Escape handling: Closes focus panel first, then closes menu via back handler.
 */
UCLASS()
class MOFRAMEWORK_API UMOInGameMenu : public UMOMenuWidgetBase
{
	GENERATED_BODY()

public:
	/** Request to close the menu (broadcasts delegate). Override from base class. */
	virtual void RequestClose() override;

	/** Refresh the save panel's list of saves. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|InGameMenu")
	void RefreshSavePanelList();

	/** Refresh the load panel's list of saves. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|InGameMenu")
	void RefreshLoadPanelList();

	/** @deprecated Use OnRequestClose (from base class) instead. Broadcast for legacy code. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|InGameMenu")
	FMOInGameMenuRequestCloseSignature OnLegacyRequestClose;

	/** Called when user confirms exit to main menu. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|InGameMenu")
	FMOInGameMenuExitToMainMenuSignature OnExitToMainMenu;

	/** Called when user confirms exit game. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|InGameMenu")
	FMOInGameMenuExitGameSignature OnExitGame;

	/** Called when user requests to save to a slot. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|InGameMenu")
	FMOInGameMenuSaveRequestedSignature OnSaveRequested;

	/** Called when user requests to load from a slot. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|InGameMenu")
	FMOInGameMenuLoadRequestedSignature OnLoadRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	// Override back action: Close focus panel first, then close menu
	virtual bool NativeOnHandleBackAction() override;

	/** Switch the focus window to show a specific panel. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|InGameMenu")
	void ShowOptionsPanel();

	UFUNCTION(BlueprintCallable, Category="MO|UI|InGameMenu")
	void ShowSavePanel();

	UFUNCTION(BlueprintCallable, Category="MO|UI|InGameMenu")
	void ShowLoadPanel();

	/** Close the focus window and return to button list. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|InGameMenu")
	void CloseFocusPanel();

	/** Check if any focus panel is currently shown. */
	UFUNCTION(BlueprintPure, Category="MO|UI|InGameMenu")
	bool IsFocusPanelOpen() const;

private:
	void BindButtonEvents();
	void SwitchToPanel(int32 PanelIndex);

	// Button handlers
	UFUNCTION() void HandleOptionsClicked();
	UFUNCTION() void HandleSaveClicked();
	UFUNCTION() void HandleLoadClicked();
	UFUNCTION() void HandleExitToMainMenuClicked();
	UFUNCTION() void HandleExitGameClicked();

	// Panel close handlers
	UFUNCTION() void HandlePanelRequestClose();

	// Save/Load forwarding handlers
	UFUNCTION() void HandleSavePanelSaveRequested(const FString& SlotName);
	UFUNCTION() void HandleLoadPanelLoadRequested(const FString& SlotName);

private:
	// ============================================================
	// BIND WIDGETS - Create these in your WBP_InGameMenu
	// ============================================================

	/** Container for the menu buttons on the left side. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> ButtonsBox;

	/** Options button. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> OptionsButton;

	/** Save game button. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> SaveButton;

	/** Load game button. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> LoadButton;

	/** Exit to main menu button. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> ExitToMainMenuButton;

	/** Exit game button. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> ExitGameButton;

	/**
	 * Widget switcher for the focus window on the right side.
	 * Index 0: Empty/None (shows nothing or placeholder)
	 * Index 1: Options panel
	 * Index 2: Save panel
	 * Index 3: Load panel
	 */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UWidgetSwitcher> FocusWindowSwitcher;

	// ============================================================
	// OPTIONAL BIND WIDGETS - Panel instances if you add them directly
	// ============================================================

	/** Options panel (optional - can be added directly to switcher in WBP). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOOptionsPanel> OptionsPanel;

	/** Save panel (optional - can be added directly to switcher in WBP). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOSavePanel> SavePanel;

	/** Load panel (optional - can be added directly to switcher in WBP). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOLoadPanel> LoadPanel;

	// ============================================================
	// State
	// ============================================================

	/** Currently active panel index in the switcher. */
	int32 CurrentPanelIndex = 0;

	/** Panel indices. */
	static constexpr int32 PanelIndex_None = 0;
	static constexpr int32 PanelIndex_Options = 1;
	static constexpr int32 PanelIndex_Save = 2;
	static constexpr int32 PanelIndex_Load = 3;
};
