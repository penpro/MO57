// The master volume must be applied ONCE (at the audio device), not also baked into every sound class: until 2026-10-12 each class volume was category x master, and the device volume multiplies
// everything again, so a Master slider at 0.49 played at 0.24 (and a quiet SFX / ambient slider on top made the game sound "broken").

#include "Misc/AutomationTest.h"
#include "MOAudioSubsystem.h"
#include "MOAudioSettings.h"
#include "Engine/GameInstance.h"
#include "Sound/SoundClass.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAudio_MasterVolumeOnce,
	"MOFramework.Audio.MasterVolumeAppliedOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAudio_MasterVolumeOnce::RunTest(const FString& Parameters)
{
	const UMOAudioSettings* Settings = GetDefault<UMOAudioSettings>();
	USoundClass* Music = Settings ? Cast<USoundClass>(Settings->MusicSoundClass.TryLoad()) : nullptr;
	USoundClass* Ambient = Settings ? Cast<USoundClass>(Settings->AmbientSoundClass.TryLoad()) : nullptr;
	USoundClass* Sfx = Settings ? Cast<USoundClass>(Settings->SFXSoundClass.TryLoad()) : nullptr;
	if (!TestNotNull(TEXT("the music / ambient / SFX sound classes load from the audio settings"), Music) || !TestNotNull(TEXT("ambient class"), Ambient) || !TestNotNull(TEXT("SFX class"), Sfx))
	{
		return false;
	}
	const float OldMusic = Music->Properties.Volume, OldAmbient = Ambient->Properties.Volume, OldSfx = Sfx->Properties.Volume;

	UGameInstance* Instance = NewObject<UGameInstance>(GetTransientPackage());   // the subsystem is a GameInstance subsystem: its ClassWithin is the instance
	UMOAudioSubsystem* Audio = NewObject<UMOAudioSubsystem>(Instance);
	Audio->SetMasterVolume(0.5f);
	Audio->SetMusicVolume(0.8f);
	Audio->SetAmbientVolume(0.4f);
	Audio->SetSFXVolume(0.3f);
	TestEqual(TEXT("music class volume = the music slider (the master is not baked in)"), Music->Properties.Volume, 0.8f);
	TestEqual(TEXT("ambient class volume = the ambient slider"), Ambient->Properties.Volume, 0.4f);
	TestEqual(TEXT("SFX class volume = the SFX slider"), Sfx->Properties.Volume, 0.3f);

	Audio->SetMasterVolume(0.2f);   // moving the master must not touch the class volumes either
	TestEqual(TEXT("music class volume is unchanged by a master change"), Music->Properties.Volume, 0.8f);
	TestEqual(TEXT("SFX class volume is unchanged by a master change"), Sfx->Properties.Volume, 0.3f);

	Music->Properties.Volume = OldMusic;
	Ambient->Properties.Volume = OldAmbient;
	Sfx->Properties.Volume = OldSfx;
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
