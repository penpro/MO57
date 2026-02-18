// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelChunkKey.h"

struct FVoxelConfig;

struct FVoxelScreenSizeHelper
{
	FVector CameraChunkPosition;
	double MinQuality;
	double MaxQuality;
	double ChunkToWorld;
	double QualityExponent;

	explicit FVoxelScreenSizeHelper(const FVoxelConfig& Config);

	FORCEINLINE double GetChunkQuality(const FVoxelChunkKey ChunkKey) const
	{
		const FVoxelIntBox Bounds = ChunkKey.GetChunkKeyBounds();

		const double DistanceToCamera = Bounds.DistanceToPoint(CameraChunkPosition);
		if (DistanceToCamera == 0)
		{
			return 0;
		}

		const int32 ChunkSize = 1 << ChunkKey.LOD;
		return DistanceToCamera / FMath::Pow(double(ChunkSize), QualityExponent);
	}
};