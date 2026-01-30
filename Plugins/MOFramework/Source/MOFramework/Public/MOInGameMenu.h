#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MOInGameMenu.generated.h"

class UMOCommonButton;
class UCommonActivatableWidgetSwitcher;
class UWidgetSwitcher;
class UMOSavePanel;
class UMOLoadPanel;
class UMOOptionsPanel;
class UPanelWidget;

/**
 * In-game menu with button list on the left and focus window on the right.
 *
 * The focus window displays different content based on which button is selected:
 * - Options panel
 * - Save panel
 * - Load panel
 * - Confirmation dialogs for exit actions
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOInGameMenuRequestCloseSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOInGameMenuExitToMainMenuSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOInGameMenuExitGameSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOInGameMenuSaveRequestedSignature, const FString&, SlotName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOInGameMenuLoadRequestedSignature, const FString&, SlotName);

UCLASS()
class MOFRAMEWORK_API UMOInGameMenu : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** Request to close the menu (broadcasts delegate). */
	UFUNCTION(BlueprintCallable, Category="MO|UI|InGameMenu")
	void RequestClose();

	/** Refresh the save panel's list of saves. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|InGameMenu")
	void RefreshSavePanelList();

	/** Refresh the load panel's list of saves. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|InGameMenu")
	void RefreshLoadPanelList();

	/** Called when menu should close. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|InGameMenu")
	FMOInGameMenuRequestCloseSignature OnRequestClose;

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
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

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
