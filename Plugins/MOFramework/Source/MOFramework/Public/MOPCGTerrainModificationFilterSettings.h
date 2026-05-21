/**
 * =============================================================================
 * MOPCGTerrainModificationFilterSettings.h - Generic point filter for modified
 * terrain zones. Insert BEFORE any spawner in a PCG graph and downstream nodes
 * never see points inside modified zones — no matter what spawner they use.
 * =============================================================================
 *
 * WHY THIS EXISTS
 *   The per-spawner filter in UMOPCGMeshSpawnerSettings /
 *   UMOPCGResourceSpawnerSettings only protects the MO custom spawn nodes.
 *   Stock UE PCG nodes (UPCGStaticMeshSpawnerSettings) and Voxel-plugin
 *   spawners go through their own paths that this project can't easily hook.
 *
 *   By making the filter a separate node that operates on POINTS, the user
 *   can insert it once in any PCG graph and every spawner downstream — stock,
 *   custom, or third-party — automatically sees the filtered point list.
 *
 * USAGE
 *   In any PCG graph that spawns foliage / resources, drop in this node
 *   between the point source (typically VoxelSampler) and the spawner
 *   chain. Points inside modified terrain zones are removed; everything
 *   else passes through unchanged.
 *
 * INVERT MODE
 *   bInvert = true makes the filter pass ONLY points INSIDE modified zones.
 *   Useful for future content like "till soil texture" or "growing crops"
 *   that should appear only where the player has worked the ground.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "MOPCGTerrainModificationFilterSettings.generated.h"

UCLASS(BlueprintType, ClassGroup=(Procedural))
class MOFRAMEWORK_API UMOPCGTerrainModificationFilterSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UMOPCGTerrainModificationFilterSettings();

	// ============================================================================
	// FILTER CONFIG
	// ============================================================================

	/**
	 * If true, the filter passes ONLY points INSIDE modified zones (inverted
	 * behavior). Use for content that should appear specifically on worked
	 * ground — e.g. crops, tilled-soil textures.
	 *
	 * Default false: pass-through points OUTSIDE zones (the normal "skip
	 * foliage on worked ground" use case).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Filter", meta=(PCG_Overridable))
	bool bInvert = false;

	// ============================================================================
	// UPCGSettings overrides
	// ============================================================================

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("MO Terrain Mod Filter"); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("MOPCG", "TerrainModFilterTitle", "MO Terrain Modification Filter"); }
	virtual FText GetNodeTooltipText() const override;
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
};

/**
 * PCG element that runs the filter. Iterates input points; for each one,
 * queries UMOTerrainModificationSubsystem::IsLocationModified and keeps or
 * drops based on bInvert.
 */
class FMOPCGTerrainModificationFilterElement : public IPCGElement
{
public:
	// Subsystem lookup needs the world, so we run on the main thread.
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	// Not cacheable because the filter depends on runtime subsystem state.
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
