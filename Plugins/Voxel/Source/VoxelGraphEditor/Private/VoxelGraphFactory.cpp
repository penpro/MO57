// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelGraphFactory.h"
#include "VoxelGraph.h"

UVoxelGraphBaseFactory::UVoxelGraphBaseFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UVoxelGraphBaseFactory::FactoryCreateNew(UClass* Class, UObject* InParent, const FName Name, const EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	const UVoxelGraph* AssetToCopy = nullptr;
	if (ensure(Class))
	{
		AssetToCopy = Class->GetDefaultObject<UVoxelGraph>()->GetFactoryInfo().Template;
	}

	if (!ensure(AssetToCopy))
	{
		return NewObject<UVoxelGraph>(InParent, Class, Name, Flags);
	}

	UVoxelGraph* NewGraph = DuplicateObject(AssetToCopy, InParent, Name);
	if (!ensure(NewGraph))
	{
		return nullptr;
	}

	return NewGraph;
}