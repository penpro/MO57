// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

struct FVoxelConfig;
struct FVoxelRenderChunkData;
struct FVoxelRenderSubsystem;
class FVoxelRuntime;
class UVoxelMeshComponent;
class UVoxelNaniteComponent;
class UVoxelCollisionComponent;
class UVoxelStaticMeshCollisionComponent;

class FVoxelRenderComponentCreator
{
public:
	FVoxelRuntime& Runtime;
	const FVoxelConfig& Config;
	const FVoxelRenderSubsystem& Subsystem;

	FVoxelRenderComponentCreator(
		FVoxelRuntime& Runtime,
		const FVoxelRenderSubsystem& Subsystem);

public:
	void ProcessRenderDatasToDestroy(const TVoxelChunkedArray<TSharedPtr<FVoxelRenderChunkData>>& RenderDatasToDestroy);
	void ProcessRenderDatasToRender(const TVoxelChunkedArray<TSharedPtr<FVoxelRenderChunkData>>& RenderDatasToRender);
	void DestroyUnusedComponents();

private:
	TVoxelChunkedArray<UVoxelMeshComponent*> MeshComponents;
	TVoxelChunkedArray<UVoxelNaniteComponent*> NaniteComponents;
	TVoxelChunkedArray<UVoxelCollisionComponent*> CollisionComponents;
	TVoxelChunkedArray<UVoxelStaticMeshCollisionComponent*> StaticMeshCollisionComponents;

private:
	struct FCollisionTask
	{
		UVoxelCollisionComponent* Component = nullptr;
		const FVoxelRenderChunkData* RenderData = nullptr;
	};
	struct FStaticMeshCollisionTask
	{
		UVoxelStaticMeshCollisionComponent* Component = nullptr;
		const FVoxelRenderChunkData* RenderData = nullptr;
	};
	struct FNaniteTask
	{
		UVoxelNaniteComponent* Component = nullptr;
		const FVoxelRenderChunkData* RenderData = nullptr;
	};
	struct FMeshTask
	{
		UVoxelMeshComponent* Component = nullptr;
		const FVoxelRenderChunkData* RenderData = nullptr;
	};

	TVoxelChunkedArray<FCollisionTask> CollisionTasks;
	TVoxelChunkedArray<FStaticMeshCollisionTask> StaticMeshCollisionTasks;
	TVoxelChunkedArray<FNaniteTask> NaniteTasks;
	TVoxelChunkedArray<FMeshTask> MeshTasks;

	void ProcessCollisionTasks();
	void ProcessStaticMeshCollisionTasks();
	void ProcessNaniteTasks();
	void ProcessMeshTasks();

private:
	FTransform GetComponentTransform(const FVoxelRenderChunkData& RenderData) const;
};