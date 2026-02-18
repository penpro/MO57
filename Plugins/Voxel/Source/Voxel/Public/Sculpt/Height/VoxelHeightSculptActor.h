// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelStampComponentBase.h"
#include "Sculpt/VoxelSculptActorBase.h"
#include "Sculpt/Height/VoxelHeightSculptStampRef.h"
#include "VoxelHeightSculptActor.generated.h"

struct FVoxelHeightSculptSave;
class UVoxelHeightSculptSaveAsset;

UCLASS()
class VOXEL_API UVoxelHeightSculptComponent : public UVoxelStampComponentBase
{
	GENERATED_BODY()

public:
	FVoxelHeightSculptStampRef GetStamp() const;

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
	FVoxelHeightSculptStampRef PrivateStamp;

	// Optional, if set save data will be stored externally
	// Make a new sculpt asset & drag it into scene to use
	UPROPERTY(VisibleAnywhere, Category = "Config")
	TObjectPtr<UVoxelHeightSculptSaveAsset> ExternalSaveAsset;

	mutable FSharedVoidPtr OnExternalSaveAssetPropertyChanged;

	friend class UVoxelStampComponent;
	friend class AVoxelHeightSculptActor;
};

UCLASS()
class VOXEL_API AVoxelHeightSculptActor : public AVoxelSculptActorBase
{
	GENERATED_BODY()

public:
	AVoxelHeightSculptActor();

public:
	UFUNCTION(BlueprintCallable, Category = "Voxel")
	FVoxelHeightSculptStampRef GetStamp() const;

	UFUNCTION(BlueprintCallable, Category = "Voxel")
	UVoxelHeightSculptSaveAsset* GetExternalSaveAsset() const;

	// Will clear existing data
	UFUNCTION(BlueprintCallable, Category = "Voxel")
	void SetExternalSaveAsset(UVoxelHeightSculptSaveAsset* NewExternalSaveAsset);

public:
	FVoxelFuture ApplyModifier(const TSharedRef<FVoxelHeightModifier>& Modifier);
	FVoxelFuture ClearSculptData();
	virtual void ClearSculptCache() override;

public:
	TVoxelFuture<FVoxelHeightSculptSave> GetSave(bool bCompress = true) const;
	FVoxelFuture LoadFromSave(const FVoxelHeightSculptSave& Save);

private:
	UPROPERTY(VisibleAnywhere, Category = "Config")
	TObjectPtr<UVoxelHeightSculptComponent> Component;
};