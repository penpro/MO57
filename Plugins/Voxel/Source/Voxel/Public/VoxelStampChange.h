// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

struct FVoxelStampRuntime;

struct FVoxelStampChange
{
	TSharedPtr<const FVoxelStampRuntime> OldStamp;
	TSharedPtr<const FVoxelStampRuntime> NewStamp;
};