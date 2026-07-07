#include "MOClockSaveDomainAdapter.h"
#include "MOGameClockSubsystem.h"
#include "MOPersistenceSubsystem.h"
#include "MOworldSaveGame.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UMOClockSaveDomainAdapter::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UGameInstance* GI = InWorld.GetGameInstance())
	{
		if (UMOPersistenceSubsystem* Persist = GI->GetSubsystem<UMOPersistenceSubsystem>())
		{
			Persist->RegisterSaveDomain(this);
		}
	}
}

void UMOClockSaveDomainAdapter::Deinitialize()
{
	if (UWorld* W = GetWorld())
	{
		if (UGameInstance* GI = W->GetGameInstance())
		{
			if (UMOPersistenceSubsystem* Persist = GI->GetSubsystem<UMOPersistenceSubsystem>())
			{
				Persist->UnregisterSaveDomain(this);
			}
		}
	}
	Super::Deinitialize();
}

void UMOClockSaveDomainAdapter::CaptureSaveDomain(UMOWorldSaveGame& Save)
{
	if (const UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this))
	{
		Save.GameClockData = Clock->BuildSaveData();
	}
}

void UMOClockSaveDomainAdapter::ApplySaveDomain(const UMOWorldSaveGame& Save)
{
	if (UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this))
	{
		// ApplySaveData no-ops internally on legacy/fresh saves (bIsValid=false).
		Clock->ApplySaveData(Save.GameClockData);
	}
}
