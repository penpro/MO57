// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

struct FVoxelVolumeSculptDefinitions
{
	static constexpr int32 ChunkSizeLog2 = 3;
	static constexpr int32 ChunkSize = 1 << ChunkSizeLog2;
	static constexpr int32 ChunkCount = FMath::Cube(ChunkSize);
};