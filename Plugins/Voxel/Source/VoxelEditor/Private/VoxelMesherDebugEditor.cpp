// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMesherDebugEditor.h"
#include "VoxelMesh.h"
#include "VoxelQuery.h"
#include "VoxelLayers.h"
#include "VoxelStackLayer.h"
#include "VoxelViewport.h"
#include "VoxelViewportInterface.h"
#include "Surface/VoxelSurfaceTypeTable.h"
#include "Buffer/VoxelDoubleBuffers.h"
#include "Buffer/VoxelFloatBuffers.h"

#include "SceneView.h"
#include "SceneManagement.h"
#include "PrimitiveSceneProxy.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "HAL/PlatformApplicationMisc.h"

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelMesherDebugSceneProxy : public FPrimitiveSceneProxy
{
public:
	const TSharedRef<const FVoxelMesh> Mesh;
	const FLinearColor Color;
	const TSet<int32> HighlightedTriangles;

	FVoxelMesherDebugSceneProxy(
		const UPrimitiveComponent& Component,
		const TSharedRef<const FVoxelMesh>& Mesh,
		const FLinearColor& Color,
		const TSet<int32>& HighlightedTriangles)
		: FPrimitiveSceneProxy(&Component)
		, Mesh(Mesh)
		, Color(Color)
		, HighlightedTriangles(HighlightedTriangles)
	{
		bWillEverBeLit = false;
		bVerifyUsedMaterials = false;
	}

	//~ Begin FPrimitiveSceneProxy Interface
	virtual SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}
	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}
	virtual bool CanBeOccluded() const override
	{
		return false;
	}
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = true;
		Result.bDynamicRelevance = true;
		Result.bRenderInMainPass = true;
		Result.bEditorPrimitiveRelevance = true;
		return Result;
	}

	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily,
		const uint32 VisibilityMap,
		FMeshElementCollector& Collector) const override
	{
		VOXEL_FUNCTION_COUNTER();

		const FMatrix LocalToWorldMatrix = GetLocalToWorld();
		const TConstVoxelArrayView<int32> Indices = Mesh->Indices;
		const TConstVoxelArrayView<FVector3f> Vertices = Mesh->Vertices;
		checkVoxelSlow(Indices.Num() % 3 == 0);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (!(VisibilityMap & (1 << ViewIndex)))
			{
				continue;
			}

			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

			for (int32 Index = 0; Index < Indices.Num(); Index += 3)
			{
				const int32 TriangleIndex = Index / 3;
				const FVector A = LocalToWorldMatrix.TransformPosition(FVector(Vertices[Indices[Index + 0]]));
				const FVector B = LocalToWorldMatrix.TransformPosition(FVector(Vertices[Indices[Index + 1]]));
				const FVector C = LocalToWorldMatrix.TransformPosition(FVector(Vertices[Indices[Index + 2]]));

				const FLinearColor DrawColor = HighlightedTriangles.Contains(TriangleIndex) ? FLinearColor::Yellow : Color;

				PDI->DrawLine(A, B, DrawColor, SDPG_World);
				PDI->DrawLine(B, C, DrawColor, SDPG_World);
				PDI->DrawLine(C, A, DrawColor, SDPG_World);
			}
		}
	}
	//~ End FPrimitiveSceneProxy Interface
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FPrimitiveSceneProxy* UVoxelMesherDebugComponent::CreateSceneProxy()
{
	if (!Mesh)
	{
		return nullptr;
	}
	return new FVoxelMesherDebugSceneProxy(*this, Mesh.ToSharedRef(), Color, HighlightedTriangles);
}

FBoxSphereBounds UVoxelMesherDebugComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (!Mesh ||
		Mesh->Vertices.Num() == 0)
	{
		return FBoxSphereBounds(ForceInit);
	}

	FBox LocalBox(ForceInit);
	for (const FVector3f& Vertex : Mesh->Vertices)
	{
		LocalBox += FVector(Vertex);
	}
	return FBoxSphereBounds(LocalBox).TransformBy(LocalToWorld);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DECLARE_DELEGATE_OneParam(FOnMarqueeComplete, FBox2D);

class SMesherDebugMarquee : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMesherDebugMarquee) {}
		SLATE_EVENT(FOnMarqueeComplete, OnMarqueeComplete)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		OnMarqueeCompleteDelegate = InArgs._OnMarqueeComplete;
		SetCanTick(false);

		SetVisibility(TAttribute<EVisibility>::CreateLambda([this]
		{
			if (bIsDragging ||
				FSlateApplication::Get().GetModifierKeys().IsShiftDown())
			{
				return EVisibility::Visible;
			}
			return EVisibility::HitTestInvisible;
		}));
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D::ZeroVector;
	}

	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override
	{
		if (!MouseEvent.IsShiftDown() ||
			MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return FReply::Unhandled();
		}

		bIsDragging = true;
		Start = Current = Geometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override
	{
		if (!bIsDragging)
		{
			return FReply::Unhandled();
		}
		Current = Geometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		return FReply::Handled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override
	{
		if (!bIsDragging ||
			MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return FReply::Unhandled();
		}

		bIsDragging = false;
		const FVector2D Min = FVector2D(FMath::Min(Start.X, Current.X), FMath::Min(Start.Y, Current.Y));
		const FVector2D Max = FVector2D(FMath::Max(Start.X, Current.X), FMath::Max(Start.Y, Current.Y));

		OnMarqueeCompleteDelegate.ExecuteIfBound(FBox2D(Min, Max));

		return FReply::Handled().ReleaseMouseCapture();
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override
	{
		if (!bIsDragging)
		{
			return LayerId;
		}

		const FVector2D Min = FVector2D(FMath::Min(Start.X, Current.X), FMath::Min(Start.Y, Current.Y));
		const FVector2D Max = FVector2D(FMath::Max(Start.X, Current.X), FMath::Max(Start.Y, Current.Y));

		const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry(Max - Min, FSlateLayoutTransform(Min));
		const FSlateBrush* Brush = FAppStyle::GetBrush("MarqueeSelection");

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			PaintGeometry,
			Brush,
			ESlateDrawEffect::None,
			FLinearColor::Yellow);

		return LayerId + 1;
	}

private:
	bool bIsDragging = false;
	FVector2D Start = FVector2D::ZeroVector;
	FVector2D Current = FVector2D::ZeroVector;
	FOnMarqueeComplete OnMarqueeCompleteDelegate;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class SVoxelMesherDebugEditor
	: public SCompoundWidget
	, public IVoxelViewportInterface
{
public:
	VOXEL_SLATE_ARGS()
	{
	};

	struct FCornerSample
	{
		FVector WorldPosition = FVector::ZeroVector;
		float Distance = 0.f;
	};

	struct FChunkEntry
	{
		int32 Index = 0;
		TWeakObjectPtr<UVoxelMesherDebugComponent> WeakComponent;
		TSharedPtr<const FVoxelMesh> Mesh;
		FLinearColor Color = FLinearColor::White;
		bool bVisible = true;
		TMap<FIntVector, FCornerSample> CornerSamples;
	};

	struct FSelectedTriangle
	{
		TSharedPtr<FChunkEntry> Entry;
		int32 TriangleIndex = 0;
		FVector V0World = FVector::ZeroVector;
		FVector V1World = FVector::ZeroVector;
		FVector V2World = FVector::ZeroVector;
	};

	void Construct(
		const FArguments& Args,
		const TArray<FVoxelMesherDebugChunk>& Chunks,
		const FVector& InWorldPosition,
		const TSharedRef<FVoxelLayers>& Layers,
		const TSharedRef<FVoxelSurfaceTypeTable>& SurfaceTypeTable,
		const FVoxelWeakStackLayer& WeakLayer)
	{
		WorldPosition = InWorldPosition;

		SAssignNew(Viewport, SVoxelViewport);

		for (int32 Index = 0; Index < Chunks.Num(); Index++)
		{
			const FVoxelMesherDebugChunk& Chunk = Chunks[Index];
			if (!Chunk.Mesh)
			{
				continue;
			}

			UVoxelMesherDebugComponent* Component = Viewport->CreateComponent<UVoxelMesherDebugComponent>();
			if (!ensure(Component))
			{
				continue;
			}

			const uint64 ColorHash = FVoxelUtilities::MurmurHashMulti(
				Chunk.Mesh->ChunkOffset.X,
				Chunk.Mesh->ChunkOffset.Y,
				Chunk.Mesh->ChunkOffset.Z,
				Chunk.Mesh->ChunkLOD);
			const FLinearColor Color = FLinearColor::MakeFromHSV8(uint8(ColorHash & 0xFF), 255, 255);

			Component->Mesh = Chunk.Mesh;
			Component->Color = Color;
			Component->SetWorldTransform(Chunk.ComponentTransform);
			Component->MarkRenderStateDirty();
			Component->UpdateBounds();

			const TSharedRef<FChunkEntry> Entry = MakeShared<FChunkEntry>();
			Entry->Index = ChunkEntries.Num();
			Entry->WeakComponent = Component;
			Entry->Mesh = Chunk.Mesh;
			Entry->Color = Color;

			// Collect unique cell corners in local index space, query SDF in world space.
			TArray<FIntVector> LocalCorners;
			{
				TSet<FIntVector> CornerSet;
				CornerSet.Reserve(Chunk.Mesh->Cells.Num() * 8);
				LocalCorners.Reserve(Chunk.Mesh->Cells.Num() * 8);

				for (const FVoxelMesh::FCell& Cell : Chunk.Mesh->Cells)
				{
					for (int32 Corner = 0; Corner < 8; Corner++)
					{
						const FIntVector LocalCorner(
							Cell.X + ((Corner & 0x1) ? 1 : 0),
							Cell.Y + ((Corner & 0x2) ? 1 : 0),
							Cell.Z + ((Corner & 0x4) ? 1 : 0));

						bool bAlreadyIn = false;
						CornerSet.Add(LocalCorner, &bAlreadyIn);
						if (!bAlreadyIn)
						{
							LocalCorners.Add(LocalCorner);
						}
					}
				}
			}

			if (LocalCorners.Num() > 0)
			{
				FVoxelDoubleVectorBuffer Positions;
				Positions.Allocate(LocalCorners.Num());

				const FTransform LocalToWorld = Chunk.ComponentTransform;
				for (int32 CornerIndex = 0; CornerIndex < LocalCorners.Num(); CornerIndex++)
				{
					Positions.Set(CornerIndex, LocalToWorld.TransformPosition(FVector(LocalCorners[CornerIndex])));
				}

				FVoxelDependencyCollector DependencyCollector(STATIC_FNAME("FVoxelMesherDebugEditor"));
				const FVoxelQuery Query(
					Chunk.Mesh->ChunkLOD,
					*Layers,
					*SurfaceTypeTable,
					DependencyCollector);

				const FVoxelFloatBuffer Distances = Query.SampleVolumeLayer(WeakLayer, Positions);

				Entry->CornerSamples.Reserve(LocalCorners.Num());
				for (int32 CornerIndex = 0; CornerIndex < LocalCorners.Num(); CornerIndex++)
				{
					FCornerSample Sample;
					Sample.WorldPosition = Positions[CornerIndex];
					Sample.Distance = Distances[CornerIndex];
					Entry->CornerSamples.Add(LocalCorners[CornerIndex], Sample);
				}
			}

			ChunkEntries.Add(Entry);
		}

		ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(360.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.FillHeight(0.4f)
					[
						SAssignNew(ListView, SListView<TSharedPtr<FChunkEntry>>)
						.ListItemsSource(&ChunkEntries)
						.SelectionMode(ESelectionMode::Single)
						.OnGenerateRow(this, &SVoxelMesherDebugEditor::GenerateChunkRow)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(4.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.f, 0.f, 8.f, 0.f)
						[
							SNew(SCheckBox)
							.IsChecked_Lambda([this] { return bShowCells ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
							.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bShowCells = State == ECheckBoxState::Checked; })
							.Content()
							[
								SNew(STextBlock).Text(INVTEXT("Show Cells"))
							]
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text_Lambda([this]
							{
								return FText::FromString(FString::Printf(
									TEXT("Selected: %d  (Shift+drag)"),
									SelectedTriangles.Num()));
							})
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.Text(INVTEXT("Copy"))
							.IsEnabled_Lambda([this] { return SelectedTriangles.Num() > 0; })
							.OnClicked(this, &SVoxelMesherDebugEditor::CopySelectedTriangles)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(4.f, 0.f, 0.f, 0.f)
						[
							SNew(SButton)
							.Text(INVTEXT("Clear"))
							.IsEnabled_Lambda([this] { return SelectedTriangles.Num() > 0; })
							.OnClicked(this, &SVoxelMesherDebugEditor::ClearSelection)
						]
					]
					+ SVerticalBox::Slot()
					.FillHeight(0.6f)
					.Padding(2.f)
					[
						SNew(SMultiLineEditableTextBox)
						.IsReadOnly(true)
						.AlwaysShowScrollbars(true)
						.Text_Lambda([this] { return FText::FromString(SelectionText); })
					]
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					Viewport.ToSharedRef()
				]
				+ SOverlay::Slot()
				[
					SNew(SMesherDebugMarquee)
					.OnMarqueeComplete(this, &SVoxelMesherDebugEditor::OnMarqueeComplete)
				]
			]
		];

		Viewport->Initialize(SharedThis(this));

		const FVector CameraOffset(100.0, 100.0, 100.0);
		Viewport->SetViewLocation(WorldPosition + CameraOffset);
		Viewport->SetViewRotation((-CameraOffset).Rotation());
	}

	TSharedRef<ITableRow> GenerateChunkRow(
		TSharedPtr<FChunkEntry> Entry,
		const TSharedRef<STableViewBase>& OwnerTable)
	{
		const FVoxelMesh& Mesh = *Entry->Mesh;

		const FString InfoText = FString::Printf(
			TEXT("LOD %d  |  Verts %d  |  Tris %d  |  Offset %lld,%lld,%lld"),
			Mesh.ChunkLOD,
			Mesh.Vertices.Num(),
			Mesh.Indices.Num() / 3,
			Mesh.ChunkOffset.X,
			Mesh.ChunkOffset.Y,
			Mesh.ChunkOffset.Z);

		return
			SNew(STableRow<TSharedPtr<FChunkEntry>>, OwnerTable)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.OnClicked_Lambda([Entry, this]
					{
						Entry->bVisible = !Entry->bVisible;
						if (UVoxelMesherDebugComponent* Component = Entry->WeakComponent.Get())
						{
							Component->SetVisibility(Entry->bVisible);
						}
						return FReply::Handled();
					})
					.Content()
					[
						SNew(SImage)
						.Image_Lambda([Entry]
						{
							return FAppStyle::Get().GetBrush(Entry->bVisible
								? TEXT("Level.VisibleIcon16x")
								: TEXT("Level.NotVisibleIcon16x"));
						})
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.f, 2.f)
				[
					SNew(SColorBlock)
					.Color(Entry->Color)
					.Size(FVector2D(14.f, 14.f))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(InfoText))
					.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
				]
			];
	}

	//~ Begin SWidget Interface
	virtual void Tick(
		const FGeometry& AllottedGeometry,
		const double InCurrentTime,
		const float InDeltaTime) override
	{
		SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

		if (!Viewport)
		{
			return;
		}

		FVoxelDebugDrawer(Viewport->GetWorld())
			.OneFrame()
			.Color(FLinearColor(FColor::Purple))
			.DrawPoint(WorldPosition, uint8(1));

		if (bShowCells)
		{
			for (const TSharedPtr<FChunkEntry>& Entry : ChunkEntries)
			{
				if (!Entry->bVisible ||
					!Entry->Mesh)
				{
					continue;
				}
				const UVoxelMesherDebugComponent* Component = Entry->WeakComponent.Get();
				if (!Component)
				{
					continue;
				}

				const FTransform ComponentTransform = Component->GetComponentTransform();

				TSet<int64> DrawnCells;
				DrawnCells.Reserve(Entry->Mesh->Cells.Num());

				{
					FVoxelDebugDrawer Drawer(Viewport->GetWorld());
					Drawer.OneFrame().Color(FLinearColor::White);

					for (const FVoxelMesh::FCell& Cell : Entry->Mesh->Cells)
					{
						const int64 Key =
							(int64(uint16(Cell.X)) << 0) |
							(int64(uint16(Cell.Y)) << 16) |
							(int64(uint16(Cell.Z)) << 32);

						bool bAlreadyIn = false;
						DrawnCells.Add(Key, &bAlreadyIn);
						if (bAlreadyIn)
						{
							continue;
						}

						const FVoxelBox LocalBox(
							FVector(Cell.X, Cell.Y, Cell.Z),
							FVector(Cell.X + 1, Cell.Y + 1, Cell.Z + 1));

						Drawer.DrawBox(LocalBox, ComponentTransform);
					}
				}

				{
					FVoxelDebugDrawer GreenDrawer(Viewport->GetWorld());
					GreenDrawer.OneFrame().Color(FLinearColor::Green);

					FVoxelDebugDrawer RedDrawer(Viewport->GetWorld());
					RedDrawer.OneFrame().Color(FLinearColor::Red);

					for (const TPair<FIntVector, FCornerSample>& Pair : Entry->CornerSamples)
					{
						if (Pair.Value.Distance > 0.f)
						{
							GreenDrawer.DrawPoint(Pair.Value.WorldPosition, uint8(4));
						}
						else
						{
							RedDrawer.DrawPoint(Pair.Value.WorldPosition, uint8(4));
						}
					}
				}
			}
		}
	}
	//~ End SWidget Interface

	//~ Begin IVoxelViewportInterface Interface
	virtual bool ShowFloor() const override
	{
		return false;
	}
#if VOXEL_ENGINE_VERSION >= 506
	virtual FString GetToolbarName() const override
	{
		return "VoxelMesherDebug.ViewportToolbar";
	}
#endif
	virtual void DrawCanvas(
		FViewport& InViewport,
		FSceneView& View,
		FCanvas& Canvas) override
	{
		if (!PendingSelectionBox.IsSet())
		{
			return;
		}

		const FBox2D Box = PendingSelectionBox.GetValue();
		PendingSelectionBox.Reset();

		RunSelection(View, Box);
	}
	//~ End IVoxelViewportInterface Interface

	void OnMarqueeComplete(FBox2D Box)
	{
		PendingSelectionBox = Box;
	}

	void RunSelection(FSceneView& View, const FBox2D& Box)
	{
		VOXEL_FUNCTION_COUNTER();

		SelectedTriangles.Reset();

		TSet<UVoxelMesherDebugComponent*> TouchedComponents;

		for (const TSharedPtr<FChunkEntry>& Entry : ChunkEntries)
		{
			UVoxelMesherDebugComponent* Component = Entry->WeakComponent.Get();
			if (!Component ||
				!Entry->bVisible ||
				!Entry->Mesh)
			{
				continue;
			}

			const FMatrix LocalToWorld = Component->GetComponentTransform().ToMatrixWithScale();
			const TConstVoxelArrayView<int32> Indices = Entry->Mesh->Indices;
			const TConstVoxelArrayView<FVector3f> Vertices = Entry->Mesh->Vertices;

			TVoxelArray<FVector2D> ScreenPositions;
			TVoxelArray<uint8> ScreenValid;
			ScreenPositions.SetNumUninitialized(Vertices.Num());
			ScreenValid.SetNumZeroed(Vertices.Num());

			for (int32 VIndex = 0; VIndex < Vertices.Num(); VIndex++)
			{
				const FVector WorldPos = LocalToWorld.TransformPosition(FVector(Vertices[VIndex]));
				FVector2D PixelPos;
				if (View.WorldToPixel(WorldPos, PixelPos))
				{
					ScreenPositions[VIndex] = PixelPos;
					ScreenValid[VIndex] = 1;
				}
			}

			Component->HighlightedTriangles.Reset();

			for (int32 Index = 0; Index < Indices.Num(); Index += 3)
			{
				const int32 I0 = Indices[Index + 0];
				const int32 I1 = Indices[Index + 1];
				const int32 I2 = Indices[Index + 2];

				const bool bAnyInside =
					(ScreenValid[I0] && Box.IsInside(ScreenPositions[I0])) ||
					(ScreenValid[I1] && Box.IsInside(ScreenPositions[I1])) ||
					(ScreenValid[I2] && Box.IsInside(ScreenPositions[I2]));

				if (!bAnyInside)
				{
					continue;
				}

				const int32 TriangleIndex = Index / 3;
				Component->HighlightedTriangles.Add(TriangleIndex);

				const TSharedRef<FSelectedTriangle> Selected = MakeShared<FSelectedTriangle>();
				Selected->Entry = Entry;
				Selected->TriangleIndex = TriangleIndex;
				Selected->V0World = LocalToWorld.TransformPosition(FVector(Vertices[I0]));
				Selected->V1World = LocalToWorld.TransformPosition(FVector(Vertices[I1]));
				Selected->V2World = LocalToWorld.TransformPosition(FVector(Vertices[I2]));
				SelectedTriangles.Add(Selected);
			}

			TouchedComponents.Add(Component);
		}

		for (UVoxelMesherDebugComponent* Component : TouchedComponents)
		{
			Component->MarkRenderStateDirty();
		}

		RebuildSelectionText();
	}

	void RebuildSelectionText()
	{
		// Group selected triangles by chunk.
		TMap<TSharedPtr<FChunkEntry>, TArray<int32>> ChunkToTriangles;
		for (const TSharedPtr<FSelectedTriangle>& Selected : SelectedTriangles)
		{
			ChunkToTriangles.FindOrAdd(Selected->Entry).Add(Selected->TriangleIndex);
		}

		FString Out;

		for (const TPair<TSharedPtr<FChunkEntry>, TArray<int32>>& Pair : ChunkToTriangles)
		{
			const FChunkEntry& Entry = *Pair.Key;
			const FVoxelMesh& Mesh = *Entry.Mesh;
			const TArray<int32>& TriangleIndices = Pair.Value;

			TSet<int32> UsedVertices;
			TSet<FIntVector> UsedCorners;
			for (const int32 TriIndex : TriangleIndices)
			{
				for (int32 Corner = 0; Corner < 3; Corner++)
				{
					const int32 VertexIndex = Mesh.Indices[3 * TriIndex + Corner];
					UsedVertices.Add(VertexIndex);

					const FVoxelMesh::FCell Cell = Mesh.Cells[VertexIndex];
					for (int32 CornerBit = 0; CornerBit < 8; CornerBit++)
					{
						UsedCorners.Add(FIntVector(
							Cell.X + ((CornerBit & 0x1) ? 1 : 0),
							Cell.Y + ((CornerBit & 0x2) ? 1 : 0),
							Cell.Z + ((CornerBit & 0x4) ? 1 : 0)));
					}
				}
			}

			TArray<int32> SortedVertices = UsedVertices.Array();
			SortedVertices.Sort();

			TArray<FIntVector> SortedCorners = UsedCorners.Array();
			SortedCorners.Sort([](const FIntVector& A, const FIntVector& B)
			{
				if (A.X != B.X) return A.X < B.X;
				if (A.Y != B.Y) return A.Y < B.Y;
				return A.Z < B.Z;
			});

			Out += FString::Printf(
				TEXT("== Chunk LOD %d  Offset %lld,%lld,%lld ==\n"),
				Mesh.ChunkLOD,
				Mesh.ChunkOffset.X, Mesh.ChunkOffset.Y, Mesh.ChunkOffset.Z);

			Out += TEXT("Vertices:\n");
			for (const int32 VertexIndex : SortedVertices)
			{
				const FVector3f& V = Mesh.Vertices[VertexIndex];
				Out += FString::Printf(TEXT("  %d: %f,%f,%f\n"), VertexIndex, V.X, V.Y, V.Z);
			}

			Out += TEXT("Cells:\n");
			for (const FIntVector& Corner : SortedCorners)
			{
				const FCornerSample* Sample = Entry.CornerSamples.Find(Corner);
				Out += FString::Printf(
					TEXT("  %d,%d,%d: %s\n"),
					Corner.X, Corner.Y, Corner.Z,
					Sample ? *FString::Printf(TEXT("%f"), Sample->Distance) : TEXT("(no sample)"));
			}

			Out += TEXT("Triangles:\n  ");
			for (int32 Index = 0; Index < TriangleIndices.Num(); Index++)
			{
				const int32 TriIndex = TriangleIndices[Index];
				Out += FString::Printf(
					TEXT("(%d,%d,%d)%s"),
					Mesh.Indices[3 * TriIndex + 0],
					Mesh.Indices[3 * TriIndex + 1],
					Mesh.Indices[3 * TriIndex + 2],
					Index + 1 < TriangleIndices.Num() ? TEXT(",") : TEXT(""));
			}
			Out += TEXT("\n\n");
		}

		SelectionText = MoveTemp(Out);
	}

	FReply ClearSelection()
	{
		for (const TSharedPtr<FChunkEntry>& Entry : ChunkEntries)
		{
			if (UVoxelMesherDebugComponent* Component = Entry->WeakComponent.Get())
			{
				if (Component->HighlightedTriangles.Num() > 0)
				{
					Component->HighlightedTriangles.Reset();
					Component->MarkRenderStateDirty();
				}
			}
		}

		SelectedTriangles.Reset();
		SelectionText.Empty();
		return FReply::Handled();
	}

	FReply CopySelectedTriangles()
	{
		FPlatformApplicationMisc::ClipboardCopy(*SelectionText);
		return FReply::Handled();
	}

private:
	TSharedPtr<SVoxelViewport> Viewport;
	TSharedPtr<SListView<TSharedPtr<FChunkEntry>>> ListView;
	TArray<TSharedPtr<FChunkEntry>> ChunkEntries;
	TArray<TSharedPtr<FSelectedTriangle>> SelectedTriangles;
	TOptional<FBox2D> PendingSelectionBox;
	FVector WorldPosition = FVector::ZeroVector;
	FString SelectionText;
	bool bShowCells = false;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMesherDebugEditor::Open(
	const TArray<FVoxelMesherDebugChunk>& Chunks,
	const FVector& WorldPosition,
	const TSharedRef<FVoxelLayers>& Layers,
	const TSharedRef<FVoxelSurfaceTypeTable>& SurfaceTypeTable,
	const FVoxelWeakStackLayer& WeakLayer)
{
	VOXEL_FUNCTION_COUNTER();

	const TSharedRef<SWindow> Window =
		SNew(SWindow)
		.Type(EWindowType::Normal)
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(1280, 720))
		.Title(INVTEXT("Voxel Mesher Debug"));

	Window->SetContent(SNew(SVoxelMesherDebugEditor, Chunks, WorldPosition, Layers, SurfaceTypeTable, WeakLayer));

	const TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
	if (!ensure(RootWindow))
	{
		return;
	}

	FSlateApplication::Get().AddWindowAsNativeChild(Window, RootWindow.ToSharedRef());
	Window->BringToFront();
}
