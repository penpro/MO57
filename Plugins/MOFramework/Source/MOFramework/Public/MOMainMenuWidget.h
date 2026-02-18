#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MOMainMenuWidget.generated.h"

class UMOCommonButton;
class UWidgetSwitcher;
class UMOLoadPanel;
class UMOOptionsPanel;
class UMONewGamePanel;
class UPanelWidget;

/**
 * Main menu widget displayed on the loading/title screen.
 *
 * Layout mirrors the in-game menu:
 * - Button list on the left
 * - Focus window (panel switcher) on the right
 *
 * Buttons:
 * - New Game: Start a new world
 * - Load Game: Load existing world (shows LoadPanel)
 * - Options: Game settings (shows OptionsPanel)
 * - Exit Game: Quit application
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOMainMenuNewGameSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOMainMenuLoadGameSignature, const FString&, SlotName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOMainMenuExitGameSignature);

UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOMainMenuWidget : public UCommonActivatableWidget
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
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	// ============================================================================
	// BUTTON HANDLERS
	// ============================================================================

	UFUNCTION() void HandleNewGameClicked();
	UFUNCTION() void HandleLoadGameClicked();
	UFUNCTION() void HandleOptionsClicked();
	UFUNCTION() void HandleExitGameClicked();

	// ============================================================================
	// PANEL HANDLERS
	// ============================================================================

	UFUNCTION() void HandlePanelRequestClose();
	UFUNCTION() void HandleLoadPanelLoadRequested(const FString& SlotName);
	UFUNCTION() void HandleNewGamePanelStartRequested();

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

	/** Exit Game button - quits application. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> ExitGameButton;

	/**
	 * Widget switcher for the focus window on the right side.
	 * Index 0: Empty/None (shows nothing or placeholder)
	 * Index 1: New Game panel (seed configuration)
	 * Index 2: Load panel
	 * Index 3: Options panel
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
};
