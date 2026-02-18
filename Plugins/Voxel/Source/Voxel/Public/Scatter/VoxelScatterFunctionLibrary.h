// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelPointSet.h"
#include "VoxelStackLayer.h"
#include "VoxelMetadataRef.h"
#include "VoxelFunctionLibrary.h"
#include "Buffer/VoxelBaseBuffers.h"
#include "VoxelScatterFunctionLibrary.generated.h"

namespace FVoxelGraphParameters
{
	struct FScatterBounds : FUniformParameter
	{
		FVoxelBox Bounds;
	};
}

UCLASS(Category = "Scatter")
class VOXEL_API UVoxelScatterFunctionLibrary : public UVoxelFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(meta = (AllowList = "Scatter", ShowInShortList))
	FVoxelBox GetScatterBounds() const;

	/**
	 * Generate points on a volume layer
	 * @param Layer					The layer to generate on
	 * @param InBounds				The bounds to generate points in, if unset will use the scatter bounds
	 * @param DistanceBetweenPoints				
	 * @param Looseness				
	 * @param Seed					
	 * @param bQuerySurfaceTypes	
	 * @param bResolveSurfaceTypes	If false smart surface types won't be resolved
	 * @param MetadatasToQuery		
	 */
	UFUNCTION(meta = (AllowList = "Scatter", ShowInShortList))
	FVoxelPointSet GeneratePoints3D(
		const FVoxelWeakStackVolumeLayer& Layer = {},
		float DistanceBetweenPoints = 100.f,
		float Looseness = 1.f,
		const FVoxelSeed& Seed = {},
		bool bQuerySurfaceTypes = true,
		bool bResolveSurfaceTypes = false,
		UPARAM(meta = (ArrayPin)) const FVoxelMetadataRefBuffer& MetadatasToQuery = {},
		UPARAM(DisplayName = "Bounds", meta = (HideDefault, AdvancedDisplay)) const FVoxelBox& InBounds = {});
};