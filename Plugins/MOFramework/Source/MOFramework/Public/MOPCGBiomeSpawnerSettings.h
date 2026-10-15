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
struct FISMComponentDescriptorBase;

namespace MOPCGBiomeSpawner
{
	/** The end draw distance that goes with a near floor: beyond every generation radius (the canopy tier reaches 7.5 km, x1.1 cleanup margin). It only exists to switch the floor on. */
	inline constexpr int32 NearFloorEndCullCm = 1500000;

	/**
	 * A node's near floor (MinDistanceFromViewerCm) on its components' descriptor: a per-frame minimum DRAW distance. The engine honours InstanceMinDrawDistance ONLY together with an end
	 * distance -- the Nanite and the classic instanced proxies both return no draw-distance range from GetInstanceDrawDistanceMinMax while InstanceEndCullDistance is 0 (UE 5.8
	 * NaniteResources.cpp, InstancedStaticMesh.cpp) -- so a minimum alone is silently ignored. The start distance stays 0 (a start turns on the classic proxy's per-instance fade). 0 = no floor.
	 */
	MOFRAMEWORK_API void ApplyNearDrawFloor(FISMComponentDescriptorBase& Descriptor, float MinDistanceCm);
}

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
	TreesOnly UMETA(DisplayName = "Trees only (MOResource_Tree)"),
	/** The FAR CANOPY tier: species tagged MOFar_Canopy (solid low-poly crown proxies, 3-9 km): beyond the stand-in trees, where a real tree is a few pixels and its masked leaves only alias. */
	FarCanopyOnly UMETA(DisplayName = "Far canopy only (MOFar_Canopy)")
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

	// ---- Clump fields (shared by the spawner, its tests and the stocking model in Tools/pcg_stocking.py) --------------------------------------
	/** The clump field a grouped species samples: PerlinNoise2D at the group's offset (period 4 x ClusterRadius, at least 10 m), mapped to 0..1. */
	static float ClumpField01(const FVector& Location, int32 ClusterGroup, float ClusterRadius);
	/** Fraction of the ground where ClumpField01 >= Core, measured once from the engine's own PerlinNoise2D. That noise is NOT uniform (std ~0.26
	 *  of its -1..1 range), so the field bunches around 0.5: a core of 0.7 covers ~7 % of the ground and 0.3 covers ~93 %, not 30 % and 70 %. */
	static float ClumpCoverage(float Core);
	/** Inside its clumps a grouped species' acceptance is multiplied by this so its authored per-hectare rate holds: 1 / ClumpCoverage(Core),
	 *  capped at MaxClumpConcentration. (Until Oct 14 2026 it was 1 / (1 - Core), right only for uniform noise: core 0.7 realised 23 % of the
	 *  authored rate, core 0.3 realised 133 %.) */
	static float ClumpCompensation(float Core);
	/** Mean of the ungrouped (legacy) clump multiplier clamp(2 * noise + 0.5, 0, 2): ~0.55, divided out so those species' rates hold too. */
	static float LegacyClumpMean();
	/** The most a clump may concentrate a species over its average density (a core so high that its clumps cover < 1/8 of the ground under-realises). */
	static constexpr float MaxClumpConcentration = 8.0f;

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
	 * Near floor of this node's instances (cm from the camera): they are not DRAWN closer than this (the components' InstanceMinDrawDistance, re-evaluated every frame). The FAR tiers set it to
	 * about the reach of the tier below them, so their larger, sparser stand-ins never stack on the dense near vegetation around the player wherever the cell was generated from. Every point
	 * is still planted (a generation-time cut left bald circles where the player used to stand). 0 = off. The name predates the change; it is kept so authored graphs keep their values.
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

	/**
	 * Past this distance (cm) the species' wind / world position offset stops (the component's WorldPositionOffsetDisableDistance; 0 = never). A swaying
	 * instance invalidates its virtual-shadow-map pages EVERY frame, so dense foliage re-rendered its shadows constantly: Rainforest spent 4.1 ms of GPU in
	 * shadow depths against 1.4 in Meadow (ProfileGPU, Oct 13 2026). Sway past ~80 m is barely visible; the cached shadows are not.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning", meta = (ClampMin = "0"))
	int32 WindDisableDistanceCm = 8000;
};

struct FMOPCGBiomeSpawnerContext;

/**
 * PCG Element for biome scatter. Main-thread (component creation), not
 * cacheable (reads DT_Biomes + terrain-mod state each execution).
 *
 * TIME-SLICED: the first slice plants (per-point work in parallel), the next ones add the instances a batch at a time while PCG's frame budget
 * (pcg.FrameTime, 5 ms) lasts. Adding a whole cell at once was 10-30 ms of game thread per execution (ISM AddInstances: physics bodies, navigation),
 * up to 70 ms frames while the far field filled in behind the reveal (CSV profile, Oct 13 2026).
 */
class FMOPCGBiomeSpawnerElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	virtual FPCGContext* CreateContext() override;

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

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
	/** The planting: fills the context's buckets and log fields. False = nothing to build (no target actor, no biomes, a shore cell with no water near). */
	bool Plant(FMOPCGBiomeSpawnerContext* Context) const;
	/** Adds instances until the frame budget is spent (at least one batch). True when every bucket is in. */
	bool BuildSlice(FMOPCGBiomeSpawnerContext* Context) const;
};
