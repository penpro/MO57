// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Render/VoxelTextureManager.h"
#include "Render/VoxelTexturePool.h"
#include "VoxelMetadataMaterialType.h"
#include "MegaMaterial/VoxelMegaMaterialProxy.h"

#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

FVoxelTextureManager::FVoxelTextureManager(const FVoxelMegaMaterialProxy& MegaMaterialProxy)
	: MetadataIndexToMetadata(MegaMaterialProxy.GetMetadataIndexToMetadata())
{
	VOXEL_FUNCTION_COUNTER();

	ChunkIndicesBufferPool = MakeShared<FVoxelTexturePool>(
		sizeof(int32),
		PF_R32_SINT,
		TEXT("Voxel.ChunkIndices"));

	MaterialBufferPool = MakeShared<FVoxelTexturePool>(
		sizeof(FVoxelRenderMaterial),
		PF_R32G32B32A32_UINT,
		TEXT("Voxel.Materials"));

	for (const FVoxelMetadataRef& Metadata : MetadataIndexToMetadata)
	{
		const TVoxelOptional<EVoxelMetadataMaterialType> MaterialType = Metadata.GetMaterialType();
		if (!ensure(MaterialType))
		{
			continue;
		}

		const int32 TypeSize = FVoxelMetadataMaterialType::GetTypeSize(*MaterialType);
		const EPixelFormat PixelFormat = FVoxelMetadataMaterialType::GetPixelFormat(*MaterialType);

		const TSharedRef<FVoxelTexturePool> BufferPool = MakeShared<FVoxelTexturePool>(
			TypeSize,
			PixelFormat,
			"Voxel.Metadata." + Metadata.GetName());

		MetadataToBufferPool.Add_EnsureNew(Metadata, BufferPool);
	}
}

void FVoxelTextureManager::UpdateInstance(UMaterialInstanceDynamic& Instance) const
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	if (UTexture2D* Texture = ChunkIndicesBufferPool->GetTexture_GameThread())
	{
		Instance.SetTextureParameterValue(
			STATIC_FNAME("VOXEL_ChunkIndices_Texture"),
			Texture);

		Instance.SetScalarParameterValue(
			STATIC_FNAME("VOXEL_ChunkIndices_TextureSizeLog2"),
			FVoxelUtilities::ExactLog2(Texture->GetSizeX()));
	}

	if (UTexture2D* Texture = MaterialBufferPool->GetTexture_GameThread())
	{
		Instance.SetTextureParameterValue(
			STATIC_FNAME("VOXEL_Materials_Texture"),
			Texture);

		Instance.SetScalarParameterValue(
			STATIC_FNAME("VOXEL_Materials_TextureSizeLog2"),
			FVoxelUtilities::ExactLog2(Texture->GetSizeX()));
	}

	for (int32 Index = 0; Index < MetadataIndexToMetadata.Num(); Index++)
	{
		const TSharedPtr<FVoxelTexturePool> BufferPool = MetadataToBufferPool.FindRef(MetadataIndexToMetadata[Index]);
		if (!BufferPool)
		{
			continue;
		}

		UTexture2D* Texture = BufferPool->GetTexture_GameThread();
		if (!Texture)
		{
			continue;
		}

		Instance.SetTextureParameterValue(
			FName(STATIC_FNAME("VOXEL_Metadata_Texture"), Index + 1),
			Texture);

		Instance.SetScalarParameterValue(
			FName(STATIC_FNAME("VOXEL_Metadata_TextureSizeLog2"), Index + 1),
			FVoxelUtilities::ExactLog2(Texture->GetSizeX()));
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelTextureManager::ProcessUploads()
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	ChunkIndicesBufferPool->ProcessUploads();
	MaterialBufferPool->ProcessUploads();

	for (const auto& It : MetadataToBufferPool)
	{
		It.Value->ProcessUploads();
	}
}

void FVoxelTextureManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	VOXEL_FUNCTION_COUNTER();

	ChunkIndicesBufferPool->AddReferencedObjects(Collector);
	MaterialBufferPool->AddReferencedObjects(Collector);

	for (const auto& It : MetadataToBufferPool)
	{
		It.Value->AddReferencedObjects(Collector);
	}
}