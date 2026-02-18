// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelMetadataRef.h"

class FVoxelMegaMaterialProxy;
class FVoxelTexturePool;

class FVoxelTextureManager
{
public:
	const TVoxelArray<FVoxelMetadataRef> MetadataIndexToMetadata;

	explicit FVoxelTextureManager(const FVoxelMegaMaterialProxy& MegaMaterialProxy);

public:
	UTexture2D* GetPerPageDataTexture() const
	{
		return PerPageDataTexture;
	}
	FVoxelTexturePool& GetNaniteIndirectionBufferPool() const
	{
		return *NaniteIndirectionBufferPool;
	}
	FVoxelTexturePool& GetChunkIndicesBufferPool() const
	{
		return *ChunkIndicesBufferPool;
	}
	FVoxelTexturePool& GetMaterialBufferPool() const
	{
		return *MaterialBufferPool;
	}
	TSharedPtr<FVoxelTexturePool> GetMetadataBufferPool(const FVoxelMetadataRef Metadata) const
	{
		return MetadataToBufferPool.FindRef(Metadata);
	}
	const TVoxelMap<FVoxelMetadataRef, TSharedPtr<FVoxelTexturePool>>& GetMetadataToBufferPool() const
	{
		return MetadataToBufferPool;
	}

public:
	void UpdateInstance(UMaterialInstanceDynamic& Instance) const;
	
public:
	void ProcessUploads();
	void AddReferencedObjects(FReferenceCollector& Collector);

private:
	TObjectPtr<UTexture2D> PerPageDataTexture;
	TSharedPtr<FVoxelTexturePool> NaniteIndirectionBufferPool;
	TSharedPtr<FVoxelTexturePool> ChunkIndicesBufferPool;
	TSharedPtr<FVoxelTexturePool> MaterialBufferPool;
	TVoxelMap<FVoxelMetadataRef, TSharedPtr<FVoxelTexturePool>> MetadataToBufferPool;
};