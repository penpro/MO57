/**
 * =============================================================================
 * MOOptionsPanel.h - Game Settings/Options Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Comprehensive settings panel for display, graphics, audio, gameplay, and
 * key bindings. Used in both main menu and in-game menu. All settings
 * persist via UMOGameSettings.
 *
 * SECTIONS:
 * - Display: FPS counter, frame time
 * - Graphics: Resolution, fullscreen, FOV, max FPS, motion blur
 * - Audio: Master, Music, SFX, Ambient volume sliders
 * - Gameplay: Camera sensitivity, invert Y, camera shake
 * - Key Bindings: Rebindable input actions
 *
 * WIDGET SETUP:
 * 1. Create WBP_OptionsPanel based on this class
 * 2. Add control widgets with matching names (see BindWidget properties)
 * 3. All widgets are BindWidgetOptional - add only what you need
 * 4. Set KeyBindingEntryClass for rebinding UI
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] APPLY VS INSTANT: Volume sliders apply instantly via
 *   HandleXxxVolumeChanged() for real-time audio feedback. Resolution,
 *   fullscreen, and other graphics settings require Apply button.
 *
 * [2024-02] RESOLUTION REVERT: Resolution changes need confirmation with
 *   15-second timeout. If user doesn't confirm, revert to previous resolution.
 *   Prevents player being locked out with unusable display settings.
 *
 * [2024-02] KEY BINDING CONFLICTS: HandleKeyBindingChanged() should check for
 *   duplicates via MOKeyBindingManager. Either warn user or auto-swap bindings.
 *
 * [2024-02] SOFT OBJECT LOADING: PawnControlContext and BuildingContext are
 *   TSoftObjectPtr. Call .LoadSynchronous() in NativeConstruct() or before
 *   PopulateKeyBindings(). Returns nullptr if asset path invalid.
 *
 * [2024-02] DELEGATE BINDING: NativeConstruct() binds all slider/button
 *   handlers. Uses RemoveAll(this) pattern before binding to prevent
 *   duplicate callbacks on widget reuse.
 *
 * [2024-02] SETTINGS PERSISTENCE: All changes go to UMOGameSettings. Call
 *   UMOGameSettings::Get()->SaveSettings() after ApplySettings() to persist
 *   to disk. RefreshUIFromSettings() reads current values on open.
 *
 * [2024-02] NULL WIDGETS: All widgets use BindWidgetOptional. Check for
 *   nullptr before accessing. Missing widgets are safely skipped.
 *
 * =============================================================================
 * RELATED FILES: MOGameSettings.h, MOKeyBindingEntryWidget.h, MOKeyBindingManager.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOActivatableWidget.h"
#include "MOKeyBindingTypes.h"
#include "MOOptionsPanel.generated.h"

class UCommonButtonBase;
class UCheckBox;
class USlider;
class UTextBlock;
class USpinBox;
class UComboBoxString;
class UScrollBox;
class UMOKeyBindingEntryWidget;
class UInputMappingContext;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOptionsPanelRequestCloseSignature);
UCLASS()
class MOFRAMEWORK_API UMOOptionsPanel : public UMOActivatableWidget
{
	GENERATED_BODY()

public:
	UMOOptionsPanel(const FObjectInitializer& ObjectInitializer);

	/** Apply current settings. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Options")
	virtual void ApplySettings();

	/** Reset settings to defaults. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Options")
	virtual void ResetToDefaults();

	/** Called when panel requests to close. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|Options")
	FMOOptionsPanelRequestCloseSignature OnRequestClose;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** Called when settings should be refreshed from current values. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|UI|Options")
	void OnRefreshSettings();

	/** Refresh all UI controls to match current settings values. */
	void RefreshUIFromSettings();

	/** Read values from UI controls and store in pending settings. */
	void ReadUIToSettings();

private:
	UFUNCTION() void HandleApplyClicked();
	UFUNCTION() void HandleResetClicked();
	UFUNCTION() void HandleBackClicked();

	// Slider value change handlers
	UFUNCTION() void HandleMasterVolumeChanged(float Value);
	UFUNCTION() void HandleMusicVolumeChanged(float Value);
	UFUNCTION() void HandleSFXVolumeChanged(float Value);
	UFUNCTION() void HandleAmbientVolumeChanged(float Value);
	UFUNCTION() void HandleWeatherVolumeChanged(float Value);
	UFUNCTION() void HandleCameraSensitivityChanged(float Value);

	// Resolution/display handlers
	UFUNCTION() void HandleResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	void UpdateVolumeLabel(UTextBlock* Label, float Value);
	void PopulateResolutionOptions();
	FIntPoint GetResolutionFromString(const FString& ResString) const;

private:
	// ============================================================
	// BUTTONS
	// ============================================================

	/** Apply settings button. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> ApplyButton;

	/** Reset to defaults button. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> ResetButton;

	/** Back/Close button. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCommonButtonBase> BackButton;

	// ============================================================
	// DISPLAY OPTIONS
	// ============================================================

	/** Toggle FPS counter display. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UCheckBox> ShowFPSCheckbox;

	/** Toggle frame time display (alongside FPS). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UCheckBox> ShowFrameTimeCheckbox;

	// ============================================================
	// GRAPHICS OPTIONS
	// ============================================================

	/** Resolution dropdown (720p, 1080p, 1440p, 4K). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UComboBoxString> ResolutionComboBox;

	/** Fullscreen mode checkbox. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UCheckBox> FullscreenCheckbox;

	/** Field of view slider (60-120). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<USlider> FOVSlider;

	/** FOV value display. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> FOVValueText;

	/** Max frame rate spinbox (0 = unlimited). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<USpinBox> MaxFrameRateSpinBox;

	/** Enable motion blur. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UCheckBox> MotionBlurCheckbox;

	// ============================================================
	// AUDIO OPTIONS
	// ============================================================

	/** Master volume slider. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<USlider> MasterVolumeSlider;

	/** Master volume percentage display. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MasterVolumeText;

	/** Music volume slider. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<USlider> MusicSlider;

	/** Music volume percentage display. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MusicVolumeText;

	/** SFX volume slider. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<USlider> SFXVolumeSlider;

	/** SFX volume percentage display. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SFXVolumeText;

	/** Ambient volume slider. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<USlider> AmbientVolumeSlider;

	/** Ambient volume percentage display. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> AmbientVolumeText;

	/** Weather volume slider (UDW rain/thunder/wind). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<USlider> WeatherVolumeSlider;

	/** Weather volume percentage display. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> WeatherVolumeText;

	// ============================================================
	// GAMEPLAY OPTIONS
	// ============================================================

	/** Camera sensitivity slider. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<USlider> CameraSensitivitySlider;

	/** Camera sensitivity value display. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CameraSensitivityText;

	/** Invert Y axis for camera. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UCheckBox> InvertYCheckbox;

	/** Enable camera shake effects. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UCheckBox> CameraShakeCheckbox;

	// ============================================================
	// KEY BINDINGS
	// ============================================================

	/** Scroll box containing key binding entries. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UScrollBox> KeyBindingsScrollBox;

	/** Button to reset all key bindings to defaults. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> ResetAllBindingsButton;

	/** Entry widget class for key binding rows. */
	UPROPERTY(EditAnywhere, Category="MO|UI|Options|KeyBindings")
	TSubclassOf<UMOKeyBindingEntryWidget> KeyBindingEntryClass;

	/** Primary input mapping context (for movement, UI, actions). */
	UPROPERTY(EditAnywhere, Category="MO|UI|Options|KeyBindings")
	TSoftObjectPtr<UInputMappingContext> PawnControlContext;

	/** Building input mapping context. */
	UPROPERTY(EditAnywhere, Category="MO|UI|Options|KeyBindings")
	TSoftObjectPtr<UInputMappingContext> BuildingContext;

	/** List of rebindable actions. Configure in Blueprint. */
	UPROPERTY(EditAnywhere, Category="MO|UI|Options|KeyBindings")
	TArray<FMOKeyBindingActionConfig> RebindableActions;

private:
	/** Populate the key bindings scroll box with entry widgets. */
	void PopulateKeyBindings();

	/** Handle when a key binding entry changes. */
	UFUNCTION()
	void HandleKeyBindingChanged(FName ActionId, FKey NewKey);

	/** Handle reset all bindings button click. */
	UFUNCTION()
	void HandleResetAllBindingsClicked();
};
