// MO.World.Creation: the New Game panel's world-creation choice from the console (tests, the harness, a quick look at a shaped world). It sets only
// what the NEXT new game is created with (UMOGameSettings::PendingWorldCreation), exactly as the panel does, and prints this world's settings.

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "MOFramework.h"
#include "MOGameSettings.h"
#include "MOGameState.h"
#include "MOWorldCreationTypes.h"

static FAutoConsoleCommandWithWorldAndArgs GMOWorldCreationCommand(
	TEXT("MO.World.Creation"),
	TEXT("The next new game's creation settings: MO.World.Creation [Knob=Value ...] [StartingBiome=<DT_Biomes row>|coast] [reset]. Knobs: BiomeSize WorldHeight Mountains ")
	TEXT("Lakes Caves Vegetation Gathering Crafting Building Terraforming Inspecting SkillGain. No arguments: print the pending and the active settings."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
		if (!Settings)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WORLDCREATION FAILED: no game settings"));
			return;
		}
		FMOWorldCreationSettings& Pending = Settings->PendingWorldCreation;
		int32 Bad = 0;
		for (const FString& Arg : Args)
		{
			if (Arg.Equals(TEXT("reset"), ESearchCase::IgnoreCase))
			{
				Pending = FMOWorldCreationSettings();
				continue;
			}
			FString Key, Value;
			if (!Arg.Split(TEXT("="), &Key, &Value))
			{
				++Bad;
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WORLDCREATION '%s' is not Key=Value"), *Arg);
				continue;
			}
			if (Key.Equals(TEXT("StartingBiome"), ESearchCase::IgnoreCase))
			{
				Pending.StartingBiome = Value.Equals(TEXT("coast"), ESearchCase::IgnoreCase) || Value.IsEmpty() ? NAME_None : FName(*Value);
			}
			else if (!FMOWorldCreation::SetKnob(Pending, FName(*Key), FCString::Atof(*Value)))
			{
				++Bad;
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WORLDCREATION no knob '%s'"), *Key);
			}
		}
		UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WORLDCREATION pending (the next new game): %s%s"), *FMOWorldCreation::Describe(Pending),
			Bad ? TEXT(" -- some arguments were refused") : TEXT(""));
		if (const AMOGameState* State = World ? World->GetGameState<AMOGameState>() : nullptr)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WORLDCREATION active (this world): %s"), *FMOWorldCreation::Describe(State->GetWorldCreation()));
		}
	}));
