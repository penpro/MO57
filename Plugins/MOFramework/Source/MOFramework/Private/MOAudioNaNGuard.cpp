#include "MOAudioNaNGuard.h"
#include "MOFramework.h"
#include "AudioDevice.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Components/AudioComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/ThreadSafeCounter64.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundSubmix.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MOAudioNaNGuard)

static TAutoConsoleVariable<int32> CVarAudioNaNGuard(TEXT("mo.Audio.NaNGuard"), 1,
	TEXT("1 = sanitize non-finite samples before the master reverb and on the master submix, and restart procedural sounds that keep producing them. 0 = off (diagnosis only)."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAudioNaNGuardCloseDry(TEXT("mo.Audio.NaNGuard.CloseDry"), 1,
	TEXT("1 = the guard sets the master submix's dry level to exactly 0, so nothing reaches the device around its effect chain (the default -96 dB is 1.6e-5 linear, ")
	TEXT("live). 0 = leave it (diagnosis only). Read when the guard installs on a device."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAudioNaNGuardReverbOff(TEXT("mo.Audio.NaNGuard.ReverbOff"), 0,
	TEXT("Diagnosis: 1 = the master reverb submix runs the guard ONLY (its reverb effect left out), to tell a reverb that makes NaN from a source ")
	TEXT("that does. Read when the reverb chain is applied (install and recoveries)."),
	ECVF_Default);

namespace MOAudioNaNGuardState
{
	FThreadSafeCounter64 GRemovedSinceTick;   // render thread -> game thread
	FThreadSafeCounter64 GRemovedTotal;
	FThreadSafeCounter64 GBlocksSinceTick;   // render thread -> game thread: blocks the guards saw / silenced since the last tick
	FThreadSafeCounter64 GBlankedSinceTick;
	FThreadSafeCounter64 GMasterBlankedSinceTick;   // the master guard's share of GBlankedSinceTick (the rest: the guard before the reverb)
	std::atomic<uint32> GMasterPresetId{ static_cast<uint32>(INDEX_NONE) };
	FThreadSafeCounter64 GSourceBlankedSinceTick;
	FThreadSafeCounter64 GSourceBlocksSinceTick;   // every source block the source guards processed (0 = the guards are not running)   // render thread -> game thread: source blocks (one sound's) silenced since the last tick
	TStrongObjectPtr<USoundEffectSourcePresetChain> GSourceChain;
	int32 GRecoveries = 0;
	int32 GNaNSeconds = 0;
	bool GDumpedThisStretch = false;   // DumpPlayingSounds once per poisoned stretch (until a clean second)
	uint32 GInstalledDeviceId = INDEX_NONE;
	TWeakObjectPtr<UWorld> GInstalledWorld;
	TStrongObjectPtr<UMOSubmixNaNGuardPreset> GPreset;   // the master's
	TStrongObjectPtr<UMOSubmixNaNGuardPreset> GReverbPreset;   // the reverb's: a preset object of its own, so their counts tell them apart
	void Install(UWorld* World, FAudioDevice* Device);

	/** A guarded submix's DRY path carries its input AROUND its effect chain, so around the guard: a submix's default dry level is -96 dB, which
	 *  the mixer converts to 1.6e-5 linear (not 0) and keeps live (FMixerSubmix::ProcessAudio, IsNearlyZero). UDW's 1e24 samples reached the
	 *  device at ~1e19 through the master's (the buzz again, Oct 22), and the whole mix went silent through the reverb's: UDW's bus-only weather
	 *  sounds poison their reverb send, and the reverb's dry path carried it around the reverb's guard into the master (N-189). Exactly 0 when the
	 *  dry level is inaudible anyway (<= -60 dB); an audible one is left as authored. True when closed. */
	bool CloseDryPath(UWorld* World, USoundSubmix* Submix)
	{
		if (!Submix || CVarAudioNaNGuardCloseDry.GetValueOnGameThread() == 0 || Submix->DryLevelModulation.Value > -60.0f)
		{
			return false;
		}
		Submix->SetSubmixDryLevel(World, 0.0f);
		return true;
	}

	/** The master reverb's chain again, guard first: NEW effect instances, so a reverb whose feedback already holds NaN (it latched before the guard
	 *  was there, in the first frames of a session) starts clean. Returns the reverb effects behind the guard, -1 without a reverb submix. */
	int32 ApplyReverbGuard(UWorld* World)
	{
		USoundSubmix* Reverb = Cast<USoundSubmix>(GetDefault<UAudioSettings>()->ReverbSubmix.TryLoad());
		if (!Reverb)
		{
			return -1;
		}
		if (!GReverbPreset.IsValid())
		{
			GReverbPreset.Reset(NewObject<UMOSubmixNaNGuardPreset>(GetTransientPackage(), TEXT("MOSubmixNaNGuardReverb")));
		}
		TArray<USoundEffectSubmixPreset*> Chain;
		Chain.Add(GReverbPreset.Get());
		for (USoundEffectSubmixPreset* Preset : Reverb->SubmixEffectChain)
		{
			if (Preset && CVarAudioNaNGuardReverbOff.GetValueOnGameThread() == 0)
			{
				Chain.Add(Preset);
			}
		}
		UAudioMixerBlueprintLibrary::SetSubmixEffectChainOverride(World, Reverb, Chain, 0.0f);
		CloseDryPath(World, Reverb);
		return Chain.Num() - 1;
	}
}

void FMOSubmixNaNGuard::Init(const FSoundEffectSubmixInitData& InData)
{
	bOnMaster = InData.ParentPresetUniqueId == MOAudioNaNGuardState::GMasterPresetId.load();
}

void FMOSubmixNaNGuard::OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData)
{
	const int64 Removed = FMOAudioNaNGuard::SanitizeBlock(InData.AudioBuffer->GetData(), OutData.AudioBuffer->GetData(), InData.NumFrames * InData.NumChannels);
	MOAudioNaNGuardState::GBlocksSinceTick.Increment();
	if (Removed > 0)
	{
		MOAudioNaNGuardState::GBlankedSinceTick.Increment();
		if (bOnMaster)
		{
			MOAudioNaNGuardState::GMasterBlankedSinceTick.Increment();
		}
		FMOAudioNaNGuard::NoteRemoved(Removed);
	}
}

void FMOSourceNaNGuard::ProcessAudio(const FSoundEffectSourceInputData& InData, float* OutAudioBufferData)
{
	const int64 Removed = FMOAudioNaNGuard::SanitizeBlock(InData.InputSourceEffectBufferPtr, OutAudioBufferData, InData.NumSamples);
	MOAudioNaNGuardState::GSourceBlocksSinceTick.Increment();
	if (Removed > 0)
	{
		MOAudioNaNGuardState::GSourceBlankedSinceTick.Increment();
		FMOAudioNaNGuard::NoteRemoved(Removed);   // the watchdog restarts a sound that keeps at it
	}
}

int64 FMOAudioNaNGuard::SanitizeBlock(const float* In, float* Out, int32 Num)
{
	int64 Invalid = 0;
	for (int32 I = 0; I < Num; ++I)
	{
		const float S = In[I];
		if (!FMath::IsFinite(S) || FMath::Abs(S) > MaxValidMagnitude)
		{
			++Invalid;
		}
	}
	if (Invalid > 0)
	{
		FMemory::Memzero(Out, sizeof(float) * Num);
	}
	else if (Out != In)
	{
		FMemory::Memcpy(Out, In, sizeof(float) * Num);
	}
	return Invalid;
}

void FMOAudioNaNGuard::NoteRemoved(int64 Samples)
{
	MOAudioNaNGuardState::GRemovedSinceTick.Add(Samples);
	MOAudioNaNGuardState::GRemovedTotal.Add(Samples);
}

void FMOAudioNaNGuard::Install(UWorld* World)
{
	if (World && World->IsGameWorld() && CVarAudioNaNGuard.GetValueOnGameThread() != 0)
	{
		const FAudioDeviceHandle Handle = World->GetAudioDevice();
		if (Handle.IsValid())
		{
			MOAudioNaNGuardState::Install(World, Handle.GetAudioDevice());
		}
	}
}

int64 FMOAudioNaNGuard::GetRemovedTotal()
{
	return MOAudioNaNGuardState::GRemovedTotal.GetValue();
}

int32 FMOAudioNaNGuard::GetRecoveries()
{
	return MOAudioNaNGuardState::GRecoveries;
}

namespace MOAudioNaNGuardState
{
	/** Once per world on its audio device: the guard first in the master reverb's chain, and on the master submix (removed first: never two). */
	void Install(UWorld* World, FAudioDevice* Device)
	{
		if (!Device || (GInstalledDeviceId == Device->DeviceID && GInstalledWorld.Get() == World))
		{
			return;
		}
		if (!GPreset.IsValid())
		{
			GPreset.Reset(NewObject<UMOSubmixNaNGuardPreset>(GetTransientPackage(), TEXT("MOSubmixNaNGuard")));
			GMasterPresetId = GPreset->GetUniqueID();
		}
		UMOSubmixNaNGuardPreset* Guard = GPreset.Get();
		UAudioMixerBlueprintLibrary::RemoveMasterSubmixEffect(World, Guard);
		UAudioMixerBlueprintLibrary::AddMasterSubmixEffect(World, Guard);
		USoundSubmix* Master = Cast<USoundSubmix>(GetDefault<UAudioSettings>()->MasterSubmix.TryLoad());
		const bool bDryZeroed = CloseDryPath(World, Master);
		const int32 ReverbEffects = ApplyReverbGuard(World);   // closes the reverb's dry path too
		GInstalledDeviceId = Device->DeviceID;
		GInstalledWorld = World;
		UE_LOG(LogMOFramework, Log, TEXT("[MOAudio] NaN guard installed on audio device %u: master submix (dry path %s), and before the master reverb's %d effect(s)"),
			Device->DeviceID, bDryZeroed ? TEXT("closed") : (Master ? TEXT("left open: mo.Audio.NaNGuard.CloseDry 0, or audible") : TEXT("NOT closed: no master submix")), ReverbEffects);
	}

	/** The source guard on every procedural sound (MetaSound) of this world that has no source effect chain of its own: a sound playing is
	 *  restarted, since a component takes its chain at Play. Returns the sounds guarded now. */
	int32 AttachSourceGuards(UWorld* World)
	{
		if (!GSourceChain.IsValid())
		{
			USoundEffectSourcePresetChain* Chain = NewObject<USoundEffectSourcePresetChain>(GetTransientPackage(), TEXT("MOSourceNaNGuardChain"));
			FSourceEffectChainEntry Entry;
			Entry.Preset = NewObject<UMOSourceNaNGuardPreset>(Chain, TEXT("MOSourceNaNGuard"));
			Chain->Chain.Add(Entry);
			GSourceChain.Reset(Chain);
		}
		// Gathered first, worked on after: a Play may make UObjects, and nothing may while the object hash is iterated (a fatal "modify UObject
		// map ... being iterated", Oct 22).
		TArray<UAudioComponent*> Unguarded;
		ForEachObjectOfClass(UAudioComponent::StaticClass(), [World, &Unguarded](UObject* Object)
		{
			UAudioComponent* Component = static_cast<UAudioComponent*>(Object);
			if (Component->GetWorld() == World && Cast<USoundWaveProcedural>(Component->Sound) && !Component->SourceEffectChain && !Component->Sound->SourceEffectChain)
			{
				Unguarded.Add(Component);   // this world's procedural sound, not guarded already, no chain of its own (those are left alone)
			}
		}, true, RF_ClassDefaultObject, EInternalObjectFlags::Garbage);
		int32 Attached = 0;
		FString Names;
		for (UAudioComponent* Component : Unguarded)
		{
			Component->SetSourceEffectChain(GSourceChain.Get());
			if (Component->IsPlaying())
			{
				Component->Stop();
				Component->Play();
			}
			++Attached;
			Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + GetNameSafe(Component->Sound);
		}
		if (Attached > 0)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOAudio] NaN guard on %d procedural sound(s): %s"), Attached, *Names);
		}
		return Attached;
	}

	/** Every sound playing in this world, once per poisoned stretch: the invalid samples are made AFTER the source effects (the source guards
	 *  see none, the master and the reverb's input both do, Oct 22), so in a source's volume / distance / panning stage -- a NaN or absurd
	 *  position or multiplier names the sound. */
	void DumpPlayingSounds(UWorld* World)
	{
		int32 Count = 0;
		ForEachObjectOfClass(UAudioComponent::StaticClass(), [World, &Count](UObject* Object)
		{
			UAudioComponent* Component = static_cast<UAudioComponent*>(Object);
			if (Component->GetWorld() != World || !Component->IsPlaying())
			{
				return;
			}
			++Count;
			const FVector Where = Component->GetComponentLocation();
			const bool bBad = Where.ContainsNaN() || Where.GetAbsMax() > 1.0e7 || !FMath::IsFinite(Component->VolumeMultiplier) || !FMath::IsFinite(Component->PitchMultiplier);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] NaN guard: playing %s%s on %s at %s vol x%.3f pitch x%.3f spatialized %d procedural %d"),
				bBad ? TEXT("SUSPECT ") : TEXT(""), *GetNameSafe(Component->Sound), *GetNameSafe(Component->GetOwner()), *Where.ToCompactString(),
				Component->VolumeMultiplier, Component->PitchMultiplier, Component->bAllowSpatialization ? 1 : 0, Cast<USoundWaveProcedural>(Component->Sound) ? 1 : 0);
		}, true, RF_ClassDefaultObject, EInternalObjectFlags::Garbage);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] NaN guard: %d sound(s) playing while the mix holds invalid samples"), Count);
	}

	/** The procedural sounds playing (MetaSounds): stopped and started again, which resets their DSP state. */
	int32 RestartProceduralSounds(UWorld* World)
	{
		int32 Restarted = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			TInlineComponentArray<UAudioComponent*> Audio(*It);
			for (UAudioComponent* Component : Audio)
			{
				if (Component && Component->IsPlaying() && Cast<USoundWaveProcedural>(Component->Sound))
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] NaN guard: restarting %s (%s on %s)"), *GetNameSafe(Component->Sound), *Component->GetName(), *It->GetName());
					Component->Stop();
					Component->Play();
					++Restarted;
				}
			}
		}
		return Restarted;
	}

	bool Tick(float)
	{
		if (CVarAudioNaNGuard.GetValueOnGameThread() == 0 || !GEngine)
		{
			return true;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || !World->IsGameWorld())
			{
				continue;
			}
			const FAudioDeviceHandle Handle = World->GetAudioDevice();
			if (!Handle.IsValid())
			{
				break;
			}
			Install(World, Handle.GetAudioDevice());
			AttachSourceGuards(World);
			const int64 Removed = GRemovedSinceTick.Set(0);
			const int64 Blocks = GBlocksSinceTick.Set(0);
			const int64 Blanked = GBlankedSinceTick.Set(0);
			const int64 MasterBlanked = GMasterBlankedSinceTick.Set(0);
			const int64 SourceBlanked = GSourceBlankedSinceTick.Set(0);
			const int64 SourceBlocks = GSourceBlocksSinceTick.Set(0);
			if (Blanked > 0 || SourceBlanked > 0)
			{
				// The mix guards (the master's and the reverb's input): the master's silenced blocks are the mix gone silent. The source guards
				// silence one poisoned sound and the rest plays on.
				UE_LOG(LogMOFramework, Log, TEXT("[MOAudio] NaN guard: silenced %lld master + %lld reverb-input of %lld mix block(s), and %lld of %lld source block(s), in the last second (%lld invalid sample(s))"),
					MasterBlanked, Blanked - MasterBlanked, Blocks, SourceBlanked, SourceBlocks, Removed);
			}
			GNaNSeconds = Removed > 0 ? GNaNSeconds + 1 : 0;
			if (Removed == 0)
			{
				GDumpedThisStretch = false;
			}
			else if (!GDumpedThisStretch)
			{
				GDumpedThisStretch = true;
				DumpPlayingSounds(World);
			}
			if (GNaNSeconds >= FMOAudioNaNGuard::RecoverAfterSeconds)
			{
				// The source first (a poisoned procedural sound), then the reverb (a feedback network keeps what it took in before the guard was there).
				const int32 Restarted = RestartProceduralSounds(World);
				ApplyReverbGuard(World);
				++GRecoveries;
				GNaNSeconds = 0;
				UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] NaN guard: %lld invalid sample(s) in the last second, %d s running: restarted %d procedural sound(s) and the master reverb (recovery %d)"),
					Removed, FMOAudioNaNGuard::RecoverAfterSeconds, Restarted, GRecoveries);
			}
			break;
		}
		return true;
	}

	struct FRegistration
	{
		FTSTicker::FDelegateHandle Handle;
		FRegistration() { Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), 1.0f); }
		~FRegistration() { FTSTicker::GetCoreTicker().RemoveTicker(Handle); }
	};
	FRegistration GRegistration;
}
