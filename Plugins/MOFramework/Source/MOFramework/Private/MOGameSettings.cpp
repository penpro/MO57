#include "MOGameSettings.h"
#include "MOFramework.h"
#include "MOAudioSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

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

	// (H9) Push the saved per-category volumes to the audio subsystem so the
	// runtime mix matches the user's saved settings. Previously this was left as
	// commented-out example code, so saved volumes never applied until the
	// Options panel was opened (which does the same push) — every session ran on
	// the audio subsystem's developer defaults.
	//
	// UMOAudioSubsystem is GameInstance-scoped, but UMOGameSettings is a
	// UGameUserSettings with no world of its own. Resolve via any live Game/PIE
	// world's GameInstance. If none exists yet (very early boot, before a world
	// loads), this no-ops cleanly — UMOAudioSubsystem::Initialize seeds itself
	// from these same saved values, so boot is still covered.
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType != EWorldType::Game && Context.WorldType != EWorldType::PIE)
			{
				continue;
			}

			if (UGameInstance* GI = Context.OwningGameInstance)
			{
				if (UMOAudioSubsystem* Audio = GI->GetSubsystem<UMOAudioSubsystem>())
				{
					Audio->SetMasterVolume(MasterVolume);
					Audio->SetMusicVolume(MusicVolume);
					Audio->SetSFXVolume(SFXVolume);
					Audio->SetAmbientVolume(AmbientVolume);
					Audio->SetWeatherVolume(WeatherVolume);
					break;
				}
			}
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameSettings] Audio applied - Master: %.2f, Music: %.2f, SFX: %.2f, Ambient: %.2f, Weather: %.2f"),
		MasterVolume, MusicVolume, SFXVolume, AmbientVolume, WeatherVolume);
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

FString UMOGameSettings::GetWorldSeedAsVoxelString()
{
	const int32 Seed = GetWorldSeed();

	// Replicate the algorithm from FVoxelExposedSeed::Randomize()
	// Generates an 8-character uppercase string (A-Z) from the seed
	const FRandomStream Stream(Seed);

	FString Result;
	Result.Reserve(8);
	for (int32 Index = 0; Index < 8; Index++)
	{
		Result += TCHAR(Stream.RandRange(TEXT('A'), TEXT('Z')));
	}

	return Result;
}
