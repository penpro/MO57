// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "VoxelToolkit.h"
#include "MegaMaterial/VoxelMegaMaterial.h"
#include "VoxelMegaMaterialToolkit.generated.h"

class SVoxelMegaMaterialSurfaceGrid;

USTRUCT()
struct FVoxelMegaMaterialToolkit : public FVoxelToolkit
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

public:
	UPROPERTY()
	TObjectPtr<UVoxelMegaMaterial> Asset;

public:
	//~ Begin FVoxelToolkit Interface
	virtual void Initialize() override;
	virtual TSharedPtr<FTabManager::FLayout> GetLayout() const override;
	virtual void RegisterTabs(FRegisterTab RegisterTab) override;
	virtual void PostEditChange(const FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostUndo() override;
	//~ End FVoxelToolkit Interface

private:
	static constexpr const TCHAR* SurfacesTabId = TEXT("FVoxelMegaMaterialToolkit_Surfaces");
	static constexpr const TCHAR* DetailsTabId = TEXT("FVoxelMegaMaterialToolkit_Details");

	TSharedPtr<SVoxelMegaMaterialSurfaceGrid> SurfaceGrid;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<SWidget> DetailsTabContent;
};
