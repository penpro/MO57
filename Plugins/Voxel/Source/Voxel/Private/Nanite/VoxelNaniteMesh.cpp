// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#include "Nanite/VoxelNaniteMesh.h"
#include "Render/VoxelTexturePool.h"
#include "Render/VoxelTextureManager.h"
#include "Render/VoxelRenderSubsystem.h"
#include "VoxelMesh.h"
#include "VoxelNaniteBuilder.h"

#include "Engine/StaticMesh.h"
#include "Rendering/NaniteResources.h"

DEFINE_VOXEL_COUNTER(STAT_VoxelNumNaniteMeshes);
DEFINE_VOXEL_COUNTER(STAT_VoxelNumNanitePages);
DEFINE_VOXEL_MEMORY_STAT(STAT_VoxelNaniteMemory);
DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelNaniteMesh);

TVoxelFuture<TSharedPtr<FVoxelNaniteMesh>> FVoxelNaniteMesh::Create(
	const FVoxelRenderSubsystem& Subsystem,
	const TSharedRef<FVoxelMesh>& Mesh,
	const TSharedRef<const FVoxelMegaMaterialRenderData>& MegaMaterialRenderData,
	const FVoxelChunkNeighborInfo& NeighborInfo)
{
	VOXEL_FUNCTION_COUNTER();

	const TSharedRef<FVoxelNaniteMesh> Result = MakeShareable(new FVoxelNaniteMesh(Mesh, MegaMaterialRenderData, NeighborInfo));
	Subsystem.AddGCObject(Result);
	return Result->Initialize(Subsystem);
}

FVoxelNaniteMesh::~FVoxelNaniteMesh()
{
	Voxel::GameTask([WeakStaticMesh = WeakStaticMesh]
	{
		VOXEL_FUNCTION_COUNTER();

		UStaticMesh* StaticMeshToRelease = WeakStaticMesh.Resolve();
		if (!StaticMeshToRelease)
		{
			return;
		}

		StaticMeshToRelease->ReleaseResources();

		// Wait for render thread before doing SetRenderData(nullptr) otherwise FStreamingManager::Remove crashes
		Voxel::RenderTask([=]
		{
			Voxel::GameTask([=]
			{
				UStaticMesh* StaticMeshToDestroy = WeakStaticMesh.Resolve();
				if (!StaticMeshToDestroy)
				{
					return;
				}

				StaticMeshToDestroy->SetRenderData(nullptr);
				StaticMeshToDestroy->MarkAsGarbage();
			});
		});
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelNaniteMesh::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(StaticMesh);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelNaniteMesh::FVoxelNaniteMesh(
	const TSharedRef<FVoxelMesh>& Mesh,
	const TSharedRef<const FVoxelMegaMaterialRenderData>& MegaMaterialRenderData,
	const FVoxelChunkNeighborInfo& NeighborInfo)
	: Mesh(Mesh)
	, MegaMaterialRenderData(MegaMaterialRenderData)
	, NeighborInfo(NeighborInfo)
{
}

TVoxelFuture<TSharedPtr<FVoxelNaniteMesh>> FVoxelNaniteMesh::Initialize(const FVoxelRenderSubsystem& Subsystem)
{
	VOXEL_FUNCTION_COUNTER();
	ensure(Mesh->Indices.Num() > 0);

	TVoxelArray<int32> VertexOffsets;
	TVoxelArray<int32> ClusteredIndices;
	const TSharedRef<TUniquePtr<FStaticMeshRenderData>> RenderDataRef = INLINE_LAMBDA
	{
		const TVoxelArray<FVector3f> Vertices = Mesh->GetDisplacedVertices(NeighborInfo);

		FVoxelNaniteBuilder NaniteBuilder;
		NaniteBuilder.Mesh.Positions = Vertices;
		NaniteBuilder.Mesh.Normals = Mesh->Normals;
		NaniteBuilder.Mesh.Indices = Mesh->Indices;

		NaniteBuilder.PositionPrecision = Subsystem.GetConfig().NanitePositionPrecision;
		NaniteBuilder.bCompressVertices = Subsystem.GetConfig().bCompressNaniteVertices;

		TUniquePtr<FStaticMeshRenderData> RenderData = NaniteBuilder.CreateRenderData(VertexOffsets, ClusteredIndices);

		if (RenderData.IsValid())
		{
			FStaticMeshSectionArray& Sections = RenderData->LODResources[0].Sections;
			Sections.Reset();

			// Actual material we use for rendering
			Sections.Emplace_GetRef().MaterialIndex = 0;
			// World grid material
			Sections.Emplace_GetRef().MaterialIndex = 1;

			for (int32 Index = 0; Index < Mesh->UsedSurfaceTypes.Num(); Index++)
			{
				Sections.Emplace_GetRef().MaterialIndex = 2 + Index;
			}
		}
		return MakeSharedCopy(MoveTemp(RenderData));
	};

	if (!ensure(*RenderDataRef))
	{
		return nullptr;
	}

	NaniteIndirectionTextureRef = Subsystem.GetTextureManager().GetNaniteIndirectionBufferPool().Upload_AnyThread(
		Subsystem.GetConfig().bCompressNaniteVertices
		? ClusteredIndices
		: Mesh->Indices);

	const int32 NumPages = (**RenderDataRef).NaniteResourcesPtr->NumRootPages;
	ensure(VertexOffsets.Num() == NumPages);

	NumNaniteMeshes = 1;
	NumNanitePages = NumPages;
	NaniteMemory = NumPages * NANITE_ROOT_PAGE_GPU_SIZE;

	return Voxel::GameTask(MakeStrongPtrLambda(this, [=, this]() -> TVoxelFuture<TSharedPtr<FVoxelNaniteMesh>>
	{
		StaticMesh = FVoxelNaniteBuilder::CreateStaticMesh(MoveTemp(*RenderDataRef));
		WeakStaticMesh = StaticMesh;

		if (!ensure(StaticMesh))
		{
			return nullptr;
		}

		// Add fake materials so that NumMaterials returns what we want in GetStreamingRenderAssetInfo
		{
			TArray<FStaticMaterial> StaticMaterials;
			// One for the actual material we use for rendering
			// One for the world grid material
			StaticMaterials.SetNum(2 + Mesh->UsedSurfaceTypes.Num());

			for (FStaticMaterial& StaticMaterial : StaticMaterials)
			{
				// Fix ensure in UStaticMesh::GetUVChannelData
				// Assume a default texel density of 100 for texture streaming
				StaticMaterial.UVChannelData = FMeshUVChannelInfo(100.f);
			}

			StaticMesh->SetStaticMaterials(StaticMaterials);
		}

		return Voxel::RenderTask(MakeStrongPtrLambda(this, [=, this]() -> TSharedPtr<FVoxelNaniteMesh>
		{
			const FStaticMeshRenderData* MeshRenderData = StaticMesh->GetRenderData();
			if (!ensure(MeshRenderData) ||
				!ensure(MeshRenderData->NaniteResourcesPtr))
			{
				return nullptr;
			}

			const int32 RootPageIndex = MeshRenderData->NaniteResourcesPtr->RootPageIndex;
			if (!ensure(RootPageIndex != -1))
			{
				return nullptr;
			}

			for (int32 Index = 0; Index < NumPages; Index++)
			{
				Pages.Add(FPage
				{
					RootPageIndex + Index,
					VertexOffsets[Index]
				});
			}

			return AsShared();
		}));
	}));
}