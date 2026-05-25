/**
 * =============================================================================
 * MOAudioSettings.h - Audio System Developer Settings
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 *
 * PURPOSE:
 * UDeveloperSettings holding soft references to audio infrastructure assets
 * (SoundClasses, Submixes, the default audio bank). Exposed in Project
 * Settings under "MO Framework -> Audio" so designers can swap them
 * without recompiling.
 *
 * SETTINGS-DRIVEN, NOT HARDCODED:
 * The subsystem reads these paths at initialization and loads the assets.
 * If a path is empty, that category just won't have a SoundClass routing —
 * playback still works but volume bus controls won't apply to it. That's
 * by design so a fresh project can boot before all the mix infrastructure
 * is set up.
 *
 * =============================================================================
 * RELATED FILES: MOAudioSubsystem.h, MOAudioTypes.h
 * LAST UPDATED: 2026-05-24
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MOAudioSettings.generated.h"

class UDataTable;
class USoundClass;
class USoundSubmix;

/**
 * Project Settings -> MO Framework -> Audio.
 * Soft refs only — assets are loaded lazily by UMOAudioSubsystem.
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="MO Framework - Audio"))
class MOFRAMEWORK_API UMOAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// =========================================================================
	// AUDIO BANK
	// =========================================================================

	/**
	 * DataTable of FMOAudioBankRow. Maps audio IDs (row names) to soft
	 * asset refs. The subsystem uses this for all bank-driven lookups
	 * (music tracks, ambient loops, UI sounds, etc.).
	 *
	 * Multiple banks can be loaded by calling LoadBank on the subsystem
	 * directly — this one is just the default loaded at startup.
	 */
	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Bank",
		meta=(AllowedClasses="/Script/Engine.DataTable"))
	FSoftObjectPath DefaultAudioBank;

	/**
	 * DataTable of FMOAmbientLayerConfig. Row name matches an ambient state
	 * audio ID (e.g., "Ambient.Outdoor.Day") and the row defines the multi-
	 * layer config (base loops + frequent + rare events).
	 *
	 * If unset, ambient falls back to single-track mode (looks up the state's
	 * audio ID in the main bank and plays it as one looped layer — the old
	 * Phase 1 behavior).
	 */
	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Bank",
		meta=(AllowedClasses="/Script/Engine.DataTable"))
	FSoftObjectPath AmbientLayersTable;

	/**
	 * Direct override for the main-menu music track. When the player is in
	 * a main menu world (AMOMainMenuGameMode), this exact asset plays as the
	 * loop — bypasses the audio bank lookup. Set so the menu music doesn't
	 * depend on having a "Music.MainMenu" row in the bank.
	 *
	 * Default: /MOFramework/Audio/Cedar_Smoke_Psalm
	 */
	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Bank",
		meta=(AllowedClasses="/Script/Engine.SoundBase"))
	FSoftObjectPath MainMenuMusicOverride = FSoftObjectPath(
		TEXT("/MOFramework/Audio/Cedar_Smoke_Psalm.Cedar_Smoke_Psalm"));

	// =========================================================================
	// SOUND CLASSES (volume buses)
	// =========================================================================
	//
	// Master is the global bus; Music / Ambient / SFX / UI inherit from it.
	// Volume changes propagate down the hierarchy.

	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Mix",
		meta=(AllowedClasses="/Script/Engine.SoundClass"))
	FSoftObjectPath MasterSoundClass;

	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Mix",
		meta=(AllowedClasses="/Script/Engine.SoundClass"))
	FSoftObjectPath MusicSoundClass;

	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Mix",
		meta=(AllowedClasses="/Script/Engine.SoundClass"))
	FSoftObjectPath AmbientSoundClass;

	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Mix",
		meta=(AllowedClasses="/Script/Engine.SoundClass"))
	FSoftObjectPath SFXSoundClass;

	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Mix",
		meta=(AllowedClasses="/Script/Engine.SoundClass"))
	FSoftObjectPath UISoundClass;

	// =========================================================================
	// DEFAULT VOLUMES (0-1)
	// =========================================================================
	//
	// Initial volumes set on the SoundClasses at startup. Player can
	// override via settings UI later (when we build one).

	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Volumes",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float DefaultMasterVolume = 1.0f;

	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Volumes",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float DefaultMusicVolume = 0.15f;

	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Volumes",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float DefaultAmbientVolume = 0.20f;

	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Volumes",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float DefaultSFXVolume = 0.50f;

	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Volumes",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float DefaultUIVolume = 1.0f;

	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Volumes",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float DefaultWeatherVolume = 0.50f;

	// =========================================================================
	// FADE TIMING
	// =========================================================================

	/**
	 * Default fade-in time (seconds) for music transitions when the music
	 * state changes. Per-state overrides come later via a config map.
	 */
	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Fades",
		meta=(ClampMin="0.0", ClampMax="30.0"))
	float MusicFadeInSeconds = 2.0f;

	/** Default fade-out time for the previous music track on state change. */
	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Fades",
		meta=(ClampMin="0.0", ClampMax="30.0"))
	float MusicFadeOutSeconds = 2.0f;

	/** Default fade-in time for ambient loop swaps (day->night, biome change). */
	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Fades",
		meta=(ClampMin="0.0", ClampMax="30.0"))
	float AmbientFadeInSeconds = 3.0f;

	/** Default fade-out time for the previous ambient layer. */
	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Fades",
		meta=(ClampMin="0.0", ClampMax="30.0"))
	float AmbientFadeOutSeconds = 3.0f;

	// =========================================================================
	// AUTOMATIC DAY/NIGHT AMBIENT SWAP
	// =========================================================================

	/**
	 * If true, the subsystem subscribes to MOGameClockSubsystem::OnDayNightChanged
	 * and auto-swaps ambient state between Outdoor_Day / Outdoor_Night.
	 * Disable if you want manual control (e.g. for a horror sequence where
	 * ambient should stay tense regardless of time).
	 */
	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Auto")
	bool bAutoSwapDayNightAmbient = true;

	// =========================================================================
	// SPATIAL AMBIENT EVENTS
	// =========================================================================
	//
	// One-shot ambient events (bird chirps, fox calls, rustles) can be placed
	// at random world positions around the listener — left/right panning, near/
	// far falloff, optional low-pass filtering for distant sounds. Sells the
	// illusion of a populated world instead of a flat 2D mix.
	//
	// All units are UE world units (1 unit = 1 cm). 5000 = 50m.

	/**
	 * Master toggle. False = events play 2D (no spatialization). Useful for
	 * cinematic/menu contexts.
	 */
	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Spatial")
	bool bSpatializeAmbientEvents = true;

	/**
	 * Closest random distance an event can spawn from the listener. Below this
	 * and the sound is inside the player — sounds unnatural. 500 = 5m.
	 */
	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Spatial",
		meta=(ClampMin="100.0", ClampMax="5000.0"))
	float AmbientEventDistanceMin = 500.0f;

	/**
	 * Farthest random distance. Beyond this the event will be inaudible due
	 * to the built-in falloff (see attenuation). 5000 = 50m. Higher = sparser
	 * but more "wide open world" feel.
	 */
	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Spatial",
		meta=(ClampMin="1000.0", ClampMax="20000.0"))
	float AmbientEventDistanceMax = 5000.0f;

	/**
	 * Min Z offset relative to listener. Negative = below (ground rustle).
	 * -100 = 1m below.
	 */
	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Spatial",
		meta=(ClampMin="-1000.0", ClampMax="1000.0"))
	float AmbientEventHeightMin = -100.0f;

	/**
	 * Max Z offset relative to listener. Positive = above (bird in tree).
	 * 400 = 4m above (canopy height). For a believable forest the upper bound
	 * matters — keeps birds in the trees instead of dive-bombing the player.
	 */
	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Spatial",
		meta=(ClampMin="0.0", ClampMax="2000.0"))
	float AmbientEventHeightMax = 400.0f;

	/**
	 * Optional USoundAttenuation override. If null, the subsystem builds a
	 * sensible default at startup (linear-falloff sphere, low-pass-filtered
	 * with distance). Set this if you want designer-tuned attenuation.
	 */
	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Spatial",
		meta=(AllowedClasses="/Script/Engine.SoundAttenuation"))
	FSoftObjectPath AmbientEventAttenuation;

	/**
	 * Falloff radius for the built-in default attenuation. Events at this
	 * distance from the listener fade to silence. Roughly matches Max distance
	 * by default. Ignored if AmbientEventAttenuation is set.
	 */
	UPROPERTY(Config, EditAnywhere, Category="MO|Audio|Spatial",
		meta=(ClampMin="500.0", ClampMax="20000.0"))
	float DefaultAttenuationFalloffDistance = 6000.0f;

	// =========================================================================
	// UDeveloperSettings overrides
	// =========================================================================

	virtual FName GetCategoryName() const override { return FName("MO Framework"); }
};
