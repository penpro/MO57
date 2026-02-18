// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelChunkKey.h"

class FVoxelMesh;
class FVoxelCollider;
class FVoxelNaniteMesh;
class FVoxelMeshRenderProxy;
class UVoxelMeshComponent;
class UVoxelNaniteComponent;
class UVoxelCollisionComponent;
class UVoxelStaticMeshCollisionComponent;

// TODO UniquePtr
// TODO Ensure is destroyed in destructor
struct FVoxelRenderChunkData
{
public:
	const FVoxelChunkKey ChunkKey;
	const FVoxelChunkNeighborInfo NeighborInfo;

	FVoxelRenderChunkData(
		const FVoxelChunkKey& ChunkKey,
		const FVoxelChunkNeighborInfo& NeighborInfo)
		: ChunkKey(ChunkKey)
		, NeighborInfo(NeighborInfo)
	{
	}

public:
	TSharedPtr<FVoxelCollider> Collider;
	TSharedPtr<FVoxelNaniteMesh> NaniteMesh;
	TSharedPtr<FVoxelMeshRenderProxy> RenderProxy;

	TVoxelObjectPtr<UVoxelMeshComponent> MeshComponent;
	TVoxelObjectPtr<UVoxelNaniteComponent> NaniteComponent;
	TVoxelObjectPtr<UVoxelCollisionComponent> CollisionComponent;
	TVoxelObjectPtr<UVoxelStaticMeshCollisionComponent> StaticMeshCollisionComponent;

	mutable TSharedPtr<FVoxelMaterialRef> MeshComponentMaterial;
};

class VOXEL_API FVoxelRenderChunk
{
public:
	const FVoxelChunkKey ChunkKey;

	explicit FVoxelRenderChunk(const FVoxelChunkKey& ChunkKey)
		: ChunkKey(ChunkKey)
	{
	}

public:
	// TODO Inline allocations
	TVoxelArray<TSharedPtr<FVoxelRenderChunk>> Children;
	TVoxelArray<TSharedPtr<FVoxelRenderChunk>> ChildrenToDestroy;

	TVoxelOptional<TSharedPtr<FVoxelMesh>> Mesh;
	TSharedPtr<FVoxelDependencyTracker> MeshDependencyTracker;
	bool bMeshInvalidated = false;

	TSharedPtr<FVoxelRenderChunkData> RenderData;
};