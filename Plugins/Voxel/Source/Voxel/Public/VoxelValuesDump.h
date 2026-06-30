// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class UVoxelLayer;
class UVoxelLayerStack;

struct VOXEL_API FVoxelValuesDump
{
	static void Log(
		UWorld* World,
		UVoxelLayerStack* Stack,
		UVoxelLayer* Layer,
		const FVector& Position);
};