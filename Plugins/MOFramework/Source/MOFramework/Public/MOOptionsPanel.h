#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
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

/**
 * Options/Settings panel for the in-game menu.
 *
 * Provides UI bindings for common game settings including display,
 * graphics, audio, and gameplay options. All settings are persisted
 * via UMOGameSettings.
 *
 * Widget Setup:
 * 1. Create WBP_OptionsPanel based on this class
 * 2. Add the control widgets with matching names (see BindWidget properties)
 * 3. Mark them as "Is Variable"
 */
UCLASS()
class MOFRAMEWORK_API UMOOptionsPanel : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
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
	TObjectPtr<USlider> MusicVolumeSlider;

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
