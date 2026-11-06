// Floating islands in the voxel world: each island within reach of a player is two additive shape stamps on the terrain's own volume layer, the rock and its grass top (fork F62 A, Wes Nov 4). On that
// layer they are terrain: the dig tool's volume sculpt works them (MOTerraformingComponent::IsOnVolumeRock) and buildings stand on them. Every machine stamps for itself from the seed, as it makes
// the rest of the terrain; nothing is replicated. The biome spawner plants the tops from the generator alone (FMOWorldGen::SampleIslandSurface), so the reach here is well past its farthest tier.

#include "MOFloatingIslandShape.h"
#include "MOFeatureRocks.h"
#include "MOWorldFeatureSubsystem.h"
#include "MOWorldSeedSubsystem.h"
#include "MOFramework.h"
#include "Buffer/VoxelDoubleBuffers.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Shape/VoxelShapeStamp.h"
#include "Surface/VoxelSurfaceTypeInterface.h"
#include "UObject/UObjectIterator.h"
#include "VoxelLayer.h"
#include "VoxelStampActor.h"
#include "VoxelStampRef.h"
#include "VoxelVolumeStamp.h"

FVoxelBox FMOFloatingIslandShape::GetLocalBounds() const
{
	return FVoxelBox(BoundsMin, BoundsMax);
}

void FMOFloatingIslandShape::Sample(TVoxelArrayView<float> OutDistances, const FVoxelDoubleVectorBuffer& Positions) const
{
	for (int32 I = 0; I < OutDistances.Num(); ++I)
	{
		if (!Params.IsValid())
		{
			OutDistances[I] = 1.0e6f;
			continue;
		}
		const FVector At = Positions[I];
		float D = FarThickenCm > 0.0f ? FMOWorldGen::IslandFarDistanceCm(*Params, At.X, At.Y, At.Z, Seed, 0.5f * FarThickenCm, FarThickenCm)
			: FMOWorldGen::IslandRockDistanceCm(*Params, At.X, At.Y, At.Z, Seed);
		if (TopSlabCm > 0.0f)
		{
			// The top slab: the rock within TopSlabCm of the highest clump's top over the column, standing proud (a lower clump under a higher one stays rock).
			const float Top = FMOWorldGen::IslandTopCm(*Params, At.X, At.Y, Seed);
			D = Top < -1.0e8f ? FMath::Max(D, 1.0f) : FMath::Max(D, (float)((double)Top - TopSlabCm - At.Z)) - FMOFloatingIsland::TopSlabProudCm;
		}
		if (D >= 1.0e5f)
		{
			// Past the island's reach: the distance to it, not "nothing here". The plugin's volume pass looks at a coarse chunk through a sparse grid (4 cells apart) and skips what those
			// samples say is far: a small island between them would never be looked at.
			const FVector Mid = 0.5 * (BoundsMin + BoundsMax);
			D = FMath::Max((float)(FVector2D::Distance(FVector2D(At), FVector2D(Mid)) - 0.5 * (BoundsMax.X - BoundsMin.X)), 1.0f);
		}
		OutDistances[I] = D;
	}
}

namespace
{
	const TCHAR* IslandRockSurface = TEXT("/Game/VoxelExamples/NaniteMaterials/Materials/Cliff/VST_Cliff.VST_Cliff");
	const TCHAR* IslandTopSurface = TEXT("/Game/VoxelExamples/NaniteMaterials/Materials/MossyGrass/VST_MI_MossyGrass.VST_MI_MossyGrass");

	TAutoConsoleVariable<int32> CVarIslandStampMaxLOD(TEXT("mo.WorldGen.IslandStampMaxLOD"), 32,
		TEXT("Dev: the highest LOD a floating island's stamps draw at (a volume stamp sends every chunk it touches through the plugin's volume pass). Read when an island is stamped."), ECVF_Default);
	// How far from a player an island is stamped: well past the farthest plants on it (the far canopy, 7.5 km out), and a big one further, or it pops into a clear sky at 15 km.
	constexpr double IslandStampReachCm = 1500000.0;
	constexpr double IslandStampReachPerRadius = 12.0;
	constexpr double IslandStampMaxReachCm = 4000000.0;
	constexpr double IslandStampDespawnFactor = 1.25;      // as the world features
	// A chunk's cells double per LOD (LOD = log2(distance / 16 m) / 1.1 with the level's voxel settings, a chunk's nearest edge deciding: cells of 32 m reach past 4 km, 64 m past 8 km, 128 m to 15 km). The
	// rock itself is drawn while the cells are fine against its thickness, its stand-in at each LOD past that, up to this one: an island whose reach runs past 32 km is thick enough to be its own rock there.
	constexpr int32 IslandFarMaxLOD = 9;
	constexpr int32 IslandTopMaxLOD = 6;                   // the grass top is 2.5 m of slab: nothing past this resolves it

	// The large rocks that make the smooth underside craggy (N-331; Wes, Nov 5: "just use large rocks to make it look more jagged and craggy").
	constexpr float IslandRockMinSizeCm = 700.0f;          // half a typical rock's width: a fifth of the clump's thickness, within these
	constexpr float IslandRockMaxSizeCm = 5000.0f;
	constexpr float IslandRockSpacing = 1.5f;              // apart, in those half widths: the rock shows between them
	constexpr int32 IslandRockMaxPerClump = 260;
	constexpr float IslandRockMinHalfHeightCm = 150.0f;    // smaller than this at a thin rim: leave it out

	TAutoConsoleVariable<int32> CVarIslandRocks(TEXT("mo.WorldGen.IslandRocks"), 1,
		TEXT("Dev: hang large rocks from a floating island's underside (0 = the voxel underside alone). Read when an island is stamped."), ECVF_Default);

	double IslandStampReach(const FMOFloatingIsland& I)
	{
		return FMath::Min(IslandStampMaxReachCm, IslandStampReachCm + IslandStampReachPerRadius * (double)I.RadiusCm);
	}

	// The islands of the cells a player has been near. A cell's island is a pure function of it, and the scan looks at every cell within 50 km twice a second.
	struct FIslandCells
	{
		TMap<FIntPoint, TOptional<FMOFloatingIsland>> Cells;
		uint32 ParamsHash = 0;
		int32 Seed = 0;

		const FMOFloatingIsland* IslandIn(const FMOWorldGenParams& Params, int64 Cx, int64 Cy, int32 InSeed)
		{
			if (ParamsHash != Params.GetHash() || Seed != InSeed)
			{
				Cells.Reset();
				ParamsHash = Params.GetHash();
				Seed = InSeed;
			}
			const FIntPoint Cell((int32)Cx, (int32)Cy);
			TOptional<FMOFloatingIsland>* Known = Cells.Find(Cell);
			if (!Known)
			{
				FMOFloatingIsland I;
				Known = &Cells.Add(Cell, FMOWorldGen::GetFloatingIslandInCell(Params, Cx, Cy, InSeed, I) ? TOptional<FMOFloatingIsland>(I) : TOptional<FMOFloatingIsland>());
			}
			return Known->IsSet() ? &Known->GetValue() : nullptr;
		}
	};

	/** The volume layer the terrain's own volume stamp (the cave stamp) is on, and its priority: a stamp there is dug by the volume sculpt. */
	UVoxelVolumeLayer* TerrainVolumeLayer(UWorld* World, const TMap<FIntPoint, TArray<TWeakObjectPtr<AActor>>>& Ours, int32& OutPriority)
	{
		OutPriority = 0;
		TSet<const AActor*> Skip;
		for (const TPair<FIntPoint, TArray<TWeakObjectPtr<AActor>>>& Pair : Ours)
		{
			for (const TWeakObjectPtr<AActor>& A : Pair.Value)
			{
				Skip.Add(A.Get());
			}
		}
		for (TActorIterator<AVoxelStampActor> It(World); It; ++It)
		{
			if (Skip.Contains(*It))
			{
				continue;
			}
			const FVoxelStampRef Stamp = It->GetStamp();
			const FVoxelVolumeStamp* Volume = Stamp.IsValid() ? Stamp.As<FVoxelVolumeStamp>() : nullptr;
			if (Volume && Volume->Layer)
			{
				OutPriority = Volume->Priority;
				return Volume->Layer;
			}
		}
		return LoadObject<UVoxelVolumeLayer>(nullptr, TEXT("/Voxel/Default/DefaultVolumeLayer.DefaultVolumeLayer"));
	}
}

void UMOWorldFeatureSubsystem::ClearIslandStamps()
{
	for (TPair<FIntPoint, TArray<TWeakObjectPtr<AActor>>>& Pair : IslandStamps)
	{
		for (const TWeakObjectPtr<AActor>& A : Pair.Value)
		{
			if (AActor* Actor = A.Get())
			{
				Actor->Destroy();
			}
		}
	}
	IslandStamps.Reset();
}

void UMOWorldFeatureSubsystem::UpdateIslandStamps()
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
	if (Seed != IslandStampsSeed || Origin != IslandStampsOrigin || T.IslandChance <= 0.0f)
	{
		ClearIslandStamps();   // another world: its islands stood elsewhere
		IslandStampsSeed = Seed;
		IslandStampsOrigin = Origin;
		if (T.IslandChance <= 0.0f)
		{
			return;
		}
	}
	// Viewers: as the compositions, every player this machine knows -- and a viewer with no pawn (the menu's backdrop, a player in the creator), whose camera the planting follows.
	TArray<FVector2D, TInlineAllocator<8>> Viewers;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (const APawn* Pawn = PC ? PC->GetPawn() : nullptr)
		{
			Viewers.Add(FVector2D(Pawn->GetActorLocation()));
		}
		else if (PC)
		{
			FVector Location;
			FRotator Rotation;
			PC->GetPlayerViewPoint(Location, Rotation);
			Viewers.Add(FVector2D(Location));
		}
	}
	if (Viewers.IsEmpty())
	{
		return;
	}
	auto NearestViewerDist = [&Viewers](const FVector2D& P)
	{
		double Best = TNumericLimits<double>::Max();
		for (const FVector2D& V : Viewers)
		{
			Best = FMath::Min(Best, FVector2D::Distance(V, P));
		}
		return Best;
	};
	// The nearest island not stamped yet, and any no player is near any more: one spawn an update (a stamp makes the chunks it touches build again).
	FMOFloatingIsland Nearest;
	FIntPoint NearestCell(0, 0);
	double NearestDist = TNumericLimits<double>::Max();
	TSet<FIntPoint> InReach;
	static FIslandCells KnownCells;
	const int64 R = (int64)FMath::CeilToDouble(IslandStampDespawnFactor * IslandStampMaxReachCm / T.IslandCellCm);
	for (const FVector2D& V : Viewers)
	{
		int64 Cx0 = 0, Cy0 = 0;
		FMOWorldGen::WorldToCell(*Params, V.X, V.Y, T.IslandCellCm, Cx0, Cy0);
		for (int64 Cy = Cy0 - R; Cy <= Cy0 + R; ++Cy)
		{
			for (int64 Cx = Cx0 - R; Cx <= Cx0 + R; ++Cx)
			{
				const FIntPoint Cell((int32)Cx, (int32)Cy);
				const FMOFloatingIsland* I = InReach.Contains(Cell) ? nullptr : KnownCells.IslandIn(*Params, Cx, Cy, Seed);
				if (!I)
				{
					continue;
				}
				const double Dist = NearestViewerDist(I->Center);
				const double StampReach = IslandStampReach(*I);
				if (Dist <= IslandStampDespawnFactor * StampReach)
				{
					InReach.Add(Cell);
				}
				if (Dist <= StampReach && Dist < NearestDist && !IslandStamps.Contains(Cell))
				{
					Nearest = *I;
					NearestCell = Cell;
					NearestDist = Dist;
				}
			}
		}
	}
	for (auto It = IslandStamps.CreateIterator(); It; ++It)
	{
		if (!InReach.Contains(It->Key))
		{
			for (const TWeakObjectPtr<AActor>& A : It->Value)
			{
				if (AActor* Actor = A.Get())
				{
					Actor->Destroy();
				}
			}
			It.RemoveCurrent();
		}
	}
	if (NearestDist == TNumericLimits<double>::Max())
	{
		return;
	}

	// Its bounds: the cluster's reach (1.3 x its radius: a clump's warped, lobed outline stays inside it), and from the top's bump (up to 300 + 0.03 R) over the highest clump down to the
	// underside (the clump's thickness, and its folds) under the lowest.
	const FMOFloatingIsland& I = Nearest;
	float MaxThick = 0.0f, MinLift = 0.0f, MaxLift = 0.0f;
	for (int32 K = 0; K < I.NumClumps; ++K)
	{
		MaxThick = FMath::Max(MaxThick, I.ClumpThicknessCm[K]);
		MinLift = FMath::Min(MinLift, I.ClumpLiftCm[K]);
		MaxLift = FMath::Max(MaxLift, I.ClumpLiftCm[K]);
	}
	const double Reach = 1.3 * I.RadiusCm + 2000.0;
	FMOFloatingIslandShape Rock;
	Rock.Params = Params;
	Rock.Seed = Seed;
	Rock.BoundsMin = FVector(I.Center.X - Reach, I.Center.Y - Reach, I.TopCm + MinLift - 1.4 * MaxThick - 1500.0);
	Rock.BoundsMax = FVector(I.Center.X + Reach, I.Center.Y + Reach, I.TopCm + MaxLift + 0.03 * I.RadiusCm + 1500.0);
	FMOFloatingIslandShape Grass = Rock;
	Grass.TopSlabCm = FMOFloatingIsland::TopSlabDepthCm;

	// The highest LOD the rock itself can be drawn at: three of its cells to the main clump's thickness (at 4 km, 32 m cells against 40-57 m of rock showed a sliver under the island's trees, Nov 4).
	// Past it each LOD gets the stand-in, thickened by a cell of that LOD.
	const int32 MaxLOD = CVarIslandStampMaxLOD.GetValueOnGameThread();
	int32 RockMaxLOD = 0;
	while (RockMaxLOD < MaxLOD && T.VoxelSizeCm * (float)(1 << (RockMaxLOD + 1)) <= I.ClumpThicknessCm[0] / 3.0f)
	{
		++RockMaxLOD;
	}
	struct FPiece
	{
		FMOFloatingIslandShape Shape;
		const TCHAR* Surface = nullptr;
		FInt32Interval LODs;
	};
	TArray<FPiece, TInlineAllocator<8>> Pieces;
	Pieces.Add({ Rock, IslandRockSurface, FInt32Interval(0, RockMaxLOD) });
	Pieces.Add({ Grass, IslandTopSurface, FInt32Interval(0, FMath::Min(RockMaxLOD, IslandTopMaxLOD)) });
	for (int32 LOD = RockMaxLOD + 1; LOD <= FMath::Min(MaxLOD, IslandFarMaxLOD); ++LOD)
	{
		FMOFloatingIslandShape Far = Rock;
		Far.FarThickenCm = T.VoxelSizeCm * (float)(1 << LOD);
		Far.BoundsMin.Z -= Far.FarThickenCm;
		Pieces.Add({ Far, IslandRockSurface, FInt32Interval(LOD, LOD) });
	}

	int32 BasePriority = 0;
	UVoxelVolumeLayer* Layer = TerrainVolumeLayer(World, IslandStamps, BasePriority);
	TArray<TWeakObjectPtr<AActor>>& Actors = IslandStamps.Add(NearestCell);
	for (const FPiece& Piece : Pieces)
	{
		FVoxelShapeStamp S;
		S.Shape = TVoxelInstancedStruct<FVoxelShape>(Piece.Shape);
		S.SurfaceType = LoadObject<UVoxelSurfaceTypeInterface>(nullptr, Piece.Surface);
		S.Layer = Layer;
		S.BlendMode = EVoxelVolumeBlendMode::Additive;
		S.Smoothness = 0.0f;
		S.Priority = BasePriority + (Piece.Shape.TopSlabCm > 0.0f ? 2 : 1);
		S.LODRange = Piece.LODs;
		FActorSpawnParameters SP;
		SP.ObjectFlags |= RF_Transient;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AVoxelStampActor* A = World->SpawnActor<AVoxelStampActor>(AVoxelStampActor::StaticClass(), FTransform::Identity, SP))
		{
			A->SetStamp(S);
			Actors.Add(A);
		}
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MOFloatingIsland] stamped the island at (%.0f, %.0f): radius %.0f, top %.0f over ground %.0f, %d stamps (its rock to LOD %d), %.0f m from the nearest player; %d stamped now"),
		I.Center.X, I.Center.Y, I.RadiusCm, I.TopCm, I.GroundCm, Actors.Num(), RockMaxLOD, NearestDist / 100.0, IslandStamps.Num());
	if (AActor* Rocks = SpawnIslandRocks(I, *Params, Seed))
	{
		Actors.Add(Rocks);
	}
}

AActor* UMOWorldFeatureSubsystem::SpawnIslandRocks(const FMOFloatingIsland& I, const FMOWorldGenParams& Params, int32 Seed)
{
	UWorld* World = GetWorld();
	if (!World || CVarIslandRocks.GetValueOnGameThread() == 0)
	{
		return nullptr;
	}
	TArray<UStaticMesh*> Meshes;
	TArray<MOFeatureRocks::FMeshSize> Sizes;
	MOFeatureRocks::LoadMeshes(Meshes, Sizes);
	if (Meshes.IsEmpty())
	{
		return nullptr;
	}
	TArray<TArray<FTransform>> ByMesh;
	ByMesh.SetNum(Meshes.Num());

	// From the seed and the island alone: every machine hangs the same rocks.
	FRandomStream Rng((int32)HashCombine(GetTypeHash(Seed), GetTypeHash(FIntPoint(FMath::RoundToInt32(I.Center.X * 0.01), FMath::RoundToInt32(I.Center.Y * 0.01)))));
	int32 Placed = 0;
	for (int32 K = 0; K < I.NumClumps; ++K)
	{
		const float R = I.ClumpRadiusCm[K];
		const float Size = FMath::Clamp(0.2f * I.ClumpThicknessCm[K], IslandRockMinSizeCm, IslandRockMaxSizeCm);
		const int32 Tries = FMath::Clamp(FMath::CeilToInt32(UE_PI * FMath::Square(1.2f * R) / FMath::Square(IslandRockSpacing * Size)), 12, IslandRockMaxPerClump);
		for (int32 N = 0; N < Tries; ++N)
		{
			const float Angle = Rng.FRandRange(0.0f, 2.0f * UE_PI);
			const float Out = 1.2f * R * FMath::Sqrt(Rng.FRand());
			const double X = I.ClumpCenter[K].X + Out * FMath::Cos(Angle), Y = I.ClumpCenter[K].Y + Out * FMath::Sin(Angle);
			const int32 MeshIndex = Rng.RandRange(0, Meshes.Num() - 1);
			const float Grow = FMath::Lerp(0.5f, 1.4f, FMath::Pow(Rng.FRand(), 2.0f));   // most modest, a few big
			const float Stretch = Rng.FRandRange(1.1f, 1.8f);                            // hanging, taller than wide
			const FRotator Rotation(Rng.FRandRange(-20.0f, 20.0f), Rng.FRandRange(0.0f, 360.0f), Rng.FRandRange(-20.0f, 20.0f));
			const float Under = FMOWorldGen::IslandUndersideCm(Params, X, Y, Seed);
			const float Top = FMOWorldGen::IslandTopCm(Params, X, Y, Seed);
			if (Under < -1.0e8f || Top < -1.0e8f)
			{
				continue;   // past the outline here
			}
			// A rock is set into the underside about a third of its height, and must end inside the rock's thickness there: at the rim it is smaller, or it would come up through the grass.
			const float HalfHeight = FMath::Min(Size * Grow * Stretch, FMath::Max(0.0f, (Top - Under - 50.0f) / 1.3f));
			if (HalfHeight < IslandRockMinHalfHeightCm)
			{
				continue;
			}
			ByMesh[MeshIndex].Add(MOFeatureRocks::MakeInstance(Sizes[MeshIndex], FVector(X, Y, (double)Under + 0.3 * HalfHeight), Rotation, HalfHeight / Stretch, HalfHeight));
			++Placed;
		}
	}
	UE_LOG(LogMOFramework, Log, TEXT("[MOFloatingIsland] %d rocks under the island at (%.0f, %.0f)"), Placed, I.Center.X, I.Center.Y);
	return MOFeatureRocks::SpawnHolder(World, Meshes, ByMesh);
}

// ---------- MO.WorldGen.IslandPlants: what stands on the islands, and how it meets the rock ----------
namespace
{
	/** The voxel ground under (X, Y) between two heights, through whatever plants stand there. */
	bool RockUnder(UWorld* World, double X, double Y, double FromZ, double ToZ, double& OutZ)
	{
		FCollisionQueryParams Query(SCENE_QUERY_STAT(MOIslandPlantsProbe), /*bTraceComplex=*/false);
		for (int32 Tries = 0; Tries < 24; ++Tries)
		{
			FHitResult Hit;
			if (!World->LineTraceSingleByChannel(Hit, FVector(X, Y, FromZ), FVector(X, Y, ToZ), ECC_Visibility, Query))
			{
				return false;
			}
			if (!Cast<UInstancedStaticMeshComponent>(Hit.GetComponent()))
			{
				OutZ = Hit.ImpactPoint.Z;
				return true;
			}
			Query.AddIgnoredComponent(Hit.GetComponent());   // a trunk or a stone: through it
		}
		return false;
	}

	void IslandPlantsCommand(const TArray<FString>& Args, UWorld* World)
	{
		const APawn* Pawn = World && World->GetFirstPlayerController() ? World->GetFirstPlayerController()->GetPawn() : nullptr;
		const UMOWorldSeedSubsystem* Seeds = World ? UMOWorldSeedSubsystem::Get(World) : nullptr;
		if (!Pawn || !Seeds || !Seeds->HasActiveSeed() || !FMOWorldGenParamsProvider::HasPublished())
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] ISLANDPLANTS FAILED: needs a local pawn and an active world seed"));
			return;
		}
		const FMOWorldGenParamsRef Params = FMOWorldGenParamsProvider::Get();
		const FMOWorldGenParams& P = *Params;
		const int32 Seed = Seeds->GetActiveTerrainSeed();
		const double Radius = Args.Num() > 0 ? FCString::Atod(*Args[0]) : IslandStampReachCm;
		const FVector C = Pawn->GetActorLocation();
		const double Cell = P.Tuning.IslandCellCm;
		int64 Cx0 = 0, Cy0 = 0;
		FMOWorldGen::WorldToCell(P, C.X, C.Y, Cell, Cx0, Cy0);
		const int64 R = (int64)FMath::CeilToDouble(Radius / Cell);
		struct FIsle { FMOFloatingIsland I; TMap<FName, int32> ByTag; TMap<FString, int32> ByMesh; int32 Total = 0, Hanging = 0; };
		TArray<FIsle> Isles;
		for (int64 Cy = Cy0 - R; Cy <= Cy0 + R; ++Cy)
		{
			for (int64 Cx = Cx0 - R; Cx <= Cx0 + R; ++Cx)
			{
				FMOFloatingIsland I;
				if (FMOWorldGen::GetFloatingIslandInCell(P, Cx, Cy, Seed, I) && FVector2D::Distance(I.Center, FVector2D(C.X, C.Y)) <= Radius)
				{
					Isles.AddDefaulted_GetRef().I = I;
				}
			}
		}
		constexpr double NearPawnCm = 6000.0, OverTopCm = 1500.0, UnderTopCm = 20000.0;   // under the top: a cliff scan's pivot can be deep in the rock, and the land is 300 m down at the least
		TArray<double> Gaps;
		int32 Floating = 0, NoHit = 0;
		for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
		{
			const UInstancedStaticMeshComponent* ISM = *It;
			if (!IsValid(ISM) || ISM->GetWorld() != World || !ISM->IsRegistered() || ISM->GetInstanceCount() == 0 || !ISM->GetStaticMesh())
			{
				continue;
			}
			const FBox Box = ISM->Bounds.GetBox();
			FName Tag = NAME_None;
			for (const FName& Candidate : ISM->ComponentTags)
			{
				const FString Text = Candidate.ToString();
				if (Text.StartsWith(TEXT("MOResource_")) || Text.StartsWith(TEXT("MOBiomeCover_")) || Text.StartsWith(TEXT("MOFar_")) || Text == TEXT("MOLandmark"))
				{
					Tag = Candidate;
					break;
				}
			}
			const FString MeshName = ISM->GetStaticMesh()->GetName();
			const FBox MeshBox = ISM->GetStaticMesh()->GetBoundingBox();
			const bool bFar = Tag.ToString().StartsWith(TEXT("MOFar_"));
			const bool bRock = Tag == FName(TEXT("MOLandmark")) || Tag == FName(TEXT("MOResource_Rock")) || Tag == FName(TEXT("MOBiomeCover_Rock"));
			for (FIsle& Isle : Isles)
			{
				const double Reach = 1.3 * Isle.I.RadiusCm + 200.0;
				const FBox Column(FVector(Isle.I.Center.X - Reach, Isle.I.Center.Y - Reach, Isle.I.TopCm - 20000.0), FVector(Isle.I.Center.X + Reach, Isle.I.Center.Y + Reach, Isle.I.TopCm + 20000.0));
				if (!Box.Intersect(Column))
				{
					continue;
				}
				const int32 N = ISM->GetInstanceCount();
				for (int32 K = 0; K < N; ++K)
				{
					FTransform Xf;
					if (!ISM->GetInstanceTransform(K, Xf, /*bWorldSpace=*/true))
					{
						continue;
					}
					const FVector L = Xf.GetLocation();
					if (!Column.IsInsideXY(L) || L.Z < Column.Min.Z || L.Z > Column.Max.Z)
					{
						continue;
					}
					const float Top = FMOWorldGen::IslandTopCm(P, L.X, L.Y, Seed);
					if (Top < -1.0e8f || L.Z > (double)Top + OverTopCm || L.Z < (double)Top - UnderTopCm)
					{
						continue;
					}
					++Isle.Total;
					++Isle.ByTag.FindOrAdd(Tag);
					++Isle.ByMesh.FindOrAdd(MeshName);
					if (bRock)
					{
						const FVector Mid = Xf.TransformPosition(FVector(0.5 * (MeshBox.Min.X + MeshBox.Max.X), 0.5 * (MeshBox.Min.Y + MeshBox.Max.Y), MeshBox.Min.Z));
						Isle.Hanging += FMOWorldGen::IslandRockDistanceCm(P, Mid.X, Mid.Y, Mid.Z, Seed) > 100.0f ? 1 : 0;
					}
					if (!bFar && FVector2D::Distance(FVector2D(L), FVector2D(C)) < NearPawnCm)
					{
						double Rock = 0.0;
						if (RockUnder(World, L.X, L.Y, L.Z + 800.0, L.Z - 1500.0, Rock))
						{
							const double Gap = L.Z + MeshBox.Min.Z * Xf.GetScale3D().Z - Rock;
							Gaps.Add(Gap);
							Floating += Gap > 60.0 ? 1 : 0;
						}
						else
						{
							++NoHit;
						}
					}
				}
			}
		}
		for (const FIsle& Isle : Isles)
		{
			TArray<FString> Tags;
			for (const TPair<FName, int32>& Pair : Isle.ByTag)
			{
				Tags.Add(FString::Printf(TEXT("%s=%d"), *Pair.Key.ToString(), Pair.Value));
			}
			Tags.Sort();
			TArray<TPair<FString, int32>> Meshes = Isle.ByMesh.Array();
			Meshes.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B) { return A.Value > B.Value; });
			TArray<FString> Most;
			for (int32 K = 0; K < FMath::Min(6, Meshes.Num()); ++K)
			{
				Most.Add(FString::Printf(TEXT("%s=%d"), *Meshes[K].Key, Meshes[K].Value));
			}
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] ISLANDPLANTS at (%.0f, %.0f) radius=%.0f top=%.0f ground=%.0f: %d instances, %d meshes, %d rocks hanging | %s | most: %s"), Isle.I.Center.X, Isle.I.Center.Y,
				Isle.I.RadiusCm, Isle.I.TopCm, Isle.I.GroundCm, Isle.Total, Meshes.Num(), Isle.Hanging, *FString::Join(Tags, TEXT(" ")), *FString::Join(Most, TEXT(" ")));
		}
		// The generator's top against the rock itself, on a 2 m lattice round the pawn: the plants are put by the one and stand on the other.
		TArray<double> Errors;
		int32 Plantable = 0, RockMissing = 0;
		for (double Dy = -NearPawnCm; Dy <= NearPawnCm; Dy += 200.0)
		{
			for (double Dx = -NearPawnCm; Dx <= NearPawnCm; Dx += 200.0)
			{
				FMOIslandSurfacePoint Surface;
				if (Dx * Dx + Dy * Dy > NearPawnCm * NearPawnCm || !FMOWorldGen::SampleIslandSurface(P, C.X + Dx, C.Y + Dy, Seed, 300.0f, Surface))
				{
					continue;
				}
				++Plantable;
				double Rock = 0.0;
				const double Ground = (double)Surface.TopCm + FMOFloatingIsland::TopSlabProudCm;
				if (RockUnder(World, C.X + Dx, C.Y + Dy, Ground + 800.0, Ground - 1500.0, Rock))
				{
					Errors.Add(Rock - Ground);
				}
				else
				{
					++RockMissing;
				}
			}
		}
		const auto Summary = [](TArray<double>& V)
		{
			if (V.IsEmpty())
			{
				return FString(TEXT("none"));
			}
			V.Sort();
			return FString::Printf(TEXT("n=%d min=%.0f p05=%.0f median=%.0f p95=%.0f max=%.0f"), V.Num(), V[0], V[V.Num() / 20], V[V.Num() / 2], V[V.Num() * 19 / 20], V.Last());
		};
		const FString GapText = Summary(Gaps);
		const FString ErrorText = Summary(Errors);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] ISLANDPLANTS near the pawn (%.0f, %.0f, %.0f): plant base over rock (cm) %s; floating over 60 cm %d; no rock under %d | generator ground vs rock (cm) %s of %d plantable; no rock %d"),
			C.X, C.Y, C.Z, *GapText, Floating, NoHit, *ErrorText, Plantable, RockMissing);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] ISLANDPLANTS done: %d island(s)"), Isles.Num());
	}

	FAutoConsoleCommandWithWorldAndArgs GMOIslandPlantsCmd(TEXT("MO.WorldGen.IslandPlants"),
		TEXT("Dev: what the biome spawner planted on each floating island near the local pawn, by tag and mesh, and how it meets the rock round the pawn. Usage: MO.WorldGen.IslandPlants [radius_cm=1500000]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&IslandPlantsCommand));
}
