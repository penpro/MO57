/**
 * =============================================================================
 * MOBiomeDatabaseSettings.h - Biome Database Project Settings (pipeline P1)
 * =============================================================================
 *
 * Project Settings pointer to DT_Biomes plus static lookups, following the
 * UMOSkillDatabaseSettings pattern (lean variant: no category cache, no mod
 * overlay yet — add the overlay when biome modding becomes a real consumer).
 *
 * =============================================================================
 * RELATED FILES: MOBiomeDefinitionRow.h, MOSkillDatabaseSettings.h (pattern)
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MOBiomeDefinitionRow.h"

#include "MOBiomeDatabaseSettings.generated.h"

class UDataTable;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Biome Database"))
class MOFRAMEWORK_API UMOBiomeDatabaseSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// UDeveloperSettings overrides — unified under "MOFramework" section in Project Settings.
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("MOFramework"); }
	virtual FName GetSectionName() const override { return TEXT("Biome Database"); }

	/** The central DataTable containing FMOBiomeDefinitionRow rows. */
	UPROPERTY(EditAnywhere, Config, Category="Database")
	TSoftObjectPtr<UDataTable> BiomeDefinitionsDataTable;

	UDataTable* GetBiomeDefinitionsDataTable() const;

	/** Look up a biome definition by ID. Returns pointer or nullptr if not found. */
	static const FMOBiomeDefinitionRow* GetBiomeDefinition(FName BiomeId);

	/** Look up a biome definition by ID. Returns true if found. */
	UFUNCTION(BlueprintCallable, Category="MO|Biome Database", meta=(DisplayName="Get Biome Definition"))
	static bool GetBiomeDefinitionBP(FName BiomeId, FMOBiomeDefinitionRow& OutDefinition);

	/** Get all biome IDs from the database. */
	UFUNCTION(BlueprintCallable, Category="MO|Biome Database")
	static void GetAllBiomeIds(TArray<FName>& OutBiomeIds);

	/** Check if the Biome Database is properly configured. */
	UFUNCTION(BlueprintCallable, Category="MO|Biome Database")
	static bool IsConfigured();

	/** Seeded 0..1 climate noise over world XY — THE mask function; the PCG
	 *  biome spawner and any query path must share it or they drift. */
	static float ClimateNoise(const FVector& Location, float PeriodUU, int32 Seed);

	/**
	 * Resolve which biome the mask assigns at a world position (highest
	 * Priority whose bands contain the sample). Pure deterministic math over
	 * DT_Biomes — usable from tests/tools without visiting the location.
	 * Height/slope are caller-supplied (pass nominal land values for map-scale
	 * structure queries).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Biome Database")
	static FName ResolveBiomeAt(FVector Location, float Height, float SlopeDeg,
		int32 Seed, float MoistureNoisePeriod, float TemperatureNoisePeriod);

private:
	/** Cached pointer to loaded DataTable to avoid repeated loading. */
	static TWeakObjectPtr<UDataTable> CachedDataTable;
};
