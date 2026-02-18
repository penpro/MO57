// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "VoxelGraphFactory.h"
#include "Surface/VoxelSurfaceTypeGraph.h"
#include "VoxelSurfaceTypeGraphFactory.generated.h"

UCLASS()
class UVoxelSurfaceTypeGraphFactory : public UVoxelGraphBaseFactory
{
	GENERATED_BODY()

public:
	UVoxelSurfaceTypeGraphFactory()
	{
		SupportedClass = UVoxelSurfaceTypeGraph::StaticClass();
	}
};