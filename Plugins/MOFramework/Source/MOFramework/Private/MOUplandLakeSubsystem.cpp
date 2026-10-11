/**
 * MOUplandLakeSubsystem.cpp - Water surfaces for the upland lakes (see the header)
 */

#include "MOUplandLakeSubsystem.h"
#include "MOFramework.h"
#include "MOWorldSeedSubsystem.h"
#include "Actors/OceanologyLakeActor.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

namespace
{
	/** Tag on every actor this subsystem spawns. */
	const FName LakeActorTag(TEXT("MOUplandLake"));

	constexpr const TCHAR* DefaultLakeClassPath = TEXT("/Oceanology_Plugin/Design/Lake/Blueprints/Lake/BP_OceanologyLake.BP_OceanologyLake_C");
}

UMOUplandLakeSubsystem::UMOUplandLakeSubsystem()
{
	LakeClass = TSoftClassPtr<AActor>(FSoftObjectPath(DefaultLakeClassPath));
}

UMOUplandLakeSubsystem* UMOUplandLakeSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UMOUplandLakeSubsystem>() : nullptr;
}

bool UMOUplandLakeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	// Played worlds only: not editor preview, not cook, not a dedicated server (nothing renders there).
	return World && World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer && Super::ShouldCreateSubsystem(Outer);
}

TStatId UMOUplandLakeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMOUplandLakeSubsystem, STATGROUP_Tickables);
}

void UMOUplandLakeSubsystem::Deinitialize()
{
	Bodies.Empty();   // the world is going away and takes its actors with it
	LakeClassHandle.Reset();
	Super::Deinitialize();
}

void UMOUplandLakeSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TimeSinceRefresh += DeltaTime;
	if (TimeSinceRefresh >= RefreshIntervalSeconds)
	{
		TimeSinceRefresh = 0.0f;
		RefreshNow();
	}
}

AOceanologyLake* UMOUplandLakeSubsystem::FindLakeBody(int64 CellX, int64 CellY) const
{
	const FLakeBody* Found = Bodies.Find(FIntPoint((int32)CellX, (int32)CellY));
	return Found ? Found->Body.Get() : nullptr;
}

FVector2D UMOUplandLakeSubsystem::ComputePlaneHalfSize(const FMOWorldGen::FLake& Lake)
{
	return Lake.PlaneHalfSizeCm();
}

AOceanologyLake* UMOUplandLakeSubsystem::SpawnLake(const FMOWorldGen::FLake& Lake, UClass* Class)
{
	UWorld* World = GetWorld();
	const AOceanologyLake* Defaults = Class ? Cast<AOceanologyLake>(Class->GetDefaultObject()) : nullptr;
	if (!World || !Defaults)
	{
		return nullptr;
	}

	// The water mesh is a quad tree over a grid of TileSize tiles; its half-size is TileSize x floor(ExtentInTiles x actor scale) around the actor snapped to that grid (UOceanologyWaterMeshComponent::
	// RebuildWaterMesh). Size it from the tiles outward: snap the centre to the grid, count the tiles that reach the far edge of the plane from there, and pick the scale that gives exactly that many
	// (+5% so the floor cannot round it down). The actor is not rotated: the quad tree is world-aligned, so the plane is the ellipse's axis-aligned bounding box (see the header).
	const double Tile = FMath::Max(Defaults->QuadTreeSettings.TileSize, 1.0f);
	const FIntPoint Extent(FMath::Max(Defaults->QuadTreeSettings.ExtentInTiles.X, 1), FMath::Max(Defaults->QuadTreeSettings.ExtentInTiles.Y, 1));
	const FVector2D Half = ComputePlaneHalfSize(Lake);
	const FVector2D Snapped(FMath::GridSnap(Lake.Center.X, Tile), FMath::GridSnap(Lake.Center.Y, Tile));
	const int32 TilesX = FMath::Max(1, FMath::CeilToInt((Half.X + FMath::Abs(Snapped.X - Lake.Center.X)) / Tile));
	const int32 TilesY = FMath::Max(1, FMath::CeilToInt((Half.Y + FMath::Abs(Snapped.Y - Lake.Center.Y)) / Tile));
	const FVector Scale((TilesX + 0.05) / Extent.X, (TilesY + 0.05) / Extent.Y, 1.0);
	const FTransform Where(FRotator::ZeroRotator, FVector(Snapped.X, Snapped.Y, Lake.SurfaceCm), Scale);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;   // derived from the seed: never saved with the level or a game
	Params.bDeferConstruction = true;     // replication is switched off before the actor exists as far as the network is concerned
	AOceanologyLake* Water = World->SpawnActor<AOceanologyLake>(Class, Where, Params);
	if (!Water)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOLakes] Spawning the water of the lake at (%.0f, %.0f) failed"), Lake.Center.X, Lake.Center.Y);
		return nullptr;
	}
	// Every machine derives the same lakes from the seed and spawns its own: a replicated copy from the host would double them on the clients.
	Water->SetReplicates(false);
	Water->SetReplicateMovement(false);
	Water->FinishSpawning(Where);
	Water->Tags.AddUnique(LakeActorTag);
#if WITH_EDITOR
	Water->SetActorLabel(FString::Printf(TEXT("MOUplandLake_%lld_%lld"), Lake.CellX, Lake.CellY));
#endif
	return Water;
}

void UMOUplandLakeSubsystem::DestroyAll()
{
	if (!Bodies.IsEmpty())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOLakes] destroying all %d lake surface(s)"), Bodies.Num());
	}
	for (TPair<FIntPoint, FLakeBody>& Pair : Bodies)
	{
		if (AOceanologyLake* Body = Pair.Value.Body.Get())
		{
			Body->Destroy();
		}
	}
	Bodies.Empty();
}

void UMOUplandLakeSubsystem::RefreshNow()
{
	UWorld* World = GetWorld();
	const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(World);
	if (!World || !Seeds || !Seeds->HasActiveSeed() || !FMOWorldGenParamsProvider::HasPublished())
	{
		if (!Bodies.IsEmpty())
		{
			DestroyAll();
		}
		return;
	}

	const FMOWorldGenParamsRef Params = FMOWorldGenParamsProvider::Get();
	const int32 Seed = Seeds->GetActiveTerrainSeed();
	if (!bHaveLastSeed || LastSeed != Seed || LastParamsHash != Params->GetHash())
	{
		DestroyAll();   // another world (new game, load, different tuning): the old lakes are in the wrong places
		LastSeed = Seed;
		LastParamsHash = Params->GetHash();
		bHaveLastSeed = true;
	}
	if (!Params->Tuning.bEnableWater || !Params->Tuning.bEnableUplandLakes)
	{
		DestroyAll();
		return;
	}

	TArray<FVector> Viewers;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		if (const APawn* Pawn = PC->GetPawn())
		{
			Viewers.Add(Pawn->GetActorLocation());
		}
		else
		{
			FVector Location;
			FRotator Rotation;
			PC->GetPlayerViewPoint(Location, Rotation);
			Viewers.Add(Location);
		}
	}
	if (Viewers.IsEmpty())
	{
		return;
	}

	// Away: lakes no player is within DespawnRadius of.
	for (auto It = Bodies.CreateIterator(); It; ++It)
	{
		bool bNear = It.Value().Body.IsValid();
		if (bNear)
		{
			bNear = false;
			for (const FVector& V : Viewers)
			{
				bNear = bNear || FVector2D::Distance(FVector2D(V.X, V.Y), It.Value().Center) <= DespawnRadiusCm;
			}
		}
		if (!bNear)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOLakes] lake at (%.0f, %.0f) goes: %s"), It.Value().Center.X, It.Value().Center.Y, It.Value().Body.IsValid() ? TEXT("no player within the despawn radius") : TEXT("its water actor no longer exists"));
			if (AOceanologyLake* Body = It.Value().Body.Get())
			{
				Body->Destroy();
			}
			It.RemoveCurrent();
		}
	}

	// Near: lakes within SpawnRadius of any player that have no actor yet.
	UClass* Class = nullptr;
	int32 Spawned = 0;
	for (const FVector& V : Viewers)
	{
		TArray<FMOWorldGen::FLake> Lakes;
		FMOWorldGen::GetUplandLakesNear(*Params, V.X, V.Y, SpawnRadiusCm, Seed, Lakes);
		for (const FMOWorldGen::FLake& Lake : Lakes)
		{
			const FIntPoint Key((int32)Lake.CellX, (int32)Lake.CellY);
			if (Bodies.Contains(Key))
			{
				continue;
			}
			if (!Class)
			{
				Class = LakeClass.Get();
				if (!Class)
				{
					// Not loaded yet: ask for it (once) and spawn on a later refresh. A lake appears a moment late instead of the game hitching to load a blueprint.
					if (LakeClass.IsNull())
					{
						if (!bWarnedNoClass)
						{
							bWarnedNoClass = true;
							UE_LOG(LogMOFramework, Warning, TEXT("[MOLakes] LakeClass is not set: upland lakes will have a bowl but no water surface"));
						}
					}
					else if (!LakeClassHandle.IsValid())
					{
						LakeClassHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(LakeClass.ToSoftObjectPath());
					}
					return;
				}
				if (!Class->IsChildOf(AOceanologyLake::StaticClass()))
				{
					if (!bWarnedNoClass)
					{
						bWarnedNoClass = true;
						UE_LOG(LogMOFramework, Warning, TEXT("[MOLakes] LakeClass %s is not an Oceanology lake: upland lakes will have a bowl but no water surface"), *Class->GetPathName());
					}
					return;
				}
			}
			if (AOceanologyLake* Body = SpawnLake(Lake, Class))
			{
				Bodies.Add(Key, { Body, Lake.Center });
				++Spawned;
			}
		}
	}
	if (Spawned > 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOLakes] %d lake surface(s) created, %d up"), Spawned, Bodies.Num());
	}
}
