// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#include "StaticMesh/VoxelStaticMeshFunctionLibrary.h"
#include "StaticMesh/VoxelStaticMeshData.h"
#include "VoxelStaticMeshFunctionLibraryImpl.ispc.generated.h"

void FVoxelStaticMeshRefPinType::Convert(
	const bool bSetObject,
	TVoxelObjectPtr<UVoxelStaticMesh>& OutObject,
	UVoxelStaticMesh& InObject,
	FVoxelStaticMeshRef& Struct)
{
	if (bSetObject)
	{
		OutObject = Struct.Object;
	}
	else
	{
		Struct.Object = InObject;
		Struct.MeshData = InObject.GetData();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelFloatBuffer UVoxelStaticMeshFunctionLibrary::SampleVoxelStaticMesh(
	const FVoxelStaticMeshRef& Mesh,
	const FVoxelVectorBuffer& Position,
	const float Scale,
	const bool bUseTricubic) const
{
	if (!Mesh.MeshData)
	{
		VOXEL_MESSAGE(Error, "{0}: Mesh is null", this);
		return {};
	}

	FVoxelVectorBuffer LocalPosition = Position;
	LocalPosition.ExpandConstants();

	FVoxelFloatBuffer Result;
	Result.Allocate(LocalPosition.Num());

	ispc::VoxelStaticMeshFunctionLibrary_SampleVoxelStaticMesh(
		LocalPosition.X.GetData(),
		LocalPosition.Y.GetData(),
		LocalPosition.Z.GetData(),
		Result.GetData(),
		LocalPosition.Num(),
		Scale,
		bUseTricubic,
		GetISPCValue(Mesh.MeshData->Size),
		Mesh.MeshData->VoxelSize,
		GetISPCValue(Mesh.MeshData->Origin),
		Mesh.MeshData->DistanceField.GetData());

	return Result;
}

FVoxelBox UVoxelStaticMeshFunctionLibrary::GetVoxelStaticMeshBounds(
	const FVoxelStaticMeshRef& Mesh,
	const float Scale) const
{
	if (!Mesh.MeshData)
	{
		VOXEL_MESSAGE(Error, "{0}: Mesh is null", this);
		return {};
	}

	return Mesh.MeshData->MeshBounds.Extend(1).Scale(Mesh.MeshData->VoxelSize).Scale(Scale);
}