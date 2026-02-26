/**
 * =============================================================================
 * MOSpawnSettings.h - Spawn Manager Project Settings
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Project Settings for the Spawn Manager system. Configures spawn behavior
 * for different entity categories (Survivor, Prey, Predator, Ambient).
 * Accessible via Project Settings > Game > MO Spawn Manager.
 *
 * SETTINGS:
 * - Per-category configs: Max population, cooldowns, spawn rules
 * - Global settings: Persistence hours, check interval, query distance
 * - Debug: Cooldown multiplier, verbose logging
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] TIME MULTIPLIER: CooldownTimeMultiplier affects ALL spawn
 *   cooldowns globally. Use for testing (0.1 = 10x faster).
 *
 * [2024-02] QUERY DISTANCE: MaxSpawnPointQueryDistance in cm (200000 = 2km).
 *   Points beyond this distance from player are not considered.
 *
 * [2024-02] PERSISTENCE: SurvivorPersistenceHours determines how long
 *   interacted survivors stay in world before being cleaned up.
 *
 * =============================================================================
 * RELATED FILES: MOSpawnTypes.h, MOSpawnManagerSubsystem.h, MOSpawnPoint.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MOSpawnTypes.h"
#include "MOSpawnSettings.generated.h"

/**
 * Project Settings for the Spawn Manager system.
 * Accessible via Project Settings > Game > MO Spawn Manager
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="MO Spawn Manager"))
class MOFRAMEWORK_API UMOSpawnSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMOSpawnSettings();

	// ============================================================================
	// CATEGORY CONFIGURATIONS
	// ============================================================================

	/** Configuration for Survivor spawns */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Categories|Survivor")
	FMOSpawnCategoryConfig SurvivorConfig;

	/** Configuration for Prey spawns */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Categories|Prey")
	FMOSpawnCategoryConfig PreyConfig;

	/** Configuration for Predator spawns */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Categories|Predator")
	FMOSpawnCategoryConfig PredatorConfig;

	/** Configuration for Ambient spawns */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Categories|Ambient")
	FMOSpawnCategoryConfig AmbientConfig;

	// ============================================================================
	// GLOBAL SETTINGS
	// ============================================================================

	/** How long interacted survivors stay persistent (hours) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Global", meta = (ClampMin = "0.1"))
	float SurvivorPersistenceHours = 10.0f;

	/** How often to check spawn conditions (seconds) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Global", meta = (ClampMin = "0.1"))
	float SpawnCheckInterval = 5.0f;

	/** Maximum distance from player to consider spawn points (cm) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Global", meta = (ClampMin = "100"))
	float MaxSpawnPointQueryDistance = 200000.0f;  // 2km

	// ============================================================================
	// DEBUG / TESTING
	// ============================================================================

	/** Global time multiplier for all cooldowns (1.0 = normal, 0.1 = 10x faster for testing) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug", meta = (ClampMin = "0.001", ClampMax = "10.0"))
	float CooldownTimeMultiplier = 1.0f;

	/** If true, logs detailed spawn manager activity */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bEnableVerboseLogging = false;

	// ============================================================================
	// ACCESS
	// ============================================================================

	/** Get the spawn settings singleton */
	UFUNCTION(BlueprintPure, Category = "MO|Spawn", meta = (DisplayName = "Get MO Spawn Settings"))
	static UMOSpawnSettings* GetSpawnSettings();

	/** Get all category configs as an array (for subsystem initialization) */
	UFUNCTION(BlueprintPure, Category = "MO|Spawn")
	TArray<FMOSpawnCategoryConfig> GetAllCategoryConfigs() const;

	// UDeveloperSettings interface
	virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }
};
