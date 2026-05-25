/**
 * =============================================================================
 * MOVoxelReadinessSubsystem.h - Voxel terrain readiness signal
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 *
 * PURPOSE:
 * Wraps the Voxel Plugin's `IsVoxelWorldReady()` query in a single-poll
 * subsystem and exposes a clean OnVoxelReady multicast delegate. Other
 * systems subscribe instead of running their own polling loops.
 *
 * THE PROBLEM THIS SOLVES:
 * The Voxel Plugin doesn't broadcast a "world is ready" event — consumers
 * have to poll `AVoxelWorld::IsVoxelWorldReady()` until it returns true.
 * Before this subsystem, MOGameMode had two separate 0.5s polling loops
 * (CheckVoxelReadyAndSpawn for new game, CheckVoxelReadyAndReground for
 * load) doing the same query. The audio subsystem, the AI spawn manager,
 * and weather integration all could legitimately need this signal too.
 *
 * Now: this subsystem polls ONCE per cycle, and everyone subscribes.
 *
 * CYCLE:
 *   Inactive
 *     -> BeginPolling() called by MOGameMode after voxel CreateRuntime
 *   WaitingForRuntime
 *     -> IsVoxelWorldReady() returns true
 *   WaitingForCollision
 *     -> CollisionGenerationDelay seconds elapse (collision generation
 *        isn't event-signaled by the Voxel Plugin, so we wait a fixed
 *        delay after runtime ready)
 *   Ready
 *     -> OnVoxelReady.Broadcast()
 *
 * Subscribers should subscribe BEFORE calling BeginPolling, or check
 * IsReady() in case they're late.
 *
 * =============================================================================
 * RELATED FILES: MOGameMode.cpp (consumer #1)
 * LAST UPDATED: 2026-05-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOVoxelReadinessSubsystem.generated.h"

class AVoxelWorld;

UENUM(BlueprintType)
enum class EMOVoxelReadinessPhase : uint8
{
	/** No polling yet. BeginPolling has not been called this cycle. */
	Inactive             UMETA(DisplayName = "Inactive"),

	/** Polling AVoxelWorld::IsVoxelWorldReady() at PollIntervalSeconds. */
	WaitingForRuntime    UMETA(DisplayName = "Waiting for Runtime"),

	/** Voxel runtime is ready; in CollisionGenerationDelay countdown. */
	WaitingForCollision  UMETA(DisplayName = "Waiting for Collision"),

	/** Voxel is fully ready; OnVoxelReady has been broadcast. */
	Ready                UMETA(DisplayName = "Ready"),
};

/**
 * Event-driven wrapper over voxel readiness polling.
 * One poll loop in the project, one delegate, N subscribers.
 */
UCLASS()
class MOFRAMEWORK_API UMOVoxelReadinessSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Fired once per polling cycle when phase transitions to Ready.
	 * Subscribers stay subscribed across cycles; the delegate re-fires
	 * each cycle (e.g., new-game then load both produce a fire).
	 */
	DECLARE_MULTICAST_DELEGATE(FMOOnVoxelReady);
	FMOOnVoxelReady OnVoxelReady;

	/**
	 * Start (or restart) a polling cycle. Called by MOGameMode after its
	 * voxel CreateRuntime kicks off. Resets Phase to WaitingForRuntime
	 * and begins the poll timer.
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Voxel")
	void BeginPolling();

	/** True once OnVoxelReady has been broadcast this cycle. */
	UFUNCTION(BlueprintPure, Category = "MO|Voxel")
	bool IsReady() const { return CurrentPhase == EMOVoxelReadinessPhase::Ready; }

	/** Current phase of the readiness state machine. */
	UFUNCTION(BlueprintPure, Category = "MO|Voxel")
	EMOVoxelReadinessPhase GetPhase() const { return CurrentPhase; }

	/** WorldContext accessor. */
	UFUNCTION(BlueprintPure, Category = "MO|Voxel", meta=(WorldContext="WorldContextObject"))
	static UMOVoxelReadinessSubsystem* Get(const UObject* WorldContextObject);

	// =========================================================================
	// CONFIG
	// =========================================================================

	/** Poll interval for IsVoxelWorldReady(). 0.5s mirrors the historical value. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "MO|Voxel",
		meta=(ClampMin="0.05", ClampMax="5.0"))
	float PollIntervalSeconds = 0.5f;

	/**
	 * After IsVoxelWorldReady() returns true, wait this many seconds before
	 * broadcasting OnVoxelReady. The Voxel Plugin reports the runtime as
	 * "ready" before its collision is fully generated; this delay gives
	 * collision a chance to settle. Mirrors MOGameMode's old constexpr 3s.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "MO|Voxel",
		meta=(ClampMin="0.0", ClampMax="30.0"))
	float CollisionGenerationDelay = 3.0f;

protected:
	virtual void Deinitialize() override;

private:
	EMOVoxelReadinessPhase CurrentPhase = EMOVoxelReadinessPhase::Inactive;

	FTimerHandle PollTimerHandle;
	FTimerHandle CollisionDelayTimerHandle;

	/** Locate the first AVoxelWorld actor in the level. Cached not required — cheap iter. */
	AVoxelWorld* FindVoxelWorld() const;

	/** Timer callback: query IsVoxelWorldReady; on true, advance to collision wait. */
	void PollVoxelReady();

	/** Timer callback: collision delay elapsed; broadcast OnVoxelReady. */
	void OnCollisionDelayElapsed();
};
