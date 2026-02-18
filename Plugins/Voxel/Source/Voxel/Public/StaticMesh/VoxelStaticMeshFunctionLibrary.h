// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelObjectPinType.h"
#include "VoxelFunctionLibrary.h"
#include "Buffer/VoxelFloatBuffers.h"
#include "StaticMesh/VoxelStaticMesh.h"
#include "VoxelStaticMeshFunctionLibrary.generated.h"

class UVoxelStaticMesh;
class FVoxelStaticMeshData;

USTRUCT()
struct VOXEL_API FVoxelStaticMeshRef
{
	GENERATED_BODY()

	TVoxelObjectPtr<UVoxelStaticMesh> Object;
	TSharedPtr<const FVoxelStaticMeshData> MeshData;
};

DECLARE_VOXEL_OBJECT_PIN_TYPE(FVoxelStaticMeshRef);

USTRUCT()
struct VOXEL_API FVoxelStaticMeshRefPinType : public FVoxelObjectPinType
{
	GENERATED_BODY()

	DEFINE_VOXEL_OBJECT_PIN_TYPE(FVoxelStaticMeshRef, UVoxelStaticMesh);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UCLASS()
class VOXEL_API UVoxelStaticMeshFunctionLibrary : public UVoxelFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Get the distance to a voxel mesh asset
	 * @param Mesh				The mesh to sample
	 * @param Position			Position
	 * @param bUseTricubic		If true will use tricubic sampling, which is slower but better looking
	 * @return	The height
	 */
	UFUNCTION(Category = "Static Mesh")
	UPARAM(DisplayName = "Distance") FVoxelFloatBuffer SampleVoxelStaticMesh(
		const FVoxelStaticMeshRef& Mesh,
		UPARAM(meta = (PositionPin)) const FVoxelVectorBuffer& Position,
		float Scale = 1.f,
		bool bUseTricubic = true) const;

	UFUNCTION(Category = "Static Mesh")
	FVoxelBox GetVoxelStaticMeshBounds(
		const FVoxelStaticMeshRef& Mesh,
		float Scale = 1.f) const;
};