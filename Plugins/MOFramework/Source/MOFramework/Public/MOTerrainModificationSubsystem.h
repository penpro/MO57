/**
 * =============================================================================
 * MOTerrainModificationSubsystem.h - World-scoped "worked ground" tracker
 * =============================================================================
 *
 * PURPOSE
 *   Owns the set of world locations that have been deliberately terraformed
 *   by any character. Acts as the authoritative source of truth for any
 *   system that needs to ask "has this position been modified by a player?"
 *
 * WHY THIS LIVES IN A SUBSYSTEM (NOT A COMPONENT)
 *   - Multiple characters can modify terrain (multiplayer / AI workers).
 *   - Multiple consumers query the same state — foliage cleanup, future
 *     crops, building snap-to-flat, AI pathing, PCG spawn suppression.
 *   - Save/load belongs to the world, not to any individual pawn.
 *
 * ARCHITECTURE NOTES
 *   - UWorldSubsystem: per-world, game-worlds only.
 *   - Stores a flat list of FMOTerrainModifiedZone (sphere = location + radius).
 *   - Maintains a 2D spatial grid for O(1)-ish lookups by world XY — needed
 *     to scale to "player built a whole city" zone counts.
 *   - Periodic sweep iterates the world's HISM/ISM components ONCE, checks
 *     each instance against zones via the grid, removes instances in zones.
 *     PCG can respawn whatever it wants; the sweep keeps modified ground bare.
 *
 * INTEGRATION POINTS
 *   - UMOTerraformingComponent  reports modifications via RegisterModifiedZone.
 *   - UMOPersistenceSubsystem   calls BuildSaveData / ApplySaveDataAuthority.
 *   - (Future) PCG spawn filter calls IsLocationModified at spawn time so
 *     instances never appear in modified zones in the first place.
 *   - (Future) Crops / building snap call IsLocationModified for game rules.
 *
 * SCALING NOTES
 *   Per-zone polling does NOT scale — that's why we use a single-pass sweep
 *   with grid lookups instead of N separate "remove foliage in this zone"
 *   calls. With a 10m grid, queries for "is XY modified?" are constant-time
 *   for typical zone counts (<10 zones per grid cell). Memory is bounded by
 *   MaxZones, with adjacent zones merging on register to keep counts in check.
 *
 * SAVE/LOAD PATTERN
 *   Follows the project's standard:
 *       BuildSaveData(OutData)               -> any caller
 *       ApplySaveDataAuthority(InData)       -> server only (rebuilds index)
 *   See UMOPersistenceSubsystem::CaptureTerrainModificationData /
 *       RestoreTerrainModificationData for the orchestration.
 *
 * RELATED FILES
 *   - MOTerraformingComponent.h   - producer (registers zones)
 *   - MOPersistenceSubsystem.h    - save/load orchestrator
 *   - MOWorldSaveGame.h           - stores TerrainModificationData field
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOSaveDomainInterface.h"
#include "MOTerrainModificationSubsystem.generated.h"

/**
 * A sphere of world that has been deliberately terraformed. Foliage / PCG
 * content inside this sphere is kept clear by the subsystem's periodic sweep.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOTerrainModifiedZone
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|TerrainMod")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="MO|TerrainMod")
	float Radius = 250.0f;

	FMOTerrainModifiedZone() = default;
	FMOTerrainModifiedZone(const FVector& InLoc, float InRadius)
		: Location(InLoc), Radius(InRadius) {}
};

/**
 * Save-game payload for the terrain modification subsystem. Tiny — just
 * the flat zone list + a version number for future migration.
 *
 * Separate from the Voxel plugin's own sculpt save (that one handles the
 * actual mesh / heightfield data; we just persist the "this area is worked"
 * metadata).
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOTerrainModificationSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|TerrainMod")
	TArray<FMOTerrainModifiedZone> Zones;

	/** Bumped when the save format changes incompatibly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|TerrainMod")
	int32 Version = 1;
};

UCLASS()
class MOFRAMEWORK_API UMOTerrainModificationSubsystem : public UTickableWorldSubsystem, public IMOSaveDomain
{
	GENERATED_BODY()

public:
	//~ Begin IMOSaveDomain (TerrainModification save data lives here, not in the persistence subsystem)
	virtual FName GetSaveDomainName() const override { return TEXT("TerrainModification"); }
	virtual int32 GetSaveDomainApplyPriority() const override { return 40; }
	virtual void CaptureSaveDomain(UMOWorldSaveGame& Save) override;
	virtual void ApplySaveDomain(const UMOWorldSaveGame& Save) override;
	//~ End IMOSaveDomain

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	/**
	 * Convenience accessor — same pattern as the other world subsystems.
	 * Returns nullptr if World is invalid or this is not a game world.
	 */
	static UMOTerrainModificationSubsystem* Get(const UObject* WorldContextObject);

	// ============================================================================
	// PRODUCER API
	// ============================================================================

	/**
	 * Record a modified zone. Adjacent / overlapping zones merge automatically
	 * to keep the list bounded. Called by UMOTerraformingComponent after every
	 * successful terraform action.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|TerrainMod")
	void RegisterModifiedZone(const FVector& Location, float Radius);

	// ============================================================================
	// QUERY API
	// ============================================================================

	/**
	 * O(zones-per-cell) check. The spatial grid keeps this tight as long as
	 * zones are geographically distributed.
	 */
	UFUNCTION(BlueprintPure, Category="MO|TerrainMod")
	bool IsLocationModified(const FVector& WorldLocation) const;

	/** Get all zones whose centers are within SearchRadius of WorldLocation. */
	UFUNCTION(BlueprintCallable, Category="MO|TerrainMod")
	void GetModifiedZonesNear(const FVector& WorldLocation, float SearchRadius,
		TArray<FMOTerrainModifiedZone>& OutZones) const;

	UFUNCTION(BlueprintPure, Category="MO|TerrainMod")
	int32 GetZoneCount() const { return Zones.Num(); }

	UFUNCTION(BlueprintPure, Category="MO|TerrainMod")
	const TArray<FMOTerrainModifiedZone>& GetZones() const { return Zones; }

	// ============================================================================
	// SAVE / LOAD (codebase pattern)
	// ============================================================================

	/** Write current zone list into OutData. Any caller. */
	UFUNCTION(BlueprintCallable, Category="MO|TerrainMod|Save")
	void BuildSaveData(FMOTerrainModificationSaveData& OutData) const;

	/**
	 * Replace zone state from a save payload and rebuild the spatial index.
	 * Server-only (state-mutating).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|TerrainMod|Save")
	bool ApplySaveDataAuthority(const FMOTerrainModificationSaveData& InData);

	// ============================================================================
	// SWEEP
	// ============================================================================

	/**
	 * Run one cleanup pass over all zones — iterates the world's HISM/ISM
	 * components once and removes any instance whose position is inside a
	 * modified zone. Called periodically on a timer; can also be invoked
	 * manually (e.g. immediately after a fresh terraform action so the
	 * player doesn't see PCG-respawned foliage flicker).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|TerrainMod")
	int32 SweepModifiedZones();

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Periodic sweep interval (seconds). 2s is a reasonable default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|TerrainMod", meta=(ClampMin="0.25"))
	float SweepInterval = 2.0f;

	/**
	 * Upper bound on number of zones — beyond this, oldest entries are
	 * dropped on insert. Realistically the merge logic keeps counts well
	 * below this for clustered work, but this is a safety net for runaway
	 * sessions.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|TerrainMod", meta=(ClampMin="1"))
	int32 MaxZones = 5000;

	/**
	 * Spatial grid cell size (world units). Smaller = more cells, fewer zones
	 * per cell, faster queries — but more memory. 1000 (= 10m) is a good
	 * tradeoff: a Radius=250 zone touches a 1-3 cell box.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|TerrainMod", meta=(ClampMin="100"))
	float GridCellSize = 1000.0f;

	/**
	 * Maximum distance from any local player at which a zone is still swept
	 * (world units). Beyond this distance, the foliage isn't visible (grass
	 * fade distance is typically much less than 25m), so respawned instances
	 * don't matter until the player gets close — at which point the next
	 * sweep tick will catch them. Default 2500 = 25m. Set to a very large
	 * value to disable player-distance filtering entirely.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|TerrainMod", meta=(ClampMin="100"))
	float SweepPlayerRange = 2500.0f;

	/**
	 * After RegisterModifiedZone, sweep EVERY FRAME for this many seconds.
	 * Catches the visible flicker where PCG respawns foliage in response to
	 * a fresh voxel sculpt — PCG can fire anywhere from same-frame to about
	 * a second later depending on graph complexity and chunk processing.
	 * Per-frame sweeping during a short window catches it at the smallest
	 * possible delay (one frame, ~17ms at 60fps) regardless of when PCG
	 * actually fires.
	 *
	 * Cost during the burst is one single-pass sweep per frame, which is
	 * the same sweep the periodic timer already runs — just at higher
	 * frequency. After the burst ends, the periodic timer (SweepInterval)
	 * handles long-tail PCG behavior like chunk-reload respawns.
	 *
	 * Default 1.0s comfortably covers observed PCG response latency in this
	 * project's voxel + PCG setup. Tune larger if you ever see PCG respawn
	 * later than 1s after a sculpt; smaller if profiling shows the burst
	 * is a CPU concern. Set to 0.0f to disable burst-mode entirely (rely
	 * on the periodic timer alone — flicker will be visible until next
	 * periodic tick).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|TerrainMod", meta=(ClampMin="0.0"))
	float PostRegisterBurstSweepDuration = 1.0f;

	/**
	 * On RegisterModifiedZone: a new zone whose center is within
	 * MergeDistanceFactor × (RadiusA, RadiusB)'s avg of an existing zone
	 * MERGES into it (radius takes the max). Aggressive merge keeps zone
	 * counts low when the player works the same patch repeatedly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|TerrainMod", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MergeDistanceFactor = 0.5f;

	// ============================================================================
	// AUTO-SWEEP SCOPE (tag-based)
	// ============================================================================
	//
	// DESIGN: terraforming should only auto-remove decorative foliage (grass,
	// flowers, ferns). Trees, rocks, berry bushes etc. are HARVESTABLE — the
	// player is expected to chop / mine / forage them explicitly. Letting the
	// sweep yank them out the moment a zone registers undermines the survival
	// loop (you'd "free wood" by digging instead of chopping).
	//
	// MECHANISM: opt-in by tag. The artist/designer tags content that should
	// auto-clear with terraform — typically on the HISM component or its
	// owning actor (component tag wins / either matches). Untagged content
	// is left alone. This is the idiomatic UE pattern for "classify these
	// instances for selective behavior" (vs. fragile string matching against
	// asset paths).
	//
	// The PCG-level filter (UMOPCGTerrainModificationFilterSettings) is the
	// separate per-graph scoping for SPAWN suppression. This sweep handles
	// the post-spawn cleanup side.

	/**
	 * The sweep auto-removes HISMs whose Component tags OR owning Actor tags
	 * contain at least one of these names. Empty list = sweep nothing (safe
	 * default for unconfigured worlds — no harvestables disappear).
	 *
	 * Default "grass" matches the project's PCG-spawned ground cover. Add
	 * additional tags here (e.g. "flower", "fern", "moss") as you author
	 * more decorative foliage families. Never add tags that overlap with
	 * harvestable content — those must persist for the player to interact
	 * with explicitly.
	 *
	 * The check is OR across tags AND across "component tags vs actor tags":
	 *   ISM->ComponentHasTag(X) || ISM->GetOwner()->ActorHasTag(X) → eligible.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|TerrainMod|Sweep")
	TArray<FName> AutoSweepTags = { TEXT("grass") };

private:
	UPROPERTY()
	TArray<FMOTerrainModifiedZone> Zones;

	/**
	 * Spatial grid: cell coordinate -> indices into Zones[] for zones that
	 * touch that cell. A zone is indexed under every cell its bounding box
	 * overlaps, so a query in any cell finds all candidate zones.
	 */
	TMap<FIntPoint, TArray<int32>> ZoneGrid;

	FTimerHandle SweepTimerHandle;

	/**
	 * Game time (UWorld::GetTimeSeconds()) at which the per-frame burst
	 * sweep should stop. While > GetTimeSeconds(), Tick runs the sweep
	 * every frame. Set by RegisterModifiedZone; checked in IsTickable +
	 * Tick. Zero/past = not bursting.
	 */
	double BurstSweepEndTime = 0.0;

	// --- Internal helpers ---

	/**
	 * Timer entry-point. FTimerManager wants a void(void) signature; the
	 * public SweepModifiedZones returns an int32 (count for diagnostics),
	 * so this thunks the call.
	 */
	void SweepTimerTick();

	/** Refresh BurstSweepEndTime to be PostRegisterBurstSweepDuration in the future. */
	void ExtendBurstSweepWindow();

	/** Convert a world position to its grid cell (XY only — terraforming is mostly horizontal). */
	FIntPoint WorldToCell(const FVector& Loc) const;

	/** Add a zone's index to every grid cell it touches. */
	void IndexZone(int32 ZoneIndex);

	/** Reset and re-populate the grid from current Zones (used after load). */
	void RebuildSpatialIndex();
};
