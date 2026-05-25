/**
 * =============================================================================
 * MOAudioTypes.h - Audio System Data Types
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Shared types for the audio subsystem. State enums, DataTable row schema
 * for the audio bank, and category routing enum.
 *
 * DESIGN:
 * - State enums (Music / Ambient) drive long-running looped audio managed
 *   by the subsystem (one AudioComponent per channel).
 * - SFX / UI play as one-shots via UGameplayStatics, no state tracking.
 * - Bank DataTable maps FName audio IDs to soft asset refs so the system
 *   can stream assets on demand without locking everything in RAM at
 *   startup. (Music tracks are MB-sized; ambient loops are similar; the
 *   bank pattern means we only resident-load what's playing.)
 *
 * AUDIO ID NAMING CONVENTION:
 *   Dotted hierarchy: <Category>.<Context>.<Variant>
 *   Examples:
 *     Music.Exploration.Day
 *     Music.Combat.Heavy
 *     Music.Menu
 *     Ambient.Outdoor.Day
 *     Ambient.Outdoor.Night
 *     Ambient.Cave.Drips
 *     SFX.UI.ButtonClick
 *     SFX.Environment.OwlHoot
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-05] STREAMING LOADING BEHAVIOR: Music tracks MUST be marked
 *   "Streaming" loading behavior on the SoundWave asset, not "Force Inline".
 *   A 5-minute music track at Force Inline = ~50MB resident. Bulk-edit
 *   imported music via Property Matrix after import.
 *
 * [2026-05] SOFT REFS ONLY: All asset references in the bank are soft
 *   (TSoftObjectPtr). The subsystem resolves them async on demand. Hard
 *   refs would defeat the streaming purpose by eagerly loading.
 *
 * =============================================================================
 * RELATED FILES: MOAudioSubsystem.h, MOAudioSettings.h
 * LAST UPDATED: 2026-05-24
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Sound/SoundBase.h"
#include "MOAudioTypes.generated.h"

/**
 * High-level music context. Drives which music track loops.
 *
 * Adding a new state? Three steps:
 *   1. Add enum entry here
 *   2. Add a row in the audio bank DataTable: "Music.<NewState>"
 *   3. (optional) Wire gameplay code to call SetMusicState(<NewState>)
 */
UENUM(BlueprintType)
enum class EMOMusicState : uint8
{
	/** Silence — no track playing. */
	None UMETA(DisplayName="None / Silence"),

	/** Main menu / title screen. */
	MainMenu UMETA(DisplayName="Main Menu"),

	/** Default exploration during daytime. */
	Exploration_Day UMETA(DisplayName="Exploration (Day)"),

	/** Exploration during nighttime — slightly more tense / sparse. */
	Exploration_Night UMETA(DisplayName="Exploration (Night)"),

	/** Combat / hostile engagement. */
	Combat UMETA(DisplayName="Combat"),

	/** Stealth — sneaking past hostile creature. */
	Stealth UMETA(DisplayName="Stealth"),

	/** Tension cue — danger sensed but not yet engaged (e.g. predator nearby). */
	DangerNear UMETA(DisplayName="Danger Near"),

	/** Death / game over sting. */
	Death UMETA(DisplayName="Death"),

	/** Discovery sting — found new POI / biome / item type. */
	Discovery UMETA(DisplayName="Discovery"),
};

/**
 * Environmental ambient context. Drives the always-on ambient loop layer
 * (insects, birds, wind, water).
 *
 * Distinct from music because they serve different purposes — ambient is
 * place-based ("where am I?"), music is mood-based ("what's happening?").
 *
 * UDW handles its own weather audio (rain/thunder/wind whoosh). This enum
 * is for everything else: biome bird/insect choruses, cave drips, water
 * proximity, indoor muffling.
 */
UENUM(BlueprintType)
enum class EMOAmbientState : uint8
{
	/** Silence — no ambient layer. Used at startup before clock initializes. */
	None UMETA(DisplayName="None / Silence"),

	/** Outdoor wilderness during daylight — birds, light breeze through foliage. */
	Outdoor_Day UMETA(DisplayName="Outdoor (Day)"),

	/** Pre-dawn / post-sunset transition — chorus shift, fewer insects, less wind. */
	Outdoor_Dusk UMETA(DisplayName="Outdoor (Dusk)"),

	/** Outdoor wilderness at night — crickets, distant owl, occasional howl. */
	Outdoor_Night UMETA(DisplayName="Outdoor (Night)"),

	/** Cave / underground — water drips, low rumble, echo. */
	Cave UMETA(DisplayName="Cave / Underground"),

	/** Indoor — muffled, quiet, occasional creak. */
	Indoor UMETA(DisplayName="Indoor"),

	/** Near water — river/ocean/stream loop. Can overlap with outdoor layer. */
	Water UMETA(DisplayName="Near Water"),
};

/**
 * Routing categories for SoundClass / Submix assignment.
 *
 * Each category maps to a SoundClass (e.g. SC_Music) which routes through
 * the matching Submix (SM_Music) to the Master submix. Volume controls
 * apply per-category via the Sound Class.
 *
 * (The actual SoundClass assets are referenced in MOAudioSettings.)
 */
UENUM(BlueprintType)
enum class EMOAudioCategory : uint8
{
	/** Stem music tracks — long, streaming, looping. */
	Music,

	/** Environmental loops (biome, cave, water). */
	Ambient,

	/** General sound effects — one-shots, world events. */
	SFX,

	/** UI sounds — button clicks, notifications, modals. */
	UI,
};

/**
 * DataTable row defining one audio asset in the bank.
 *
 * Use a DataTable (not hardcoded assets in the subsystem) so:
 *   - Modders can override audio by replacing the bank
 *   - Designers can tune without recompiling
 *   - Soft refs let us only RAM-resident what's actually playing
 *
 * Row name = the AudioId (FName, e.g. "Music.Combat.Heavy"). Dot-separated
 * convention but enforced by convention only — any FName works.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOAudioBankRow : public FTableRowBase
{
	GENERATED_BODY()

	/**
	 * The actual sound asset. Soft ref so the bank doesn't eagerly load
	 * every track in RAM. Resolved on demand by MOAudioSubsystem when
	 * playback is requested.
	 *
	 * Type is USoundBase so this accepts USoundCue, USoundWave, or
	 * UMetaSoundSource — anything playable.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Audio")
	TSoftObjectPtr<USoundBase> Sound;

	/**
	 * Routing category. Determines which SoundClass / volume bus this
	 * audio plays through.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Audio")
	EMOAudioCategory Category = EMOAudioCategory::SFX;

	/**
	 * Optional volume multiplier applied at playback. 1.0 = full volume.
	 * Use to balance per-track loudness without resaving the source asset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Audio",
		meta=(ClampMin="0.0", ClampMax="2.0"))
	float VolumeMultiplier = 1.0f;

	/**
	 * Optional pitch multiplier. 1.0 = source pitch.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Audio",
		meta=(ClampMin="0.5", ClampMax="2.0"))
	float PitchMultiplier = 1.0f;

	/**
	 * Human-readable description — for the asset editor, debug logs, and
	 * future audio test UI. Doesn't affect playback.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Audio")
	FText Description;
};

/**
 * One base layer slot in an ambient state.
 *
 * Two playback modes:
 *   - Continuous loop (DelayBetweenLoopsMax = 0): the underlying SoundWave's
 *     bLooping flag plays it back-to-back forever. Use for true "bed" sounds
 *     (wind, general forest noise) where you want no gaps.
 *   - Delayed loop (DelayBetweenLoopsMax > 0): plays once, waits a random
 *     interval in [Min, Max], plays again. Use for short loops where a
 *     continuous repeat would be too obvious (rustles, breeze swells).
 *
 * The subsystem checks DelayBetweenLoopsMax > 0 to switch modes.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOAmbientBaseLayer
{
	GENERATED_BODY()

	/** Audio ID (row name in DT_MOSound) for this layer's source asset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Audio|Ambient")
	FName AudioId;

	/**
	 * Per-layer volume multiplier. Multiplies into the final mix as:
	 *   Final = FMOAmbientLayerConfig::BaseLayerVolume
	 *         * Volume                                (this field)
	 *         * FMOAudioBankRow::VolumeMultiplier     (asset-level)
	 *
	 * Range 0.0 - 3.0 so individual quiet assets can be boosted into the mix.
	 * Default 1.0 = no change.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Audio|Ambient",
		meta=(ClampMin="0.0", ClampMax="3.0"))
	float Volume = 1.0f;

	/**
	 * Min seconds of silence between loop iterations. 0 = play continuously
	 * (use SoundWave's native looping).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Audio|Ambient",
		meta=(ClampMin="0.0", ClampMax="300.0"))
	float DelayBetweenLoopsMin = 0.0f;

	/**
	 * Max seconds of silence between loop iterations. 0 = continuous.
	 * If > 0, layer plays one full cycle, waits FRandRange(Min, Max), repeats.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Audio|Ambient",
		meta=(ClampMin="0.0", ClampMax="300.0"))
	float DelayBetweenLoopsMax = 0.0f;
};

/**
 * One group of semantically-related ambient event sounds (e.g., all bird
 * calls, all wildlife rustles, all wind gusts). Used by FMOAmbientLayerConfig
 * to organize event pools so the subsystem can:
 *   - Cool down the ENTIRE group after firing one of its members (prevents
 *     "the same bird 10x in a row" — playing BirdsChirping1 cools down
 *     BirdsChirping2 too, since they're the same semantic event)
 *   - Bias selection toward currently-dominant groups (e.g., birds heavy
 *     for 90s, then insects heavy for 90s)
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOAmbientEventGroup
{
	GENERATED_BODY()

	/** Display name — used for logging + dominance picking. E.g. "Birds", "Insects". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Audio|Ambient")
	FName GroupName;

	/** Audio IDs in this group — all are variants of the same semantic sound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Audio|Ambient")
	TArray<FName> AudioIds;

	/**
	 * Seconds after the LAST event from this group fires before any member
	 * can fire again. Prevents same group repeating in close succession.
	 * Different groups have no cooldown effect on each other.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Audio|Ambient",
		meta=(ClampMin="0.0", ClampMax="600.0"))
	float CooldownSeconds = 25.0f;

	/**
	 * Weight when this group is the currently-dominant group in the
	 * dominance rotation. Higher = more likely to be picked.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Audio|Ambient",
		meta=(ClampMin="0.0", ClampMax="100.0"))
	float DominantWeight = 5.0f;

	/**
	 * Weight when this group is NOT the dominant one. Still possible to
	 * fire, just rarer. Should be 1-2 for a clear contrast with dominant.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Audio|Ambient",
		meta=(ClampMin="0.0", ClampMax="100.0"))
	float BackgroundWeight = 1.0f;
};

/**
 * Multi-layer ambient configuration for a single EMOAmbientState.
 *
 * Real-world ambient soundscapes are LAYERED, not single-track:
 *   - Base layers (continuous, simultaneous loops): general forest sound,
 *     constant insect drone, light breeze. Plays for the full duration of
 *     the state.
 *   - Frequent events (random pick from pool, fires every 8-25s): bird
 *     chirps, leaf rustles, atmospheric details. Adds dynamic life on top
 *     of the base.
 *   - Rare events (random pick from pool, fires every 45-180s): distant
 *     wildlife, occasional creature movement. Gives the world depth and
 *     suggests presence of unseen creatures.
 *
 * Stored in DT_AmbientLayers (FMOAmbientLayerConfig DataTable). Row name
 * matches the ambient state ID (e.g., "Ambient.Outdoor.Day").
 *
 * AudioIds in the arrays reference rows in the main audio bank (DT_MOSound).
 * The subsystem resolves each on demand — so a layer config can mention
 * IDs that don't exist yet without breaking (logged warning).
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOAmbientLayerConfig : public FTableRowBase
{
	GENERATED_BODY()

	// =========================================================================
	// BASE LAYERS (continuous loops)
	// =========================================================================

	/**
	 * Base layers that play simultaneously as the constant background.
	 * Typically 2-4 entries — wind / general / insect drone / atmospheric.
	 * Each entry can be continuous (DelayBetweenLoopsMax = 0) or play with
	 * random gaps between iterations.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Audio|Ambient")
	TArray<FMOAmbientBaseLayer> BaseLayers;

	/** Volume multiplier applied to all base layers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Audio|Ambient",
		meta=(ClampMin="0.0", ClampMax="2.0"))
	float BaseLayerVolume = 1.0f;

	// =========================================================================
	// EVENT GROUPS (semantically-grouped one-shots with cooldowns)
	// =========================================================================
	//
	// Replaces flat FrequentEvents/RareEvents pools. Each group represents a
	// semantic category (Birds, Insects, Wildlife, etc.) and has its own
	// cooldown. After playing any member of a group, the entire group is
	// silent for CooldownSeconds — so picking BirdsChirping1 cools down
	// BirdsChirping2 too. Forces variety.

	/** All event groups for this ambient state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Audio|Ambient")
	TArray<FMOAmbientEventGroup> EventGroups;

	/** Min/max seconds between any event fire (across all groups). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Audio|Ambient",
		meta=(ClampMin="1.0", ClampMax="300.0"))
	float EventIntervalMin = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Audio|Ambient",
		meta=(ClampMin="1.0", ClampMax="300.0"))
	float EventIntervalMax = 18.0f;

	/** Base volume multiplier for all events (before per-fire jitter). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Audio|Ambient",
		meta=(ClampMin="0.0", ClampMax="2.0"))
	float EventVolume = 0.85f;

	// =========================================================================
	// JITTER RANGES (per-fire random variation — keeps it from feeling like a tape loop)
	// =========================================================================

	/**
	 * Volume jitter range per event fire. Final volume = EventVolume *
	 * RandRange(VolumeJitterMin, VolumeJitterMax). Wide ranges (0.25-1.0)
	 * make repeated identical assets sound like nearby vs distant calls.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Audio|Ambient",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float VolumeJitterMin = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Audio|Ambient",
		meta=(ClampMin="0.0", ClampMax="2.0"))
	float VolumeJitterMax = 1.0f;

	/**
	 * Pitch jitter per event. ±15% (0.85-1.15) is the typical max before it
	 * sounds artificial. Keeps same asset from sounding identical on replay.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Audio|Ambient",
		meta=(ClampMin="0.1", ClampMax="2.0"))
	float PitchJitterMin = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Audio|Ambient",
		meta=(ClampMin="0.1", ClampMax="2.0"))
	float PitchJitterMax = 1.15f;

	// =========================================================================
	// DOMINANCE SHIFTS
	// =========================================================================
	//
	// One group is "dominant" at a time — its weight bias makes it likely to
	// be picked. Every DominanceShiftIntervalMin..Max seconds, a different
	// group becomes dominant. Creates evolving phases (birds for 90s, then
	// insects for 90s, then wind, etc.).

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Audio|Ambient",
		meta=(ClampMin="10.0", ClampMax="600.0"))
	float DominanceShiftIntervalMin = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Audio|Ambient",
		meta=(ClampMin="10.0", ClampMax="600.0"))
	float DominanceShiftIntervalMax = 120.0f;
};
