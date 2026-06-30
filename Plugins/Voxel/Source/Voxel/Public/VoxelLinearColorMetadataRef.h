// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelMetadataRef.h"
#include "VoxelLinearColorMetadataRef.generated.h"

USTRUCT()
struct VOXEL_API FVoxelLinearColorMetadataRef : public FVoxelMetadataRef
{
	GENERATED_BODY()
	GENERATED_VOXEL_METADATA_REF_BODY(FVoxelLinearColorMetadataRef, UVoxelLinearColorMetadata, FVoxelLinearColorBuffer);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DECLARE_VOXEL_TERMINAL_BUFFER(FVoxelLinearColorMetadataRefBuffer, FVoxelLinearColorMetadataRef);

USTRUCT()
struct VOXEL_API FVoxelLinearColorMetadataRefBuffer final : public FVoxelTerminalBuffer
{
	GENERATED_BODY()
	GENERATED_VOXEL_TERMINAL_BUFFER_BODY(FVoxelLinearColorMetadataRefBuffer, FVoxelLinearColorMetadataRef);
};