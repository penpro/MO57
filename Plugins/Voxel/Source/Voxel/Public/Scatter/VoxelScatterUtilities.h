// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelStackLayer.h"
#include "VoxelMetadataRef.h"

class FVoxelQuery;
struct FVoxelPointSet;

struct VOXEL_API FVoxelScatterUtilities
{
	static FVoxelPointSet ScatterPoints3D(
		const FVoxelQuery& Query,
		const FVector& Start,
		const FIntVector& Size,
		float DistanceBetweenPoints,
		uint64 Seed,
		float Looseness,
		const FVoxelWeakStackLayer& WeakLayer,
		bool bQuerySurfaceTypes,
		bool bResolveSurfaceTypes,
		TConstVoxelArrayView<FVoxelMetadataRef> MetadatasToQuery);
};