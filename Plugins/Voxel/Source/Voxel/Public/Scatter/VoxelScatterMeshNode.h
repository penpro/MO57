// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Scatter/VoxelNode_ScatterBase.h"
#include "Scatter/VoxelScatterNodeRuntime.h"
#include "VoxelScatterMeshNode.generated.h"

struct FVoxelSubsystem;

USTRUCT()
struct VOXEL_API FVoxelNode_ScatterMesh : public FVoxelNode_ScatterBase
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

public:
	// In meters
	VOXEL_INPUT_PIN(float, RenderDistance, 64);

public:
	//~ Begin FVoxelNode_ScatterBase Interface
	virtual TSharedRef<FVoxelScatterNodeRuntime> MakeRuntime() const override;
	//~ End FVoxelNode_ScatterBase Interface
};

class VOXEL_API FVoxelScatterMeshNodeRuntime : public TVoxelScatterNodeRuntime<FVoxelNode_ScatterMesh>
{
public:
	//~ Begin FVoxelScatterNodeRuntime Interface
	virtual void Initialize(FVoxelGraphQueryImpl& Query) override;
	virtual void Compute(const FVoxelSubsystem& Subsystem) override;
	virtual void Render(FVoxelRuntime& Runtime) override;
	virtual void Destroy(FVoxelRuntime& Runtime) override;
	//~ End FVoxelScatterNodeRuntime Interface

private:
	float RenderDistance = 0;
	int32 ChunkSize = 0;
	int32 RenderDistanceInChunks = 0;

private:
	struct FRenderData
	{
		TVoxelArray<FTransform> Transforms;
	};
	struct FChunk
	{
		const FIntVector ChunkKey;
		TSharedPtr<FVoxelDependencyTracker> DependencyTracker;
		TVoxelMap<TVoxelObjectPtr<UStaticMesh>, TSharedPtr<FRenderData>> MeshToRenderData;
		TVoxelMap<TVoxelObjectPtr<UStaticMesh>, TVoxelObjectPtr<UInstancedStaticMeshComponent>> MeshToComponent;

		explicit FChunk(const FIntVector& ChunkKey)
			: ChunkKey(ChunkKey)
		{
		}
	};
	TVoxelMap<FIntVector, TSharedPtr<FChunk>> ChunkKeyToChunk;

	TVoxelChunkedArray<TSharedPtr<FChunk>> ChunksToRender;
	TVoxelChunkedArray<TSharedPtr<FChunk>> ChunksToDestroy;

	void ComputeChunk(
		const FVoxelSubsystem& Subsystem,
		FChunk& Chunk) const;

	static void RenderChunk(
		FVoxelRuntime& Runtime,
		FChunk& Chunk);

	static void DestroyChunk(
		FVoxelRuntime& Runtime,
		FChunk& Chunk);
};