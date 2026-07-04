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
 * One scatterable species inside a biome's palette (a tree, bush, rock...).
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOBiomeSpeciesEntry
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
	 * HISM component tag applied to this species' instances. The harvest /
	 * interaction path resolves what an instance IS from this tag (existing
	 * MOResource_* convention) — an untagged species is scenery the player
	 * can't interact with, so validation requires it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Species")
	FName HISMTag;

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
struct MOFRAMEWORK_API FMOBiomeDefinitionRow : public FTableRowBase
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

	/** Density cross-fade width at biome borders, in UU. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Bands", meta=(ClampMin="0.0"))
	float EdgeBlendWidth = 5000.0f;

	// --- What grows here ---

	/** Scatterable species palette. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Biome|Species")
	TArray<FMOBiomeSpeciesEntry> Species;
};
