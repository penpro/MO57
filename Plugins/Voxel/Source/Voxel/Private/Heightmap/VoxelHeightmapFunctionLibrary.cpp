// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#include "Heightmap/VoxelHeightmapFunctionLibrary.h"
#include "Heightmap/VoxelHeightmap_Height.h"
#include "VoxelHeightmapFunctionLibraryImpl.ispc.generated.h"

void FVoxelHeightmapRefPinType::Convert(
	const bool bSetObject,
	TVoxelObjectPtr<UVoxelHeightmap>& OutObject,
	UVoxelHeightmap& InObject,
	FVoxelHeightmapRef& Struct)
{
	if (bSetObject)
	{
		OutObject = Struct.Object;
	}
	else
	{
		const UVoxelHeightmap_Height* Height = InObject.Height;
		if (!ensure(Height))
		{
			return;
		}

		Struct.Object = InObject;
		Struct.ScaleXY = InObject.ScaleXY;
		Struct.ScaleZ = Height->ScaleZ;
		Struct.OffsetZ = Height->OffsetZ;
		Struct.HeightData = Height->GetData();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelFloatBuffer UVoxelHeightmapFunctionLibrary::SampleHeightmap(
	const FVoxelHeightmapRef& Heightmap,
	const FVoxelVector2DBuffer& Position,
	const bool bUseBicubic) const
{
	if (!Heightmap.HeightData)
	{
		VOXEL_MESSAGE(Error, "{0}: Heightmap is null", this);
		return {};
	}

	FVoxelVector2DBuffer LocalPosition = Position;
	LocalPosition.ExpandConstants();

	FVoxelFloatBuffer Result;
	Result.Allocate(LocalPosition.Num());

	float ScaleZ;
	float OffsetZ;
	FVoxelUtilities::CombineScaleAndOffset(
		Heightmap.HeightData->InternalScaleZ,
		Heightmap.HeightData->InternalOffsetZ,
		Heightmap.ScaleZ,
		Heightmap.OffsetZ,
		ScaleZ,
		OffsetZ);

	if (Heightmap.HeightData->bIsUINT16)
	{
		ispc::VoxelHeightmapFunctionLibrary_SampleHeightmap_uint16(
			LocalPosition.X.GetData(),
			LocalPosition.Y.GetData(),
			Result.GetData(),
			LocalPosition.Num(),
			bUseBicubic,
			ScaleZ / MAX_uint16,
			OffsetZ,
			Heightmap.ScaleXY,
			Heightmap.HeightData->SizeX,
			Heightmap.HeightData->SizeY,
			Heightmap.HeightData->RawData.View<uint16>().GetData());
	}
	else
	{
		ispc::VoxelHeightmapFunctionLibrary_SampleHeightmap_float(
			LocalPosition.X.GetData(),
			LocalPosition.Y.GetData(),
			Result.GetData(),
			LocalPosition.Num(),
			bUseBicubic,
			ScaleZ,
			OffsetZ,
			Heightmap.ScaleXY,
			Heightmap.HeightData->SizeX,
			Heightmap.HeightData->SizeY,
			Heightmap.HeightData->RawData.View<float>().GetData());
	}

	return Result;
}

FIntPoint UVoxelHeightmapFunctionLibrary::GetHeightmapSize(const FVoxelHeightmapRef& Heightmap) const
{
	if (!Heightmap.HeightData)
	{
		VOXEL_MESSAGE(Error, "{0}: Heightmap is null", this);
		return FIntPoint(ForceInit);
	}

	return FIntPoint(
		Heightmap.HeightData->SizeX,
		Heightmap.HeightData->SizeY);
}