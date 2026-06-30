// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

#if WITH_EDITOR
class VOXELGRAPH_API FVoxelGraphEditorUtilities
{
public:
	static TDelegate<void(const TVoxelSet<const UEdGraphNode*>&)> OnSelectNodes;
};
#endif