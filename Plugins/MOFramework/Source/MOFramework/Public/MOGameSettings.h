#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "MOGameSettings.generated.h"

/**
 * Game user settings for MO57.
 * Extends UGameUserSettings with game-specific options.
 *
 * Access via UMOGameSettings::GetMOGameSettings()
 */
UCLASS()
class MOFRAMEWORK_API UMOGameSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UMOGameSettings();

	/** Get the MO game settings instance. */
	UFUNCTION(BlueprintCallable, Category="MO|Settings")
	static UMOGameSettings* GetMOGameSettings();

	// ============================================================================
	// DISPLAY OPTIONS
	// ============================================================================

	/** Whether to show FPS counter on screen. */
	UPROPERTY(Config, BlueprintReadWrite, Category="Display")
	bool bShowFPSCounter = false;

	/** Whether to show frame time (ms) alongside FPS. */
	UPROPERTY(Config, BlueprintReadWrite, Category="Display")
	bool bShowFrameTime = false;

	// ============================================================================
	// GRAPHICS OPTIONS
	// ============================================================================

	/** Field of view for gameplay camera. */
	UPROPERTY(Config, BlueprintReadWrite, Category="Graphics", meta=(ClampMin="60", ClampMax="120"))
	float FieldOfView = 90.0f;

	/** Maximum frame rate (0 = unlimited). */
	UPROPERTY(Config, BlueprintReadWrite, Category="Graphics", meta=(ClampMin="0", ClampMax="240"))
	int32 MaxFrameRate = 0;

	/** Enable motion blur. */
	UPROPERTY(Config, BlueprintReadWrite, Category="Graphics")
	bool bEnableMotionBlur = true;

	// ============================================================================
	// AUDIO OPTIONS
	// ============================================================================

	/** Master volume (0-1). */
	UPROPERTY(Config, BlueprintReadWrite, Category="Audio", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MasterVolume = 1.0f;

	/** Music volume (0-1). */
	UPROPERTY(Config, BlueprintReadWrite, Category="Audio", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MusicVolume = 0.8f;

	/** Sound effects volume (0-1). */
	UPROPERTY(Config, BlueprintReadWrite, Category="Audio", meta=(ClampMin="0.0", ClampMax="1.0"))
	float SFXVolume = 1.0f;

	/** Ambient/environment volume (0-1). */
	UPROPERTY(Config, BlueprintReadWrite, Category="Audio", meta=(ClampMin="0.0", ClampMax="1.0"))
	float AmbientVolume = 1.0f;

	// ============================================================================
	// MAIN MENU / FIRST RUN
	// ============================================================================

	/** Whether to play the intro video on game launch. Set to false after first playback. */
	UPROPERTY(Config, BlueprintReadWrite, Category="MainMenu")
	bool bPlayIntro = true;

	/** Whether the user has completed their first run (seen intro, etc.). */
	UPROPERTY(Config, BlueprintReadWrite, Category="MainMenu")
	bool bHasCompletedFirstRun = false;

	/**
	 * Transient flag indicating a new game is pending after level load.
	 * Set before loading gameplay level, cleared after spawning initial pawn.
	 */
	UPROPERTY(Config, BlueprintReadWrite, Category="MainMenu")
	bool bPendingNewGame = false;

	/** Slot name for pending new game save. */
	UPROPERTY(Config, BlueprintReadWrite, Category="MainMenu")
	FString PendingNewGameSlot;

	// ============================================================================
	// GAMEPLAY OPTIONS
	// ============================================================================

	/** Camera sensitivity for mouse/controller. */
	UPROPERTY(Config, BlueprintReadWrite, Category="Gameplay", meta=(ClampMin="0.1", ClampMax="5.0"))
	float CameraSensitivity = 1.0f;

	/** Invert Y axis for camera. */
	UPROPERTY(Config, BlueprintReadWrite, Category="Gameplay")
	bool bInvertYAxis = false;

	/** Enable camera shake effects. */
	UPROPERTY(Config, BlueprintReadWrite, Category="Gameplay")
	bool bEnableCameraShake = true;

	// ============================================================================
	// METHODS
	// ============================================================================

	/** Apply all settings. Call after changing values. */
	virtual void ApplySettings(bool bCheckForCommandLineOverrides) override;

	/** Set all settings to their default values. */
	virtual void SetToDefaults() override;

	/** Apply MO-specific settings (called by ApplySettings). */
	UFUNCTION(BlueprintCallable, Category="MO|Settings")
	void ApplyMOSettings();

	/** Reset the intro playback flag so the intro plays again on next launch. */
	UFUNCTION(BlueprintCallable, Category="MO|Settings")
	void ResetIntroPlayback();

protected:
	/** Apply audio settings to sound mix. */
	void ApplyAudioSettings();

	/** Apply graphics settings. */
	void ApplyGraphicsSettings();
};
