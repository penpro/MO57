// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#include "Render/VoxelScreenSizeHelper.h"
#include "VoxelSubsystem.h"

FVoxelScreenSizeHelper::FVoxelScreenSizeHelper(const FVoxelConfig& Config)
{
	CameraChunkPosition =
		FVector(Config.LocalToWorld.InverseTransformPosition(Config.CameraPosition.GetValue()))
		/ Config.VoxelSize
		/ Config.RenderChunkSize;

	FFloatInterval Quality =
		Config.bIsEditorWorld && !Config.LODQuality.bAlwaysUseGameQuality
		? Config.LODQuality.EditorQuality
		: Config.LODQuality.GameQuality;

	Quality.Min *= 32 / double(Config.RenderChunkSize);
	Quality.Max *= 32 / double(Config.RenderChunkSize);

	Quality.Min = FMath::Max(Quality.Min, 0);
	Quality.Max = FMath::Max(Quality.Min, Quality.Max);

	MinQuality = Quality.Min;
	MaxQuality = Quality.Max;

	ensure(MinQuality <= MaxQuality);

	ChunkToWorld = Config.RenderChunkSize * Config.VoxelSize;
	QualityExponent = FMath::Clamp(Config.QualityExponent, 0.001f, 100.f);
}