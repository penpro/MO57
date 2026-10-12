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
#include "AudioDeviceManager.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace
{
	TSet<TWeakObjectPtr<USoundClass>> GReportedOrphans;
}

namespace
{
	/** The inspection of one audio device (audio thread). */
	void InspectDevice(FAudioDevice* Device, Audio::FDeviceId DeviceId, bool bRegisterOrphans, bool bLogAll)
	{
		int32 Mixes = 0, Orphans = 0;

		// The mixes that trigger the log are often created at run time by blueprints with per-class OVERRIDES (SetSoundMixClassOverride), which are not listed in the mix asset's own adjusters
		// and cannot be read from the device. Whatever class they name is a loaded USoundClass, so make the device know every loaded class: that is what InitSoundClasses does at device start for
		// the classes that exist then; classes loaded later (plugin content, streamed assets) only register through their own PostLoad, which does not cover every path.
		for (TObjectIterator<USoundClass> It; It; ++It)
		{
			USoundClass* Class = *It;
			if (bLogAll && Class)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] loaded sound class %s | %s | known=%d children=%d parent=%s"), *Class->GetPathName(), *GetNameSafe(Class->GetClass()), Device->GetSoundClassCurrentProperties(Class) != nullptr ? 1 : 0, Class->ChildClasses.Num(), *GetNameSafe(Class->ParentClass));
			}
			if (Class && !Class->HasAnyFlags(RF_ClassDefaultObject) && Device->GetSoundClassCurrentProperties(Class) == nullptr)
			{
				++Orphans;
				if (bRegisterOrphans)
				{
					Device->RegisterSoundClass(Class);
					if (!GReportedOrphans.Contains(Class))
					{
						GReportedOrphans.Add(Class);
						UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] sound class '%s' was loaded but unknown to the audio device: registered it (a sound mix override naming it makes the engine log 'RecursiveApplyAdjuster failed' on every audio update)"), *Class->GetPathName());
					}
				}
			}
		}
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
			// The engine applies an adjuster to its class AND (bApplyToChildren) to every child class, and logs 'RecursiveApplyAdjuster failed' for any class in that tree the device does not know.
			// So walk the whole subtree, not just the adjuster's own class.
			TFunction<void(USoundClass*, int32)> Visit = [&](USoundClass* Class, int32 Depth)
			{
				if (!Class || Depth > 16)
				{
					return;
				}
				const bool bKnown = Device->GetSoundClassCurrentProperties(Class) != nullptr;
				if (bLogAll)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio]    %sclass %s (%s) children=%d"), *FString::ChrN(Depth * 2, TEXT(' ')), *Class->GetName(), bKnown ? TEXT("registered") : TEXT("NOT REGISTERED"), Class->ChildClasses.Num());
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
							UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] sound mix '%s' reaches sound class '%s', which the audio device did not know: registered it (without this the engine logs 'RecursiveApplyAdjuster failed' on every audio update)"), *Mix->GetName(), *Class->GetName());
						}
					}
				}
				for (USoundClass* Child : Class->ChildClasses)
				{
					Visit(Child, Depth + 1);
				}
			};
			for (const FSoundClassAdjuster& Adj : Mix->SoundClassEffects)
			{
				if (bLogAll)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio]    adjuster vol=%.2f pitch=%.2f applyToChildren=%d"), Adj.VolumeAdjuster, Adj.PitchAdjuster, Adj.bApplyToChildren ? 1 : 0);
				}
				Visit(Adj.SoundClassObject, 0);
			}
		}
		if (bLogAll)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] device %u: %d active mix(es), %d sound class(es) unknown to it"), (uint32)DeviceId, Mixes, Orphans);
		}
	}
}

void MOAudioMix::Inspect(bool bRegisterOrphans, bool bLogAll)
{
	if (!GEngine || !GEngine->GetAudioDeviceManager())
	{
		return;
	}
	FAudioThread::RunCommandOnAudioThread([bRegisterOrphans, bLogAll]()
	{
		if (FAudioDeviceManager* Manager = FAudioDeviceManager::Get())
		{
			// EVERY audio device: a class registers with the devices that exist when it loads, so a device created later can lack classes the first one has.
			Manager->IterateOverAllDevices([bRegisterOrphans, bLogAll](Audio::FDeviceId Id, FAudioDevice* Device)
			{
				if (Device)
				{
					InspectDevice(Device, Id, bRegisterOrphans, bLogAll);
				}
			});
		}
	}, TStatId());
}

namespace
{
	FAutoConsoleCommand GAudioClearMixesCmd(
		TEXT("MO.Audio.ClearMixes"),
		TEXT("Dev: clear every active sound mix modifier (diagnosis: does the log spam stop, and does the owner of the mix push it again?)."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (!GEngine)
			{
				return;
			}
			FAudioDeviceHandle Handle = GEngine->GetMainAudioDevice();
			if (Handle.IsValid())
			{
				FAudioThread::RunCommandOnAudioThread([Handle]()
				{
					if (FAudioDevice* Device = Handle.GetAudioDevice())
					{
						Device->ClearSoundMixModifiers();
						UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] cleared every sound mix modifier"));
					}
				}, TStatId());
			}
		}));

	FAutoConsoleCommand GAudioMixesCmd(
		TEXT("MO.Audio.Mixes"),
		TEXT("Dev: list the active sound mixes, their adjusters and whether the audio device knows each adjuster's sound class. Usage: MO.Audio.Mixes [register]"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			MOAudioMix::Inspect(Args.Num() > 0 && Args[0].Equals(TEXT("register"), ESearchCase::IgnoreCase), true);
		}));
}
