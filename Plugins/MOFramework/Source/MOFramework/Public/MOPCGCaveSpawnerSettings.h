/**
 * MOPCGCaveSpawnerSettings.h - the cave scatter: rocks, rubble, floor gravel and ore on the surfaces of the generator's caves
 *
 * Wes, Oct 17 2026: "pcg inside caves, rocks, rocky materials on walls and ceilings, rock and dirt floors, different ores can also be found ... I still
 * want realism". The node needs no sampler upstream: it finds the cave surfaces under its PCG cell itself (FMOWorldGen::FindCaveSurfaces -- the same
 * cave field the voxel stamp carves, so co-op machines agree), and plants the rows of a DT of FMOCaveScatterRow on them: each row says which surface
 * (floor / wall / ceiling), which bedrock, and which ore must be in the rock behind the point (FMOWorldGen::SampleOreIn: coal seams in the coal measures,
 * hematite and chert in limestone, copper in basalt and at granite margins, tin in granite). Harvestable rows (ResourceNodeId) get the same tag bundle
 * and interaction registration as the biome spawner's trees and rocks -- it IS the biome spawner's build (this class only overrides the planting).
 *
 * Removal ledger: removed instances are checked with their height (FMOWorldRemovalLedger keys a 1 m Z cell), so the ceiling rock over a quarried floor
 * stone stays.
 */
#pragma once

#include "CoreMinimal.h"
#include "MOPCGBiomeSpawnerSettings.h"
#include "MOWorldGen.h"
#include "MOBiomeDefinitionRow.h"
#include "Engine/DataTable.h"
#include "MOPCGCaveSpawnerSettings.generated.h"

class UStaticMesh;
class UMaterialInterface;

/** One kind of thing the cave scatter places (a row of the cave scatter DT). */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOCaveScatterRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Which surfaces carry it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cave")
	EMOCaveSurfaceKind Surface = EMOCaveSurfaceKind::Floor;

	/** The bedrock it belongs to; empty = any rock. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cave")
	TArray<EMOBedrock> Bedrock;

	/** None = scenery. Otherwise it stands only where the rock just behind the surface holds this ore (at least MinOreGrade). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cave")
	EMOOreKind Ore = EMOOreKind::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cave", meta=(ClampMin="0", ClampMax="1"))
	float MinOreGrade = 0.2f;

	/** Instances per square metre of that surface (where it qualifies). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cave", meta=(ClampMin="0"))
	float PerSquareMetre = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Optional material for every slot (a recoloured rock: coal, hematite, malachite). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
	TSoftObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual", meta=(ClampMin="0.01"))
	float ScaleMin = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual", meta=(ClampMin="0.01"))
	float ScaleMax = 1.2f;

	/** 0 = upright (world Z up), 1 = along the surface's normal (a slab lying on a sloping floor, a rock stuck to a wall or hanging from the ceiling). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual", meta=(ClampMin="0", ClampMax="1"))
	float AlignToSurface = 1.0f;

	/** Pushed this far into the rock (cm, x scale): a rubble stone half buried, an ore lump set into the wall. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
	float SinkCm = 10.0f;

	/** DT_Resources row: makes it harvestable (tags, actions, yields). None = decorative. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Harvest")
	FName ResourceNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Harvest")
	EMOSpeciesCollision Collision = EMOSpeciesCollision::Auto;

	/** Decorative tag (MOCave_Rubble, MOCave_Gravel); harvestable rows take their resource's tags instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Harvest")
	FName HISMTag;
};

UCLASS(BlueprintType, ClassGroup=(MO))
class MOFRAMEWORK_API UMOPCGCaveSpawnerSettings : public UMOPCGBiomeSpawnerSettings
{
	GENERATED_BODY()

public:
	virtual FPCGElementPtr CreateElement() const override;

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("MO Cave Spawner")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("MOFramework", "MOCaveSpawnerTitle", "MO Cave Spawner"); }
	virtual FText GetNodeTooltipText() const override;
#endif

	// Input: the inherited points pin. Connect the tier's upstream to it: the points are ignored (the node finds the cave surfaces of its own cell), but the
	// connection gives the node its hierarchical grid -- a node with nothing upstream would run on the graph's default grid.

	/** The rows to plant (FMOCaveScatterRow). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave", meta = (RequiredAssetDataTags = "RowStructure=/Script/MOFramework.MOCaveScatterRow"))
	TObjectPtr<UDataTable> CaveScatterTable;

	/** One column traced through the cave band every this many cm (each surface point stands for its column's footprint over the surface's tilt). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave", meta = (PCG_Overridable, ClampMin = "25"))
	float ColumnSpacingCm = 100.0f;

	/** Outcrop rows: one ground column every this many cm is tested for bare rock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave", meta = (PCG_Overridable, ClampMin = "50"))
	float OutcropSpacingCm = 200.0f;

	/** Outcrop rows: ground at least this steep (over 3 m) is bare rock -- the soil has washed off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave", meta = (PCG_Overridable, ClampMin = "5", ClampMax = "85"))
	float OutcropMinSlopeDeg = 30.0f;

	/** Multiplies every row's PerSquareMetre (mo.PCG.CaveDensityScale multiplies again). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave", meta = (PCG_Overridable, ClampMin = "0"))
	float DensityScale = 1.0f;
};

class FMOPCGCaveSpawnerElement : public FMOPCGBiomeSpawnerElement
{
protected:
	virtual bool Plant(FMOPCGBiomeSpawnerContext* Context) const override;
};
