/**
 * =============================================================================
 * MOSpawnManagerSubsystem.h - Dynamic Entity Spawn Manager
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * World subsystem that manages dynamic spawning of creatures and survivors
 * around the player. Handles spawn cooldowns, population caps, FIFO
 * despawning, and structure avoidance.
 *
 * SPAWN CATEGORIES:
 * - Survivor: Recruitable NPCs (protected after interaction)
 * - Prey: Huntable wildlife (deer, rabbits)
 * - Predator: Hostile wildlife (wolves, bears)
 * - Ambient: Background wildlife (birds, squirrels)
 *
 * SPAWN ALGORITHM:
 * 1. Timer fires based on category cooldown
 * 2. Find spawn point in valid range (min/max distance from player)
 * 3. Check structure avoidance (not near buildings)
 * 4. Spawn group (1-N pawns based on config)
 * 5. Track in FMOSpawnedEntityRecord
 * 6. If over cap, FIFO despawn oldest non-persistent
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] PERSISTENCE: Interacted survivors become persistent (won't FIFO
 *   despawn until PersistenceExpiry). Check bPersistent flag.
 *
 * [2024-02] STRUCTURE CHECK: MinDistanceFromStructure checks for any actor
 *   with UMOBuildProgressComponent. Completed buildings block spawns.
 *
 * [2024-02] SPAWN POINT VALIDATION: Uses AMOSpawnPoint actors for positions.
 *   If no spawn points, falls back to random positions.
 *
 * [2024-02] WEAK REFERENCES: FMOSpawnedEntityRecord uses TWeakObjectPtr.
 *   If pawn destroyed externally, record becomes invalid. Clean up on tick.
 *
 * [2026-02] RECRUITED PAWNS: When a pawn is recruited via ForceRecruit(),
 *   it calls RemoveFromTracking() to completely remove it from SpawnedEntities.
 *   This ensures recruited pawns can NEVER be despawned by the spawn manager.
 *   Previously used ClearEntityPersistence() which only cleared the flag,
 *   leaving recruited pawns eligible for FIFO despawn - this was a bug.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOSpawnTypes.h - Spawn category configs and records
 * - MOSpawnSettings.h - Project settings for spawn
 * - MOSpawnPoint.h - Spawn point actors
 * - MORecruitmentComponent.h - Survivor recruitment state
 *
 * LAST UPDATED: 2026-02-28
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOSpawnTypes.h"
#include "MOSpawnManagerSubsystem.generated.h"

class AMOSpawnPoint;
class APawn;
class AMOBuildableActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnEntitySpawned, APawn*, SpawnedPawn, EMOSpawnCategory, Category);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnEntityDespawned, APawn*, DespawnedPawn, EMOSpawnCategory, Category);

/**
 * World subsystem managing dynamic entity spawning.
 * See file header for spawn algorithm and pitfalls.
 */
UCLASS()
class MOFRAMEWORK_API UMOSpawnManagerSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UMOSpawnManagerSubsystem();

	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// UTickableWorldSubsystem interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Configuration for each spawn category */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Config")
	TArray<FMOSpawnCategoryConfig> CategoryConfigs;

	/** How long interacted survivors stay persistent (hours) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Config")
	float SurvivorPersistenceHours = 10.0f;

	/** How often to check spawn conditions (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Config")
	float SpawnCheckInterval = 5.0f;

	/** Maximum distance from player to consider spawn points */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Config")
	float MaxSpawnPointQueryDistance = 2000.0f;

	// ============================================================================
	// EVENTS
	// ============================================================================

	/** Fired when an entity is spawned */
	UPROPERTY(BlueprintAssignable, Category = "Spawn Events")
	FMOOnEntitySpawned OnEntitySpawned;

	/** Fired when an entity is despawned by the manager */
	UPROPERTY(BlueprintAssignable, Category = "Spawn Events")
	FMOOnEntityDespawned OnEntityDespawned;

	// ============================================================================
	// CORE API
	// ============================================================================

	/** Attempt to spawn an entity for the given category. Returns spawned pawn or nullptr. */
	UFUNCTION(BlueprintCallable, Category = "Spawn Manager")
	APawn* TrySpawnForCategory(EMOSpawnCategory Category);

	/** Force spawn at a specific location (bypasses cooldowns and spawn points) */
	UFUNCTION(BlueprintCallable, Category = "Spawn Manager")
	APawn* ForceSpawnAtLocation(EMOSpawnCategory Category, FVector Location, FRotator Rotation = FRotator::ZeroRotator);

	/** Despawn the oldest entity of the given category (respects persistence) */
	UFUNCTION(BlueprintCallable, Category = "Spawn Manager")
	bool DespawnOldest(EMOSpawnCategory Category);

	/** Despawn a specific entity */
	UFUNCTION(BlueprintCallable, Category = "Spawn Manager")
	void DespawnEntity(APawn* Pawn);

	/** Mark an entity as persistent (won't be despawned by FIFO) */
	UFUNCTION(BlueprintCallable, Category = "Spawn Manager")
	void MarkEntityPersistent(APawn* Pawn, float PersistenceHours);

	/** Remove persistence from an entity (can be despawned again) */
	UFUNCTION(BlueprintCallable, Category = "Spawn Manager")
	void ClearEntityPersistence(APawn* Pawn);

	/** Remove an entity from spawn tracking entirely (used when recruited - will NEVER despawn) */
	UFUNCTION(BlueprintCallable, Category = "Spawn Manager")
	void RemoveFromTracking(APawn* Pawn);

	/** Convert a pawn to a corpse (for failed quests) */
	UFUNCTION(BlueprintCallable, Category = "Spawn Manager")
	void ConvertToCorpse(APawn* Pawn);

	// ============================================================================
	// QUERIES
	// ============================================================================

	/** Get configuration for a category */
	UFUNCTION(BlueprintPure, Category = "Spawn Manager")
	FMOSpawnCategoryConfig GetCategoryConfig(EMOSpawnCategory Category) const;

	/** Get count of spawned entities for a category */
	UFUNCTION(BlueprintPure, Category = "Spawn Manager")
	int32 GetSpawnedCount(EMOSpawnCategory Category) const;

	/** Get remaining cooldown for a category (seconds) */
	UFUNCTION(BlueprintPure, Category = "Spawn Manager")
	float GetCategoryCooldownRemaining(EMOSpawnCategory Category) const;

	/** Check if a location is valid for spawning (not too close to structures) */
	UFUNCTION(BlueprintPure, Category = "Spawn Manager")
	bool IsLocationValidForSpawn(FVector Location, float MinStructureDistance) const;

	/** Check if a location is valid for spawning (not too close to other spawned entities) */
	UFUNCTION(BlueprintPure, Category = "Spawn Manager")
	bool IsLocationValidForSpawnNearEntities(FVector Location, float MinEntityDistance) const;

	/** Get all valid spawn points for a category near the player */
	UFUNCTION(BlueprintCallable, Category = "Spawn Manager")
	TArray<AMOSpawnPoint*> GetValidSpawnPoints(EMOSpawnCategory Category) const;

	/** Check if spawning is allowed (game state, player exists, etc.) */
	UFUNCTION(BlueprintPure, Category = "Spawn Manager")
	bool CanSpawn() const;

	/** Reload spawn settings from Project Settings. Use during development to apply changes without restarting PIE. */
	UFUNCTION(BlueprintCallable, Category = "Spawn Manager", meta=(DevelopmentOnly))
	void ReloadSettings();

protected:
	// ============================================================================
	// RUNTIME STATE
	// ============================================================================

	/** All spawned entities tracked by this subsystem */
	UPROPERTY(Transient)
	TArray<FMOSpawnedEntityRecord> SpawnedEntities;

	/** Last spawn time per category */
	UPROPERTY(Transient)
	TMap<EMOSpawnCategory, FDateTime> LastSpawnTimes;

	/** Current randomized cooldown per category */
	UPROPERTY(Transient)
	TMap<EMOSpawnCategory, float> CurrentCooldowns;

	/** Whether first spawn has occurred per category */
	UPROPERTY(Transient)
	TMap<EMOSpawnCategory, bool> HasSpawnedOnce;

	/** Accumulated time since last spawn check */
	float TimeSinceLastCheck = 0.0f;

	/** Cached spawn check interval to avoid lookups every frame */
	float CachedSpawnCheckInterval = 5.0f;

	// ============================================================================
	// INTERNAL
	// ============================================================================

	void ProcessAllCategories();
	void ProcessCategory(EMOSpawnCategory Category, const FMOSpawnCategoryConfig& Config);
	void CleanupDeadEntities();
	void UpdatePersistence();

	APawn* SpawnAtPoint(AMOSpawnPoint* Point, const FMOSpawnCategoryConfig& Config);
	APawn* SpawnPawnAtLocation(TSubclassOf<APawn> PawnClass, FVector Location, FRotator Rotation);
	APawn* TryFallbackSpawn(EMOSpawnCategory Category, const FMOSpawnCategoryConfig& Config);

	float RollNewCooldown(const FMOSpawnCategoryConfig& Config) const;
	APawn* GetPlayerPawn() const;
	void NotifyPlayerOfSpawn(APawn* SpawnedPawn);

	void InitializeDefaultConfigs();
};
