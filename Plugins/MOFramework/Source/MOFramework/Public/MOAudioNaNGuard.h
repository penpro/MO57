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

#pragma once

#include "CoreMinimal.h"
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
	virtual void Init(const FSoundEffectSubmixInitData& InData) override {}
	virtual void OnPresetChanged() override {}
	virtual void OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData) override;
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

struct MOFRAMEWORK_API FMOAudioNaNGuard
{
	/** Audio render thread: a guard removed this many invalid samples. */
	static void NoteRemoved(int64 Samples);

	/** Install on this world's audio device now (idempotent; the 1 Hz watchdog also does it): called at map load so no stretch goes unguarded. */
	static void Install(UWorld* World);

	/** A sample beyond this (linear, +24 dBFS) is a numerical blow-up, not sound. */
	static constexpr float MaxValidMagnitude = 16.0f;

	/** Totals this session: samples removed, and how many times the procedural sounds were restarted. */
	static int64 GetRemovedTotal();
	static int32 GetRecoveries();

	/** Seconds of continuous NaN before the procedural sounds are restarted. */
	static constexpr int32 RecoverAfterSeconds = 3;
};
