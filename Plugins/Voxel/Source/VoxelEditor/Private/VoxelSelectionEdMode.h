// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "Tools/UEdMode.h"
#include "Tools/LegacyEdModeInterfaces.h"
#include "VoxelSelectionEdMode.generated.h"

class AVoxelWorld;
struct FVoxelStampRuntime;

UCLASS()
class UVoxelSelectionEdModeSettings : public UObject
{
	GENERATED_BODY()
};

UCLASS()
class UVoxelSelectionEdMode : public UEdMode, public ILegacyEdModeViewportInterface
{
	GENERATED_BODY()

public:
	UVoxelSelectionEdMode();

	//~ Begin UEdMode Interface
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override
	{
		return true;
	}
	virtual bool UsesToolkits() const override
	{
		return false;
	}

	virtual bool HandleClick(FEditorViewportClient* ViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	//~ End UEdMode Interface

	//~ Begin ILegacyEdModeViewportInterface Interface
	// This mode is always active as a default mode and only handles click-selection; it doesn't need legacy
	// gizmos/widgets. Returning the interface default (true) would globally disable the new 5.8 TRS gizmos.
#if VOXEL_ENGINE_VERSION >= 508
	virtual bool RequiresLegacyViewportInteractions() const override
	{
		return false;
	}
#endif
	//~ End ILegacyEdModeViewportInterface Interface

private:
	TVoxelSet<TWeakPtr<const FVoxelStampRuntime>> IgnoredStamps;
	FVector LastViewLocation;
	FRotator LastViewRotation;

	TVoxelObjectPtr<AVoxelWorld> WeakWorld;
	TVoxelObjectPtr<UInstancedStaticMeshComponent> WeakISMC;
};