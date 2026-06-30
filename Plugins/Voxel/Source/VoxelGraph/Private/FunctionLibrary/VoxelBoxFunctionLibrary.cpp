// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "FunctionLibrary/VoxelBoxFunctionLibrary.h"

VOXEL_REGISTER_FUNCTION(UVoxelBoxFunctionLibrary, MakeBox);
VOXEL_REGISTER_FUNCTION(UVoxelBoxFunctionLibrary, BreakBox);
VOXEL_REGISTER_FUNCTION(UVoxelBoxFunctionLibrary, MakeBoxFromRadius);
VOXEL_REGISTER_FUNCTION(UVoxelBoxFunctionLibrary, MakeInfiniteBox);
VOXEL_REGISTER_FUNCTION(UVoxelBoxFunctionLibrary, IsBoxValid);
VOXEL_REGISTER_FUNCTION(UVoxelBoxFunctionLibrary, TransformBox);
VOXEL_REGISTER_FUNCTION(UVoxelBoxFunctionLibrary, ExtendBox);

VOXEL_REGISTER_FUNCTION(UVoxelBoxFunctionLibrary, MakeBox2D);
VOXEL_REGISTER_FUNCTION(UVoxelBoxFunctionLibrary, MakeBox2DFromRadius);
VOXEL_REGISTER_FUNCTION(UVoxelBoxFunctionLibrary, ExtendBox2D);
VOXEL_REGISTER_FUNCTION(UVoxelBoxFunctionLibrary, IsBox2DValid);
VOXEL_REGISTER_FUNCTION(UVoxelBoxFunctionLibrary, BreakBox2D);
