// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class FVoxelVolumeSculptEditor;
class FVoxelVolumeSculptInnerData;
class UVoxelVolumeSculptSaveAsset;

DECLARE_UNIQUE_VOXEL_ID(FVoxelVolumeSculptDataId);

class VOXEL_API FVoxelVolumeSculptData : public TSharedFromThis<FVoxelVolumeSculptData>
{
public:
	const FVoxelVolumeSculptDataId SculptDataId = FVoxelVolumeSculptDataId::New();
	const TSharedRef<FVoxelDependency3D> Dependency;
	const TVoxelObjectPtr<UVoxelVolumeSculptSaveAsset> SaveAsset;
	const bool bUseFastDistances;
	FSimpleMulticastDelegate OnChanged;

	explicit FVoxelVolumeSculptData(
		TVoxelObjectPtr<UVoxelVolumeSculptSaveAsset> SaveAsset,
		bool bUseFastDistances);

	explicit FVoxelVolumeSculptData(
		TVoxelObjectPtr<UVoxelVolumeSculptSaveAsset> SaveAsset,
		const TSharedRef<const FVoxelVolumeSculptInnerData>& InnerData);

	TSharedRef<const FVoxelVolumeSculptInnerData> GetInnerData() const;
	FVoxelFuture AddTask(TFunction<FVoxelBox(FVoxelVolumeSculptInnerData&)> DoWork);

private:
	FVoxelSharedCriticalSection InnerData_CriticalSection;
	TSharedRef<const FVoxelVolumeSculptInnerData> InnerData_RequiresLock;

private:
	struct FTask
	{
		TFunction<FVoxelBox(FVoxelVolumeSculptInnerData&)> DoWork;
		TSharedRef<const FVoxelInvalidationCallstack> Callstack;
		FVoxelPromise Promise;
	};
	FVoxelFuture Future_GameThread;

	void RunTask(const FTask& Task);
};