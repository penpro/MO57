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
 * Accessible via Project Settings > MOFramework > Spawn Manager
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="Spawn Manager"))
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
	// SURVIVOR DISCOVERY BEACON
	// ============================================================================
	//
	// When a survivor spawns, an optional actor (typically a lit campfire BP)
	// is placed nearby to act as a visual landmark — helps the player spot a
	// survivor at distance. Beacon lifecycle is tied to the survivor's record:
	// destroyed automatically when the survivor despawns / dies / is recruited.
	//
	// To enable: set SurvivorBeaconActorClass to your campfire BP (e.g.
	// /MOFramework/BP_SurvivorCampfire). If the class derives from
	// AMOCraftingStationActor, the spawn manager will also call
	// SetStationActive(true) so the fire particles + light + audio run
	// without requiring the player to manually light it.

	/**
	 * Actor class to spawn next to each survivor on spawn. None = feature off.
	 *
	 * Recommended: a Blueprint subclass of the campfire (AMOCraftingStationActor).
	 * The spawn manager auto-lights stations of that type via SetStationActive,
	 * matching the right-click "Light" menu path. Other AActor subclasses just
	 * get placed (the BP is responsible for its own visual state in BeginPlay).
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Survivor Beacon",
		meta = (DisplayName = "Survivor Beacon Actor Class"))
	TSoftClassPtr<AActor> SurvivorBeaconActorClass;

	/**
	 * Offset (cm) from the survivor's location to place the beacon.
	 * X axis is forward relative to the survivor's yaw (so X=300 puts the
	 * beacon 3m in front of her facing direction).
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Survivor Beacon",
		meta = (DisplayName = "Beacon Offset (cm)"))
	FVector SurvivorBeaconOffset = FVector(300.0f, 0.0f, 0.0f);

	/**
	 * If true, line-trace down from the offset to find ground and place the
	 * beacon there. Recommended for uneven terrain so the beacon doesn't
	 * float / sink. Disable if your beacon class handles its own ground snap.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Survivor Beacon",
		meta = (DisplayName = "Snap Beacon To Ground"))
	bool bSnapBeaconToGround = true;

	// ============================================================================
	// A SURVIVOR NEAR EVERY PLAYER (Wes, Oct 29 2026: "let's make it so there's always one within 200m with the campfire going")
	// ============================================================================
	//
	// The survivor category alone spawned one survivor in the first minute (~180 m off, a random direction), then one per ~11 min up to its
	// cap, never moved them and never despawned them: a player who walked away from the start never met another ("i haven't seen one in
	// ages", and a co-op player had never seen one). Every spawn check now makes sure each player has a wandering survivor within
	// NearbySurvivorRadiusM whose campfire burns; if not, one is spawned NearbySurvivorSpawnMinM..MaxM away (behind the player's view when
	// it can be), with her lit fire. UMOSpawnManagerSubsystem::EnsureSurvivorNearEachPlayer.

	/** Keep a wandering survivor with a lit campfire near every player. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Survivor Near Players")
	bool bKeepSurvivorNearPlayers = true;

	/** The survivor counts as near within this distance of a player (meters). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Survivor Near Players", meta = (ClampMin = "10", Units = "m"))
	float NearbySurvivorRadiusM = 200.0f;

	/** A new one is placed at least this far from every player (meters): out of the way, not popping up beside anyone. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Survivor Near Players", meta = (ClampMin = "0", Units = "m"))
	float NearbySurvivorSpawnMinM = 80.0f;

	/** ... and at most this far from the player it is for (meters; below NearbySurvivorRadiusM so she counts as near). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Survivor Near Players", meta = (ClampMin = "10", Units = "m"))
	float NearbySurvivorSpawnMaxM = 180.0f;

	/**
	 * ... and at least this far from any finished building (meters): not inside a camp. NOT the survivor category's MinDistanceFromStructure
	 * (the map's spawn settings actor: 200 m, a wilderness rule): with it, a campfire the player built ruled out every spot near them, and
	 * every survivor's own fire ruled out the ground round her (Oct 29: "no spot for one was found" for a minute beside a new campfire).
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Survivor Near Players", meta = (ClampMin = "0", Units = "m"))
	float NearbySurvivorMinFromBuildingsM = 20.0f;

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

	// UDeveloperSettings interface — show under unified "MOFramework" category in Project Settings.
	virtual FName GetCategoryName() const override { return FName(TEXT("MOFramework")); }
};
