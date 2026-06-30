// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelEditorMinimal.h"

#include "VoxelMesh.h"
#include "VoxelQuery.h"
#include "VoxelState.h"
#include "VoxelWorld.h"
#include "VoxelLayers.h"
#include "VoxelRuntime.h"
#include "VoxelValuesDump.h"
#include "VoxelEditorSettings.h"
#include "VoxelMesherDebugEditor.h"
#include "SVoxelStampDeltaList.h"
#include "Render/VoxelMeshComponent.h"
#include "Render/VoxelMeshRenderProxy.h"
#include "Collision/VoxelCollisionChannels.h"

#include "ToolMenus.h"
#include "LevelEditorViewport.h"
#include "LevelEditorMenuContext.h"

class FVoxelViewportContextMenu
{
public:
	static void OnConstructDynamicSection(UToolMenu* ToolMenu)
	{
		if (!GetDefault<UVoxelEditorSettings>()->bEnableViewportContextMenuActions)
		{
			return;
		}

		const ULevelEditorContextMenuContext* LevelEditorContext = ToolMenu->FindContext<ULevelEditorContextMenuContext>();
		if (!LevelEditorContext ||
			!LevelEditorContext->HitProxyActor.IsValid())
		{
			return;
		}

		const AVoxelWorld* VoxelWorld = Cast<AVoxelWorld>(LevelEditorContext->HitProxyActor.Get());
		if (!VoxelWorld)
		{
			return;
		}

		const TSharedPtr<FVoxelRuntime> Runtime = VoxelWorld->GetRuntime();
		if (!Runtime)
		{
			return;
		}

		const TSharedPtr<FVoxelState> State = Runtime->GetState();
		if (!State)
		{
			return;
		}

		FVector Start;
		FVector End;
		if (!ensure(FVoxelEditorUtilities::GetRayInfo(GCurrentLevelEditingViewportClient, Start, End)))
		{
			return;
		}

		UWorld* World = GCurrentLevelEditingViewportClient->GetWorld();
		if (!ensure(World))
		{
			return;
		}

		FHitResult HitResult;
		if (!World->LineTraceSingleByChannel(HitResult, Start, End, ECC_VoxelEditor))
		{
			return;
		}

		const FVector WorldPosition = HitResult.Location;
		const FVector LocalPosition = VoxelWorld->ActorToWorld().InverseTransformPosition(WorldPosition);

		const TVoxelArray<FVoxelStampDelta> StampDeltas = State->Layers->GetStampDeltas(
			State->Config->LayerToRender,
			LocalPosition,
			0);

		FToolMenuSection& Section = ToolMenu->AddSection("VoxelStamps", INVTEXT("Voxel Stamps"));

		if (StampDeltas.Num() > 0)
		{
			if (GVoxelStampDeltaListTabManager->IsTabOpened())
			{
				Section.AddMenuEntry(
					"VoxelStamps",
					INVTEXT("Voxel Stamps [" + LexToString(StampDeltas.Num()) + "]"),
					{},
					FSlateIcon(FVoxelEditorStyle::GetStyleSetName(), "VoxelIcon"),
					FToolUIActionChoice(MakeLambdaDelegate([StampDeltas]
					{
						GVoxelStampDeltaListTabManager->OpenStampDeltaList(StampDeltas);
					})));
			}
			else
			{
				Section.AddSubMenu(
					"VoxelStamps",
					FText::FromString("Voxel Stamps [" + LexToString(StampDeltas.Num()) + "]"),
					{},
					FNewToolMenuDelegate::CreateLambda([StampDeltas](UToolMenu* Menu)
					{
						Menu->bSearchable = false;

						TSharedPtr<SVoxelStampDeltaList> StampsListWidget;
						const TSharedRef<SWidget> Widget =
							SNew(SBox)
							.MaxDesiredHeight(400.f)
							.MinDesiredWidth(500.f)
							[
								SAssignNew(StampsListWidget, SVoxelStampDeltaList)
							];

						StampsListWidget->UpdateStamps(StampDeltas);

						FToolMenuSection& LocalSection = Menu->AddSection("Stamps");
						LocalSection.AddEntry(FToolMenuEntry::InitWidget("PickStamp", Widget, FText::GetEmpty(), false, false, true));
					}),
					false,
					FSlateIcon(FVoxelEditorStyle::GetStyleSetName(), "VoxelIcon"));
			}
		}

		Section.AddMenuEntry(
			"VoxelDump",
			INVTEXT("Dump voxel values to log"),
			{},
			FSlateIcon(FVoxelEditorStyle::GetStyleSetName(), "VoxelIcon"),
			FToolUIActionChoice(MakeLambdaDelegate([
				WeakWorld = MakeVoxelObjectPtr(World),
				LayerToRender = State->Config->LayerToRender,
				WorldPosition]
			{
				FVoxelValuesDump::Log(
					WeakWorld.Resolve_Ensured(),
					LayerToRender.Stack.Resolve_Ensured(),
					LayerToRender.Layer.Resolve_Ensured(),
					WorldPosition);
			})));

		const TArray<FVoxelMesherDebugChunk> NearbyChunks = INLINE_LAMBDA -> TArray<FVoxelMesherDebugChunk>
		{
			constexpr double SearchRadius = 1600.0;

			TArray<FVoxelMesherDebugChunk> Result;

			for (UVoxelMeshComponent* MeshComponent : TInlineComponentArray<UVoxelMeshComponent*>(LevelEditorContext->HitProxyActor.Get()))
			{
				const TSharedPtr<FVoxelMeshRenderProxy> RenderProxy = MeshComponent->GetRenderProxy();
				if (!RenderProxy)
				{
					continue;
				}

				const FBoxSphereBounds Bounds = MeshComponent->Bounds;
				const FBox Box(Bounds.Origin - Bounds.BoxExtent, Bounds.Origin + Bounds.BoxExtent);

				if (Box.ComputeSquaredDistanceToPoint(WorldPosition) > FMath::Square(SearchRadius))
				{
					continue;
				}

				Result.Add({ RenderProxy->Mesh, MeshComponent->GetComponentTransform() });
			}

			return Result;
		};

		if (NearbyChunks.Num() > 0)
		{
			Section.AddMenuEntry(
				"VoxelMesherDebug",
				INVTEXT("Debug meshing"),
				{},
				FSlateIcon(FVoxelEditorStyle::GetStyleSetName(), "VoxelIcon"),
				FToolUIActionChoice(MakeLambdaDelegate([
					NearbyChunks,
					WorldPosition,
					Layers = State->Layers,
					SurfaceTypeTable = State->SurfaceTypeTable,
					WeakLayer = State->Config->LayerToRender]
				{
					FVoxelMesherDebugEditor::Open(NearbyChunks, WorldPosition, Layers, SurfaceTypeTable, WeakLayer);
				})));
		}

#if 0
		Section.AddMenuEntry(
			"VoxelStampDebug",
			INVTEXT("Debug Voxel Stamps"),
			{},
			FSlateIcon(FVoxelEditorStyle::GetStyleSetName(), "VoxelIcon"),
			FToolUIActionChoice(MakeLambdaDelegate([=]
			{
				FVoxelStampDebug::Open(*State, WorldPosition);
			})));
#endif
	}
};

VOXEL_RUN_ON_STARTUP_EDITOR()
{
	UToolMenu* ComponentMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.ActorContextMenu");
	if (!ComponentMenu)
	{
		return;
	}

	ComponentMenu->AddDynamicSection(
		"Voxel",
		FNewToolMenuDelegate::CreateLambda([](UToolMenu* ToolMenu)
		{
			FVoxelViewportContextMenu::OnConstructDynamicSection(ToolMenu);
		}),
		FToolMenuInsert("ActorGeneral", EToolMenuInsertType::Before));
}