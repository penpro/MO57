#include "MOGameSettings.h"
#include "MOFramework.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"

UMOGameSettings::UMOGameSettings()
{
}

UMOGameSettings* UMOGameSettings::GetMOGameSettings()
{
	return Cast<UMOGameSettings>(UGameUserSettings::GetGameUserSettings());
}

void UMOGameSettings::ApplySettings(bool bCheckForCommandLineOverrides)
{
	// NOTE: We intentionally do NOT call Super::ApplySettings() here.
	// The parent implementation can trigger resolution/fullscreen mode changes
	// which may cause viewport reconstruction and world reload, destroying
	// components and losing their state (like DefaultPawnClassForNewCharacter).
	//
	// Our custom settings (FPS counter, audio, etc.) don't require the parent
	// behavior. If you need to apply resolution/fullscreen changes, use
	// ApplyResolutionSettings() or ApplyNonResolutionSettings() explicitly.

	ApplyMOSettings();
}

void UMOGameSettings::SetToDefaults()
{
	Super::SetToDefaults();

	// Display
	bShowFPSCounter = false;
	bShowFrameTime = false;

	// Graphics
	FieldOfView = 90.0f;
	MaxFrameRate = 0;
	bEnableMotionBlur = true;

	// Audio
	MasterVolume = 1.0f;
	MusicVolume = 0.8f;
	SFXVolume = 1.0f;
	AmbientVolume = 1.0f;

	// Main Menu / First Run
	bPlayIntro = true;
	bHasCompletedFirstRun = false;
	bPendingNewGame = false;
	PendingNewGameSlot.Empty();

	// Gameplay
	CameraSensitivity = 1.0f;
	bInvertYAxis = false;
	bEnableCameraShake = true;
}

void UMOGameSettings::ApplyMOSettings()
{
	ApplyAudioSettings();
	ApplyGraphicsSettings();

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameSettings] Applied MO settings - FPS Counter: %s, FOV: %.1f, MaxFPS: %d"),
		bShowFPSCounter ? TEXT("ON") : TEXT("OFF"), FieldOfView, MaxFrameRate);
}

void UMOGameSettings::ApplyAudioSettings()
{
	// Audio settings would typically be applied via Sound Mix or Audio Component volumes
	// For now, just log the values - actual implementation depends on your audio setup
	UE_LOG(LogMOFramework, Log, TEXT("[MOGameSettings] Audio - Master: %.2f, Music: %.2f, SFX: %.2f, Ambient: %.2f"),
		MasterVolume, MusicVolume, SFXVolume, AmbientVolume);
}

void UMOGameSettings::ApplyGraphicsSettings()
{
	// Apply max frame rate via console variable (more reliable than GEngine->SetMaxFPS)
	static IConsoleVariable* MaxFPSCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
	if (MaxFPSCVar)
	{
		MaxFPSCVar->Set(MaxFrameRate);
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameSettings] Set t.MaxFPS to %d"), MaxFrameRate);
	}

	// Also set via GEngine for completeness
	if (GEngine)
	{
		GEngine->SetMaxFPS(MaxFrameRate > 0 ? static_cast<float>(MaxFrameRate) : 0.0f);
	}

	// Motion blur would be applied via post-process settings or console command
	static IConsoleVariable* MotionBlurCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality"));
	if (MotionBlurCVar)
	{
		MotionBlurCVar->Set(bEnableMotionBlur ? 4 : 0);
	}
}

void UMOGameSettings::ResetIntroPlayback()
{
	bPlayIntro = true;
	bHasCompletedFirstRun = false;
	SaveSettings();
	UE_LOG(LogMOFramework, Log, TEXT("[MOGameSettings] Intro playback reset - will play on next launch"));
}
