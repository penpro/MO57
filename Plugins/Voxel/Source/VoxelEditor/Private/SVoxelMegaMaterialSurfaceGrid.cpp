// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "SVoxelMegaMaterialSurfaceGrid.h"
#include "Surface/VoxelSurfaceTypeAsset.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "PropertyCustomizationHelpers.h"
#include "Editor/EditorWidgets/Public/SAssetDropTarget.h"

FVoxelMegaMaterialSurfaceItem::FVoxelMegaMaterialSurfaceItem(UVoxelSurfaceTypeAsset* InSurfaceType, const int32 InIndex)
	: WeakSurfaceType(InSurfaceType)
	, AssetThumbnail(MakeShared<FAssetThumbnail>(InSurfaceType, 64, 64, FVoxelEditorUtilities::GetThumbnailPool()))
	, Index(InIndex)
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelMegaMaterialSurfaceDragDropOp> FVoxelMegaMaterialSurfaceDragDropOp::New(const TSharedPtr<FVoxelMegaMaterialSurfaceItem>& InItem)
{
	const TSharedRef<FVoxelMegaMaterialSurfaceDragDropOp> DragDrop = MakeShared<FVoxelMegaMaterialSurfaceDragDropOp>();
	DragDrop->Item = InItem;
	DragDrop->SetValidTarget(false);
	DragDrop->SetupDefaults();
	DragDrop->Construct();
	return DragDrop;
}

void FVoxelMegaMaterialSurfaceDragDropOp::SetValidTarget(const bool bIsValidTarget)
{
	if (bIsValidTarget)
	{
		CurrentHoverText = INVTEXT("Move surface here");
		CurrentIconBrush = FAppStyle::GetBrush("Graph.ConnectorFeedback.OK");
	}
	else
	{
		CurrentHoverText = INVTEXT("Cannot move surface here");
		CurrentIconBrush = FAppStyle::GetBrush("Graph.ConnectorFeedback.Error");
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SVoxelMegaMaterialSurfaceGrid::Construct(const FArguments& InArgs)
{
	WeakAsset = InArgs._Asset;

	CommandList = MakeShared<FUICommandList>();
	BindCommands();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
					.HAlign(HAlign_Left)
					.Padding(12.f, 6.f)
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "ButtonText")
						.Text(INVTEXT("Surface Types"))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.f, 0.f)
				[
					SAssignNew(AddButton, SComboButton)
					.ToolTipText(INVTEXT("Add Surface Type"))
					.HasDownArrow(false)
					.OnGetMenuContent(this, &SVoxelMegaMaterialSurfaceGrid::MakeAddMenu)
					.ButtonContent()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SImage)
							.Image(FAppStyle::Get().GetBrush("Icons.Plus"))
							.ColorAndOpacity(FStyleColors::AccentGreen)
						]
						+ SHorizontalBox::Slot()
						.Padding(3.f, 0.f, 0.f, 0.f)
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(STextBlock)
							.TextStyle(FAppStyle::Get(), "SmallButtonText")
							.Text(INVTEXT("Add"))
						]
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SAssetDropTarget)
				.OnAreAssetsAcceptableForDropWithReason_Lambda([](TArrayView<FAssetData> AssetDatas, FText& OutReason)
				{
					for (const FAssetData& Asset : AssetDatas)
					{
						const UClass* Class = Asset.GetClass();
						if (!Class || !Class->IsChildOf(UVoxelSurfaceTypeAsset::StaticClass()))
						{
							OutReason = INVTEXT("Only Voxel Surface Type assets are accepted");
							return false;
						}
					}
					return true;
				})
				.OnAssetsDropped_Lambda([this](const FDragDropEvent&, TArrayView<FAssetData> AssetDatas)
				{
					for (const FAssetData& Data : AssetDatas)
					{
						if (UVoxelSurfaceTypeAsset* SurfaceType = Cast<UVoxelSurfaceTypeAsset>(Data.GetAsset()))
						{
							AddSurfaceType(SurfaceType);
						}
					}
				})
				[
					SAssignNew(TileView, STileView<TSharedPtr<FVoxelMegaMaterialSurfaceItem>>)
					.ListItemsSource(&Items)
					.SelectionMode(ESelectionMode::Multi)
					.ItemWidth(96.f)
					.ItemHeight(112.f)
					.OnGenerateTile(this, &SVoxelMegaMaterialSurfaceGrid::MakeTile)
					.OnContextMenuOpening(this, &SVoxelMegaMaterialSurfaceGrid::OnContextMenuOpening)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(12.f, 6.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]
				{
					const UVoxelMegaMaterial* Asset = WeakAsset.Resolve();
					if (!Asset)
					{
						return INVTEXT("0 surface types");
					}
					return FText::FromString(FText::AsNumber(Asset->SurfaceTypes.Num()).ToString() + " surface types");
				})
			]
		]
	];

	Rebuild();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SVoxelMegaMaterialSurfaceGrid::Rebuild()
{
	ON_SCOPE_EXIT
	{
		if (TileView)
		{
			TileView->RequestListRefresh();
		}
	};

	Items.Reset();

	const UVoxelMegaMaterial* Asset = WeakAsset.Resolve();
	if (!Asset)
	{
		return;
	}

	Items.Reserve(Asset->SurfaceTypes.Num());
	for (int32 Index = 0; Index < Asset->SurfaceTypes.Num(); Index++)
	{
		Items.Add(MakeShared<FVoxelMegaMaterialSurfaceItem>(Asset->SurfaceTypes[Index], Index));
	}
}

void SVoxelMegaMaterialSurfaceGrid::OnPropertyChanged(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_STATIC(UVoxelMegaMaterial, SurfaceTypes))
	{
		Rebuild();
	}
}

FReply SVoxelMegaMaterialSurfaceGrid::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList && CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SVoxelMegaMaterialSurfaceGrid::BindCommands()
{
	CommandList->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SVoxelMegaMaterialSurfaceGrid::DeleteSelected),
		FCanExecuteAction::CreateLambda([this]
		{
			return TileView && TileView->GetNumItemsSelected() > 0;
		}));
}

TSharedPtr<SWidget> SVoxelMegaMaterialSurfaceGrid::OnContextMenuOpening()
{
	if (!TileView ||
		TileView->GetNumItemsSelected() == 0)
	{
		return SNullWidget::NullWidget;
	}

	FMenuBuilder MenuBuilder(true, CommandList);
	MenuBuilder.BeginSection("BasicOperations");
	{
		MenuBuilder.AddMenuEntry(
			INVTEXT("Browse to Asset"),
			INVTEXT("Show this surface type asset in the Content Browser"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "SystemWideCommands.FindInContentBrowser.Small"),
			FUIAction(FExecuteAction::CreateSP(this, &SVoxelMegaMaterialSurfaceGrid::BrowseToSelected)));
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete);
	}
	MenuBuilder.EndSection();
	return MenuBuilder.MakeWidget();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<ITableRow> SVoxelMegaMaterialSurfaceGrid::MakeTile(TSharedPtr<FVoxelMegaMaterialSurfaceItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const TSharedRef<SWidget> TileContent =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(4.f)
		[
			SNew(SBox)
			.WidthOverride(72.f)
			.HeightOverride(72.f)
			[
				Item->AssetThumbnail->MakeThumbnailWidget()
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(2.f, 0.f)
		[
			SNew(STextBlock)
			.Text_Lambda([Item]
			{
				const UVoxelSurfaceTypeAsset* Asset = Item->WeakSurfaceType.Resolve();
				return Asset ? FText::FromString(Asset->GetName()) : INVTEXT("None");
			})
			.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
			.ColorAndOpacity(FSlateColor::UseForeground())
			.WrapTextAt(88.f)
		];

	return
		SNew(STableRow<TSharedPtr<FVoxelMegaMaterialSurfaceItem>>, OwnerTable)
		.Padding(2.f)
		.OnDragDetected_Lambda([Item](const FGeometry&, const FPointerEvent& MouseEvent)
		{
			if (!MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
			{
				return FReply::Unhandled();
			}
			return FReply::Handled().BeginDragDrop(FVoxelMegaMaterialSurfaceDragDropOp::New(Item));
		})
		.OnCanAcceptDrop_Lambda([](const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FVoxelMegaMaterialSurfaceItem> TargetItem) -> TOptional<EItemDropZone>
		{
			const TSharedPtr<FVoxelMegaMaterialSurfaceDragDropOp> Op = DragDropEvent.GetOperationAs<FVoxelMegaMaterialSurfaceDragDropOp>();
			if (!Op)
			{
				return {};
			}
			Op->SetValidTarget(true);
			// Force above-or-below; tile views report OntoItem which we map to AboveItem
			return DropZone == EItemDropZone::BelowItem ? EItemDropZone::BelowItem : EItemDropZone::AboveItem;
		})
		.OnAcceptDrop_Lambda([this](const FDragDropEvent& DragDropEvent, const EItemDropZone DropZone, TSharedPtr<FVoxelMegaMaterialSurfaceItem> TargetItem) -> FReply
		{
			const TSharedPtr<FVoxelMegaMaterialSurfaceDragDropOp> Op = DragDropEvent.GetOperationAs<FVoxelMegaMaterialSurfaceDragDropOp>();
			if (!Op)
			{
				return FReply::Unhandled();
			}
			const TSharedPtr<FVoxelMegaMaterialSurfaceItem> Source = Op->Item.Pin();
			UVoxelMegaMaterial* Asset = WeakAsset.Resolve();
			if (!Source ||
				Source == TargetItem ||
				!ensure(Asset) ||
				!Asset->SurfaceTypes.IsValidIndex(Source->Index) ||
				!Asset->SurfaceTypes.IsValidIndex(TargetItem->Index))
			{
				return FReply::Unhandled();
			}

			{
				FVoxelTransaction Transaction(Asset, "Reorder Surface Types");

				UVoxelSurfaceTypeAsset* Moving = Asset->SurfaceTypes[Source->Index];
				int32 InsertIndex = TargetItem->Index + (DropZone == EItemDropZone::BelowItem ? 1 : 0);

				Asset->SurfaceTypes.RemoveAt(Source->Index);
				if (Source->Index < InsertIndex)
				{
					InsertIndex--;
				}
				InsertIndex = FMath::Clamp(InsertIndex, 0, Asset->SurfaceTypes.Num());
				Asset->SurfaceTypes.Insert(Moving, InsertIndex);
			}

			Rebuild();
			return FReply::Handled();
		})
		[
			TileContent
		];
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SVoxelMegaMaterialSurfaceGrid::MakeAddMenu()
{
	const TWeakPtr<SVoxelMegaMaterialSurfaceGrid> WeakSelf = SharedThis(this);

	FAssetPickerConfig Config;
	Config.Filter.ClassPaths.Add(UVoxelSurfaceTypeAsset::StaticClass()->GetClassPathName());
	Config.Filter.bRecursiveClasses = true;
	Config.InitialAssetViewType = EAssetViewType::List;
	Config.bAllowDragging = false;
	Config.bAllowNullSelection = false;
	Config.bFocusSearchBoxWhenOpened = true;
	Config.SaveSettingsName = TEXT("VoxelMegaMaterialSurfaceGrid");
	Config.OnShouldFilterAsset = FOnShouldFilterAsset::CreateLambda([WeakSelf](const FAssetData& Data)
	{
		const TSharedPtr<SVoxelMegaMaterialSurfaceGrid> This = WeakSelf.Pin();
		if (!This)
		{
			return true;
		}
		const UVoxelMegaMaterial* Asset = This->WeakAsset.Resolve();
		if (!Asset)
		{
			return true;
		}
		// Hide entries already present
		const FSoftObjectPath Path = Data.ToSoftObjectPath();
		for (const TObjectPtr<UVoxelSurfaceTypeAsset>& Existing : Asset->SurfaceTypes)
		{
			if (Existing && FSoftObjectPath(Existing) == Path)
			{
				return true;
			}
		}
		return false;
	});
	Config.OnAssetSelected = FOnAssetSelected::CreateLambda([WeakSelf](const FAssetData& Data)
	{
		const TSharedPtr<SVoxelMegaMaterialSurfaceGrid> This = WeakSelf.Pin();
		if (!This)
		{
			return;
		}
		if (UVoxelSurfaceTypeAsset* SurfaceType = Cast<UVoxelSurfaceTypeAsset>(Data.GetAsset()))
		{
			This->AddSurfaceType(SurfaceType);
		}
		if (This->AddButton)
		{
			This->AddButton->SetIsOpen(false);
		}
	});

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	return
		SNew(SBox)
		.WidthOverride(300.f)
		.HeightOverride(400.f)
		[
			ContentBrowserModule.Get().CreateAssetPicker(Config)
		];
}

void SVoxelMegaMaterialSurfaceGrid::AddSurfaceType(UVoxelSurfaceTypeAsset* SurfaceType)
{
	UVoxelMegaMaterial* Asset = WeakAsset.Resolve();
	if (!ensure(Asset) ||
		!SurfaceType ||
		Asset->SurfaceTypes.Contains(SurfaceType))
	{
		return;
	}

	{
		FVoxelTransaction Transaction(Asset, "Add Surface Type");
		Asset->SurfaceTypes.Add(SurfaceType);
	}

	Rebuild();

	if (Items.Num() > 0)
	{
		const TSharedPtr<FVoxelMegaMaterialSurfaceItem> NewItem = Items.Last();
		TileView->SetSelection(NewItem);
		TileView->RequestScrollIntoView(NewItem);
	}
}

void SVoxelMegaMaterialSurfaceGrid::BrowseToSelected()
{
	if (!TileView)
	{
		return;
	}

	TArray<UObject*> Objects;
	for (const TSharedPtr<FVoxelMegaMaterialSurfaceItem>& Item : TileView->GetSelectedItems())
	{
		if (UVoxelSurfaceTypeAsset* Asset = Item->WeakSurfaceType.Resolve())
		{
			Objects.Add(Asset);
		}
	}

	if (Objects.Num() > 0)
	{
		GEditor->SyncBrowserToObjects(Objects);
	}
}

void SVoxelMegaMaterialSurfaceGrid::DeleteSelected()
{
	if (!TileView)
	{
		return;
	}

	UVoxelMegaMaterial* Asset = WeakAsset.Resolve();
	if (!ensure(Asset))
	{
		return;
	}

	const TArray<TSharedPtr<FVoxelMegaMaterialSurfaceItem>> Selection = TileView->GetSelectedItems();
	if (Selection.Num() == 0)
	{
		return;
	}

	TArray<int32> IndicesToRemove;
	IndicesToRemove.Reserve(Selection.Num());
	for (const TSharedPtr<FVoxelMegaMaterialSurfaceItem>& Item : Selection)
	{
		if (Asset->SurfaceTypes.IsValidIndex(Item->Index))
		{
			IndicesToRemove.Add(Item->Index);
		}
	}
	IndicesToRemove.Sort([](const int32 A, const int32 B) { return A > B; });

	{
		FVoxelTransaction Transaction(Asset, "Remove Surface Types");
		for (const int32 Index : IndicesToRemove)
		{
			Asset->SurfaceTypes.RemoveAt(Index);
		}
	}

	Rebuild();
}
