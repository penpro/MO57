// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class FVoxelHeightSculptEditor;
class FVoxelHeightSculptInnerData;
class UVoxelHeightSculptSaveAsset;

DECLARE_UNIQUE_VOXEL_ID(FVoxelHeightSculptDataId);

class VOXEL_API FVoxelHeightSculptData : public TSharedFromThis<FVoxelHeightSculptData>
{
public:
	const FVoxelHeightSculptDataId SculptDataId = FVoxelHeightSculptDataId::New();
	const TSharedRef<FVoxelDependency2D> Dependency;
	const TVoxelObjectPtr<UVoxelHeightSculptSaveAsset> SaveAsset;
	FSimpleMulticastDelegate OnChanged;

	explicit FVoxelHeightSculptData(TVoxelObjectPtr<UVoxelHeightSculptSaveAsset> SaveAsset);

	explicit FVoxelHeightSculptData(
		TVoxelObjectPtr<UVoxelHeightSculptSaveAsset> SaveAsset,
		const TSharedRef<const FVoxelHeightSculptInnerData>& InnerData);

	TSharedRef<const FVoxelHeightSculptInnerData> GetInnerData() const;
	FVoxelFuture AddTask(TFunction<FVoxelBox2D(FVoxelHeightSculptInnerData&)> DoWork);

private:
	FVoxelSharedCriticalSection InnerData_CriticalSection;
	TSharedRef<const FVoxelHeightSculptInnerData> InnerData_RequiresLock;

private:
	struct FTask
	{
		TFunction<FVoxelBox2D(FVoxelHeightSculptInnerData&)> DoWork;
		TSharedRef<const FVoxelInvalidationCallstack> Callstack;
		FVoxelPromise Promise;
	};
	FVoxelFuture Future_GameThread;

	void RunTask(const FTask& Task);
};