// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMegaMaterialToolkit.h"
#include "SVoxelMegaMaterialSurfaceGrid.h"

void FVoxelMegaMaterialToolkit::Initialize()
{
	Super::Initialize();

	{
		FDetailsViewArgs Args;
		Args.bHideSelectionTip = true;
		Args.NotifyHook = GetNotifyHook();
		Args.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		DetailsView = PropertyModule.CreateDetailView(Args);
		DetailsView->SetObject(Asset);
	}

	SurfaceGrid =
		SNew(SVoxelMegaMaterialSurfaceGrid)
		.Asset(Asset);

	DetailsTabContent =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text(INVTEXT(
				"A Mega Material gathers all the surface types your voxel world needs to render "
				"- grass, rock, sand, etc. - and produces the optimized materials the "
				"renderer uses for each path (Nanite, Lumen, non-Nanite).\n\n"
				"Surface types will be auto added as they are used in your voxel world."))
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			DetailsView.ToSharedRef()
		];
}

TSharedPtr<FTabManager::FLayout> FVoxelMegaMaterialToolkit::GetLayout() const
{
	return FTabManager::NewLayout("FVoxelMegaMaterialToolkit_Layout_v1")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->SetSizeCoefficient(1.f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.65f)
					->SetHideTabWell(true)
					->AddTab(SurfacesTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.35f)
					->SetHideTabWell(true)
					->AddTab(DetailsTabId, ETabState::OpenedTab)
				)
			)
		);
}

void FVoxelMegaMaterialToolkit::RegisterTabs(const FRegisterTab RegisterTab)
{
	Super::RegisterTabs(RegisterTab);

	RegisterTab(SurfacesTabId, INVTEXT("Surface Types"), "ContentBrowser.AssetTreeFolderOpen", SurfaceGrid);
	RegisterTab(DetailsTabId, INVTEXT("Details"), "LevelEditor.Tabs.Details", DetailsTabContent);
}

void FVoxelMegaMaterialToolkit::PostEditChange(const FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChange(PropertyChangedEvent);

	if (SurfaceGrid)
	{
		SurfaceGrid->OnPropertyChanged(PropertyChangedEvent);
	}
}

void FVoxelMegaMaterialToolkit::PostUndo()
{
	if (SurfaceGrid)
	{
		SurfaceGrid->Rebuild();
	}
}
