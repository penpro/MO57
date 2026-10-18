#include "MODevMode.h"
#include "MOGameSettings.h"
#include "MOGameState.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

bool FMODevMode::IsForcedByBuild()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return true;
#endif
}

bool FMODevMode::IsEnabled()
{
	if (IsForcedByBuild())
	{
		return true;
	}
	static const bool bCommandLine = FParse::Param(FCommandLine::Get(), TEXT("devmode"));
	if (bCommandLine)
	{
		return true;
	}
	const UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	return Settings && Settings->bDeveloperMode;
}

bool FMODevMode::IsHostInDevMode(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World || World->GetNetMode() != NM_Client)
	{
		return IsEnabled();
	}
	const AMOGameState* GameState = World->GetGameState<AMOGameState>();
	return GameState && GameState->IsHostDeveloperMode();
}
