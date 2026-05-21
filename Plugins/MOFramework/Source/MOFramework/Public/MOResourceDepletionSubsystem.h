/**
 * =============================================================================
 * MOResourceDepletionSubsystem.h - Per-resource-node yield depletion + respawn
 * =============================================================================
 *
 * PURPOSE:
 * Tracks how many of each yield (sticks, bark, loose rocks, etc.) remain on
 * each resource node instance (HISM-instance trees / rocks). When all yields
 * are depleted, the node is marked with a timestamp and "respawns" (drops out
 * of the depletion map) after RespawnHoursReal real-time hours.
 *
 * DESIGN NOTES:
 * - Keyed by a position-based string so the key stays stable across game
 *   sessions even though HISM instance indices can shift.
 * - Initial counts are rolled the first time a node is interacted with (lazy
 *   initialization) using ranges configured in InitialCountByItem.
 * - "Game closed -> reopened" handling: when persistence integration is wired
 *   in later, expired entries are pruned on load — nodes that aged past
 *   RespawnHoursReal naturally come back as "not tracked", i.e. fresh.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOResourceDepletionSubsystem.generated.h"

class UInstancedStaticMeshComponent;

/** Min/max count range used to roll initial yield counts. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOResourceYieldRange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource", meta = (ClampMin = "0"))
	int32 MinCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource", meta = (ClampMin = "0"))
	int32 MaxCount = 5;
};

/** Per-node depletion state. */
USTRUCT()
struct MOFRAMEWORK_API FMOResourceNodeDepletion
{
	GENERATED_BODY()

	/** Remaining counts per ItemId. Initialized lazily on first interaction. */
	UPROPERTY()
	TMap<FName, int32> RemainingByItem;

	/** Set when all yields hit 0 — used to compute respawn time. */
	UPROPERTY()
	FDateTime FullyDepletedAt = FDateTime::MinValue();

	bool IsFullyDepleted() const
	{
		if (RemainingByItem.Num() == 0)
		{
			return false;
		}
		for (const TPair<FName, int32>& Pair : RemainingByItem)
		{
			if (Pair.Value > 0) return false;
		}
		return true;
	}
};

UCLASS(Config = Game, DefaultConfig)
class MOFRAMEWORK_API UMOResourceDepletionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Convenience static accessor. */
	static UMOResourceDepletionSubsystem* Get(const UObject* WorldCtx);

	/**
	 * Check whether the named yield is still available on this node.
	 * Returns true if the node hasn't been tracked yet (treated as fresh) or
	 * the remaining count for this item is > 0.
	 */
	UFUNCTION(BlueprintPure, Category = "Resource Depletion")
	bool CanYield(const FString& NodeKey, FName ItemId) const;

	/**
	 * Attempt to consume one of ItemId from the node. On first call for this
	 * node, initializes the per-yield counts from InitialCountByItem. Returns
	 * true if successfully consumed, false if no remaining or no config.
	 */
	UFUNCTION(BlueprintCallable, Category = "Resource Depletion")
	bool ConsumeYield(const FString& NodeKey, FName ItemId);

	/** Stable position-based key for an HISM/ISM instance. */
	UFUNCTION(BlueprintPure, Category = "Resource Depletion")
	static FString MakeNodeKey(UInstancedStaticMeshComponent* MeshComp, int32 InstanceIndex);

	/**
	 * Hours of real time (not game time) after full depletion before the node
	 * counts as "respawned" — drops out of the map and a fresh interaction will
	 * re-roll its initial counts.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Resource Depletion",
		meta = (ClampMin = "0.01"))
	float RespawnHoursReal = 24.0f;

	/**
	 * Per-ItemId initial yield range. Examples (defaults provided):
	 *   "stick" → 0–5
	 *   "loose_rock" → 0–5
	 *   "bark" → 10–20
	 * Edit in DefaultGame.ini under [/Script/MOFramework.MOResourceDepletionSubsystem]
	 * or via Project Settings if a settings page is wired up.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Resource Depletion")
	TMap<FName, FMOResourceYieldRange> InitialCountByItem;

private:
	UPROPERTY(Transient)
	TMap<FString, FMOResourceNodeDepletion> DepletionMap;

	FTimerHandle RespawnCheckTimerHandle;

	/** Periodic sweep: prune entries whose FullyDepletedAt is older than RespawnHoursReal. */
	void CheckRespawns();

	/** Roll an initial count for an item id; 0 if not configured. */
	int32 RollInitialCount(FName ItemId) const;
};
