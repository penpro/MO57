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
	PendingWorldName.Empty();
	PendingWorldSeed = 0;

	// Gameplay
	CameraSensitivity = 1.0f;
	bInvertYAxis = false;
	bEnableCameraShake = true;

	// Key Bindings - Note: We don't clear custom bindings on SetToDefaults
	// Use ClearAllCustomBindings() explicitly if needed
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
	// Get audio device to set master volume
	if (FAudioDeviceHandle AudioDevice = GEngine ? GEngine->GetMainAudioDevice() : FAudioDeviceHandle())
	{
		// Set transient master volume (affects all audio output)
		// This multiplies with all other volume settings
		AudioDevice->SetTransientPrimaryVolume(MasterVolume);
	}

	// For Sound Class volumes, use the gameplay statics approach if you have sound classes set up.
	// Create Sound Classes in content (SC_Music, SC_SFX, SC_Ambient) and assign them to sounds.
	// Then load and apply mix overrides:
	//
	// Example setup (would require assets to be created):
	// USoundClass* MusicClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Audio/SoundClasses/SC_Music.SC_Music"));
	// USoundClass* SFXClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Audio/SoundClasses/SC_SFX.SC_SFX"));
	// USoundClass* AmbientClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Audio/SoundClasses/SC_Ambient.SC_Ambient"));
	//
	// if (MusicClass)
	// {
	//     UGameplayStatics::SetSoundMixClassOverride(GEngine->GetWorld(), nullptr, MusicClass, MusicVolume, 1.0f, 0.0f, true);
	// }
	//
	// For now, we rely on the master volume and individual sounds using audio components can check these values.
	// Blueprint audio components can read MusicVolume, SFXVolume, AmbientVolume and apply them manually.

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameSettings] Audio applied - Master: %.2f, Music: %.2f, SFX: %.2f, Ambient: %.2f"),
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

void UMOGameSettings::MarkReturningFromGameplay()
{
	bPlayIntro = false;
	UE_LOG(LogMOFramework, Log, TEXT("[MOGameSettings] Marked returning from gameplay - intro will NOT play"));
}

// ============================================================================
// KEY BINDINGS
// ============================================================================

const FMOKeyBindingEntry* UMOGameSettings::FindCustomBinding(FName ActionId, int32 SlotIndex) const
{
	for (const FMOKeyBindingEntry& Entry : CustomKeyBindings)
	{
		if (Entry.ActionId == ActionId && Entry.SlotIndex == SlotIndex)
		{
			return &Entry;
		}
	}
	return nullptr;
}

void UMOGameSettings::SetCustomBinding(FName ActionId, FKey NewKey, int32 SlotIndex)
{
	// Check if binding already exists
	for (FMOKeyBindingEntry& Entry : CustomKeyBindings)
	{
		if (Entry.ActionId == ActionId && Entry.SlotIndex == SlotIndex)
		{
			Entry.OverrideKey = NewKey;
			UE_LOG(LogMOFramework, Log, TEXT("[MOGameSettings] Updated key binding: %s -> %s"),
				*ActionId.ToString(), *NewKey.ToString());
			return;
		}
	}

	// Add new binding
	FMOKeyBindingEntry NewEntry(ActionId, NewKey, SlotIndex);
	CustomKeyBindings.Add(NewEntry);
	UE_LOG(LogMOFramework, Log, TEXT("[MOGameSettings] Added new key binding: %s -> %s"),
		*ActionId.ToString(), *NewKey.ToString());
}

void UMOGameSettings::RemoveCustomBinding(FName ActionId, int32 SlotIndex)
{
	const int32 RemovedCount = CustomKeyBindings.RemoveAll([ActionId, SlotIndex](const FMOKeyBindingEntry& Entry)
	{
		return Entry.ActionId == ActionId && Entry.SlotIndex == SlotIndex;
	});

	if (RemovedCount > 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameSettings] Removed custom binding for: %s"), *ActionId.ToString());
	}
}

void UMOGameSettings::ClearAllCustomBindings()
{
	const int32 Count = CustomKeyBindings.Num();
	CustomKeyBindings.Empty();
	UE_LOG(LogMOFramework, Log, TEXT("[MOGameSettings] Cleared %d custom key bindings"), Count);
}

// ============================================================================
// WORLD SEED
// ============================================================================

int32 UMOGameSettings::GenerateRandomSeed()
{
	// Generate a random seed using current time and random int
	PendingWorldSeed = FMath::Rand();
	UE_LOG(LogMOFramework, Log, TEXT("[MOGameSettings] Generated random seed: %d"), PendingWorldSeed);
	return PendingWorldSeed;
}

int32 UMOGameSettings::SeedFromString(const FString& SeedString)
{
	if (SeedString.IsEmpty())
	{
		return 0;
	}

	// Try to parse as integer first
	if (SeedString.IsNumeric())
	{
		return FCString::Atoi(*SeedString);
	}

	// Otherwise hash the string
	return GetTypeHash(SeedString);
}

int32 UMOGameSettings::GetWorldSeed()
{
	const UMOGameSettings* Settings = GetMOGameSettings();
	if (Settings)
	{
		return Settings->PendingWorldSeed;
	}
	return 0;
}
