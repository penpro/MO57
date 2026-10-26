/**
 * =============================================================================
 * MOWorldGenSettings.h - Project Settings for the C++ world generator (FMOWorldGen)
 * =============================================================================
 *
 * PURPOSE:
 * Holds the designer-facing generator tuning (Project Settings -> MOFramework -> World Generation) and turns
 * tuning + DT_Biomes into the immutable FMOWorldGenParams snapshot every consumer reads (the Voxel terrain
 * node on worker threads, the PCG biome spawner, tests, the map dump).
 *
 * WHEN THE SNAPSHOT IS (RE)PUBLISHED -- all on the game thread:
 *   - module startup (so editor graph previews and PIE have data before anything runs),
 *   - UMOWorldSeedSubsystem::RegenerateVoxelWorld (host AND client, before CreateRuntime),
 *   - the PCG biome spawner (cheap; picks up DT_Biomes edits between generations).
 * Worker threads only ever call FMOWorldGenParamsProvider::Get().
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-10] EDITING TUNING IN THE EDITOR does not regenerate a running voxel world: restart PIE (or re-run the seed apply). Hot reload through
 *   FVoxelDependency invalidation is an open item (Docs/Terrain_Generation_Exploration.md, risk 4).
 * [2026-10] Changing the generator math or any tuning default changes every world's base terrain: see RULES in MOWorldGen.h.
 *
 * =============================================================================
 * RELATED FILES: MOWorldGen.h (Core), MOTerrainFunctionLibrary.h, MOBiomeDatabaseSettings.h
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MOWorldGen.h"
#include "MOWorldCreationTypes.h"
#include "MOWorldGenSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="World Generation"))
class MOFRAMEWORK_API UMOWorldGenSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("MOFramework"); }
	virtual FName GetSectionName() const override { return TEXT("World Generation"); }

	/** Everything that shapes the generated world. Defaults live in FMOWorldGenTuning. */
	UPROPERTY(EditAnywhere, Config, Category="Generator")
	FMOWorldGenTuning Tuning;

	/**
	 * The voxel surface type painted on an upland lake's bed (a VST_ asset listed in NaniteMegaMaterial.surface_types): rock / gravel / mud instead of the surrounding biome's ground (the forest's
	 * muddy leaves under 5 m of water read as a flooded forest floor). Empty = the biome's ground continues under the water.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Water", meta=(AllowedClasses="/Script/VoxelCore.VoxelSurfaceTypeInterface"))
	FSoftObjectPath UplandLakeBedSurfaceType = FSoftObjectPath(TEXT("/Game/VoxelExamples/NaniteMaterials/Materials/DarkPebbles/VST_MI_DarkPebbles.VST_MI_DarkPebbles"));

	/**
	 * Voxel float metadata the cave stamp (VVG_MOCaves) writes 1 into on every surface it carves: rock overhead, so Ultra Dynamic Weather's wetness, puddles and snow
	 * must not reach it (BR18-16). The stamp's own blend alpha carries it, so the open ground keeps the default 0 and a cave mouth fades between. The terrain
	 * material reads it (Get Voxel Metadata in M_QuixelBase, Content/Python/upgrade_terrain_material.py). Empty = caves get the weather.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Caves", meta=(AllowedClasses="/Script/Voxel.VoxelFloatMetadata"))
	FSoftObjectPath CaveWeatherShelterMetadata = FSoftObjectPath(TEXT("/Game/Penumbra/Maps/VFM_MOWeatherShelter.VFM_MOWeatherShelter"));

	/** Build a fresh snapshot from the current settings + DT_Biomes (game thread). Does not publish. */
	static FMOWorldGenParamsRef BuildParams();

	/** BuildParams() + FMOWorldGenParamsProvider::Publish(). Returns what was published. Game thread only. */
	static FMOWorldGenParamsRef RefreshPublishedParams();

	/**
	 * The current world's shape (its creation settings, MOWorldCreationTypes.h), folded into the tuning by every BuildParams from now on. Set by
	 * UMOWorldSeedSubsystem::RegenerateVoxelWorld (host, joiner, menu backdrop) right before it publishes the params. Game thread only.
	 */
	static void SetActiveWorldShape(const FMOWorldShape& Shape);
	static const FMOWorldShape& GetActiveWorldShape();

	/** The current world's origin in the generator's field (FMOWorldCreationSettings::WorldOrigin), set beside the shape and written into every BuildParams. */
	static void SetActiveWorldOrigin(const FVector2D& Origin);
	static const FVector2D& GetActiveWorldOrigin();
};
