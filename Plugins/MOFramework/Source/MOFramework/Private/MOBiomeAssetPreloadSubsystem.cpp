#include "MOBiomeAssetPreloadSubsystem.h"
#include "MOBiomeDatabaseSettings.h"
#include "MOFramework.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

bool UMOBiomeAssetPreloadSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UMOBiomeAssetPreloadSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	TArray<FName> BiomeIds;
	UMOBiomeDatabaseSettings::GetAllBiomeIds(BiomeIds);
	TSet<FSoftObjectPath> Paths;
	for (const FName& Id : BiomeIds)
	{
		if (const FMOBiomeDefinitionRow* Row = UMOBiomeDatabaseSettings::GetBiomeDefinition(Id))
		{
			for (const FMOBiomeSpeciesEntry& Sp : Row->Species)
			{
				if (!Sp.Mesh.IsNull())
				{
					Paths.Add(Sp.Mesh.ToSoftObjectPath());
				}
			}
			for (const FMOBiomeSpeciesEntry& Sp : Row->WaterEdgeSpecies)
			{
				if (!Sp.Mesh.IsNull())
				{
					Paths.Add(Sp.Mesh.ToSoftObjectPath());
				}
			}
		}
	}
	NumRequested = Paths.Num();
	if (NumRequested == 0)
	{
		bComplete = true;
		return;
	}
	RequestTime = FPlatformTime::Seconds();
	Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths.Array(), FStreamableDelegate::CreateUObject(this, &UMOBiomeAssetPreloadSubsystem::HandleLoaded),
		FStreamableManager::AsyncLoadHighPriority);
	UE_LOG(LogMOFramework, Log, TEXT("[MOBiomePreload] requested %d biome species meshes (%d biomes)"), NumRequested, BiomeIds.Num());
}

void UMOBiomeAssetPreloadSubsystem::HandleLoaded()
{
	Loaded.Reset();
	if (Handle.IsValid())
	{
		TArray<UObject*> Assets;
		Handle->GetLoadedAssets(Assets);
		for (UObject* A : Assets)
		{
			if (UStaticMesh* Mesh = Cast<UStaticMesh>(A))
			{
				Loaded.Add(Mesh);
			}
		}
	}
	bComplete = true;
	UE_LOG(LogMOFramework, Warning, TEXT("[MOBiomePreload] %d of %d biome species meshes resident after %.1f s"), Loaded.Num(), NumRequested, FPlatformTime::Seconds() - RequestTime);
}

void UMOBiomeAssetPreloadSubsystem::Deinitialize()
{
	if (Handle.IsValid())
	{
		Handle->CancelHandle();
		Handle.Reset();
	}
	Loaded.Reset();
	Super::Deinitialize();
}
