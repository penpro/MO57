// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "VoxelGraphFactory.h"
#include "VoxelPCGGraph.h"
#include "VoxelPCGGraphFactory.generated.h"

UCLASS()
class UVoxelPCGGraphFactory : public UVoxelGraphBaseFactory
{
	GENERATED_BODY()

public:
	UVoxelPCGGraphFactory()
	{
		SupportedClass = UVoxelPCGGraph::StaticClass();
	}
};