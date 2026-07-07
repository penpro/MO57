#include "MOVoxelSculptSaveDomain.h"
#include "MOFramework.h"
#include "MOPersistenceSubsystem.h"
#include "MOVoxelAlias.h"
#include "MOworldSaveGame.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UMOVoxelSculptSaveDomain::OnWorldBeginPlay(UWorld& InWorld)
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

void UMOVoxelSculptSaveDomain::Deinitialize()
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

void UMOVoxelSculptSaveDomain::CaptureSaveDomain(UMOWorldSaveGame& Save)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	Save.VoxelSculptData.Reset();

	// Capture height sculpt actors (facade hides the Voxel actor type + async save)
	TArray<AActor*> HeightActors;
	MOVoxel::GetHeightSculptActors(World, HeightActors);

	for (AActor* HeightActor : HeightActors)
	{
		if (!IsValid(HeightActor))
		{
			continue;
		}

		FMOVoxelSculptSaveRecord Record;
		Record.ActorName = HeightActor->GetName();
		Record.bIsVolumeSculpt = false;

		if (MOVoxel::SaveHeightSculpt(HeightActor, Record.SculptData))
		{
			Record.bHasValidData = true;
			UE_LOG(LogMOFramework, Log, TEXT("[MOVoxelSave] Captured height sculpt '%s': %d bytes"),
				*Record.ActorName, Record.SculptData.Num());
			Save.VoxelSculptData.Add(Record);
		}
	}

	// Capture volume sculpt actors
	TArray<AActor*> VolumeActors;
	MOVoxel::GetVolumeSculptActors(World, VolumeActors);

	for (AActor* VolumeActor : VolumeActors)
	{
		if (!IsValid(VolumeActor))
		{
			continue;
		}

		FMOVoxelSculptSaveRecord Record;
		Record.ActorName = VolumeActor->GetName();
		Record.bIsVolumeSculpt = true;

		if (MOVoxel::SaveVolumeSculpt(VolumeActor, Record.SculptData))
		{
			Record.bHasValidData = true;
			UE_LOG(LogMOFramework, Log, TEXT("[MOVoxelSave] Captured volume sculpt '%s': %d bytes"),
				*Record.ActorName, Record.SculptData.Num());
			Save.VoxelSculptData.Add(Record);
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOVoxelSave] Captured %d voxel sculpt actor(s) with modifications"),
		Save.VoxelSculptData.Num());
}

void UMOVoxelSculptSaveDomain::ApplySaveDomain(const UMOWorldSaveGame& Save)
{
	UWorld* World = GetWorld();
	if (!World || Save.VoxelSculptData.Num() == 0)
	{
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOVoxelSave] Restoring %d voxel sculpt record(s)"), Save.VoxelSculptData.Num());

	for (const FMOVoxelSculptSaveRecord& Record : Save.VoxelSculptData)
	{
		if (!Record.bHasValidData || Record.SculptData.Num() == 0)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOVoxelSave] Skipping invalid sculpt record '%s'"),
				*Record.ActorName);
			continue;
		}

		// Resolve the sculpt actor by name (facade hides the Voxel type)
		TArray<AActor*> Actors;
		if (Record.bIsVolumeSculpt)
		{
			MOVoxel::GetVolumeSculptActors(World, Actors);
		}
		else
		{
			MOVoxel::GetHeightSculptActors(World, Actors);
		}

		AActor* FoundActor = nullptr;
		for (AActor* Actor : Actors)
		{
			if (Actor->GetName() == Record.ActorName)
			{
				FoundActor = Actor;
				break;
			}
		}

		if (!FoundActor)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOVoxelSave] Sculpt actor '%s' not found in world"),
				*Record.ActorName);
			continue;
		}

		const bool bLoaded = Record.bIsVolumeSculpt
			? MOVoxel::LoadVolumeSculpt(FoundActor, Record.SculptData)
			: MOVoxel::LoadHeightSculpt(FoundActor, Record.SculptData);
		if (bLoaded)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOVoxelSave] Restored %s sculpt '%s'"),
				Record.bIsVolumeSculpt ? TEXT("volume") : TEXT("height"), *Record.ActorName);
		}
	}
}
