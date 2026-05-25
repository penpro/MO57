/**
 * =============================================================================
 * MOAudioSubsystem.h - Audio System Coordinator
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Central coordinator for MO audio. GameInstance-scoped (survives map
 * transitions) so music can flow from main menu through level loads.
 *
 * RESPONSIBILITIES:
 *   - State machines: EMOMusicState + EMOAmbientState. State changes
 *     trigger crossfaded loop transitions on dedicated AudioComponents.
 *   - DataTable bank lookups: AudioId (FName) -> USoundBase via soft-ref
 *     resolution. Async load on demand.
 *   - One-shot SFX/UI playback via UGameplayStatics passthrough (no state).
 *   - Volume controls per category — applied to SoundClass volume buses.
 *   - Day/night auto-ambient: subscribes to MOGameClockSubsystem and
 *     swaps Outdoor_Day <-> Outdoor_Night on the clock's daytime flip.
 *
 * WHAT THIS DOES NOT DO:
 *   - Weather audio (UDW owns it — rain, thunder, wind whoosh).
 *   - Per-actor diegetic sounds (footsteps, weapon swings — those belong
 *     on the actor / animation notify).
 *   - Biome zone transitions (Phase 3 — AMOAudioZoneVolume actor).
 *   - Dynamic music layering / Quartz sync (Phase 5 polish).
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-05] WORLD CHANGES: This is a GameInstanceSubsystem, so the
 *   AudioComponents persist across world changes. On PIE end / level
 *   transition, the old world's AudioComponents become orphans. We rebuild
 *   them on world ready via OnWorldBeginPlay. If music skips during PIE
 *   stop, that's why.
 *
 * [2026-05] BANK LOOKUP MISSES: If an audio ID isn't in the loaded bank,
 *   playback is a no-op (logged Warning). Don't assume Play succeeds.
 *   Test with MO.Audio.Play <Id> to confirm a row exists.
 *
 * =============================================================================
 * RELATED FILES: MOAudioTypes.h, MOAudioSettings.h
 * LAST UPDATED: 2026-05-24
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MOAudioTypes.h"
#include "MOAudioSubsystem.generated.h"

class UAudioComponent;
class USoundAttenuation;
class USoundBase;
class USoundClass;
class UDataTable;

// =============================================================================
// DELEGATES
// =============================================================================

/** Broadcast when the active music state changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOAudioMusicStateChanged,
	EMOMusicState, OldState, EMOMusicState, NewState);

/** Broadcast when the active ambient state changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOAudioAmbientStateChanged,
	EMOAmbientState, OldState, EMOAmbientState, NewState);

/**
 * Broadcast when weather volume changes. UDW lives outside our SoundClass
 * hierarchy so BP_WeatherBridge listens to this and applies the new value
 * via UDW's native volume property.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOAudioWeatherVolumeChanged,
	float, NewVolume);

// =============================================================================
// SUBSYSTEM
// =============================================================================

UCLASS()
class MOFRAMEWORK_API UMOAudioSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// =========================================================================
	// LIFECYCLE
	// =========================================================================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Per-world audio context setup: picks main-menu music for menu worlds
	 * (AMOMainMenuGameMode) or ambient + exploration music for gameplay.
	 * Called from PostLoadMapWithWorld AND from AMOGameMode::BeginPlay
	 * (the BeginPlay call is the reliable signal for first-launch since
	 * PostLoadMap fires before GameMode is spawned).
	 *
	 * Idempotent: SetMusicState early-returns when state matches.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Audio")
	void HandleWorldAudioContext(UWorld* World);

	/**
	 * Static accessor — same pattern as MOGameClockSubsystem::Get.
	 * Resolves via WorldContext -> GameInstance -> Subsystem.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Audio",
		meta=(WorldContext="WorldContextObject"))
	static UMOAudioSubsystem* Get(const UObject* WorldContextObject);

	// =========================================================================
	// STATE MACHINES
	// =========================================================================

	/**
	 * Set the active music state. Triggers crossfade to the corresponding
	 * track from the audio bank (audio ID = "Music.<StateName>").
	 *
	 * If the bank doesn't have a row for the new state, the current music
	 * fades out and stays silent — no error. Allows ship-as-you-go (build
	 * the state machine first, add tracks later).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Audio|Music")
	void SetMusicState(EMOMusicState NewState);

	UFUNCTION(BlueprintPure, Category="MO|Audio|Music")
	EMOMusicState GetMusicState() const { return CurrentMusicState; }

	/**
	 * Set the active ambient state. Crossfades the looped ambient layer
	 * to "Ambient.<StateName>".
	 *
	 * Manual override blocks auto-day/night swap until you call this with
	 * a state that matches the current clock (or restart PIE).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Audio|Ambient")
	void SetAmbientState(EMOAmbientState NewState);

	UFUNCTION(BlueprintPure, Category="MO|Audio|Ambient")
	EMOAmbientState GetAmbientState() const { return CurrentAmbientState; }

	UPROPERTY(BlueprintAssignable, Category="MO|Audio|Music")
	FMOAudioMusicStateChanged OnMusicStateChanged;

	UPROPERTY(BlueprintAssignable, Category="MO|Audio|Ambient")
	FMOAudioAmbientStateChanged OnAmbientStateChanged;

	// =========================================================================
	// ONE-SHOT PLAYBACK
	// =========================================================================

	/**
	 * Play a one-shot sound by audio ID. Used for UI clicks, world SFX,
	 * stingers. Returns true if the ID resolved and playback was initiated.
	 *
	 * 2D playback (no spatialization). For positional one-shots use the
	 * AtLocation variant.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Audio|SFX")
	bool PlayOneShot2D(FName AudioId);

	/** Play a one-shot at a world location. Spatialized via the source asset's attenuation settings. */
	UFUNCTION(BlueprintCallable, Category="MO|Audio|SFX",
		meta=(WorldContext="WorldContextObject"))
	bool PlayOneShotAtLocation(const UObject* WorldContextObject, FName AudioId, FVector Location);

	// =========================================================================
	// VOLUME CONTROLS
	// =========================================================================
	//
	// Each setter clamps to [0, 1] and applies to the corresponding SoundClass
	// volume property. Master propagates to all children.

	UFUNCTION(BlueprintCallable, Category="MO|Audio|Volume")
	void SetMasterVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category="MO|Audio|Volume")
	void SetMusicVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category="MO|Audio|Volume")
	void SetAmbientVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category="MO|Audio|Volume")
	void SetSFXVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category="MO|Audio|Volume")
	void SetUIVolume(float Volume);

	/**
	 * Set weather audio volume. Doesn't route through a SoundClass (UDW
	 * lives outside our hierarchy) — broadcasts OnWeatherVolumeChanged so
	 * BP_WeatherBridge can apply via UDW's native volume.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Audio|Volume")
	void SetWeatherVolume(float Volume);

	UFUNCTION(BlueprintPure, Category="MO|Audio|Volume")
	float GetMasterVolume() const { return MasterVolume; }

	UFUNCTION(BlueprintPure, Category="MO|Audio|Volume")
	float GetMusicVolume() const { return MusicVolume; }

	UFUNCTION(BlueprintPure, Category="MO|Audio|Volume")
	float GetAmbientVolume() const { return AmbientVolume; }

	UFUNCTION(BlueprintPure, Category="MO|Audio|Volume")
	float GetSFXVolume() const { return SFXVolume; }

	UFUNCTION(BlueprintPure, Category="MO|Audio|Volume")
	float GetUIVolume() const { return UIVolume; }

	UFUNCTION(BlueprintPure, Category="MO|Audio|Volume")
	float GetWeatherVolume() const { return WeatherVolume; }

	/** Fired when SetWeatherVolume is called. BP_WeatherBridge subscribes. */
	UPROPERTY(BlueprintAssignable, Category="MO|Audio|Volume")
	FMOAudioWeatherVolumeChanged OnWeatherVolumeChanged;

	// =========================================================================
	// BANK MANAGEMENT
	// =========================================================================

	/**
	 * Load an additional bank DataTable. Rows from this bank merge into
	 * the in-memory lookup. Duplicate IDs: last-loaded wins (designers can
	 * override defaults by loading a more-specific bank after the default).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Audio|Bank")
	void LoadBank(UDataTable* Bank);

	/** Reverse of LoadBank. Rows from this bank are removed from the lookup. */
	UFUNCTION(BlueprintCallable, Category="MO|Audio|Bank")
	void UnloadBank(UDataTable* Bank);

	/**
	 * Find a bank row by ID. Returns nullptr if not found.
	 * BP-friendly accessor for tests and tools.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Audio|Bank")
	bool FindAudioBankRow(FName AudioId, FMOAudioBankRow& OutRow) const;

	/** All known audio IDs across all loaded banks. For tooling / debug list. */
	UFUNCTION(BlueprintPure, Category="MO|Audio|Bank")
	TArray<FName> GetAllAudioIds() const;

protected:
	// =========================================================================
	// INTERNAL — clock hook
	// =========================================================================

	UFUNCTION()
	void HandleClockDayNightChanged(bool bNewIsDaytime, const FDateTime& NewDateTime);

	// =========================================================================
	// INTERNAL — playback helpers
	// =========================================================================

	/**
	 * Resolve an AudioId to its bank row + async-load its asset. Calls
	 * OnLoaded once the asset is in memory.
	 */
	void ResolveAndLoadAsync(FName AudioId, TFunction<void(USoundBase* Sound, const FMOAudioBankRow& Row)> OnLoaded);

	/** Stop the music AudioComponent with fadeout, clear ref. */
	void StopMusicWithFade(float FadeOutSeconds);

	/** Stop ALL ambient layers + clear event timers. */
	void StopAmbientWithFade(float FadeOutSeconds);

	/** Start music with fade-in on the dedicated component. */
	void StartMusic(USoundBase* Sound, const FMOAudioBankRow& Row, float FadeInSeconds);

	/**
	 * Begin multi-layer ambient playback. Looks up the FMOAmbientLayerConfig
	 * for the state, spawns AudioComponents for each base layer, schedules
	 * frequent + rare event timers. Falls back to single-track playback if
	 * no layer config is registered.
	 */
	void StartAmbientLayered(EMOAmbientState NewState, float FadeInSeconds);

	/** Old single-track fallback when no FMOAmbientLayerConfig is configured. */
	void StartAmbientSingle(USoundBase* Sound, const FMOAudioBankRow& Row, float FadeInSeconds);

	/** Look up the FMOAmbientLayerConfig for a state in the configured table. Nullptr if no row. */
	const FMOAmbientLayerConfig* FindAmbientLayerConfig(EMOAmbientState State) const;

	/**
	 * Start a base layer. Routes to continuous-loop or delayed-loop mode
	 * based on whether DelayBetweenLoopsMax > 0 in the layer config.
	 */
	void StartBaseLayer(const FMOAmbientBaseLayer& Layer, int32 SlotIdx, float Volume, float FadeInSeconds);

	/** Continuous loop (uses SoundWave's bLooping). */
	void StartBaseLayerContinuous(FName AudioId, int32 SlotIdx, float Volume, float FadeInSeconds);

	/** Play-once + delay + restart cycle. Re-spawns via timer. */
	void StartBaseLayerDelayed(FName AudioId, int32 SlotIdx, float Volume, float FadeInSeconds, float DelayMin, float DelayMax);

	/**
	 * Unified event picker — fires every EventIntervalMin..Max seconds.
	 * Selects an FMOAmbientEventGroup using weighted random where the
	 * dominant group has its DominantWeight and others have BackgroundWeight.
	 * Groups still on cooldown are skipped. After picking a group, picks a
	 * random AudioId variant from it, applies wide jitter, plays one-shot,
	 * marks the group cooldown.
	 */
	UFUNCTION()
	void HandleEventTimer();

	void ScheduleNextEvent();

	/**
	 * Dominance shift: every DominanceShiftIntervalMin..Max seconds, picks a
	 * different group to be dominant. Drives the "phases" feel — birds
	 * dominate for 90s, then insects, then wildlife, etc.
	 */
	UFUNCTION()
	void HandleDominanceShiftTimer();

	void ScheduleNextDominanceShift();

	/** Pick a group weighted by dominance, filtering out cooldown'd groups. */
	int32 PickWeightedGroup() const;

	/**
	 * Base layer "breathing" — every 4-8s, pick a random currently-playing
	 * base layer and smoothly drift its volume to a new random target over
	 * 6-12s. Creates organic ebb-and-flow on top of the static base loops so
	 * the soundscape never feels like a tape loop.
	 */
	UFUNCTION()
	void HandleBaseLayerBreatheTimer();

	void ScheduleNextBaseLayerBreathe();

	/**
	 * Periodic logger (every 10s) listing all currently-playing base layers
	 * with their current volume. Helps identify which assets sound bad so
	 * they can be replaced / re-tagged.
	 */
	UFUNCTION()
	void HandleBaseLayerStatusLog();

	/** Map EMOMusicState / EMOAmbientState -> audio ID string ("Music.Combat", etc.). */
	static FName MakeMusicAudioId(EMOMusicState State);
	static FName MakeAmbientAudioId(EMOAmbientState State);

	/** Resolve the SoundClass for a category from settings. */
	USoundClass* GetSoundClassForCategory(EMOAudioCategory Category) const;

	/** Apply current volume value to a SoundClass. */
	void ApplyVolumeToSoundClass(USoundClass* SoundClass, float Volume);

	// =========================================================================
	// INTERNAL — spatialization
	// =========================================================================

	/**
	 * Current listener world position. Used as the origin for random event
	 * placement. Resolves: PlayerCameraManager -> first player pawn -> origin.
	 */
	FVector GetListenerLocation() const;

	/**
	 * Pick a random world location around the listener within the configured
	 * distance + height ranges (see MOAudioSettings). Used per event fire so
	 * each fire feels like it's coming from a different spot.
	 */
	FVector PickRandomEventLocation() const;

	/**
	 * Resolve the attenuation asset to use for ambient events. Returns the
	 * configured override if set, otherwise the cached default (built once
	 * at Initialize).
	 */
	USoundAttenuation* GetAmbientEventAttenuation() const;

private:
	// =========================================================================
	// STATE
	// =========================================================================

	UPROPERTY()
	EMOMusicState CurrentMusicState = EMOMusicState::None;

	UPROPERTY()
	EMOAmbientState CurrentAmbientState = EMOAmbientState::None;

	UPROPERTY()
	float MasterVolume = 1.0f;

	UPROPERTY()
	float MusicVolume = 0.6f;

	UPROPERTY()
	float AmbientVolume = 0.7f;

	UPROPERTY()
	float SFXVolume = 1.0f;

	UPROPERTY()
	float UIVolume = 1.0f;

	UPROPERTY()
	float WeatherVolume = 1.0f;

	/** Dedicated component for the current music loop. Recreated per session. */
	UPROPERTY()
	TObjectPtr<UAudioComponent> MusicComponent;

	/**
	 * All currently-playing base-layer AudioComponents. Multi-layer ambient
	 * spawns N of these (one per FMOAmbientLayerConfig::BaseLayers entry).
	 * Single-track fallback uses index 0 only.
	 *
	 * Cleared on ambient state change. Stopped via FadeOut + auto-destroy.
	 */
	UPROPERTY()
	TArray<TObjectPtr<UAudioComponent>> AmbientLayerComponents;

	/** Cached active layer config — used by event timers to pick random IDs. */
	FMOAmbientLayerConfig CurrentLayerConfig;

	/** Timer handles. Cleared when ambient state changes. */
	FTimerHandle EventTimerHandle;
	FTimerHandle DominanceShiftTimerHandle;
	FTimerHandle BaseLayerBreatheTimerHandle;
	FTimerHandle BaseLayerStatusLogTimerHandle;

	/** Index into CurrentLayerConfig.EventGroups for the currently-dominant group. -1 = none. */
	int32 CurrentDominantGroupIdx = -1;

	/**
	 * Per-group last-fire timestamps (world time seconds). Indexed parallel
	 * to CurrentLayerConfig.EventGroups. Group is on cooldown until
	 * (LastFireTime + CooldownSeconds < WorldTimeSeconds).
	 */
	TArray<float> GroupLastFireTimes;

	/**
	 * Sentinel for delayed base-layer restart callbacks. Cleared when ambient
	 * state changes via StopAmbientWithFade. Pending restart timers check this
	 * before respawning — prevents leaked timers from firing on the wrong state.
	 */
	bool bAmbientStreamActive = false;

	/** Loaded ambient layers DataTable (cached from settings on init). */
	UPROPERTY()
	TObjectPtr<UDataTable> CachedAmbientLayersTable;

	/**
	 * Default attenuation profile built at Initialize. Used for spatialized
	 * ambient events when MOAudioSettings::AmbientEventAttenuation is unset.
	 * Configured with natural-sound falloff + low-pass filter for distance —
	 * faraway sounds get quieter AND muffled, like real ambient cues.
	 */
	UPROPERTY()
	TObjectPtr<USoundAttenuation> DefaultEventAttenuation;

	/** Loaded banks. Order matters — later banks override earlier ones. */
	UPROPERTY()
	TArray<TObjectPtr<UDataTable>> LoadedBanks;

	/**
	 * Compiled lookup of AudioId -> row pointer + owning bank. Rebuilt
	 * whenever banks load/unload. Pointers into the bank's row map — valid
	 * as long as the bank is loaded.
	 */
	TMap<FName, const FMOAudioBankRow*> AudioIdLookup;

	/** Handle for FCoreUObjectDelegates::PostLoadMapWithWorld so we can unbind. */
	FDelegateHandle WorldBeginPlayHandle;

	/** Handle for FCoreUObjectDelegates::PreLoadMap — stop all audio on level transition. */
	FDelegateHandle PreLoadMapHandle;

	/**
	 * Immediately stop all music/ambient + cancel all event/dominance/breathe
	 * timers. No fade — used when the level is about to unload (PreLoadMap)
	 * or when the subsystem is shutting down (Deinitialize). Audio components
	 * are stopped (not just faded) so they don't survive into the next map.
	 */
	void StopAllAudio();

};
