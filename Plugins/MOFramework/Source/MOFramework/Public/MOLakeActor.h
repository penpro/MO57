#pragma once

#include "CoreMinimal.h"
#include "MOWaterActorBase.h"
#include "MOLakeActor.generated.h"

/**
 * Bounded lake/pond water body.
 * Can be manually sized or automatically detect bounds from voxel world.
 */
UCLASS(Blueprintable)
class MOFRAMEWORK_API AMOLakeActor : public AMOWaterActorBase
{
	GENERATED_BODY()

public:
	AMOLakeActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// ============================================================================
	// LAKE CONFIGURATION
	// ============================================================================

	/** Size of the lake in X direction (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Lake", meta=(ClampMin="100.0"))
	float LakeSizeX = 2000.0f;

	/** Size of the lake in Y direction (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Lake", meta=(ClampMin="100.0"))
	float LakeSizeY = 2000.0f;

	/** Water level Z offset from actor location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Lake")
	float WaterLevelOffset = 0.0f;

	/** Whether to automatically detect lake bounds from voxel world. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Lake|Voxel")
	bool bAutoDetectFromVoxel = false;

	/** Maximum search radius when auto-detecting lake bounds (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Lake|Voxel", meta=(ClampMin="100.0", EditCondition="bAutoDetectFromVoxel"))
	float MaxDetectionRadius = 10000.0f;

	/** Sample spacing when detecting voxel bounds (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Lake|Voxel", meta=(ClampMin="10.0", EditCondition="bAutoDetectFromVoxel"))
	float VoxelSampleSpacing = 100.0f;

	/** Minimum depth below water level for a point to be considered part of the lake (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Lake|Voxel", meta=(ClampMin="0.0", EditCondition="bAutoDetectFromVoxel"))
	float MinDepthThreshold = 50.0f;

	// ============================================================================
	// SHAPE OPTIONS
	// ============================================================================

	/** If true, use an elliptical shape instead of rectangular. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Lake|Shape")
	bool bEllipticalShape = false;

	/** Edge falloff distance for wave dampening near shore (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Lake|Shape", meta=(ClampMin="0.0"))
	float EdgeFalloffDistance = 200.0f;

	// ============================================================================
	// RUNTIME FUNCTIONS
	// ============================================================================

	/** Manually trigger voxel-based bound detection. */
	UFUNCTION(BlueprintCallable, Category="MO|Lake")
	void DetectBoundsFromVoxel();

	/** Set lake size directly. */
	UFUNCTION(BlueprintCallable, Category="MO|Lake")
	void SetLakeSize(float NewSizeX, float NewSizeY);

protected:
	virtual float GetBaseWaterLevel() const override;
	virtual bool IsInWaterBounds(const FVector2D& WorldXY) const override;
	virtual FVector2D GetMeshSize() const override;

	/** Override to generate mesh with optional elliptical shape. */
	virtual void GenerateWaterMesh() override;

	/** Calculate wave amplitude multiplier based on distance from edge. */
	float GetEdgeFalloffMultiplier(const FVector2D& LocalXY) const;

private:
	/** Cached actor location for bounds checking. */
	FVector2D CachedCenter;

	/** Update cached values. */
	void UpdateCachedValues();

	/** Check if voxel at world position is empty (water can exist there). */
	bool IsVoxelEmptyAt(const FVector& WorldPosition) const;

	/** Find the extent of empty voxels in one direction from center. */
	float FindEmptyExtent(const FVector& Center, const FVector2D& Direction) const;
};
