// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelStampComponentBase.h"
#include "Sculpt/VoxelSculptActorBase.h"
#include "Sculpt/Volume/VoxelVolumeSculptStampRef.h"
#include "VoxelVolumeSculptActor.generated.h"

struct FVoxelVolumeSculptSave;
class UVoxelVolumeSculptSaveAsset;

UCLASS()
class VOXEL_API UVoxelVolumeSculptComponent : public UVoxelStampComponentBase
{
	GENERATED_BODY()

public:
	FVoxelVolumeSculptStampRef GetStamp() const;

	//~ Begin UVoxelStampComponentBase Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual FVoxelStampRef GetStamp_Internal() const override;
#if WITH_EDITOR
	using Super::CanEditChange;
	virtual bool CanEditChange(const FEditPropertyChain& PropertyChain) const override;
#endif
	//~ End UVoxelStampComponentBase Interface

private:
	UPROPERTY(EditAnywhere, Category = "Config", DisplayName = "Stamp", meta = (ShowOnlyInnerProperties, HideTransform))
	FVoxelVolumeSculptStampRef PrivateStamp;

	// Optional, if set save data will be stored externally
	// Make a new sculpt asset & drag it into scene to use
	UPROPERTY(VisibleAnywhere, Category = "Config")
	TObjectPtr<UVoxelVolumeSculptSaveAsset> ExternalSaveAsset;

	mutable FSharedVoidPtr OnExternalSaveAssetPropertyChanged;

	friend class UVoxelStampComponent;
	friend class AVoxelVolumeSculptActor;
};

UCLASS()
class VOXEL_API AVoxelVolumeSculptActor : public AVoxelSculptActorBase
{
	GENERATED_BODY()

public:
	AVoxelVolumeSculptActor();

public:
	UFUNCTION(BlueprintCallable, Category = "Voxel")
	FVoxelVolumeSculptStampRef GetStamp() const;

	UFUNCTION(BlueprintCallable, Category = "Voxel")
	UVoxelVolumeSculptSaveAsset* GetExternalSaveAsset() const;

	// Will clear existing data
	UFUNCTION(BlueprintCallable, Category = "Voxel")
	void SetExternalSaveAsset(UVoxelVolumeSculptSaveAsset* NewExternalSaveAsset);

public:
	FVoxelFuture ApplyModifier(const TSharedRef<FVoxelVolumeModifier>& Modifier);
	FVoxelFuture ClearSculptData();
	virtual void ClearSculptCache() override;

public:
	TVoxelFuture<FVoxelVolumeSculptSave> GetSave(bool bCompress = true) const;
	FVoxelFuture LoadFromSave(const FVoxelVolumeSculptSave& Save);

private:
	UPROPERTY(VisibleAnywhere, Category = "Config")
	TObjectPtr<UVoxelVolumeSculptComponent> Component;
};