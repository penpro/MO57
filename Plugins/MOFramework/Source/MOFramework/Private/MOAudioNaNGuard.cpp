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

#include UE_INLINE_GENERATED_CPP_BY_NAME(MOAudioNaNGuard)

static TAutoConsoleVariable<int32> CVarAudioNaNGuard(TEXT("mo.Audio.NaNGuard"), 1,
	TEXT("1 = sanitize non-finite samples before the master reverb and on the master submix, and restart procedural sounds that keep producing them. 0 = off (diagnosis only)."),
	ECVF_Default);

namespace MOAudioNaNGuardState
{
	FThreadSafeCounter64 GRemovedSinceTick;   // render thread -> game thread
	FThreadSafeCounter64 GRemovedTotal;
	int32 GRecoveries = 0;
	int32 GNaNSeconds = 0;
	uint32 GInstalledDeviceId = INDEX_NONE;
	TWeakObjectPtr<UWorld> GInstalledWorld;
	TStrongObjectPtr<UMOSubmixNaNGuardPreset> GPreset;
	void Install(UWorld* World, FAudioDevice* Device);

	/** The master reverb's chain again, guard first: NEW effect instances, so a reverb whose feedback already holds NaN (it latched before the guard
	 *  was there, in the first frames of a session) starts clean. Returns the reverb effects behind the guard, -1 without a reverb submix. */
	int32 ApplyReverbGuard(UWorld* World)
	{
		USoundSubmix* Reverb = Cast<USoundSubmix>(GetDefault<UAudioSettings>()->ReverbSubmix.TryLoad());
		if (!Reverb || !GPreset.IsValid())
		{
			return -1;
		}
		TArray<USoundEffectSubmixPreset*> Chain;
		Chain.Add(GPreset.Get());
		for (USoundEffectSubmixPreset* Preset : Reverb->SubmixEffectChain)
		{
			if (Preset)
			{
				Chain.Add(Preset);
			}
		}
		UAudioMixerBlueprintLibrary::SetSubmixEffectChainOverride(World, Reverb, Chain, 0.0f);
		return Chain.Num() - 1;
	}
}

void FMOSubmixNaNGuard::OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData)
{
	const int32 Num = InData.NumFrames * InData.NumChannels;
	const float* In = InData.AudioBuffer->GetData();
	float* Out = OutData.AudioBuffer->GetData();
	int64 Removed = 0;
	for (int32 I = 0; I < Num; ++I)
	{
		const float S = In[I];
		if (FMath::IsFinite(S) && FMath::Abs(S) <= FMOAudioNaNGuard::MaxValidMagnitude)
		{
			Out[I] = S;
		}
		else
		{
			Out[I] = 0.0f;
			++Removed;
		}
	}
	if (Removed > 0)
	{
		FMOAudioNaNGuard::NoteRemoved(Removed);
	}
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
		}
		UMOSubmixNaNGuardPreset* Guard = GPreset.Get();
		UAudioMixerBlueprintLibrary::RemoveMasterSubmixEffect(World, Guard);
		UAudioMixerBlueprintLibrary::AddMasterSubmixEffect(World, Guard);
		const int32 ReverbEffects = ApplyReverbGuard(World);
		GInstalledDeviceId = Device->DeviceID;
		GInstalledWorld = World;
		UE_LOG(LogMOFramework, Log, TEXT("[MOAudio] NaN guard installed on audio device %u: master submix, and before the master reverb's %d effect(s)"), Device->DeviceID, ReverbEffects);
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
			const int64 Removed = GRemovedSinceTick.Set(0);
			GNaNSeconds = Removed > 0 ? GNaNSeconds + 1 : 0;
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
