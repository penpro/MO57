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

#if !UE_BUILD_SHIPPING
// =============================================================================
// AUDIO HEARTBEAT -- for the intermittent "dddd" buzz / pops / then silence (Oct 16 2026). The engine logs nothing when it happens (no underrun, no device
// swap, no error in 1,504 logs), so this samples the audio device once a second while a game world exists and logs ONLY what changed or looks wrong:
//   - the output device (name / id) changing        -> the "switching" heard as pops
//   - a sound started 4+ times within a second      -> a restart storm (the first few ms of a sound on repeat)
//   - active sounds at the voice limit              -> voices starved (the silence after)
//   - primary volume / mute / app volume changing   -> silence by a volume (the app volume drops when the window loses focus)
// plus a one-line summary every 30 s. Prefix [MOAudio] HB. Off: mo.Audio.Heartbeat 0.
// =============================================================================
#include "AudioMixerDevice.h"
#include "ISubmixBufferListener.h"
#include "Sound/SoundSubmix.h"
#include "Containers/Ticker.h"
#include "Misc/App.h"

static TAutoConsoleVariable<int32> CVarAudioHeartbeat(TEXT("mo.Audio.Heartbeat"), 1,
	TEXT("1 = sample the audio device once a second during play and log changes / restart storms / voice starvation ([MOAudio] HB). 0 = off."), ECVF_Default);

namespace MOAudioHeartbeat
{
	/**
	 * Measures what actually leaves the main submix (the mix the device plays): peak, RMS, NaN / Inf samples, buffers of a constant non-zero value. Wes
	 * heard nothing while 8-10 sounds played at full volume on the right device (Oct 16 2026): the engine's own state was fine, so the samples are next.
	 */
	class FOutputMeter : public ISubmixBufferListener
	{
	public:
		struct FStats
		{
			float Peak = 0.0f;
			double SumSquares = 0.0;
			int64 Samples = 0;
			int64 NonFinite = 0;
			int32 Buffers = 0;
			int32 FlatBuffers = 0;
			float FlatValue = 0.0f;
			float Rms() const { return Samples > 0 ? (float)FMath::Sqrt(SumSquares / (double)Samples) : 0.0f; }
		};

		virtual void OnNewSubmixBuffer(const USoundSubmix*, float* AudioData, int32 NumSamples, int32, const int32, double) override
		{
			float Peak = 0.0f, Low = TNumericLimits<float>::Max(), High = TNumericLimits<float>::Lowest();
			double SumSquares = 0.0;
			int64 NonFinite = 0;
			for (int32 I = 0; I < NumSamples; ++I)
			{
				const float S = AudioData[I];
				if (!FMath::IsFinite(S))
				{
					++NonFinite;
					continue;
				}
				Peak = FMath::Max(Peak, FMath::Abs(S));
				SumSquares += (double)S * S;
				Low = FMath::Min(Low, S);
				High = FMath::Max(High, S);
			}
			FScopeLock Lock(&Mutex);
			Acc.Peak = FMath::Max(Acc.Peak, Peak);
			Acc.SumSquares += SumSquares;
			Acc.Samples += NumSamples;
			Acc.NonFinite += NonFinite;
			Acc.Buffers++;
			if (NumSamples > 0 && High >= Low && High - Low < 1e-5f && FMath::Abs(High) > 0.01f)
			{
				Acc.FlatBuffers++;   // one constant, non-zero value for a whole buffer: a stuck signal (inaudible, but it pops when the gain steps)
				Acc.FlatValue = High;
			}
		}

		virtual const FString& GetListenerName() const override
		{
			static const FString Name(TEXT("MOOutputMeter"));
			return Name;
		}

		FStats Take()
		{
			FScopeLock Lock(&Mutex);
			const FStats Out = Acc;
			Acc = FStats();
			return Out;
		}

	private:
		FCriticalSection Mutex;
		FStats Acc;
	};

	TSharedPtr<FOutputMeter, ESPMode::ThreadSafe> GMeter;   // game thread
	uint32 GMeterDeviceId = INDEX_NONE;                       // the device it is registered on

	/** Audio-thread state: what the device looked like a second ago. */
	struct FState
	{
		FString Device;
		float PrimaryVolume = -1.0f;
		bool bMuted = false;
		TSet<const FActiveSound*> Seen;
		double LastSummarySeconds = 0.0;
		double LastVoiceWarnSeconds = 0.0;
		double LastOutputWarnSeconds = 0.0;
		int32 SilentSeconds = 0;
	};
	FState GState;
	float GLastAppVolume = -1.0f;   // game thread

	void SampleOnAudioThread(FAudioDevice* Device, const FOutputMeter::FStats& Out, bool bMetered)
	{
		FState& S = GState;
		const double Now = FPlatformTime::Seconds();

		FString DeviceName = TEXT("(no mixer platform)");
		if (const Audio::IAudioMixerPlatformInterface* Platform = static_cast<Audio::FMixerDevice*>(Device)->GetAudioMixerPlatform())
		{
			const Audio::FAudioPlatformDeviceInfo Info = Platform->GetPlatformDeviceInfo();
			DeviceName = FString::Printf(TEXT("'%s' [%s] %d Hz %d ch"), *Info.Name, *Info.DeviceId, Info.SampleRate, Info.NumChannels);
		}
		if (DeviceName != S.Device)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] HB output device %s -> %s"), S.Device.IsEmpty() ? TEXT("(start)") : *S.Device, *DeviceName);
			S.Device = DeviceName;
		}

		const float Primary = Device->GetTransientPrimaryVolume();
		const bool bMuted = Device->IsAudioDeviceMuted();
		if (!FMath::IsNearlyEqual(Primary, S.PrimaryVolume, 0.01f) || bMuted != S.bMuted)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] HB primary volume %.2f -> %.2f, muted %s"), S.PrimaryVolume, Primary, bMuted ? TEXT("YES") : TEXT("no"));
			S.PrimaryVolume = Primary;
			S.bMuted = bMuted;
		}

		// New active sounds this second, by sound: a sound that keeps being started is heard as its first milliseconds on repeat.
		const TArray<FActiveSound*>& Active = Device->GetActiveSounds();
		TMap<FString, int32> Started;
		TSet<const FActiveSound*> NowSeen;
		for (const FActiveSound* Sound : Active)
		{
			if (!Sound)
			{
				continue;
			}
			NowSeen.Add(Sound);
			if (!S.Seen.Contains(Sound))
			{
				Started.FindOrAdd(Sound->GetSound() ? Sound->GetSound()->GetName() : FString(TEXT("?")))++;
			}
		}
		S.Seen = MoveTemp(NowSeen);
		for (const TPair<FString, int32>& Pair : Started)
		{
			if (Pair.Value >= 4)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] HB RESTART STORM: %s started %d times in the last second"), *Pair.Key, Pair.Value);
			}
		}

		// What actually left the main submix this second (FOutputMeter). Alarms are rate-limited to one every 5 s.
		if (bMetered)
		{
			FString Alarm;
			if (Out.Buffers == 0)
			{
				Alarm = TEXT("OUTPUT NOT RENDERING: the main submix produced no buffer in the last second");
			}
			else if (Out.NonFinite > 0)
			{
				Alarm = FString::Printf(TEXT("OUTPUT BROKEN: %lld NaN / Inf sample(s) of %lld (a filter or reverb blew up; NaN spreads and silences the mix)"), Out.NonFinite, Out.Samples);
			}
			else if (Out.FlatBuffers > Out.Buffers / 2)
			{
				Alarm = FString::Printf(TEXT("OUTPUT STUCK: %d of %d buffers one constant value %.4f"), Out.FlatBuffers, Out.Buffers, Out.FlatValue);
			}
			else if (Out.Peak >= 0.99f)
			{
				Alarm = FString::Printf(TEXT("OUTPUT CLIPPING: peak %.2f (rms %.3f)"), Out.Peak, Out.Rms());
			}
			S.SilentSeconds = (Out.Buffers > 0 && Out.Peak < 1e-5f && Active.Num() > 0) ? S.SilentSeconds + 1 : 0;
			if (Alarm.IsEmpty() && S.SilentSeconds >= 5)
			{
				Alarm = FString::Printf(TEXT("OUTPUT SILENT for %d s while %d sound(s) play"), S.SilentSeconds, Active.Num());
			}
			if (!Alarm.IsEmpty() && Now - S.LastOutputWarnSeconds > 5.0)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] HB %s"), *Alarm);
				S.LastOutputWarnSeconds = Now;
			}
		}

		const int32 MaxChannels = Device->GetMaxChannels();
		const bool bAtVoiceLimit = MaxChannels > 0 && Active.Num() >= MaxChannels;
		if ((bAtVoiceLimit && Now - S.LastVoiceWarnSeconds > 10.0) || Now - S.LastSummarySeconds > 30.0)
		{
			TMap<FString, int32> ByName;
			for (const FActiveSound* Sound : Active)
			{
				if (Sound && Sound->GetSound())
				{
					ByName.FindOrAdd(Sound->GetSound()->GetName())++;
				}
			}
			ByName.ValueSort(TGreater<int32>());
			FString Top;
			int32 Shown = 0;
			for (const TPair<FString, int32>& Pair : ByName)
			{
				if (Shown++ == 6)
				{
					break;
				}
				Top += FString::Printf(TEXT("%s%s x%d"), Top.IsEmpty() ? TEXT("") : TEXT(", "), *Pair.Key, Pair.Value);
			}
			UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] HB %s%d active sound(s) / %d voices on %s, output peak %.3f rms %.4f: %s"), bAtVoiceLimit ? TEXT("AT VOICE LIMIT: ") : TEXT(""),
				Active.Num(), MaxChannels, *S.Device, Out.Peak, Out.Rms(), Top.IsEmpty() ? TEXT("(none)") : *Top);
			S.LastSummarySeconds = Now;
			if (bAtVoiceLimit)
			{
				S.LastVoiceWarnSeconds = Now;
			}
		}
	}

	bool Tick(float)
	{
		if (CVarAudioHeartbeat.GetValueOnGameThread() == 0 || !GEngine)
		{
			return true;
		}
		// The first game world (PIE or game): its audio device is the one the player hears.
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || !World->IsGameWorld())
			{
				continue;
			}
			// The application volume drops (to the unfocused multiplier) when the window loses focus: silence that is not the audio path's fault.
			const float AppVolume = FApp::GetVolumeMultiplier();
			if (!FMath::IsNearlyEqual(AppVolume, GLastAppVolume, 0.01f))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] HB app volume multiplier %.2f -> %.2f (unfocused multiplier %.2f)"), GLastAppVolume, AppVolume,
					FApp::GetUnfocusedVolumeMultiplier());
				GLastAppVolume = AppVolume;
			}
			const FAudioDeviceHandle Handle = World->GetAudioDevice();
			if (Handle.IsValid())
			{
				// The output meter on the main submix: registered once per device (the registration is queued to the audio thread), read each second.
				FOutputMeter::FStats Out;
				bool bMetered = false;
				if (FAudioDevice* GameDevice = Handle.GetAudioDevice())
				{
					if (!GMeter.IsValid())
					{
						GMeter = MakeShared<FOutputMeter, ESPMode::ThreadSafe>();
					}
					if (GMeterDeviceId != GameDevice->DeviceID)
					{
						GameDevice->RegisterSubmixBufferListener(GMeter.ToSharedRef(), GameDevice->GetMainSubmixObject());
						GMeterDeviceId = GameDevice->DeviceID;
						GMeter->Take();   // the first full second starts clean
					}
					else
					{
						Out = GMeter->Take();
						bMetered = true;
					}
				}
				FAudioThread::RunCommandOnAudioThread([Handle, Out, bMetered]()
				{
					if (FAudioDevice* Device = Handle.GetAudioDevice())
					{
						SampleOnAudioThread(Device, Out, bMetered);
					}
				});
			}
			break;
		}
		return true;
	}

	/** Registered for the module's lifetime; it does nothing until a game world exists. */
	struct FRegistration
	{
		FTSTicker::FDelegateHandle Handle;
		FRegistration() { Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), 1.0f); }
		~FRegistration() { FTSTicker::GetCoreTicker().RemoveTicker(Handle); }
	};
	FRegistration GRegistration;
}
#endif // !UE_BUILD_SHIPPING
