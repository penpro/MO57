// The master volume must be applied ONCE (at the audio device), not also baked into every sound class: until 2026-10-12 each class volume was category x master, and the device volume multiplies
// everything again, so a Master slider at 0.49 played at 0.24 (and a quiet SFX / ambient slider on top made the game sound "broken").

#include "Misc/AutomationTest.h"
#include "MOAudioSubsystem.h"
#include "MOAudioSettings.h"
#include "Engine/GameInstance.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundBase.h"
#include "Sound/AudioSettings.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/PackageName.h"

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

// Every sound the game can play must be under a slider (Wes, Oct 25 2026: "the volume sliders are hit or miss both in game and in the main menu"): its
// class -- its own, or the project's default (AudioSettings) when it has none -- must have SC_Master, or one of the plugins' classes a slider also sets
// (UMOAudioSettings Weather / ExtraAmbient / ExtraSFX lists), as itself or an ancestor. A sound's class is a hard dependency of its package, so the asset
// registry answers without loading a sound. And the Weather slider must reach Ultra Dynamic Sky's class (it did nothing before).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAudio_EverySoundOnASlider,
	"MOFramework.Audio.EverySoundOnASlider",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAudio_EverySoundOnASlider::RunTest(const FString& Parameters)
{
	const UMOAudioSettings* Settings = GetDefault<UMOAudioSettings>();
	if (!TestNotNull(TEXT("audio settings"), Settings))
	{
		return false;
	}
	TSet<FName> SliderRoots;
	auto AddRoot = [&SliderRoots](const FSoftObjectPath& Path) { if (!Path.IsNull()) { SliderRoots.Add(FName(*Path.GetLongPackageName())); } };
	AddRoot(Settings->MasterSoundClass);   // Music / Ambient / SFX / UI are its children
	for (const FSoftObjectPath& Path : Settings->WeatherSoundClasses) { AddRoot(Path); }
	for (const FSoftObjectPath& Path : Settings->ExtraAmbientSoundClasses) { AddRoot(Path); }
	for (const FSoftObjectPath& Path : Settings->ExtraSFXSoundClasses) { AddRoot(Path); }
	// Classes whose sounds need no slider, each with the measured reason.
	const TMap<FName, const TCHAR*> Exempt = {
		{ TEXT("/Engine/EngineSounds/SFX"), TEXT("only Ultra Dynamic Sky's BrownianNoise / DistantThunderLoop waves, played inside UDS MetaSounds whose class governs") },
		{ TEXT("/Engine/EngineSounds/Music"), TEXT("only Oceanology's Cue_TropicalEnvironment, placed by its demo map alone") },
	};
	const FString DefaultClass = GetDefault<UAudioSettings>()->DefaultSoundClassName.GetLongPackageName();
	TestEqual(TEXT("a sound with no class plays in SC_FX (Config/DefaultEngine.ini AudioSettings)"), DefaultClass, Settings->SFXSoundClass.GetLongPackageName());

	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	FARFilter Filter;
	Filter.ClassPaths.Add(USoundBase::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	Filter.PackagePaths = { TEXT("/Game"), TEXT("/MOFramework"), TEXT("/Oceanology_Plugin") };
	Filter.bRecursivePaths = true;
	TArray<FAssetData> Sounds;
	Registry.GetAssets(Filter, Sounds);

	const FTopLevelAssetPath SoundClassPath = USoundClass::StaticClass()->GetClassPathName();
	TMap<FName, bool> Covered;
	auto IsCovered = [&](const FName ClassPackage)
	{
		if (const bool* Known = Covered.Find(ClassPackage))
		{
			return *Known;
		}
		const FString Name = ClassPackage.ToString();
		const USoundClass* SoundClass = LoadObject<USoundClass>(nullptr, *(Name + TEXT(".") + FPackageName::GetShortName(Name)));
		bool bCovered = false;
		for (const USoundClass* Class = SoundClass; Class && !bCovered; Class = Class->ParentClass)
		{
			bCovered = SliderRoots.Contains(Class->GetOutermost()->GetFName());
		}
		return Covered.Add(ClassPackage, bCovered);
	};
	int32 Checked = 0, Uncovered = 0;
	for (const FAssetData& Sound : Sounds)
	{
		const FString Package = Sound.PackageName.ToString();
		if (Package.StartsWith(TEXT("/Game/Megascans/")) || Package.StartsWith(TEXT("/Game/Fab/")) || Package.StartsWith(TEXT("/Game/VoxelExamples/")))
		{
			continue;   // sample content: no sounds the game plays
		}
		TArray<FName> Dependencies;
		Registry.GetDependencies(Sound.PackageName, Dependencies, UE::AssetRegistry::EDependencyCategory::Package, UE::AssetRegistry::EDependencyQuery::Hard);
		FName ClassPackage(*DefaultClass);
		for (const FName Dependency : Dependencies)
		{
			TArray<FAssetData> Assets;
			Registry.GetAssetsByPackageName(Dependency, Assets);
			if (Assets.ContainsByPredicate([&SoundClassPath](const FAssetData& Asset) { return Asset.AssetClassPath == SoundClassPath; }))
			{
				ClassPackage = Dependency;
				break;
			}
		}
		if (Exempt.Contains(ClassPackage))
		{
			continue;
		}
		++Checked;
		if (!IsCovered(ClassPackage) && ++Uncovered <= 20)
		{
			AddError(FString::Printf(TEXT("%s plays in %s, which no volume slider reaches"), *Package, *ClassPackage.ToString()));
		}
	}
	AddInfo(FString::Printf(TEXT("%d sound(s) checked, %d on no slider"), Checked, Uncovered));
	TestTrue(TEXT("the sounds were found (the check is not vacuous)"), Checked > 300);

	// The Weather slider sets Ultra Dynamic Sky's class.
	USoundClass* Weather = Settings->WeatherSoundClasses.IsEmpty() ? nullptr : Cast<USoundClass>(Settings->WeatherSoundClasses[0].TryLoad());
	if (TestNotNull(TEXT("the weather sound class loads"), Weather))
	{
		// the setter re-applies every slider (this bare subsystem's own values): put each class back afterwards
		TMap<USoundClass*, float> Old;
		for (const FSoftObjectPath& Path : { Settings->MusicSoundClass, Settings->AmbientSoundClass, Settings->SFXSoundClass, Settings->UISoundClass, Settings->WeatherSoundClasses[0] })
		{
			if (USoundClass* Class = Cast<USoundClass>(Path.TryLoad()))
			{
				Old.Add(Class, Class->Properties.Volume);
			}
		}
		UGameInstance* Instance = NewObject<UGameInstance>(GetTransientPackage());
		UMOAudioSubsystem* Audio = NewObject<UMOAudioSubsystem>(Instance);
		Audio->SetWeatherVolume(0.37f);
		TestEqual(TEXT("UDS_Weather's volume = the weather slider"), Weather->Properties.Volume, 0.37f);
		for (const TPair<USoundClass*, float>& Pair : Old)
		{
			Pair.Key->Properties.Volume = Pair.Value;
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
