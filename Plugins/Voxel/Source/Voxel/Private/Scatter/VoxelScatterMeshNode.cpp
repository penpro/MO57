// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#include "Scatter/VoxelScatterMeshNode.h"
#include "Scatter/VoxelScatterFunctionLibrary.h"
#include "VoxelQuery.h"
#include "VoxelConfig.h"
#include "VoxelRuntime.h"
#include "VoxelSubsystem.h"
#include "Buffer/VoxelFloatBuffers.h"
#include "Buffer/VoxelGraphStaticMeshBuffer.h"
#include "Graphs/VoxelStampGraphParameters.h"
#include "Components/InstancedStaticMeshComponent.h"

VOXEL_CONSOLE_VARIABLE(
	VOXEL_API, int32, GVoxelScatterMeshNumChunks, 3,
	"voxel.scatter.mesh.NumChunks",
	"Number of chunks to target in each direction",
	Voxel::RefreshAll);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelScatterNodeRuntime> FVoxelNode_ScatterMesh::MakeRuntime() const
{
	return MakeShared<FVoxelScatterMeshNodeRuntime>();
}

void FVoxelScatterMeshNodeRuntime::Initialize(FVoxelGraphQueryImpl& Query)
{
	VOXEL_FUNCTION_COUNTER();

	RenderDistance = GetEvaluator()->RenderDistancePin.GetSynchronous(Query) * 100.f;
	ChunkSize = FMath::CeilToInt(RenderDistance / FMath::Max(GVoxelScatterMeshNumChunks, 1));
	RenderDistanceInChunks = FMath::CeilToInt(RenderDistance / ChunkSize);
}

void FVoxelScatterMeshNodeRuntime::Compute(const FVoxelSubsystem& Subsystem)
{
	VOXEL_FUNCTION_COUNTER();

	ensure(ChunksToRender.Num() == 0);
	ensure(ChunksToDestroy.Num() == 0);

	if (!Subsystem.GetConfig().CameraPosition.IsSet())
	{
		return;
	}

	const FVector CameraPosition = Subsystem.GetConfig().CameraPosition.GetValue();
	const FIntVector CameraChunkPosition = FVoxelUtilities::RoundToInt(CameraPosition / ChunkSize);

	const FIntVector Min = CameraChunkPosition - RenderDistanceInChunks;
	const FIntVector Max = CameraChunkPosition + RenderDistanceInChunks;

	TVoxelArray<FIntVector> NewChunkKeys;
	{
		VOXEL_SCOPE_COUNTER("Find NewChunkKeys");

		NewChunkKeys.Reserve(FVoxelIntBox(Min, Max + 1).Count_int32());

		for (int32 Z = Min.Z; Z <= Max.Z; Z++)
		{
			for (int32 Y = Min.Y; Y <= Max.Y; Y++)
			{
				for (int32 X = Min.X; X <= Max.X; X++)
				{
					const FIntVector ChunkKey = FIntVector(X, Y, Z);

					if (FVector::Distance(FVector(ChunkKey) * ChunkSize, CameraPosition) > RenderDistance)
					{
						continue;
					}

					NewChunkKeys.Add_EnsureNoGrow(ChunkKey);
				}
			}
		}
	}

	{
		VOXEL_SCOPE_COUNTER("Build ChunkKeyToChunk");

		TVoxelMap<FIntVector, TSharedPtr<FChunk>> OldChunkKeyToChunk = MoveTemp(ChunkKeyToChunk);

		ChunkKeyToChunk.Reserve(NewChunkKeys.Num());

		for (const FIntVector& ChunkKey : NewChunkKeys)
		{
			TSharedPtr<FChunk> Chunk;
			OldChunkKeyToChunk.RemoveAndCopyValue(ChunkKey, Chunk);

			if (Chunk &&
				Chunk->DependencyTracker->IsInvalidated())
			{
				ChunksToDestroy.Add(Chunk);
				Chunk.Reset();
			}

			if (!Chunk)
			{
				Chunk = MakeShared<FChunk>(ChunkKey);
				ChunksToRender.Add(Chunk);
			}

			ChunkKeyToChunk.Add_EnsureNew(ChunkKey, Chunk);
		}

		for (const auto& It : OldChunkKeyToChunk)
		{
			ChunksToDestroy.Add(It.Value);
		}
	}

	for (const TSharedPtr<FChunk>& Chunk : ChunksToRender)
	{
		Voxel::AsyncTask(MakeWeakPtrLambda(this, [this, &Subsystem, Chunk]
		{
			ComputeChunk(Subsystem, *Chunk);
		}));
	}
}

void FVoxelScatterMeshNodeRuntime::Render(FVoxelRuntime& Runtime)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	for (const TSharedPtr<FChunk>& Chunk : ChunksToRender)
	{
		RenderChunk(Runtime, *Chunk);
	}
	ChunksToRender.Empty();

	for (const TSharedPtr<FChunk>& Chunk : ChunksToDestroy)
	{
		DestroyChunk(Runtime, *Chunk);
	}
	ChunksToDestroy.Empty();
}

void FVoxelScatterMeshNodeRuntime::Destroy(FVoxelRuntime& Runtime)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	ensure(ChunksToRender.Num() == 0);
	ensure(ChunksToDestroy.Num() == 0);

	for (const auto& It : ChunkKeyToChunk)
	{
		DestroyChunk(Runtime, *It.Value);
	}
	ChunkKeyToChunk.Empty();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelScatterMeshNodeRuntime::ComputeChunk(
	const FVoxelSubsystem& Subsystem,
	FChunk& Chunk) const
{
	VOXEL_FUNCTION_COUNTER();

	FVoxelDependencyCollector DependencyCollector(STATIC_FNAME("FVoxelScatterMeshNodeRuntime Chunk"));

	ON_SCOPE_EXIT
	{
		Chunk.DependencyTracker = Subsystem.Finalize(DependencyCollector);
	};

	FVoxelGraphContext Context = GetEvaluator().MakeContext(DependencyCollector);
	FVoxelGraphQueryImpl& Query = Context.MakeQuery();

	FVoxelQuery VoxelQuery(
		0,
		Subsystem.GetLayers(),
		Subsystem.GetSurfaceTypeTable(),
		DependencyCollector);

	Query.AddParameter<FVoxelGraphParameters::FQuery>(VoxelQuery);

	Query.AddParameter<FVoxelGraphParameters::FScatterBounds>().Bounds = FVoxelBox(
		ChunkSize * Chunk.ChunkKey,
		ChunkSize * (Chunk.ChunkKey + 1));

	const TSharedRef<const FVoxelPointSet> Points = GetEvaluator()->InPin.GetSynchronous(Query);
	if (Points->Num() == 0)
	{
		return;
	}

	const FVoxelGraphStaticMeshBuffer* MeshBuffer = Points->Find<FVoxelGraphStaticMeshBuffer>(FVoxelPointAttributes::Mesh);
	const FVoxelDoubleVectorBuffer* PositionBuffer = Points->Find<FVoxelDoubleVectorBuffer>(FVoxelPointAttributes::Position);
	const FVoxelQuaternionBuffer* RotationBuffer = Points->Find<FVoxelQuaternionBuffer>(FVoxelPointAttributes::Rotation);
	const FVoxelVectorBuffer* ScaleBuffer = Points->Find<FVoxelVectorBuffer>(FVoxelPointAttributes::Scale);
	if (!MeshBuffer ||
		!PositionBuffer)
	{
		return;
	}

	VOXEL_SCOPE_COUNTER("Build render data");

	for (int32 Index = 0; Index < Points->Num(); Index++)
	{
		const TVoxelObjectPtr<UStaticMesh> Mesh = (*MeshBuffer)[Index].StaticMesh;

		FRenderData* RenderData = Chunk.MeshToRenderData.FindSmartPtr(Mesh);
		if (!RenderData)
		{
			const TSharedRef<FRenderData> NewRenderData = MakeShared<FRenderData>();
			NewRenderData->Transforms.Reserve(Points->Num());
			Chunk.MeshToRenderData.Add_EnsureNew(Mesh, NewRenderData);
			RenderData = &NewRenderData.Get();
		}

		const FVector Position = (*PositionBuffer)[Index];
		const FQuat4f Rotation = RotationBuffer ? (*RotationBuffer)[Index] : FQuat4f::Identity;
		const FVector3f Scale = ScaleBuffer ? (*ScaleBuffer)[Index] : FVector3f::OneVector;

		if (Position.ContainsNaN() ||
			Rotation.ContainsNaN() ||
			Scale.ContainsNaN())
		{
			continue;
		}

		RenderData->Transforms.Add_EnsureNoGrow(FTransform(
			FQuat(Rotation),
			Position,
			FVector(Scale)));
	}
}

void FVoxelScatterMeshNodeRuntime::RenderChunk(
	FVoxelRuntime& Runtime,
	FChunk& Chunk)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	ensure(Chunk.MeshToComponent.Num() == 0);
	Chunk.MeshToComponent.Reserve(Chunk.MeshToRenderData.Num());

	for (const auto& It : Chunk.MeshToRenderData)
	{
		UInstancedStaticMeshComponent* Component = Runtime.NewComponent<UInstancedStaticMeshComponent>();
		if (!ensureVoxelSlow(Component))
		{
			continue;
		}

		Component->bHasPerInstanceHitProxies = true;
		Component->bDisableCollision = true;
		Component->SetStaticMesh(It.Key.Resolve_Ensured());
		Component->AddInstances(It.Value->Transforms, false);

		Chunk.MeshToComponent.Add_EnsureNew(It.Key, Component);
	}
	Chunk.MeshToRenderData.Empty();
}

void FVoxelScatterMeshNodeRuntime::DestroyChunk(
	FVoxelRuntime& Runtime,
	FChunk& Chunk)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	for (const auto& It : Chunk.MeshToComponent)
	{
		UInstancedStaticMeshComponent* Component = It.Value.Resolve();
		if (!ensureVoxelSlow(Component))
		{
			continue;
		}

		Component->SetStaticMesh(nullptr);
		Component->ClearInstances();

		Runtime.RemoveComponent(Component);
	}
}