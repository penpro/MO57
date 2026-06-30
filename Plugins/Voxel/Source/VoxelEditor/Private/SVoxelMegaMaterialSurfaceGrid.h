// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "MegaMaterial/VoxelMegaMaterial.h"

class UVoxelSurfaceTypeAsset;

struct FVoxelMegaMaterialSurfaceItem
{
	const TVoxelObjectPtr<UVoxelSurfaceTypeAsset> WeakSurfaceType;
	const TSharedRef<FAssetThumbnail> AssetThumbnail;
	const int32 Index;

	FVoxelMegaMaterialSurfaceItem(UVoxelSurfaceTypeAsset* InSurfaceType, int32 InIndex);
};

class FVoxelMegaMaterialSurfaceDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FVoxelMegaMaterialSurfaceDragDropOp, FDecoratedDragDropOp)
	TWeakPtr<FVoxelMegaMaterialSurfaceItem> Item;

	static TSharedRef<FVoxelMegaMaterialSurfaceDragDropOp> New(const TSharedPtr<FVoxelMegaMaterialSurfaceItem>& InItem);

	void SetValidTarget(bool bIsValidTarget);
};

class SVoxelMegaMaterialSurfaceGrid : public SCompoundWidget
{
public:
	VOXEL_SLATE_ARGS()
	{
		SLATE_ARGUMENT(TVoxelObjectPtr<UVoxelMegaMaterial>, Asset)
	};

	void Construct(const FArguments& InArgs);

	void OnPropertyChanged(const FPropertyChangedEvent& PropertyChangedEvent);
	void Rebuild();

	//~ Begin SCompoundWidget Interface
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End SCompoundWidget Interface

private:
	void BindCommands();
	TSharedRef<ITableRow> MakeTile(TSharedPtr<FVoxelMegaMaterialSurfaceItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedPtr<SWidget> OnContextMenuOpening();

	TSharedRef<SWidget> MakeAddMenu();
	void AddSurfaceType(UVoxelSurfaceTypeAsset* SurfaceType);
	void BrowseToSelected();
	void DeleteSelected();

private:
	TVoxelObjectPtr<UVoxelMegaMaterial> WeakAsset;
	TSharedPtr<STileView<TSharedPtr<FVoxelMegaMaterialSurfaceItem>>> TileView;
	TSharedPtr<class SComboButton> AddButton;
	TArray<TSharedPtr<FVoxelMegaMaterialSurfaceItem>> Items;
	TSharedPtr<FUICommandList> CommandList;
};
