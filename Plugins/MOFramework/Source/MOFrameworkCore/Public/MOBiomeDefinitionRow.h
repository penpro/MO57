/**
 * =============================================================================
 * MOBiomeDefinitionRow.h - Biome definition DataTable row (pipeline P1, #172)
 * =============================================================================
 *
 * PURPOSE:
 * Data-first PCG: a biome is DATA (terrain bands + species palette), not graph
 * topology. The PCG side (P2's UMOPCGBiomeSpawnerSettings) stays a thin stable
 * sampler that reads these rows; all biome variety is authored here via
 * `ue.py rows set` — no editor graph wiring per biome.
 *
 * BANDS:
 * A world-XY sample belongs to a biome when height/slope/moisture/temperature
 * all fall inside the bands (moisture/temperature come from seeded noise until
 * real climate exists). Overlaps resolve by Priority (higher wins);
 * EdgeBlendWidth softens species density across the boundary.
 *
 * VALIDATION:
 * MO.Test.ValidateData carries a Data:Biomes block — species mesh paths must
 * resolve and HISM tags must be non-empty (harvest depends on tags), bands
 * must be min<=max. Keep it green.
 *
 * =============================================================================
 * RELATED FILES: MOBiomeDatabaseSettings.h, Docs/Fable5_PCG_Path.md
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"

#include "MOBiomeDefinitionRow.generated.h"

class UStaticMesh;

/**
 * How a scattered species treats the pawn. Auto follows the resource type (solid for trees, rocks, ore and generic nodes; walk-through for bushes and herbs).
 * Passable is for things that are "trees" in the resource table but ground-level plants in the world: every Black Locust variant is 0.5 - 1.5 m tall, yet its resource node is a Tree, so
 * by resource type it blocked the pawn like an oak (you walked into a knee-high bush and stopped). It is the override for that mismatch; interaction traces still reach the species.
 * Solid forces BlockAll (a decorative log or stump with no resource node).
 */
UENUM(BlueprintType)
enum class EMOSpeciesCollision : uint8
{
	Auto		UMETA(DisplayName = "Auto (from resource type)"),
	Passable	UMETA(DisplayName = "Walk-through"),
	Solid		UMETA(DisplayName = "Solid")
};

/**
 * One scatterable species inside a biome's palette (a tree, bush, rock...).
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORKCORE_API FMOBiomeSpeciesEntry
{
	GENERATED_BODY()

	/** Mesh to scatter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Species")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Target instances per 100m x 100m at full biome strength. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Species", meta=(ClampMin="0.0"))
	float DensityPerHectare = 50.0f;

	/** Cluster radius in UU (0 = uniform scatter, no clustering). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Species", meta=(ClampMin="0.0"))
	float ClusterRadius = 0.0f;

	/**
	 * Oasis/canopy grouping: species sharing a ClusterGroup (>= 0) within a
	 * biome sample the SAME clump-noise field, so they co-locate into natural
	 * stands — trees as the top cap, bushes/saplings as undergrowth around
	 * them. -1 = independent scatter (no shared field).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Species")
	int32 ClusterGroup = -1;

	/**
	 * Where in the shared clump this species lives (only used when
	 * ClusterGroup >= 0): the clump field is 0..1 and the species spawns
	 * where field >= ClusterCore. High (0.75+) = clump cores only (canopy
	 * trees). Mid (0.5) = core + surrounding ring (undergrowth). Density is
	 * auto-compensated so the authored per-hectare rate is preserved.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Species", meta=(ClampMin="0.0", ClampMax="0.95"))
	float ClusterCore = 0.6f;

	/**
	 * HISM component tag for DECORATIVE species (no ResourceNodeId). When
	 * ResourceNodeId is set the full harvest tag bundle replaces this.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Species")
	FName HISMTag;

	/**
	 * DT_ResourceNodes row for HARVESTABLE species (trees, rocks, bushes).
	 * Applies the exact tag bundle the native resource spawner uses
	 * (Name/MOResource_/Action_/Gives_/RequiresTool_/KeepOnHarvest +
	 * ResourceNode_<Id>) and registers the interaction-subsystem mapping —
	 * biome scatter is fully interactable, not scenery. None = decorative.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Species")
	FName ResourceNodeId;

	/**
	 * Align to the terrain normal (rocks/debris). FALSE (default) = grow
	 * world-up with a small random tilt — trees on slopes must NOT lean with
	 * the surface normal or they read wonky.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Species")
	bool bAlignToSurfaceNormal = false;

	/** Max random tilt from vertical in degrees (world-up species). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Species", meta=(ClampMin="0.0", ClampMax="45.0"))
	float MaxRandomTiltDeg = 8.0f;

	/**
	 * Decorative ground cover: swept away when the ground under it is
	 * terraformed (adds the terrain-mod subsystem's "grass" AutoSweep tag).
	 * Leave FALSE for harvestable species (trees/rocks) — those persist as
	 * explicit interaction targets and are never auto-yanked by a dig.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Species")
	bool bAutoSweepOnTerraform = false;

	/** Does this species stop the pawn? Auto = by resource type (see EMOSpeciesCollision). Interaction traces still reach a walk-through species. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Species")
	EMOSpeciesCollision Collision = EMOSpeciesCollision::Auto;

	/** Random uniform scale range applied per instance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Species", meta=(ClampMin="0.01"))
	float MinScale = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Species", meta=(ClampMin="0.01"))
	float MaxScale = 1.25f;
};

/**
 * A biome: where it lives (terrain bands) and what grows there (palette).
 * Row name = BiomeId.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORKCORE_API FMOBiomeDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Display name for UI / debug. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome")
	FText DisplayName;

	// --- Terrain bands (a sample must satisfy ALL to belong) ---

	/** World-Z band in UU. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Bands")
	float HeightMin = -100000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Bands")
	float HeightMax = 100000.0f;

	/** Surface slope band in degrees (0 = flat). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Bands", meta=(ClampMin="0.0", ClampMax="90.0"))
	float SlopeMinDeg = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Bands", meta=(ClampMin="0.0", ClampMax="90.0"))
	float SlopeMaxDeg = 90.0f;

	/** Moisture band 0..1 (seeded noise until real climate exists). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Bands", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MoistureMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Bands", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MoistureMax = 1.0f;

	/** Temperature band 0..1 (seeded noise until real climate exists). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Bands", meta=(ClampMin="0.0", ClampMax="1.0"))
	float TemperatureMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Bands", meta=(ClampMin="0.0", ClampMax="1.0"))
	float TemperatureMax = 1.0f;

	/** Higher priority wins where bands overlap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Bands")
	int32 Priority = 0;

	/**
	 * Ground material painted under this biome by the MO terrain node (a UVoxelSurfaceTypeInterface asset -- typed as UObject here because
	 * this module has no Voxel dependency). None = the node writes no surface for this biome and the voxel material graph decides.
	 * The vegetation spawner never reads this; it resolves the SAME biome through FMOWorldGen, so material and species cannot disagree.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Terrain", meta=(AllowedClasses="/Script/CoreUObject.Object"))
	TSoftObjectPtr<UObject> GroundSurfaceType;

	/** Density cross-fade width at biome borders, in UU. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Bands", meta=(ClampMin="0.0"))
	float EdgeBlendWidth = 5000.0f;

	/**
	 * How rugged the ground is where this biome's CLIMATE applies: multiplies the HILLS of the terrain generator (1 = unchanged, 0.3 = prairie that stays
	 * gently rolling, 1.2 = hilly). Only biomes without a height band or a minimum slope shape the relief (see FMOWorldGenTuning::ReliefClimateBlend); for the others it is ignored.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Terrain", meta=(ClampMin="0.0"))
	float ReliefScale = 1.0f;

	/** The same for MOUNTAIN RANGES (and the ranges that rise from the coast): 0.03 keeps a prairie free of mountains, 1 = unchanged, 1.3 = more rugged. Climate-only biomes only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Terrain", meta=(ClampMin="0.0"))
	float MountainScale = 1.0f;

	// --- What grows here ---

	/** Scatterable species palette. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Species")
	TArray<FMOBiomeSpeciesEntry> Species;

	/**
	 * Species that grow at the WATER'S EDGE in this biome: reeds, rushes, sedge, damp-ground ferns. Planted ADDITIVELY to Species on dry ground within WaterEdgeBandCm above the local water surface and
	 * within ~25 m of the water -- the sea, rivers, ponds and upland lakes alike (FMOPCGWaterMask::ShoreFactor). Their density falls linearly from the waterline (full DensityPerHectare) to the edge of the band (0).
	 * Empty = nothing special grows at the water in this biome. The input sampler's emission rate (MOPCGBiomeSpawnerSettings::InputPointsPerHectare) caps the density.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Species")
	TArray<FMOBiomeSpeciesEntry> WaterEdgeSpecies;

	/** Height above the local water surface (cm) over which the water-edge species grow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Species", meta=(ClampMin="0.0"))
	float WaterEdgeBandCm = 250.0f;

	/** Band test shared by the spawner and the mask query — keep in ONE place. */
	bool Contains(float Height, float SlopeDeg, float Moisture, float Temperature) const
	{
		return Height >= HeightMin && Height <= HeightMax
			&& SlopeDeg >= SlopeMinDeg && SlopeDeg <= SlopeMaxDeg
			&& Moisture >= MoistureMin && Moisture <= MoistureMax
			&& Temperature >= TemperatureMin && Temperature <= TemperatureMax;
	}
};
