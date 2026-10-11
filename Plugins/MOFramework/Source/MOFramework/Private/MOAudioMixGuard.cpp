// A sound mix that references a sound class the audio device never registered makes the engine log "RecursiveApplyAdjuster failed, likely because we are clearing the level." on EVERY audio update
// (about 100 lines a second, forever: 31,000 lines in six minutes of a packaged run) and does the failed lookup each time. The message is misleading: nothing is clearing the level, a mix is active and
// one of its adjusters points at a class that is missing from FAudioDevice::SoundClasses. Registering the class (what the engine does for every class it sees at device start) makes the lookup succeed.
//
//   MO.Audio.Mixes      dump every active sound mix, its adjusters and whether the device knows each adjuster's class
//   (automatic)         UMOAudioSubsystem calls MOAudioMix::Inspect(true, false) once a second: orphan classes are registered and reported once.

#include "MOAudioMixGuard.h"
#include "MOFramework.h"
#include "AudioDevice.h"
#include "AudioThread.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace
{
	TSet<TWeakObjectPtr<USoundClass>> GReportedOrphans;
}

void MOAudioMix::Inspect(bool bRegisterOrphans, bool bLogAll)
{
	if (!GEngine)
	{
		return;
	}
	FAudioDeviceHandle Handle = GEngine->GetMainAudioDevice();
	if (!Handle.IsValid())
	{
		return;
	}
	FAudioThread::RunCommandOnAudioThread([Handle, bRegisterOrphans, bLogAll]()
	{
		FAudioDevice* Device = Handle.GetAudioDevice();
		if (!Device)
		{
			return;
		}
		int32 Mixes = 0, Orphans = 0;
		for (const TPair<USoundMix*, FSoundMixState>& Pair : Device->GetSoundMixModifiers())
		{
			USoundMix* Mix = Pair.Key;
			if (!Mix)
			{
				continue;
			}
			++Mixes;
			if (bLogAll)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] mix %s: %d class adjuster(s), interp=%.2f"), *Mix->GetName(), Mix->SoundClassEffects.Num(), Pair.Value.InterpValue);
			}
			for (const FSoundClassAdjuster& Adj : Mix->SoundClassEffects)
			{
				USoundClass* Class = Adj.SoundClassObject;
				if (!Class)
				{
					continue;
				}
				const bool bKnown = Device->GetSoundClassCurrentProperties(Class) != nullptr;
				if (bLogAll)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio]    adjuster -> class %s (%s) vol=%.2f pitch=%.2f children=%d"), *Class->GetName(), bKnown ? TEXT("registered") : TEXT("NOT REGISTERED"), Adj.VolumeAdjuster, Adj.PitchAdjuster, Adj.bApplyToChildren ? 1 : 0);
				}
				if (!bKnown)
				{
					++Orphans;
					if (bRegisterOrphans)
					{
						Device->RegisterSoundClass(Class);
						if (!GReportedOrphans.Contains(Class))
						{
							GReportedOrphans.Add(Class);
							UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] sound mix '%s' references sound class '%s', which the audio device did not know: registered it (without this the engine logs 'RecursiveApplyAdjuster failed' on every audio update)"), *Mix->GetName(), *Class->GetName());
						}
					}
				}
			}
		}
		if (bLogAll)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] %d active mix(es), %d adjuster class(es) unknown to the device"), Mixes, Orphans);
		}
	}, TStatId());
}

namespace
{
	FAutoConsoleCommand GAudioMixesCmd(
		TEXT("MO.Audio.Mixes"),
		TEXT("Dev: list the active sound mixes, their adjusters and whether the audio device knows each adjuster's sound class. Usage: MO.Audio.Mixes [register]"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			MOAudioMix::Inspect(Args.Num() > 0 && Args[0].Equals(TEXT("register"), ESearchCase::IgnoreCase), true);
		}));
}
