#include "MOGameInstance.h"
#include "MOFramework.h"
#include "MOGameSettings.h"

void UMOGameInstance::Init()
{
	Super::Init();

	// Reset intro playback flag on fresh game launch
	// This ensures the intro plays when the game starts, but not when
	// returning to main menu from gameplay (which sets bPlayIntro = false)
	if (UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings())
	{
		Settings->bPlayIntro = true;
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameInstance] Init - Reset bPlayIntro to true for fresh game launch"));
	}
}
