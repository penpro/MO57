// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelObjectPinType.h"
#include "VoxelFunctionLibrary.h"
#include "Buffer/VoxelFloatBuffers.h"
#include "Heightmap/VoxelHeightmap.h"
#include "VoxelHeightmapFunctionLibrary.generated.h"

struct FVoxelHeightmap_HeightData;

USTRUCT()
struct VOXEL_API FVoxelHeightmapRef
{
	GENERATED_BODY()

	TVoxelObjectPtr<UVoxelHeightmap> Object;
	float ScaleXY = 0;
	float ScaleZ = 0;
	float OffsetZ = 0;
	TSharedPtr<const FVoxelHeightmap_HeightData> HeightData;
};

DECLARE_VOXEL_OBJECT_PIN_TYPE(FVoxelHeightmapRef);

USTRUCT()
struct VOXEL_API FVoxelHeightmapRefPinType : public FVoxelObjectPinType
{
	GENERATED_BODY()

	DEFINE_VOXEL_OBJECT_PIN_TYPE(FVoxelHeightmapRef, UVoxelHeightmap);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UCLASS()
class VOXEL_API UVoxelHeightmapFunctionLibrary : public UVoxelFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Sample a heightmap asset
	 * Will return NaN if Position is outside the heightmap bounds
	 * @param Heightmap			The heightmap to sample
	 * @param Position			Position, note that the heightmap ScaleXY is applied
	 * @param bUseBicubic		If true will use bicubic sampling, which is slower but better looking
	 * @return	The height
	 */
	UFUNCTION(Category = "Heightmap")
	UPARAM(DisplayName = "Height") FVoxelFloatBuffer SampleHeightmap(
		const FVoxelHeightmapRef& Heightmap,
		UPARAM(meta = (PositionPin)) const FVoxelVector2DBuffer& Position,
		bool bUseBicubic = true) const;

	UFUNCTION(Category = "Heightmap")
	FIntPoint GetHeightmapSize(const FVoxelHeightmapRef& Heightmap) const;
};