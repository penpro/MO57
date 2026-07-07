/**
 * =============================================================================
 * MOPCGBiomeSpawnerSettings.h - Biome-driven voxel-surface scatter (P2)
 * =============================================================================
 *
 * PURPOSE:
 * All-in-one PCG node (modeled on MOPCGMeshSpawnerSettings) that turns
 * voxel-surface sample points into biome vegetation/rock scatter:
 *
 *   1. Resolves the biome at each point: height/slope from the point,
 *      moisture/temperature from seeded low-frequency noise over world XY,
 *      candidate rows from DT_Biomes (highest Priority wins).
 *   2. Picks a species from the biome palette with density-scaled acceptance
 *      (DensityPerHectare vs. InputPointsPerHectare), cluster-noise weighted
 *      when ClusterRadius > 0.
 *   3. Groups accepted points by (mesh, biome) and creates managed HISM
 *      components tagged with the species HISMTag (harvest path) AND
 *      "MOBiome_<BiomeId>" (probe/QA path - the P2 gate counts these).
 *
 * DESIGN (Fable5_PCG_Path):
 * The graph stays a thin stable sampler -> this node; ALL biome variety lives
 * in DT_Biomes rows authored via `ue.py rows set`. Adding a biome or species
 * is a data edit, not graph surgery.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-07] EDGE BLEND: wired (P4 r2) — 4 climate taps at BlendW/2 feather
 *   acceptance 0.25..1 across boundaries; elevation/slope edges stay crisp.
 *   applied - bands are hard until P4 (transitions). Noise-driven masks keep
 *   borders organic in the meantime.
 *
 * [2026-07] DENSITY MATH: acceptance = DensityPerHectare / InputPointsPerHectare.
 *   InputPointsPerHectare MUST match the upstream sampler's real emission rate
 *   or absolute densities skew (relative ratios survive regardless).
 *
 * =============================================================================
 * RELATED FILES: MOBiomeDefinitionRow.h, MOPCGMeshSpawnerSettings.h (pattern)
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "MOBiomeDefinitionRow.h"
#include "MOPCGBiomeSpawnerSettings.generated.h"

class UStaticMesh;
class UInstancedStaticMeshComponent;
class UDataTable;

UCLASS(BlueprintType, ClassGroup=(MO))
class MOFRAMEWORK_API UMOPCGBiomeSpawnerSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UMOPCGBiomeSpawnerSettings();

	// UPCGSettings interface
	virtual FPCGElementPtr CreateElement() const override;
	virtual bool UseSeed() const override { return true; }

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("MO Biome Spawner")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("MOFramework", "MOBiomeSpawnerTitle", "MO Biome Spawner"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

public:
	// ============================================================================
	// BIOME MASK (moisture/temperature come from seeded noise until real climate)
	// ============================================================================

	/** Extra seed on top of the PCG component seed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (PCG_Overridable))
	int32 SeedOffset = 0;

	/** Moisture noise period in UU (low frequency = large coherent regions).
	 *  A biome region spans roughly half a period — 300k UU ≈ 1.5 km regions
	 *  (look-review verdict 2026-07-04: the small test regions read as noise,
	 *  real biomes should be 10-50x that). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (PCG_Overridable, ClampMin = "1000"))
	float MoistureNoisePeriod = 300000.0f;

	/** Temperature noise period in UU. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (PCG_Overridable, ClampMin = "1000"))
	float TemperatureNoisePeriod = 450000.0f;

	// ============================================================================
	// DENSITY
	// ============================================================================

	/**
	 * Emission rate of the UPSTREAM surface sampler, points per hectare
	 * (100m x 100m). Species acceptance = DensityPerHectare / this.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Density", meta = (PCG_Overridable, ClampMin = "1"))
	float InputPointsPerHectare = 2000.0f;

	// ============================================================================
	// SPAWNING (mirrors MOPCGMeshSpawnerSettings)
	// ============================================================================

	/** Skip points inside terraformed zones (never scatter on worked ground). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning", meta = (PCG_Overridable))
	bool bRespectTerrainModifications = true;

	/**
	 * DT_ResourceNodes table (FMOResourceNodeDefinitionRow) used to resolve
	 * species ResourceNodeId into the full harvest tag bundle. Same asset the
	 * MO Resource Spawner points at.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
	TObjectPtr<UDataTable> ResourceNodeDataTable;

	/** Register MOResource_<Id> tag-item mappings with the PCG interaction subsystem. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
	bool bRegisterWithSubsystem = true;

	/** Collision profile for the created HISM components. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
	FCollisionProfileName CollisionProfile = FCollisionProfileName(TEXT("BlockAll"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
	bool bCastShadows = true;
};

/**
 * PCG Element for biome scatter. Main-thread (component creation), not
 * cacheable (reads DT_Biomes + terrain-mod state each execution).
 */
class FMOPCGBiomeSpawnerElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
	struct FSpeciesBucket
	{
		TObjectPtr<UStaticMesh> Mesh = nullptr;
		FName HISMTag;
		FName BiomeId;
		FName ResourceNodeId;
		bool bAutoSweep = false;
		TArray<FTransform> Transforms;
	};
};
