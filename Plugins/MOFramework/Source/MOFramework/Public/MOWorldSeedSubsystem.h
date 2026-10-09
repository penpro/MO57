/**
 * =============================================================================
 * MOWorldSeedSubsystem.h - One place that turns a world seed into voxel terrain
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH VOXEL SEEDING
 *
 * THE RULE: terrain is a pure function of the seed, and EVERY MACHINE generates its own copy
 * (AVoxelWorld is a per-machine level actor; only gameplay state replicates). Co-op therefore
 * only works if host and clients apply the SAME seed before creating the voxel runtime.
 *
 * THE FLOW:
 *   HOST   AMOGameMode (new game / load save) picks the seed
 *            -> InitializeVoxelWorldWithSeed()
 *               -> PublishWorldSeed()   writes FMOWorldSeedInfo onto AMOGameState (replicated)
 *               -> RegenerateVoxelWorld() applies it to the host's own terrain
 *   CLIENT OnWorldBeginPlay (earliest hook, before any actor's BeginPlay)
 *            -> sets bCreateRuntimeOnBeginPlay=false on every AVoxelWorld, so the client does NOT
 *               generate default-seed terrain and then throw it away
 *          AMOGameState::OnRep_WorldSeed -> HandleReplicatedWorldSeed()
 *            -> RegenerateVoxelWorld() with the host's seed, then restarts voxel readiness polling
 *
 * The apply/regenerate mechanics are static and shared: the host and the client run the SAME
 * code, so they cannot drift (they did before: the host had the only copy).
 * AMOGameMode's ApplySeedToVoxelStamps / ApplySeedToHeightGraphParameter / IntSeedToVoxelSeedString
 * are kept as thin forwarders for Blueprint callers.
 *
 * ORDER-ROBUST: RegenerateVoxelWorld destroys an already-created runtime before recreating it,
 * so it is correct even if a level's BeginPlay beat the lockout.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOWorldSeedTypes.h"
#include "MOWorldSeedSubsystem.generated.h"

UCLASS()
class MOFRAMEWORK_API UMOWorldSeedSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UMOWorldSeedSubsystem* Get(const UObject* WorldContextObject);

	// =========================================================================
	// SHARED MECHANICS (static: no GameMode needed, so a client can run them)
	// =========================================================================

	/** Same 8-letter format as FVoxelExposedSeed::Randomize(); a given int seed always yields the same string. */
	static FString IntSeedToVoxelSeedString(int32 Seed);

	/** Set the seed on every VoxelStampComponent (and the height-graph stamp's own override). Returns stamps updated. */
	static int32 ApplySeedToStamps(UWorld* World, int32 WorldSeed, FName VoxelSeedParameterName, const UObject* LogContext);

	/** Set the seed parameter on every loaded UVoxelHeightGraph and every voxel parameter-override owner. */
	static bool ApplySeedToHeightGraphs(int32 WorldSeed, FName VoxelSeedParameterName, const UObject* LogContext);

	/**
	 * Apply the seed everywhere, destroy any already-created voxel runtime, and CreateRuntime() so
	 * terrain generates from this seed. Returns false if the level has no AVoxelWorld.
	 */
	static bool RegenerateVoxelWorld(UWorld* World, int32 WorldSeed, FName VoxelSeedParameterName, const UObject* LogContext);

	// =========================================================================
	// HOST: publish
	// =========================================================================

	/**
	 * Authority only. Records the seed as this world's active seed and replicates it through
	 * AMOGameState so every client (present and future) generates the same terrain.
	 */
	void PublishWorldSeed(int32 WorldSeed, FName VoxelSeedParameterName);

	// =========================================================================
	// CLIENT: receive
	// =========================================================================

	/** Called by AMOGameState::OnRep_WorldSeed on a client. Idempotent for an unchanged seed. */
	void HandleReplicatedWorldSeed(const FMOWorldSeedInfo& Info);

	// =========================================================================
	// QUERY
	// =========================================================================

	/** True once a seed has been applied to this machine's terrain (host: published; client: received + applied). */
	bool HasActiveSeed() const { return bHasActiveSeed; }

	/** The seed this machine's terrain was generated from. Only meaningful if HasActiveSeed(). */
	int32 GetActiveSeed() const { return ActiveSeed; }

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	bool bHasActiveSeed = false;
	int32 ActiveSeed = 0;
	FName ActiveParameterName;

	FTimerHandle SeedTimeoutHandle;

	/** Client only: nothing arrived from the host in time. Say so loudly; do NOT guess a seed. */
	void WarnSeedNeverArrived();
};
