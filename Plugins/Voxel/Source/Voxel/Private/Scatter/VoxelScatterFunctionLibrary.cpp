// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#include "Scatter/VoxelScatterFunctionLibrary.h"
#include "VoxelQuery.h"
#include "VoxelLayers.h"
#include "Scatter/VoxelScatterUtilities.h"
#include "Graphs/VoxelStampGraphParameters.h"

FVoxelBox UVoxelScatterFunctionLibrary::GetScatterBounds() const
{
	const FVoxelGraphParameters::FScatterBounds* Parameter = Query->FindParameter<FVoxelGraphParameters::FScatterBounds>();
	if (!Parameter)
	{
		VOXEL_MESSAGE(Error, "{0}: Cannot query scatter bounds here", this);
		return {};
	}

	return Parameter->Bounds;
}

FVoxelPointSet UVoxelScatterFunctionLibrary::GeneratePoints3D(
	const FVoxelWeakStackVolumeLayer& Layer,
	const float DistanceBetweenPoints,
	const float Looseness,
	const FVoxelSeed& Seed,
	const bool bQuerySurfaceTypes,
	const bool bResolveSurfaceTypes,
	const FVoxelMetadataRefBuffer& MetadatasToQuery,
	const FVoxelBox& InBounds)
{
	VOXEL_FUNCTION_COUNTER();

	FVoxelBox Bounds = InBounds;

	if (Bounds == FVoxelBox())
	{
		Bounds = GetScatterBounds();
	}
	else
	{
		Bounds = Bounds.IntersectWith(GetScatterBounds());
	}

	if (!Bounds.IsValidAndNotEmpty())
	{
		return {};
	}

	const FVoxelGraphParameters::FQuery* Parameter = Query->FindParameter<FVoxelGraphParameters::FQuery>();
	if (!Parameter)
	{
		VOXEL_MESSAGE(Error, "{0}: Cannot query here", this);
		return {};
	}
	const FVoxelQuery& VoxelQuery = Parameter->Query;

	if (!VoxelQuery.CheckNoRecursion(Layer) ||
		!VoxelQuery.Layers.HasLayer(Layer, Query->Context.DependencyCollector))
	{
		return {};
	}

	if (!VoxelQuery.HasStamps(Layer, Bounds, EVoxelStampBehavior::AffectShape))
	{
		return {};
	}

	const FIntVector Size = FVoxelUtilities::CeilToInt(Bounds.Size() / DistanceBetweenPoints);

	TVoxelArray<FVoxelMetadataRef> Metadatas;
	FVoxelUtilities::SetNum(Metadatas, MetadatasToQuery.Num());
	for (int32 Index = 0; Index < MetadatasToQuery.Num(); Index++)
	{
		Metadatas.Add(MetadatasToQuery[Index]);
	}

	return
		FVoxelScatterUtilities::ScatterPoints3D(
			VoxelQuery,
			Bounds.Min,
			Size,
			DistanceBetweenPoints,
			Seed.Seed,
			Looseness,
			Layer,
			bQuerySurfaceTypes,
			bResolveSurfaceTypes,
			Metadatas);
}