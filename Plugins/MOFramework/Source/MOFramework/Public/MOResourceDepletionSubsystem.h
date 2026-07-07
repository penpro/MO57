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
#include "MOSaveDomainInterface.h"
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

/**
 * (H37) Serializable per-node depletion entry for the save game. Flattens the
 * runtime TMap<FString, FMOResourceNodeDepletion> into an array of entries so
 * the depletion state round-trips through USaveGame. The remaining-per-item map
 * is itself serializable (TMap<FName,int32>), so it is stored directly.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOResourceNodeDepletionSaveEntry
{
	GENERATED_BODY()

	/** Position-based node key (see MakeNodeKey). */
	UPROPERTY()
	FString NodeKey;

	/** Remaining counts per ItemId at save time. */
	UPROPERTY()
	TMap<FName, int32> RemainingByItem;

	/** Timestamp the node became fully depleted (MinValue if not depleted). */
	UPROPERTY()
	FDateTime FullyDepletedAt = FDateTime::MinValue();
};

/**
 * (H37) Whole-subsystem depletion save payload. Held in UMOWorldSaveGame.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOResourceDepletionSaveData
{
	GENERATED_BODY()

	/** All tracked node depletion entries. */
	UPROPERTY()
	TArray<FMOResourceNodeDepletionSaveEntry> Entries;

	/** True if populated from a real save (vs. default-init / legacy save). */
	UPROPERTY()
	bool bHasValidData = false;
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
class MOFRAMEWORK_API UMOResourceDepletionSubsystem : public UWorldSubsystem, public IMOSaveDomain
{
	GENERATED_BODY()

public:
	//~ Begin IMOSaveDomain (ResourceDepletion save data lives here, not in the persistence subsystem)
	virtual FName GetSaveDomainName() const override { return TEXT("ResourceDepletion"); }
	virtual int32 GetSaveDomainApplyPriority() const override { return 70; }
	virtual void CaptureSaveDomain(UMOWorldSaveGame& Save) override;
	virtual void ApplySaveDomain(const UMOWorldSaveGame& Save) override;
	//~ End IMOSaveDomain

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
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

	// ============================================================================
	// SAVE / LOAD  (H37)
	// ============================================================================

	/**
	 * (H37) Snapshot the current depletion map for persistence. Any caller.
	 * Prunes already-respawned (expired) entries so stale data isn't saved.
	 */
	UFUNCTION(BlueprintCallable, Category = "Resource Depletion|Save")
	void BuildSaveData(FMOResourceDepletionSaveData& OutSaveData) const;

	/**
	 * (H37) Restore depletion state from a save. Replaces the runtime map.
	 * No-ops on legacy saves (bHasValidData=false) so a fresh world isn't
	 * cleared. Expired entries are dropped on restore (nodes that aged past
	 * RespawnHoursReal come back fresh, matching CheckRespawns semantics).
	 */
	UFUNCTION(BlueprintCallable, Category = "Resource Depletion|Save")
	void ApplySaveDataAuthority(const FMOResourceDepletionSaveData& InSaveData);

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
