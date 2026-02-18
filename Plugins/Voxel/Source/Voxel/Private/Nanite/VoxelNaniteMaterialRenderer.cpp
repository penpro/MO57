// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#include "Nanite/VoxelNaniteMaterialRenderer.h"
#include "Nanite/VoxelNaniteMesh.h"
#include "Nanite/VoxelNaniteMaterialRendererImpl.h"
#include "MegaMaterial/VoxelMegaMaterialProxy.h"
#include "MegaMaterial/VoxelMegaMaterialRenderUtilities.h"
#include "VoxelMesh.h"
#include "Render/VoxelTexturePool.h"
#include "Render/VoxelTextureManager.h"
#include "Render/VoxelRenderSubsystem.h"

#include "TextureResource.h"
#include "MaterialCachedData.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

FVoxelNaniteMaterialRenderer::FVoxelNaniteMaterialRenderer(const TSharedRef<FVoxelMegaMaterialProxy>& MegaMaterialProxy)
	: Impl(FVoxelNaniteMaterialRendererImpl::Create(MegaMaterialProxy))
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedPtr<FVoxelMaterialInstanceRef> FVoxelNaniteMaterialRenderer::GetMaterialInstance(const FVoxelMaterialRenderIndex RenderIndex) const
{
	return Impl->GetMaterialInstance(RenderIndex);
}

void FVoxelNaniteMaterialRenderer::PrepareRender(TVoxelSet<TSharedPtr<const FVoxelNaniteMesh>>&& NewMeshes)
{
	VOXEL_FUNCTION_COUNTER();

	Meshes = MoveTemp(NewMeshes);

	{
		TVoxelSet<FVoxelSurfaceType> NewUsedSurfaceTypes;
		NewUsedSurfaceTypes.Reserve(64);

		for (const TSharedPtr<const FVoxelNaniteMesh>& Mesh : Meshes)
		{
			NewUsedSurfaceTypes.Append(Mesh->Mesh->UsedSurfaceTypes);
		}

		UsedSurfaceTypes = NewUsedSurfaceTypes.Array();
	}
	UsedSurfaceTypes.AddUnique(FVoxelSurfaceType());
	UsedSurfaceTypes.Sort();

	ensure(PerPageData.Num() == 0);
	FVoxelUtilities::SetNumFast(PerPageData, 256 * 256);
	FVoxelUtilities::SetAll(PerPageData, FIntPoint(-1));

	for (const TSharedPtr<const FVoxelNaniteMesh>& Mesh : Meshes)
	{
		for (const FVoxelNaniteMesh::FPage& Page : Mesh->Pages)
		{
			if (!ensure(PerPageData.IsValidIndex(Page.Index)))
			{
				continue;
			}

			const int64 NaniteIndirectionIndex = Mesh->NaniteIndirectionTextureRef->GetIndex();
			checkVoxelSlow(NaniteIndirectionIndex <= MAX_int32);

			// See GetVertexIndexInVoxelChunk
			PerPageData[Page.Index].X = int32(NaniteIndirectionIndex) + Page.VertexOffset;
			PerPageData[Page.Index].Y = Mesh->MegaMaterialRenderData->ChunkIndicesIndex;
		}
	}
}

void FVoxelNaniteMaterialRenderer::UpdateRender(
	const FVoxelRenderSubsystem& Subsystem,
	const FTransform& NewLocalToWorld)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	{
		VOXEL_SCOPE_COUNTER("Update material instances");

		for (const auto& It : Impl->MaterialIndexToMaterialInstance)
		{
			UMaterialInstanceDynamic* Instance = It.Value->GetInstance();
			if (!ensure(Instance))
			{
				continue;
			}

			Subsystem.GetTextureManager().UpdateInstance(*Instance);
		}
	}

	UTexture2D* Texture = Subsystem.GetTextureManager().GetPerPageDataTexture();
	if (!Texture)
	{
		ensure(Meshes.Num() == 0);
		return;
	}

	FTextureResource* Resource = Texture->GetResource();
	if (!ensure(Resource))
	{
		return;
	}

	using FQueuedData = FVoxelNaniteMaterialRendererImpl::FQueuedData;

	const TSharedRef<FQueuedData> QueuedData = MakeSharedCopy(FQueuedData
	{
		Subsystem.GetMaterialInstanceRef(EVoxelMegaMaterialTarget::NaniteMaterialSelection),
		NewLocalToWorld,
		UsedSurfaceTypes,
		MoveTemp(PerPageData)
	});

	Voxel::RenderTask([Impl = Impl, Resource, QueuedData]
	{
		QueuedData->PerPageData_Texture = Resource->GetTexture2DRHI();

		Impl->QueuedData_RenderThread = QueuedData;
	});
}