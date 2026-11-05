// The audio mix a new player starts with and Options' Reset returns to (UMOGameSettings::SetToDefaults). The engine calls it on a first run, when no
// GameUserSettings.ini exists (UGameUserSettings::ValidateSettings), so until Nov 2026 its own second set of numbers was what a new player heard: music
// 0.8 against the tuned 0.15, ambient 1.0 against 0.2, effects 1.0 against 0.5. Run: python Tools/ue.py auto --filter "MOFramework.Settings.ResetIsTheTunedMix"

#include "Misc/AutomationTest.h"
#include "MOGameSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSettings_ResetIsTheTunedMix,
	"MOFramework.Settings.ResetIsTheTunedMix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSettings_ResetIsTheTunedMix::RunTest(const FString& Parameters)
{
	UMOGameSettings* Settings = NewObject<UMOGameSettings>(GetTransientPackage());
	Settings->MasterVolume = Settings->MusicVolume = Settings->SFXVolume = Settings->AmbientVolume = Settings->WeatherVolume = 0.37f;   // a player's own mix

	Settings->SetToDefaults();

	// The numbers themselves, so changing the mix is a decision this test has to be told about.
	TestEqual(TEXT("master"), Settings->MasterVolume, 1.0f);
	TestEqual(TEXT("music is the tuned 0.15, not 0.8"), Settings->MusicVolume, 0.15f);
	TestEqual(TEXT("effects are the tuned 0.5, not 1.0"), Settings->SFXVolume, 0.50f);
	TestEqual(TEXT("ambient is the tuned 0.2, not 1.0"), Settings->AmbientVolume, 0.20f);
	TestEqual(TEXT("weather goes back too (Reset left it where the player had it)"), Settings->WeatherVolume, 0.50f);

	// A fresh object's own defaults (the C++ initializers) are the same numbers: one source.
	TestEqual(TEXT("the music field's initializer is the reset value"), UMOGameSettings::DefaultMusicVolume, Settings->MusicVolume);
	TestEqual(TEXT("the effects field's initializer is the reset value"), UMOGameSettings::DefaultSFXVolume, Settings->SFXVolume);
	TestEqual(TEXT("the ambient field's initializer is the reset value"), UMOGameSettings::DefaultAmbientVolume, Settings->AmbientVolume);
	return true;
}

#endif
