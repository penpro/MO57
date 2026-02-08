#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MOWaterTypes.h"
#include "MOWaterActorBase.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * Base class for water bodies with Gerstner wave simulation.
 * Provides wave calculation and surface queries for buoyancy.
 */
UCLASS(Abstract)
class MOFRAMEWORK_API AMOWaterActorBase : public AActor
{
	GENERATED_BODY()

public:
	AMOWaterActorBase();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ============================================================================
	// WAVE CONFIGURATION
	// ============================================================================

	/** Array of wave configurations. Multiple waves are summed for realistic appearance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water|Waves")
	TArray<FMOGerstnerWave> Waves;

	/** Global wave amplitude multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water|Waves", meta=(ClampMin="0.0"))
	float WaveAmplitudeMultiplier = 1.0f;

	/** Global wave speed multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water|Waves", meta=(ClampMin="0.0"))
	float WaveSpeedMultiplier = 1.0f;

	/** Time accumulator for wave animation. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Water|Waves")
	float WaveTime = 0.0f;

	// ============================================================================
	// MESH CONFIGURATION
	// ============================================================================

	/** Number of vertices per side of the water mesh grid. Higher = more detail but more expensive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water|Mesh", meta=(ClampMin="4", ClampMax="512"))
	int32 MeshResolution = 64;

	/** Whether to update mesh vertices on CPU every frame. Very expensive - prefer material WPO. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water|Mesh")
	bool bUpdateMeshVertices = false;

	/** Material to apply to the water surface. Should support WPO for best results. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water|Material")
	TObjectPtr<UMaterialInterface> WaterMaterial;

	// ============================================================================
	// SURFACE QUERIES
	// ============================================================================

	/**
	 * Get water surface information at a world location.
	 * @param WorldLocation The world position to query
	 * @return Surface info including height, normal, and whether location is in bounds
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Water")
	FMOWaterSurfaceInfo GetWaterSurfaceInfo(const FVector& WorldLocation) const;

	/**
	 * Get just the water surface height at a world location (faster than full query).
	 * @param WorldLocation The world position to query
	 * @return Water surface Z height at that location
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Water")
	float GetWaterHeightAtLocation(const FVector& WorldLocation) const;

	/**
	 * Check if a world location is underwater.
	 * @param WorldLocation The world position to check
	 * @return True if the location is below the water surface
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Water")
	bool IsUnderwater(const FVector& WorldLocation) const;

	/**
	 * Get the depth below water surface (negative if above water).
	 * @param WorldLocation The world position to check
	 * @return Depth below surface (positive = underwater)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Water")
	float GetDepthAtLocation(const FVector& WorldLocation) const;

	// ============================================================================
	// WAVE MATH
	// ============================================================================

	/**
	 * Calculate Gerstner wave displacement at a position.
	 * @param Position2D The XY world position
	 * @param Time Current wave time
	 * @return 3D displacement vector (XY = horizontal, Z = vertical)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Water|Math")
	FVector CalculateGerstnerDisplacement(const FVector2D& Position2D, float Time) const;

	/**
	 * Calculate Gerstner wave normal at a position.
	 * @param Position2D The XY world position
	 * @param Time Current wave time
	 * @return Surface normal vector
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Water|Math")
	FVector CalculateGerstnerNormal(const FVector2D& Position2D, float Time) const;

protected:
	// ============================================================================
	// COMPONENTS
	// ============================================================================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Water")
	TObjectPtr<UProceduralMeshComponent> WaterMeshComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WaterMaterialInstance;

	// ============================================================================
	// MESH GENERATION
	// ============================================================================

	/** Generate the water mesh grid. Override in subclasses for different shapes. */
	virtual void GenerateWaterMesh();

	/** Update mesh vertices with current wave displacement (if bUpdateMeshVertices is true). */
	virtual void UpdateMeshVertices();

	/** Get the base water level Z height. Override for different water body types. */
	virtual float GetBaseWaterLevel() const { return GetActorLocation().Z; }

	/** Check if a world XY position is within this water body's bounds. */
	virtual bool IsInWaterBounds(const FVector2D& WorldXY) const { return true; }

	/** Get the size of the water mesh. Override in subclasses. */
	virtual FVector2D GetMeshSize() const { return FVector2D(10000.0f, 10000.0f); }

	// ============================================================================
	// MATERIAL UPDATES
	// ============================================================================

	/** Update material parameters with current wave settings. */
	virtual void UpdateMaterialParameters();

	/** Create default waves if none are configured. */
	void EnsureDefaultWaves();

	// Cached mesh data for updates
	TArray<FVector> BaseVertices;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;

private:
	/** Calculate displacement from a single wave. */
	FVector CalculateSingleWaveDisplacement(const FMOGerstnerWave& Wave, const FVector2D& Position2D, float Time) const;
};
