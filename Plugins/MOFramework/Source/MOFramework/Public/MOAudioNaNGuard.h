// MOAudioNaNGuard.h -- no NaN reaches the master reverb or the audio device; a sound that keeps making NaN is restarted.
//
// Oct 16 2026 (the joiner's "buzz starting randomly ... had to close the game", Wes's "no audio until I click into the window"): the Ultra Dynamic
// Weather MetaSound UDS_Directional_WeatherSounds started a session emitting ~29 non-finite samples in every 2,048-sample buffer (its filter state
// poisoned once; restarting the sound cleared it, starting it again did not bring it back). Two things followed, measured on the main submix:
//   - the engine's master reverb (MasterReverbSubmixDefault, a feedback network) took the NaN in and never let go: from then on EVERY sample of the
//     mix was NaN -- silence until the game was closed (au.BypassAllSubmixEffects 1 / clearing that reverb: 100% -> 1.4% NaN);
//   - the rest, ~29 NaN per buffer at ~47 buffers a second, went to the device: a periodic burst a driver turns into a buzz or a click train.
// So:
//   1. UMOSubmixNaNGuardPreset (FMOSubmixNaNGuard): copies its input, invalid samples -> 0, and counts them. Invalid = non-finite OR beyond
//      MaxValidMagnitude: the same poisoned sound also made FINITE samples of 1e8 .. 1e24 (measured) that a finite check lets through and a
//      driver clamps to full scale -- the buzz. Installed per audio device FIRST in
//      the master reverb submix's chain (the reverb never sees a NaN) and on the master submix (the device never does).
//   2. A 1 Hz watchdog: when the guard keeps removing NaN for RecoverAfterSeconds, the procedural sounds playing (MetaSounds: USoundWaveProcedural)
//      are stopped and started again -- what cleared the poisoned weather sound by hand.
// Both are reported in the bug report (Audio.NaNGuard). Runs in every build (Shipping included).
// Oct 22 (the buzz again, Wes in PIE): two leaks. (a) The master submix's DRY path carried its input around the guard (the default -96 dB dry level
// is 1.6e-5 linear, live): the guard sets it to exactly 0. (b) Zeroing only the invalid samples left the garbage around them in the block: with the
// dry path closed the output still peaked at 13.6-15 (rms 0.5-0.7, a full-scale buzz). A block with ANY invalid sample is corrupt and goes out
// silent whole (SanitizeBlock), and the threshold is +12 dBFS: a master mix sample beyond that is never sound in this game. On the master that
// silenced the whole mix for ~16 s after each menu load, so the same rule also runs on every procedural sound (FMOSourceNaNGuard, attached by the
// watchdog): the poisoned weather sound goes silent alone, and the master guard stays the backstop.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundEffectSource.h"
#include "Sound/SoundEffectSubmix.h"
#include "MOAudioNaNGuard.generated.h"

class UWorld;

/** The guard has no settings; the struct exists because a submix effect preset needs one. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOSubmixNaNGuardSettings
{
	GENERATED_BODY()
};

class MOFRAMEWORK_API FMOSubmixNaNGuard : public FSoundEffectSubmix
{
public:
	virtual void Init(const FSoundEffectSubmixInitData& InData) override;
	virtual void OnPresetChanged() override {}
	virtual void OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData) override;

private:
	bool bOnMaster = false;   // the master's guard (its own preset object), else the one before the reverb: counted apart
};

UCLASS()
class MOFRAMEWORK_API UMOSubmixNaNGuardPreset : public USoundEffectSubmixPreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(MOSubmixNaNGuard)

	UPROPERTY(EditAnywhere, Category = SubmixEffectPreset, Meta = (ShowOnlyInnerProperties))
	FMOSubmixNaNGuardSettings Settings;
};

/** The same block rule on ONE source (a MetaSound): a poisoned weather sound goes silent alone, and the rest of the mix plays on. On the master
 *  the corrupt blocks are the whole mix (Oct 22: the menu was silent ~16 s after every load while UDW's sounds were poisoned). */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOSourceNaNGuardSettings
{
	GENERATED_BODY()
};

class MOFRAMEWORK_API FMOSourceNaNGuard : public FSoundEffectSource
{
public:
	virtual void Init(const FSoundEffectSourceInitData& InInitData) override {}
	virtual void OnPresetChanged() override {}
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, float* OutAudioBufferData) override;
};

UCLASS()
class MOFRAMEWORK_API UMOSourceNaNGuardPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(MOSourceNaNGuard)

	/** Any channel count (the guard walks every sample): a chain whose presets support fewer channels than the source is NOT instantiated, and
	 *  the default is 2 -- UDW's multichannel weather sounds ran unguarded (0 source blocks while the master saw every block poisoned, Oct 22). */
	virtual int32 GetMaxSupportedChannels() const override { return 8; }   // AUDIO_MIXER_MAX_OUTPUT_CHANNELS

	UPROPERTY(EditAnywhere, Category = SourceEffectPreset, Meta = (ShowOnlyInnerProperties))
	FMOSourceNaNGuardSettings Settings;
};

struct MOFRAMEWORK_API FMOAudioNaNGuard
{
	/** Audio render thread: a guard removed this many invalid samples. */
	static void NoteRemoved(int64 Samples);

	/** Install on this world's audio device now (idempotent; the 1 Hz watchdog also does it): called at map load so no stretch goes unguarded. */
	static void Install(UWorld* World);

	/** A sample beyond this (linear, +12 dBFS) is a numerical blow-up, not sound (was 16: UDW's garbage of 13-15 passed). */
	static constexpr float MaxValidMagnitude = 4.0f;

	/** Out = In, unless the block holds any invalid sample (non-finite or beyond MaxValidMagnitude): then all of Out is 0 -- the samples
	 *  around a blow-up are garbage too. Returns the invalid samples found. Pure (unit-tested: MOFramework.Audio.NaNGuard). */
	static int64 SanitizeBlock(const float* In, float* Out, int32 Num);

	/** Totals this session: samples removed, and how many times the procedural sounds were restarted. */
	static int64 GetRemovedTotal();
	static int32 GetRecoveries();

	/** Seconds of continuous NaN before the procedural sounds are restarted. */
	static constexpr int32 RecoverAfterSeconds = 3;
};
