// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Sculpt/Height/VoxelHeightSculptStampRef.h"
#include "Sculpt/Volume/VoxelVolumeSculptStampRef.h"
#include "VoxelSculptSaveAsset.generated.h"

UCLASS(meta = (VoxelAssetType, AssetColor=Red))
class VOXEL_API UVoxelHeightSculptSaveAsset : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Config")
	float ScaleXY = 100;

	// If true height will be stored relative to the previous stamp heights
	UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
	bool bRelativeHeight = false;

	// Use this if this stamp is not rendered in the Voxel World stack
	// This stack will be used during sculpting to query the distances before any sculpt is applied
	UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
	TObjectPtr<UVoxelLayerStack> StackOverride;

public:
	FSimpleMulticastDelegate OnPropertyChanged;

	TSharedRef<FVoxelHeightSculptData> GetSculptData();

	//~ Begin UVoxelVolumeSculptSaveAsset Interface
	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UVoxelVolumeSculptSaveAsset Interface

private:
	TSharedPtr<FVoxelHeightSculptData> PrivateData;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UCLASS(meta = (VoxelAssetType, AssetColor=Red))
class VOXEL_API UVoxelVolumeSculptSaveAsset : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Config")
	float Scale = 100;

	// If true will compress distances to one byte
	// Setting this will clear any existing data
	UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
	bool bUseFastDistances = false;

	// If false, edits won't be diffed
	// This make editing up to 5x faster, but will lead to obvious chunks if you move the underlying stamps after editing
	UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
	bool bEnableDiffing = true;

	// Use this if this stamp is not rendered in the Voxel World stack
	// This stack will be used during sculpting to query the distances before any sculpt is applied
	UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
	TObjectPtr<UVoxelLayerStack> StackOverride;

public:
	FSimpleMulticastDelegate OnPropertyChanged;

	TSharedRef<FVoxelVolumeSculptData> GetSculptData();

	//~ Begin UVoxelVolumeSculptSaveAsset Interface
	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UVoxelVolumeSculptSaveAsset Interface

private:
	TSharedPtr<FVoxelVolumeSculptData> PrivateData;
};