// Options' Defaults, pressed in a world, must not touch that world (UMOGameSettings::SetToDefaults). It zeroed PendingWorldSeed, the save wrote that field, and the next load had no seed
// for the terrain; every way out of a world saves, so one button lost the world. Expected: the fields as they were; failing before: seed 0, slot and name empty.
// Run: python Tools/ue.py auto --filter "MOFramework.Settings.ResetKeepsTheWorld"

#include "Misc/AutomationTest.h"
#include "MOGameSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSettings_ResetKeepsTheWorld,
	"MOFramework.Settings.ResetKeepsTheWorld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOSettings_ResetKeepsTheWorld::RunTest(const FString& Parameters)
{
	UMOGameSettings* Settings = NewObject<UMOGameSettings>(GetTransientPackage());
	Settings->PendingWorldSeed = 670711536;
	Settings->PendingNewGameSlot = TEXT("World_Test");
	Settings->PendingWorldName = TEXT("Test");
	Settings->FieldOfView = 70.0f;

	Settings->SetToDefaults();

	TestEqual(TEXT("the world's seed stands"), Settings->PendingWorldSeed, 670711536);
	TestEqual(TEXT("the flow's slot stands"), Settings->PendingNewGameSlot, FString(TEXT("World_Test")));
	TestEqual(TEXT("the flow's world name stands"), Settings->PendingWorldName, FString(TEXT("Test")));
	TestEqual(TEXT("an option does go back (the control: Defaults still works)"), Settings->FieldOfView, 90.0f);
	return true;
}

#endif
