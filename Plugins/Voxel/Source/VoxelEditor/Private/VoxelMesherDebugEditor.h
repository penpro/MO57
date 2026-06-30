// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "VoxelMesherDebugEditor.generated.h"

class FVoxelMesh;
class FVoxelLayers;
class FVoxelSurfaceTypeTable;
struct FVoxelWeakStackLayer;

struct FVoxelMesherDebugChunk
{
	TSharedPtr<const FVoxelMesh> Mesh;
	FTransform ComponentTransform;
};

class FVoxelMesherDebugEditor
{
public:
	static void Open(
		const TArray<FVoxelMesherDebugChunk>& Chunks,
		const FVector& WorldPosition,
		const TSharedRef<FVoxelLayers>& Layers,
		const TSharedRef<FVoxelSurfaceTypeTable>& SurfaceTypeTable,
		const FVoxelWeakStackLayer& WeakLayer);
};

UCLASS()
class UVoxelMesherDebugComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	TSharedPtr<const FVoxelMesh> Mesh;
	FLinearColor Color = FLinearColor::White;
	TSet<int32> HighlightedTriangles;

	//~ Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End UPrimitiveComponent Interface
};
