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

	/** Build a fresh snapshot from the current settings + DT_Biomes (game thread). Does not publish. */
	static FMOWorldGenParamsRef BuildParams();

	/** BuildParams() + FMOWorldGenParamsProvider::Publish(). Returns what was published. Game thread only. */
	static FMOWorldGenParamsRef RefreshPublishedParams();
};
