#include "MOCarcassSaveDomain.h"
#include "MOCarcassActor.h"
#include "MOFramework.h"
#include "MOPersistenceSubsystem.h"
#include "MOworldSaveGame.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void UMOCarcassSaveDomain::OnWorldBeginPlay(UWorld& InWorld)
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

void UMOCarcassSaveDomain::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UMOPersistenceSubsystem* Persist = GI->GetSubsystem<UMOPersistenceSubsystem>())
			{
				Persist->UnregisterSaveDomain(this);
			}
		}
	}
	Super::Deinitialize();
}

void UMOCarcassSaveDomain::CaptureSaveDomain(UMOWorldSaveGame& Save)
{
	Save.CarcassData.Carcasses.Reset();
	for (TActorIterator<AMOCarcassActor> It(GetWorld()); It; ++It)
	{
		It->BuildSaveRecord(Save.CarcassData.Carcasses.AddDefaulted_GetRef());
	}
}

void UMOCarcassSaveDomain::ApplySaveDomain(const UMOWorldSaveGame& Save)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// The world being loaded over holds its own kills, not the save's.
	for (TActorIterator<AMOCarcassActor> It(World); It; ++It)
	{
		It->Destroy();
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;   // it lies where it lay
	for (const FMOCarcassSaveRecord& Record : Save.CarcassData.Carcasses)
	{
		if (AMOCarcassActor* Carcass = World->SpawnActor<AMOCarcassActor>(AMOCarcassActor::StaticClass(), Record.Transform, Params))
		{
			Carcass->ApplySaveRecord(Record);
		}
	}
	UE_LOG(LogMOFramework, Log, TEXT("[MOCarcass] Restored %d carcasses from the save"), Save.CarcassData.Carcasses.Num());
}
