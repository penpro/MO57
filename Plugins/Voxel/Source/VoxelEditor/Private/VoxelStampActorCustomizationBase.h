// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"

class FVoxelStampActorCustomizationBase : public FVoxelDetailCustomization
{
public:
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
};