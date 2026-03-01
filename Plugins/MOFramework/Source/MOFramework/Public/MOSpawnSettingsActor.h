/**
 * =============================================================================
 * MOSpawnSettingsActor.h - World-Placed Mob Spawning Configuration
 * =============================================================================
 *
 * PURPOSE:
 * A placeable actor that configures all mob spawning settings for the level.
 * Place one in your persistent level and tweak settings in the Details panel.
 *
 * USAGE:
 * 1. Place BP_SpawnSettings (or this class) in your level
 * 2. Configure category settings in the Details panel
 * 3. MOSpawnManagerSubsystem will automatically find and use these settings
 *
 * If no actor is placed, the system uses MOSpawnSettings (Project Settings).
 *
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MOSpawnTypes.h"
#include "MOSpawnSettingsActor.generated.h"

/**
 * Global spawn manager settings.
 */
USTRUCT(BlueprintType)
struct FMOSpawnGlobalSettings
{
	GENERATED_BODY()

	/** How long interacted survivors stay persistent (hours). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Global", meta=(ClampMin="0.1", Units="hours"))
	float SurvivorPersistenceHours = 10.0f;

	/** How often to check spawn conditions (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Global", meta=(ClampMin="0.1", Units="s"))
	float SpawnCheckInterval = 5.0f;

	/** Maximum distance from player to consider spawn points (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Global", meta=(ClampMin="100", Units="cm"))
	float MaxSpawnPointQueryDistance = 200000.0f;  // 2km

	/** Minimum distance from player before entities can be despawned (cm). Entities closer than this won't despawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Global", meta=(ClampMin="0", Units="cm"))
	float MinDespawnDistance = 100000.0f;  // 1km - mobs won't pop out of existence in front of you
};

/**
 * Debug/testing settings for spawn manager.
 */
USTRUCT(BlueprintType)
struct FMOSpawnDebugSettings
{
	GENERATED_BODY()

	/** Global time multiplier for all cooldowns (1.0 = normal, 0.1 = 10x faster for testing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug", meta=(ClampMin="0.001", ClampMax="10.0"))
	float CooldownTimeMultiplier = 1.0f;

	/** If true, logs detailed spawn manager activity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
	bool bEnableVerboseLogging = false;
};

/**
 * World actor for configuring all mob spawning settings.
 * Place one in your level and configure in the Details panel.
 */
UCLASS(Blueprintable, meta=(DisplayName="Spawn Settings"))
class MOFRAMEWORK_API AMOSpawnSettingsActor : public AActor
{
	GENERATED_BODY()

public:
	AMOSpawnSettingsActor();

	// ============================================================================
	// CATEGORY CONFIGURATIONS
	// ============================================================================

	/** Configuration for Survivor spawns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Categories|Survivor")
	FMOSpawnCategoryConfig SurvivorConfig;

	/** Configuration for Prey spawns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Categories|Prey")
	FMOSpawnCategoryConfig PreyConfig;

	/** Configuration for Predator spawns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Categories|Predator")
	FMOSpawnCategoryConfig PredatorConfig;

	/** Configuration for Ambient spawns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Categories|Ambient")
	FMOSpawnCategoryConfig AmbientConfig;

	// ============================================================================
	// GLOBAL SETTINGS
	// ============================================================================

	/** Global spawn manager settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Global")
	FMOSpawnGlobalSettings GlobalSettings;

	/** Debug and testing settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
	FMOSpawnDebugSettings DebugSettings;

	// ============================================================================
	// STATIC ACCESS
	// ============================================================================

	/**
	 * Find the spawn settings actor in the world.
	 * Returns nullptr if none exists (fall back to Project Settings).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Spawn", meta=(WorldContext="WorldContextObject"))
	static AMOSpawnSettingsActor* GetSpawnSettingsActor(const UObject* WorldContextObject);

	/**
	 * Get survivor spawn config (from actor if exists, otherwise Project Settings).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Spawn", meta=(WorldContext="WorldContextObject"))
	static FMOSpawnCategoryConfig GetSurvivorConfig(const UObject* WorldContextObject);

	/**
	 * Get prey spawn config (from actor if exists, otherwise Project Settings).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Spawn", meta=(WorldContext="WorldContextObject"))
	static FMOSpawnCategoryConfig GetPreyConfig(const UObject* WorldContextObject);

	/**
	 * Get predator spawn config (from actor if exists, otherwise Project Settings).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Spawn", meta=(WorldContext="WorldContextObject"))
	static FMOSpawnCategoryConfig GetPredatorConfig(const UObject* WorldContextObject);

	/**
	 * Get ambient spawn config (from actor if exists, otherwise Project Settings).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Spawn", meta=(WorldContext="WorldContextObject"))
	static FMOSpawnCategoryConfig GetAmbientConfig(const UObject* WorldContextObject);

	/**
	 * Get config for a specific category.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Spawn", meta=(WorldContext="WorldContextObject"))
	static FMOSpawnCategoryConfig GetCategoryConfig(const UObject* WorldContextObject, EMOSpawnCategory Category);

	/**
	 * Get all category configs as an array.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Spawn", meta=(WorldContext="WorldContextObject"))
	static TArray<FMOSpawnCategoryConfig> GetAllCategoryConfigs(const UObject* WorldContextObject);

	/**
	 * Get global spawn settings (from actor if exists, otherwise Project Settings).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Spawn", meta=(WorldContext="WorldContextObject"))
	static FMOSpawnGlobalSettings GetGlobalSettings(const UObject* WorldContextObject);

	/**
	 * Get debug settings (from actor if exists, otherwise Project Settings).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Spawn", meta=(WorldContext="WorldContextObject"))
	static FMOSpawnDebugSettings GetDebugSettings(const UObject* WorldContextObject);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** Cached instance for fast lookup (includes world validation). */
	static TWeakObjectPtr<AMOSpawnSettingsActor> CachedInstance;
	static TWeakObjectPtr<UWorld> CachedWorld;

	/** Register this actor as the settings provider. */
	void RegisterAsSettingsProvider();

	/** Unregister when destroyed. */
	void UnregisterAsSettingsProvider();

	/** Initialize default category values. */
	void InitializeDefaults();
};
