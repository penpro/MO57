/**
 * =============================================================================
 * MOTerrainFunctionLibrary.h - The "MO Terrain" Voxel graph node
 * =============================================================================
 *
 * PURPOSE:
 * One Voxel graph node that turns a world position into everything a height graph needs, by calling the
 * pure C++ generator (FMOWorldGen, MOFrameworkCore). All terrain logic lives in C++ + DataTables; the graph is
 * a stable adapter that never has to be re-tuned node by node.
 *
 *   [Get Position 2D (double), World] --> WorldPosition
 *   [Seed parameter]                  --> Seed              (wire it STRAIGHT in: no Mix Seeds, no autocast detours)
 *                                         MO Terrain Sample  --Height-------> OutputHeight.Height
 *                                                            --HeightRange--> OutputHeight.HeightRange
 *                                                            --Bounds-------> OutputHeight.Bounds
 *                                                            --SurfaceType--> OutputHeight.SurfaceType   (optional)
 *                                                            --BiomeIndex / Moisture / Temperature -----> metadata (optional)
 *
 * The stamp that runs this graph must sit at the origin with an IDENTITY transform: positions and heights are
 * absolute world values (a moved stamp would translate the terrain's inputs but not its outputs).
 *
 * PCG: the vegetation spawner (UMOPCGBiomeSpawnerSettings) resolves climate and biome through the SAME FMOWorldGen
 * functions with the SAME terrain seed, so what the terrain paints and what the spawner plants cannot drift apart.
 * To drive stock PCG graphs from the terrain instead, wire BiomeIndex/Moisture/Temperature into float metadata
 * layers and read them with the Voxel Sampler's "Query Metadata" list.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-10] Every UFUNCTION on a UVoxelFunctionLibrary subclass needs a VOXEL_REGISTER_FUNCTION line in the .cpp or the plugin logs an
 *   error at startup (non-shipping check in VoxelFunctionLibrary.cpp). Compute() is deliberately NOT a UFUNCTION so tests can call it
 *   without a graph query.
 * [2026-10] Worker threads call this: it may only read the published FMOWorldGenParams snapshot and the surface table below.
 * [2026-10] SurfaceType blends are only produced for biomes whose DT row has a GroundSurfaceType; an unwired SurfaceType pin is fine.
 *
 * =============================================================================
 * RELATED FILES: MOWorldGen.h, MOWorldGenSettings.h, Docs/Terrain_Generation_Exploration.md
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "VoxelMinimal.h"
#include "VoxelFunctionLibrary.h"
#include "Buffer/VoxelFloatBuffers.h"
#include "Buffer/VoxelDoubleBuffers.h"
#include "Surface/VoxelSurfaceTypeBlendBuffer.h"
#include "MOWorldGen.h"
#include "MOTerrainFunctionLibrary.generated.h"

UCLASS()
class MOFRAMEWORK_API UMOTerrainFunctionLibrary : public UVoxelFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Height, biome and climate at world positions, from the C++ world generator.
	 * @param WorldPosition World-space XY (cm), double precision.
	 * @param Seed The terrain seed (the graph's Seed parameter).
	 * @param Height Ground height, world Z in cm. Feed OutputHeight.Height.
	 * @param BiomeIndex Index into the biome list (sorted by priority), -1 = none. Slope is assumed flat here.
	 * @param Moisture 0..1 climate moisture.
	 * @param Temperature 0..1 climate temperature at the ground height (lapse rate applied).
	 * @param SurfaceType Per-biome ground material from the biome rows (empty where the biome has none).
	 * @param Bounds The generated world's footprint. Feed OutputHeight.Bounds.
	 * @param HeightRange Hard bounds of Height. Feed OutputHeight.HeightRange (its default of +-10 m would flatten the mountains).
	 */
	UFUNCTION(Category = "MO|Terrain", DisplayName = "MO Terrain Sample")
	void MOTerrain(
		const FVoxelDoubleVector2DBuffer& WorldPosition,
		const FVoxelSeed& Seed,
		FVoxelFloatBuffer& Height,
		FVoxelFloatBuffer& BiomeIndex,
		FVoxelFloatBuffer& Moisture,
		FVoxelFloatBuffer& Temperature,
		FVoxelSurfaceTypeBlendBuffer& SurfaceType,
		FVoxelBox2D& Bounds,
		FVoxelFloatRange& HeightRange) const;

	/** The node's whole body, graph-free (the UFUNCTION above only adds the LOD lookup). Worker-thread safe. */
	static void Compute(
		const FVoxelDoubleVector2DBuffer& WorldPosition,
		int32 Seed,
		int32 LOD,
		FVoxelFloatBuffer& Height,
		FVoxelFloatBuffer& BiomeIndex,
		FVoxelFloatBuffer& Moisture,
		FVoxelFloatBuffer& Temperature,
		FVoxelSurfaceTypeBlendBuffer& SurfaceType,
		FVoxelBox2D& Bounds,
		FVoxelFloatRange& HeightRange);

	/**
	 * Resolve each biome's GroundSurfaceType asset into a ready-made blend (game thread). Called by UMOWorldGenSettings::RefreshPublishedParams
	 * right after the params are published; the node reads the result on worker threads.
	 */
	static void RefreshSurfaceTable(const FMOWorldGenParams& Params);
};
