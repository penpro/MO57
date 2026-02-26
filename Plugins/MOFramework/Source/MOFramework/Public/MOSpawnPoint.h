/**
 * =============================================================================
 * MOSpawnPoint.h - Spawn Point Actor for Entity Spawning
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Spawn point actor placed by PCG or manually to mark valid spawn locations.
 * The SpawnManagerSubsystem queries these points when spawning entities.
 *
 * FEATURES:
 * - Category filtering (Prey, Predator, Survivor, Ambient)
 * - Spawn radius for position randomization
 * - Per-point cooldown system
 * - Selection weight for spawn probability
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] COOLDOWN: PointCooldownSeconds starts when MarkUsed() is called.
 *   IsAvailable() returns false until cooldown expires.
 *
 * [2024-02] CATEGORY MATCHING: CanSpawnCategory() checks exact match.
 *   Points cannot spawn multiple categories currently.
 *
 * [2024-02] EDITOR ONLY: SpriteComponent and RadiusVisualization are
 *   WITH_EDITORONLY_DATA - not available in packaged builds.
 *
 * =============================================================================
 * RELATED FILES: MOSpawnTypes.h, MOSpawnManagerSubsystem.h, MOPCGSpawnPointSettings.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MOSpawnTypes.h"
#include "MOSpawnPoint.generated.h"

/**
 * Spawn point actor placed by PCG or manually to mark valid spawn locations.
 * The SpawnManagerSubsystem queries these points when spawning entities.
 */
UCLASS(BlueprintType, Blueprintable)
class MOFRAMEWORK_API AMOSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AMOSpawnPoint();

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Category of entities that can spawn at this point */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point")
	EMOSpawnCategory SpawnCategory = EMOSpawnCategory::Prey;

	/** Random offset radius from this point for actual spawn location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point", meta = (ClampMin = "0"))
	float SpawnRadius = 50.0f;

	/** Whether this spawn point is currently enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point")
	bool bEnabled = true;

	/** Cooldown before this specific point can be used again (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point", meta = (ClampMin = "0"))
	float PointCooldownSeconds = 300.0f;  // 5 min

	/** Priority weight for selection (higher = more likely to be chosen) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point", meta = (ClampMin = "0.1"))
	float SelectionWeight = 1.0f;

	// ============================================================================
	// RUNTIME STATE
	// ============================================================================

	/** When this point was last used for spawning */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Spawn Point|Runtime")
	FDateTime LastUsedTime;

	// ============================================================================
	// API
	// ============================================================================

	/** Check if this spawn point is available for use */
	UFUNCTION(BlueprintPure, Category = "Spawn Point")
	bool IsAvailable() const;

	/** Check if this spawn point can spawn the given category */
	UFUNCTION(BlueprintPure, Category = "Spawn Point")
	bool CanSpawnCategory(EMOSpawnCategory Category) const;

	/** Get a random spawn location within the spawn radius */
	UFUNCTION(BlueprintPure, Category = "Spawn Point")
	FVector GetRandomSpawnLocation() const;

	/** Mark this spawn point as used, starting the cooldown */
	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	void MarkUsed();

	/** Get remaining cooldown time in seconds (0 if available) */
	UFUNCTION(BlueprintPure, Category = "Spawn Point")
	float GetRemainingCooldown() const;

protected:
#if WITH_EDITORONLY_DATA
	/** Billboard for editor visualization */
	UPROPERTY()
	TObjectPtr<class UBillboardComponent> SpriteComponent;

	/** Sphere showing spawn radius */
	UPROPERTY()
	TObjectPtr<class USphereComponent> RadiusVisualization;
#endif
};
