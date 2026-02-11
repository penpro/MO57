#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "MOPCGDistanceCullingSettings.generated.h"

/**
 * Source for the distance reference point.
 */
UENUM(BlueprintType)
enum class EMOPCGDistanceSource : uint8
{
	/** Use the first local player controller's pawn location. Most performant for single player. */
	FirstLocalPlayer UMETA(DisplayName = "First Local Player"),

	/** Query all active generation sources and use the nearest one. Best for runtime generation. */
	NearestGenSource UMETA(DisplayName = "Nearest Generation Source"),

	/** Use a manually specified world location. */
	ManualLocation UMETA(DisplayName = "Manual Location")
};

/**
 * PCG node that splits input points based on distance from a reference point.
 *
 * Useful for LOD systems where nearby objects get full detail and distant objects
 * get reduced detail (e.g., full tree meshes vs. billboard imposters).
 *
 * This node should come after the voxel sampler and before mesh spawners.
 */
UCLASS(BlueprintType, ClassGroup=(MO))
class MOFRAMEWORK_API UMOPCGDistanceCullingSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UMOPCGDistanceCullingSettings();

	// UPCGSettings interface
	virtual FPCGElementPtr CreateElement() const override;
	virtual bool UseSeed() const override { return true; }

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("MO Distance Culling")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("MOFramework", "MODistanceCullingTitle", "MO Distance Culling"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

public:
	// ============================================================================
	// DISTANCE SOURCE
	// ============================================================================

	/** How to determine the reference point for distance calculations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Source", meta=(PCG_Overridable))
	EMOPCGDistanceSource DistanceSource = EMOPCGDistanceSource::FirstLocalPlayer;

	/** Manual reference location (only used when DistanceSource is ManualLocation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Source", meta=(PCG_Overridable, EditCondition="DistanceSource == EMOPCGDistanceSource::ManualLocation"))
	FVector ManualReferenceLocation = FVector::ZeroVector;

	// ============================================================================
	// DISTANCE THRESHOLDS
	// ============================================================================

	/** Distance (in cm) at which points are considered "near" (full detail). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Distance", meta=(PCG_Overridable, ClampMin="0.0", Units="cm"))
	float NearDistance = 5000.0f;

	/** Distance (in cm) at which points are considered "far" (reduced detail). Points beyond this are output to Far pin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Distance", meta=(PCG_Overridable, ClampMin="0.0", Units="cm"))
	float FarDistance = 10000.0f;

	/** If true, use 2D distance (ignoring Z). Good for mostly flat terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Distance", meta=(PCG_Overridable))
	bool bUse2DDistance = false;

	// ============================================================================
	// FALLOFF
	// ============================================================================

	/** If true, points in the transition zone (between Near and Far distances) are probabilistically distributed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Falloff", meta=(PCG_Overridable))
	bool bUseFalloff = true;

	/** Random seed offset for falloff randomization. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Falloff", meta=(PCG_Overridable))
	int32 SeedOffset = 0;

	// ============================================================================
	// DEBUG
	// ============================================================================

	/** If true, adds a 'MODistance' attribute to output points with their distance from the reference. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Debug", meta=(PCG_Overridable))
	bool bOutputDistanceAttribute = false;
};

/**
 * PCG Element that executes the distance culling logic.
 */
class FMOPCGDistanceCullingElement : public IPCGElement
{
public:
	// Can run on worker threads
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return false; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; } // Not cacheable - depends on player position

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
	/** Get the reference position based on settings. Returns false if position couldn't be determined. */
	bool GetReferencePosition(FPCGContext* Context, const UMOPCGDistanceCullingSettings* Settings, FVector& OutPosition) const;

	/** Get position from first local player. */
	bool GetFirstLocalPlayerPosition(FPCGContext* Context, FVector& OutPosition) const;

	/** Get position from nearest generation source. */
	bool GetNearestGenSourcePosition(FPCGContext* Context, FVector& OutPosition) const;

	/** Fallback: Get position from execution source (PCG component location). Used in editor. */
	bool GetExecutionSourcePosition(FPCGContext* Context, FVector& OutPosition) const;
};
