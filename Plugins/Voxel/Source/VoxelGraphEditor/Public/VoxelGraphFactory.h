// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "Factories/Factory.h"
#include "VoxelGraphFactory.generated.h"

UCLASS(Abstract)
class VOXELGRAPHEDITOR_API UVoxelGraphBaseFactory : public UFactory
{
	GENERATED_BODY()

public:
	UVoxelGraphBaseFactory();

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ End UFactory Interface
};