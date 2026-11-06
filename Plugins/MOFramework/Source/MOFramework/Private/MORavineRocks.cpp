// Rocks set into the walls of the ravines (N-330; Wes, Nov 5: "the rocks need a different sampler so they're not looking for horizontal surface to spawn on but looking to spawn on the sides of the ravine,
// and we need big rocks too not just little ones, all of which need pushed into the ravine laterally"). The scatter samples ground from above, which a sheer wall does not have, so these come from the
// generator's own wall (FMOWorldGen::SampleRavineWall). Every machine places them for itself from the seed; nothing is replicated. Placement marches through the ravine's 3D noise, so it runs off the
// game thread: one ravine at a time, and its components are made when the job is done.

#include "MOFeatureRocks.h"
#include "MOFramework.h"
#include "MOWorldFeatureSubsystem.h"
#include "MOWorldGen.h"
#include "MOWorldSeedSubsystem.h"
#include "Async/Async.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarRavineRocks(TEXT("mo.WorldGen.RavineRocks"), 1,
		TEXT("Dev: set rocks into the ravines' walls (0 = the voxel walls alone). Read when a ravine comes within reach."), ECVF_Default);

	constexpr double RavineRockReachCm = 120000.0;    // a ravine is a slot: from further off nothing shows of its walls
	constexpr double RavineRockDespawnFactor = 1.25;
	constexpr float RavineRockSpacingCm = 1000.0f;    // a wall has about one rock per this square
	constexpr int32 RavineRockMaxPerWall = 220;
	constexpr float RavineBigRockShare = 0.3f;        // Wes: "we need big rocks too not just little ones"
	constexpr float RavineRockSetIn = 0.5f;           // how far into the wall, in the rock's half width: "pushed into the ravine laterally"

	/** A ravine's rocks, per mesh. Pure function of the world and the cell: every machine gets the same. Runs on a worker. */
	TArray<TArray<FTransform>> PlaceRavineRocks(const FMOWorldGenParamsRef& Params, int64 Cx, int64 Cy, int32 Seed, const FMORavine& Ravine, const TArray<MOFeatureRocks::FMeshSize>& Sizes)
	{
		TArray<TArray<FTransform>> ByMesh;
		ByMesh.SetNum(Sizes.Num());
		FRandomStream Rng((int32)HashCombine(GetTypeHash(Seed), GetTypeHash(FIntPoint((int32)Cx, (int32)Cy))));
		const int32 PerWall = FMath::Clamp(FMath::CeilToInt32(Ravine.LengthCm * Ravine.DepthCm / FMath::Square(RavineRockSpacingCm)), 20, RavineRockMaxPerWall);
		for (const float Side : { -1.0f, 1.0f })
		{
			for (int32 N = 0; N < PerWall; ++N)
			{
				const float S = Rng.FRandRange(0.06f, 0.94f);
				const float Down = Rng.FRandRange(0.03f, 0.97f);
				const int32 MeshIndex = Rng.RandRange(0, Sizes.Num() - 1);
				const bool bBig = Rng.FRand() < RavineBigRockShare;
				// The slot narrows to a third towards its floor: big rocks there would close it.
				const float BigLimit = FMath::Lerp(1.0f, 0.3f, Down) * Ravine.HalfWidthCm;
				const float HalfWidth = bBig ? Rng.FRandRange(0.55f, 1.0f) * BigLimit : Rng.FRandRange(70.0f, 200.0f);
				const float HalfHeight = HalfWidth * Rng.FRandRange(0.7f, 1.3f);
				const FRotator Rotation(Rng.FRandRange(-25.0f, 25.0f), Rng.FRandRange(0.0f, 360.0f), Rng.FRandRange(-25.0f, 25.0f));
				FVector Wall;
				FVector2D Airward;
				if (FMOWorldGen::SampleRavineWall(*Params, Cx, Cy, Seed, S, Side, Down, Wall, Airward))
				{
					const FVector Center = Wall - FVector(Airward * (double)(RavineRockSetIn * HalfWidth), 0.0);
					ByMesh[MeshIndex].Add(MOFeatureRocks::MakeInstance(Sizes[MeshIndex], Center, Rotation, HalfWidth, HalfHeight));
				}
			}
		}
		return ByMesh;
	}
}

void UMOWorldFeatureSubsystem::ClearRavineRocks()
{
	if (RavineRockJob.IsValid())
	{
		RavineRockJob.Wait();   // it only reads the params it holds; its result is dropped
		RavineRockJob = TFuture<TArray<TArray<FTransform>>>();
	}
	for (TPair<FIntPoint, TWeakObjectPtr<AActor>>& Pair : RavineRocks)
	{
		if (AActor* Actor = Pair.Value.Get())
		{
			Actor->Destroy();
		}
	}
	RavineRocks.Reset();
}

void UMOWorldFeatureSubsystem::UpdateRavineRocks()
{
	UWorld* World = GetWorld();
	const UMOWorldSeedSubsystem* Seeds = World ? UMOWorldSeedSubsystem::Get(World) : nullptr;
	if (!Seeds || !Seeds->HasActiveSeed() || !FMOWorldGenParamsProvider::HasPublished())
	{
		return;
	}
	const FMOWorldGenParamsRef Params = FMOWorldGenParamsProvider::Get();
	const FMOWorldGenTuning& T = Params->Tuning;
	const int32 Seed = Seeds->GetActiveTerrainSeed();
	const FVector2D Origin(Params->Origin.X, Params->Origin.Y);
	if (Seed != RavineRocksSeed || Origin != RavineRocksOrigin)
	{
		ClearRavineRocks();   // another world: its ravines ran elsewhere
		RavineRocksSeed = Seed;
		RavineRocksOrigin = Origin;
	}
	if (T.RavineChance <= 0.0f || CVarRavineRocks.GetValueOnGameThread() == 0)
	{
		return;
	}
	// A finished job: its rocks, if the ravine is still wanted (the despawn below takes it otherwise).
	if (RavineRockJob.IsValid() && RavineRockJob.IsReady())
	{
		TArray<UStaticMesh*> Meshes;
		for (const TObjectPtr<UStaticMesh>& Mesh : RavineRockMeshes)
		{
			Meshes.Add(Mesh.Get());
		}
		const TArray<TArray<FTransform>> ByMesh = RavineRockJob.Get();
		int32 Rocks = 0;
		for (const TArray<FTransform>& Instances : ByMesh)
		{
			Rocks += Instances.Num();
		}
		RavineRocks.Add(RavineRockJobCell, MOFeatureRocks::SpawnHolder(World, Meshes, ByMesh));
		RavineRockJob = TFuture<TArray<TArray<FTransform>>>();
		UE_LOG(LogMOFramework, Log, TEXT("[MORavineRocks] %d rocks in the ravine of cell (%d, %d); %d ravines dressed"), Rocks, RavineRockJobCell.X, RavineRockJobCell.Y, RavineRocks.Num());
	}

	TArray<FVector2D, TInlineAllocator<8>> Viewers;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (const APawn* Pawn = PC ? PC->GetPawn() : nullptr)
		{
			Viewers.Add(FVector2D(Pawn->GetActorLocation()));
		}
	}
	if (Viewers.IsEmpty())
	{
		return;
	}
	TSet<FIntPoint> InReach;
	double NearestDist = TNumericLimits<double>::Max();
	FIntPoint NearestCell(0, 0);
	FMORavine Nearest;
	const int64 R = (int64)FMath::CeilToDouble(RavineRockDespawnFactor * RavineRockReachCm / T.RavineCellCm) + 1;
	for (const FVector2D& V : Viewers)
	{
		int64 Cx0 = 0, Cy0 = 0;
		FMOWorldGen::WorldToCell(*Params, V.X, V.Y, T.RavineCellCm, Cx0, Cy0);
		for (int64 Cy = Cy0 - R; Cy <= Cy0 + R; ++Cy)
		{
			for (int64 Cx = Cx0 - R; Cx <= Cx0 + R; ++Cx)
			{
				FMORavine Ravine;
				if (!FMOWorldGen::GetRavineInCell(*Params, Cx, Cy, Seed, Ravine))
				{
					continue;
				}
				const FIntPoint Cell((int32)Cx, (int32)Cy);
				const double Dist = FVector2D::Distance(Ravine.Center, V);
				if (Dist <= RavineRockDespawnFactor * RavineRockReachCm)
				{
					InReach.Add(Cell);
				}
				if (Dist <= RavineRockReachCm && Dist < NearestDist && !RavineRocks.Contains(Cell))
				{
					NearestDist = Dist;
					NearestCell = Cell;
					Nearest = Ravine;
				}
			}
		}
	}
	for (auto It = RavineRocks.CreateIterator(); It; ++It)
	{
		if (!InReach.Contains(It->Key))
		{
			if (AActor* Actor = It->Value.Get())
			{
				Actor->Destroy();
			}
			It.RemoveCurrent();
		}
	}
	if (NearestDist == TNumericLimits<double>::Max() || RavineRockJob.IsValid())
	{
		return;
	}
	TArray<UStaticMesh*> Meshes;
	TArray<MOFeatureRocks::FMeshSize> Sizes;
	MOFeatureRocks::LoadMeshes(Meshes, Sizes);
	if (Meshes.IsEmpty())
	{
		return;
	}
	RavineRockMeshes.Reset();
	for (UStaticMesh* Mesh : Meshes)
	{
		RavineRockMeshes.Add(Mesh);
	}
	RavineRockJobCell = NearestCell;
	const int64 JobCx = NearestCell.X, JobCy = NearestCell.Y;
	RavineRockJob = Async(EAsyncExecution::ThreadPool, [Params, JobCx, JobCy, Seed, Ravine = Nearest, Sizes]()
	{
		return PlaceRavineRocks(Params, JobCx, JobCy, Seed, Ravine, Sizes);
	});
}
