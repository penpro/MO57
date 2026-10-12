/**
 * MOAudioDiagCommands.cpp - two dev commands for "I hear nothing / only scratchy noise" reports.
 *
 *   MO.Audio.Diag   what the engine thinks the audio path is doing: device sample rate / buffer, active sounds, master volumes (engine multiplier + MO's own), mute.
 *   MO.Audio.Tone   plays a 1 s 440 Hz sine through the 2D path, bypassing every sound class / mix / content asset. Heard = the device output path works and the problem is in content / mix /
 *                   volumes; not heard = the OS device (wrong endpoint, muted per-app volume, exclusive-mode app) or the audio mixer's output.
 *
 * Both print at Warning level so they show in the log and on screen with LogAudio at Warning.
 */

#include "MOFramework.h"
#include "MOAudioSubsystem.h"

#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "AudioThread.h"
#include "ActiveSound.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWaveProcedural.h"

namespace
{
	void DumpAudio(UWorld* World)
	{
		const FAudioDeviceHandle Handle = World ? World->GetAudioDevice() : FAudioDeviceHandle();
		if (!Handle.IsValid())
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] DIAG: no audio device (running with -nosound, or the audio mixer failed to start)"));
			return;
		}
		const UMOAudioSubsystem* Audio = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UMOAudioSubsystem>() : nullptr;
		UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] DIAG: MO master volume %.2f"), Audio ? Audio->GetMasterVolume() : -1.0f);
		// The device's own state is read on the audio thread.
		FAudioThread::RunCommandOnAudioThread([Handle]()
		{
			const FAudioDevice* Device = Handle.GetAudioDevice();
			if (!Device)
			{
				return;
			}
			UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] DIAG: device id %u, %d active sound(s), sample rate %.0f Hz, buffer %d frames, engine primary volume %.2f, muted %s"),
				Device->DeviceID, Device->GetActiveSounds().Num(), Device->GetSampleRate(), Device->GetBufferLength(), Device->GetTransientPrimaryVolume(),
				Device->IsAudioDeviceMuted() ? TEXT("YES") : TEXT("no"));
			int32 Shown = 0;
			for (const FActiveSound* Active : Device->GetActiveSounds())
			{
				if (Active && Active->GetSound() && Shown++ < 8)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] DIAG:   playing %s (volume %.2f, %s)"), *Active->GetSound()->GetName(), Active->VolumeMultiplier, Active->bIsUISound ? TEXT("UI") : TEXT("world"));
				}
			}
		});
	}

	void PlayTone(UWorld* World)
	{
		if (!World)
		{
			return;
		}
		constexpr int32 Rate = 48000;
		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>();
		Wave->SetSampleRate(Rate);
		Wave->NumChannels = 1;
		Wave->Duration = 1.0f;
		Wave->SoundGroup = SOUNDGROUP_Default;
		Wave->bLooping = false;
		TArray<int16> Samples;
		Samples.SetNumUninitialized(Rate);
		for (int32 i = 0; i < Rate; ++i)
		{
			const float Fade = FMath::Min(1.0f, FMath::Min(i, Rate - i) / 2400.0f);   // 50 ms ramps so the tone itself does not click
			Samples[i] = static_cast<int16>(FMath::Sin(2.0f * PI * 440.0f * i / Rate) * 12000.0f * Fade);
		}
		Wave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
		UGameplayStatics::PlaySound2D(World, Wave, 1.0f);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] TONE: playing a 1 s 440 Hz sine (bypasses sound classes and mixes)"));
	}
}

static FAutoConsoleCommandWithWorld GMOAudioDiag(TEXT("MO.Audio.Diag"), TEXT("Log the audio device state (sample rate, buffer, active sounds, volumes, mute)."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&DumpAudio));

static FAutoConsoleCommandWithWorld GMOAudioTone(TEXT("MO.Audio.Tone"), TEXT("Play a 1 s 440 Hz tone straight through the 2D audio path (tests the device output)."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&PlayTone));
