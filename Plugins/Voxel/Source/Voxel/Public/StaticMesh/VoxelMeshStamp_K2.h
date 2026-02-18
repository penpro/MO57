// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelMeshStampRef.h"
#include "VoxelStampBlueprintFunctionLibrary.h"
#include "VoxelMeshStamp_K2.generated.h"

////////////////////////////////////////////////////
///////// The code below is auto-generated /////////
////////////////////////////////////////////////////

UCLASS()
class VOXEL_API UVoxelMeshStamp_K2 : public UVoxelStampBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Voxel|Stamp|Casting", meta = (ExpandEnumAsExecs = "Result"))
	static FVoxelMeshStampRef CastToMeshStamp(
		const FVoxelStampRef Stamp,
		EVoxelStampCastResult& Result)
	{
		return CastToStampImpl<FVoxelMeshStamp>(Stamp, Result);
	}

	// Make a copy of this stamp
	// You can then call Set XXXX on the copy without having the original stamp be modified
	UFUNCTION(BlueprintPure, Category = "Voxel|Stamp|Mesh", DisplayName = "Get Mesh Stamp")
	static void MakeCopy(
		const FVoxelMeshStampRef Stamp,
		FVoxelMeshStampRef& Copy)
	{
		Copy = FVoxelMeshStampRef(Stamp.MakeCopy());
	}

	/**
	 * @param bUseTricubic Tricubic interpolation is ~3x slower but better looking
	 * @param Layer Layer that this stamps belong to
	 * You can control the order of layers in Layer Stacks
	 * You can select the layer stack to use in your Voxel World or PCG Sampler settings
	 * @param Priority Priority of the stamp within its layer
	 * Higher priority stamps will be applied last
	 * @param LODRange This stamp will only be applied on LODs within this range (inclusive)
	 * @param bDisableStampSelection If true you won\'t be able to select this stamp by clicking on it
	 * @param bApplyOnVoid If false, this stamp will only apply on parts where another stamp has been applied first
	 * This is useful to avoid having stamps going beyond world bounds
	 * Only used if BlendMode is not Override nor Intersect
	 * @param BoundsExtension By how much to extend the bounds, relative to the bounds size
	 * Increase this if you are using a high smoothness
	 * Increasing this will lead to more stamps being sampled per voxel, increasing generation cost
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Stamp|Mesh", DisplayName = "Make Voxel Mesh Stamp", meta = (Keywords = "Construct, Create", bUseTricubic = "True", Layer = "/Script/Voxel.VoxelVolumeLayer'/Voxel/Default/DefaultVolumeLayer.DefaultVolumeLayer'", AutoCreateRefTerm = "AdditionalLayers", Behavior = "AffectAll", Smoothness = "100.000000", LODRange = "(Min=0,Max=32)", bApplyOnVoid = "True", BoundsExtension = "1.000000"))
	static void Make(
		FVoxelMeshStampRef& Stamp,
		UPARAM(DisplayName = "Mesh") UVoxelStaticMesh* NewMesh,
		UVoxelSurfaceTypeInterface* SurfaceType,
		bool bUseTricubic,
		UVoxelVolumeLayer* Layer,
		EVoxelVolumeBlendMode BlendMode,
		TArray<UVoxelVolumeLayer*> AdditionalLayers,
		FTransform Transform,
		EVoxelStampBehavior Behavior,
		int32 Priority,
		float Smoothness,
		FVoxelMetadataOverrides MetadataOverrides,
		FVoxelExposedSeed StampSeed,
		UPARAM(DisplayName = "LOD Range") FInt32Interval LODRange,
		bool bDisableStampSelection,
		bool bApplyOnVoid,
		float BoundsExtension)
	{
		Stamp = FVoxelMeshStampRef::New();
		Stamp->NewMesh = NewMesh;
		Stamp->SurfaceType = SurfaceType;
		Stamp->bUseTricubic = bUseTricubic;
		Stamp->Layer = Layer;
		Stamp->BlendMode = BlendMode;
		Stamp->AdditionalLayers = AdditionalLayers;
		Stamp->Transform = Transform;
		Stamp->Behavior = Behavior;
		Stamp->Priority = Priority;
		Stamp->Smoothness = Smoothness;
		Stamp->MetadataOverrides = MetadataOverrides;
		Stamp->StampSeed = StampSeed;
		Stamp->LODRange = LODRange;
		Stamp->bDisableStampSelection = bDisableStampSelection;
		Stamp->bApplyOnVoid = bApplyOnVoid;
		Stamp->BoundsExtension = BoundsExtension;
	}

	/**
	 * @param bUseTricubic Tricubic interpolation is ~3x slower but better looking
	 * @param Layer Layer that this stamps belong to
	 * You can control the order of layers in Layer Stacks
	 * You can select the layer stack to use in your Voxel World or PCG Sampler settings
	 * @param Priority Priority of the stamp within its layer
	 * Higher priority stamps will be applied last
	 * @param LODRange This stamp will only be applied on LODs within this range (inclusive)
	 * @param bDisableStampSelection If true you won\'t be able to select this stamp by clicking on it
	 * @param bApplyOnVoid If false, this stamp will only apply on parts where another stamp has been applied first
	 * This is useful to avoid having stamps going beyond world bounds
	 * Only used if BlendMode is not Override nor Intersect
	 * @param BoundsExtension By how much to extend the bounds, relative to the bounds size
	 * Increase this if you are using a high smoothness
	 * Increasing this will lead to more stamps being sampled per voxel, increasing generation cost
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Stamp|Mesh", DisplayName = "Break Voxel Mesh Stamp", meta = (Keywords = "Break", AutoCreateRefTerm = "AdditionalLayers"))
	static void Break(
		const FVoxelMeshStampRef Stamp,
		UPARAM(DisplayName = "Mesh") UVoxelStaticMesh*& NewMesh,
		UVoxelSurfaceTypeInterface*& SurfaceType,
		bool& bUseTricubic,
		UVoxelVolumeLayer*& Layer,
		EVoxelVolumeBlendMode& BlendMode,
		TArray<UVoxelVolumeLayer*>& AdditionalLayers,
		FTransform& Transform,
		EVoxelStampBehavior& Behavior,
		int32& Priority,
		float& Smoothness,
		FVoxelMetadataOverrides& MetadataOverrides,
		FVoxelExposedSeed& StampSeed,
		UPARAM(DisplayName = "LOD Range") FInt32Interval& LODRange,
		bool& bDisableStampSelection,
		bool& bApplyOnVoid,
		float& BoundsExtension)
	{
		NewMesh = FVoxelUtilities::MakeSafe<UVoxelStaticMesh*>();
		SurfaceType = FVoxelUtilities::MakeSafe<UVoxelSurfaceTypeInterface*>();
		bUseTricubic = FVoxelUtilities::MakeSafe<bool>();
		Layer = FVoxelUtilities::MakeSafe<UVoxelVolumeLayer*>();
		BlendMode = FVoxelUtilities::MakeSafe<EVoxelVolumeBlendMode>();
		AdditionalLayers = FVoxelUtilities::MakeSafe<TArray<UVoxelVolumeLayer*>>();
		Transform = FVoxelUtilities::MakeSafe<FTransform>();
		Behavior = FVoxelUtilities::MakeSafe<EVoxelStampBehavior>();
		Priority = FVoxelUtilities::MakeSafe<int32>();
		Smoothness = FVoxelUtilities::MakeSafe<float>();
		MetadataOverrides = FVoxelUtilities::MakeSafe<FVoxelMetadataOverrides>();
		StampSeed = FVoxelUtilities::MakeSafe<FVoxelExposedSeed>();
		LODRange = FVoxelUtilities::MakeSafe<FInt32Interval>();
		bDisableStampSelection = FVoxelUtilities::MakeSafe<bool>();
		bApplyOnVoid = FVoxelUtilities::MakeSafe<bool>();
		BoundsExtension = FVoxelUtilities::MakeSafe<float>();

		if (!Stamp.IsValid())
		{
			VOXEL_MESSAGE(Error, "Stamp is invalid");
			return;
		}

		NewMesh = Stamp->NewMesh;
		SurfaceType = Stamp->SurfaceType;
		bUseTricubic = Stamp->bUseTricubic;
		Layer = Stamp->Layer;
		BlendMode = Stamp->BlendMode;
		AdditionalLayers = Stamp->AdditionalLayers;
		Transform = Stamp->Transform;
		Behavior = Stamp->Behavior;
		Priority = Stamp->Priority;
		Smoothness = Stamp->Smoothness;
		MetadataOverrides = Stamp->MetadataOverrides;
		StampSeed = Stamp->StampSeed;
		LODRange = Stamp->LODRange;
		bDisableStampSelection = Stamp->bDisableStampSelection;
		bApplyOnVoid = Stamp->bApplyOnVoid;
		BoundsExtension = Stamp->BoundsExtension;
	}

	// Get Mesh
	UFUNCTION(BlueprintPure, Category = "Voxel|Stamp|Mesh", DisplayName = "Get New Mesh")
	static void GetNewMesh(
		UPARAM(Required) FVoxelMeshStampRef Stamp,
		UPARAM(DisplayName = "Mesh") UVoxelStaticMesh*& NewMesh)
	{
		NewMesh = FVoxelUtilities::MakeSafe<UVoxelStaticMesh*>();

		if (!Stamp.IsValid())
		{
			VOXEL_MESSAGE(Error, "Stamp is invalid");
			return;
		}

		NewMesh = Stamp->NewMesh;
	}

	// Set Mesh
	// This will automatically refresh the stamp
	UFUNCTION(BlueprintCallable, Category = "Voxel|Stamp|Mesh", DisplayName = "Set New Mesh")
	static void SetNewMesh(
		UPARAM(Required) FVoxelMeshStampRef Stamp,
		UPARAM(DisplayName = "Stamp") FVoxelMeshStampRef& OutStamp,
		UPARAM(DisplayName = "Mesh") UVoxelStaticMesh* NewMesh)
	{
		OutStamp = Stamp;

		if (!Stamp.IsValid())
		{
			VOXEL_MESSAGE(Error, "Stamp is invalid");
			return;
		}

		Stamp->NewMesh = NewMesh;
		Stamp.Update();
	}

	// Get Surface Type
	UFUNCTION(BlueprintPure, Category = "Voxel|Stamp|Mesh", DisplayName = "Get Surface Type")
	static void GetSurfaceType(
		UPARAM(Required) FVoxelMeshStampRef Stamp,
		UVoxelSurfaceTypeInterface*& SurfaceType)
	{
		SurfaceType = FVoxelUtilities::MakeSafe<UVoxelSurfaceTypeInterface*>();

		if (!Stamp.IsValid())
		{
			VOXEL_MESSAGE(Error, "Stamp is invalid");
			return;
		}

		SurfaceType = Stamp->SurfaceType;
	}

	// Set Surface Type
	// This will automatically refresh the stamp
	UFUNCTION(BlueprintCallable, Category = "Voxel|Stamp|Mesh", DisplayName = "Set Surface Type")
	static void SetSurfaceType(
		UPARAM(Required) FVoxelMeshStampRef Stamp,
		UPARAM(DisplayName = "Stamp") FVoxelMeshStampRef& OutStamp,
		UVoxelSurfaceTypeInterface* SurfaceType)
	{
		OutStamp = Stamp;

		if (!Stamp.IsValid())
		{
			VOXEL_MESSAGE(Error, "Stamp is invalid");
			return;
		}

		Stamp->SurfaceType = SurfaceType;
		Stamp.Update();
	}

	/**
	 * Get Use Tricubic
	 * Tricubic interpolation is ~3x slower but better looking
	 * @param bUseTricubic Tricubic interpolation is ~3x slower but better looking
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Stamp|Mesh", DisplayName = "Get Use Tricubic")
	static void GetUseTricubic(
		UPARAM(Required) FVoxelMeshStampRef Stamp,
		bool& bUseTricubic)
	{
		bUseTricubic = FVoxelUtilities::MakeSafe<bool>();

		if (!Stamp.IsValid())
		{
			VOXEL_MESSAGE(Error, "Stamp is invalid");
			return;
		}

		bUseTricubic = Stamp->bUseTricubic;
	}

	/**
	 * Set Use Tricubic
	 * Tricubic interpolation is ~3x slower but better looking
	 * This will automatically refresh the stamp
	 * @param bUseTricubic Tricubic interpolation is ~3x slower but better looking
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Stamp|Mesh", DisplayName = "Set Use Tricubic", meta = (bUseTricubic = "True"))
	static void SetUseTricubic(
		UPARAM(Required) FVoxelMeshStampRef Stamp,
		UPARAM(DisplayName = "Stamp") FVoxelMeshStampRef& OutStamp,
		bool bUseTricubic)
	{
		OutStamp = Stamp;

		if (!Stamp.IsValid())
		{
			VOXEL_MESSAGE(Error, "Stamp is invalid");
			return;
		}

		Stamp->bUseTricubic = bUseTricubic;
		Stamp.Update();
	}
};