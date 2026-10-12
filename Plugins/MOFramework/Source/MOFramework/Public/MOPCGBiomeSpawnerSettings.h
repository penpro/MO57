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
 *      moisture/temperature from the shared world generator (FMOWorldGen -- the
 *      same climate the MO Terrain voxel node paints the ground from) or, with
 *      bUseWorldGenerator off, from legacy seeded noise; candidate rows from
 *      DT_Biomes (highest Priority wins).
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

/**
 * Which species of the biome palettes a spawner node handles. One graph runs this node at SEVERAL hierarchical grid sizes (the PCG hi-gen idea: small things on a small, near grid; big things on a larger grid
 * that reaches further): ground cover (grass, flowers, debris) at GRID16 reaches 32 m, large species (trees, shrubs, bushes, boulders) at GRID128 reach 256 m. The split is by the species' HISMTag, so a new
 * species needs no flag: MOResource_Tree / MOResource_Rock / MOResource_Bush / MOBiomeCover_Shrub are LARGE, everything else is ground cover.
 * The biome rows' WATER-EDGE species (reeds, rushes) are NOT part of ground cover / large: they belong to their own tier, WaterEdgeOnly, which runs on a much denser sampler (a reed belt wants about one clump per square
 * metre at the waterline, far more than the ground-cover sampler emits) and skips every cell with no water in or near it. All = everything (the legacy single node).
 */
UENUM(BlueprintType)
enum class EMOBiomeSpeciesFilter : uint8
{
	All UMETA(DisplayName = "All species"),
	GroundCoverOnly UMETA(DisplayName = "Ground cover only (grass, flowers, debris)"),
	LargeOnly UMETA(DisplayName = "Large only (trees, shrubs, bushes, boulders)"),
	WaterEdgeOnly UMETA(DisplayName = "Water-edge species only (reeds, rushes, shore flowers)"),
	/** The FAR ground cover tier: species whose HISMTag starts with MOFar_ and is MOFar_Grass (sparse, larger tufts that carry the grass out to hundreds of metres past the dense near tier). */
	FarGroundCoverOnly UMETA(DisplayName = "Far ground cover only (MOFar_Grass)"),
	/** The FAR large tier: species tagged MOFar_Tree (stand-ins for distant trees: proxies, low LODs). */
	FarLargeOnly UMETA(DisplayName = "Far large only (MOFar_Tree)"),
	/** TREES only (HISMTag MOResource_Tree), on their own large hierarchical grid so the forest reaches far past the shrubs, rocks and undergrowth of the LargeOnly tier. */
	TreesOnly UMETA(DisplayName = "Trees only (MOResource_Tree)")
};

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

	/**
	 * Resolve moisture/temperature through the shared world generator (FMOWorldGen, the same math and the same terrain seed as the MO Terrain
	 * voxel node), so vegetation follows the terrain's own climate and biome map. Temperature gets the lapse rate for the point's height.
	 * OFF = the legacy per-spawner noise below (kept so old worlds/graphs and A/B comparisons still work).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (PCG_Overridable))
	bool bUseWorldGenerator = true;

	/** LEGACY (only when bUseWorldGenerator is off): moisture noise period in UU (low frequency = large coherent regions).
	 *  A biome region spans roughly half a period — 300k UU ≈ 1.5 km regions
	 *  (look-review verdict 2026-07-04: the small test regions read as noise,
	 *  real biomes should be 10-50x that). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (PCG_Overridable, ClampMin = "1000"))
	float MoistureNoisePeriod = 300000.0f;

	/** LEGACY (only when bUseWorldGenerator is off): temperature noise period in UU. */
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

	/**
	 * Drop points closer than this (2D, cm) to the local viewer at generation time. The FAR tiers set it to about the near tier's radius so their larger, sparser stand-ins do not stack on top of
	 * the dense near vegetation around the player (cells are generated once, where the player stood then). 0 = off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Density", meta = (PCG_Overridable, ClampMin = "0"))
	float MinDistanceFromViewerCm = 0.0f;

	/** Which species this node places (see EMOBiomeSpeciesFilter): run one node per tier on its own hierarchical grid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Density", meta = (PCG_Overridable))
	EMOBiomeSpeciesFilter SpeciesFilter = EMOBiomeSpeciesFilter::All;

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

	/**
	 * Collision profile for DECORATIVE species (no ResourceNodeId: grass, flowers). Harvestable species (trees, rocks, bushes) take the profile of their resource type from
	 * DT_Resources (FMOResourceNodeDefinitionRow::GetPlacementCollisionProfile): this node used to give EVERY species its one profile, which was NoCollision, so every tree
	 * and rock it placed could be walked through.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
	FCollisionProfileName DecorativeCollisionProfile = FCollisionProfileName(TEXT("NoCollision"));

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
		EMOSpeciesCollision Collision = EMOSpeciesCollision::Auto;
		TArray<FTransform> Transforms;
	};
};
