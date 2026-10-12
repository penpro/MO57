/**
 * =============================================================================
 * MOWorldReadinessSubsystem.h - "Is the world on screen actually there yet?"
 * =============================================================================
 *
 * PURPOSE:
 * One signal for "the terrain, its vegetation and the textures the view needs have arrived", so the loading overlay (new game, load,
 * main menu) is lifted when the player would see a finished landscape instead of bare materials that fill in over the next ten seconds.
 *
 * THE PROBLEM THIS SOLVES:
 * UMOVoxelReadinessSubsystem only knows the voxel RUNTIME is up (+ a fixed collision delay). PCG runtime generation (the trees, bushes, grass,
 * rocks, shore plants) is spawned afterwards, cell by cell, around the generation sources, and the textures / Nanite meshes stream in after that.
 * Dismissing the loading screen on "voxel ready" showed a barren landscape that populated in front of the player.
 *
 * STAGES (each has a floor and a cap so a stuck signal never traps the player on a black screen):
 *   Terrain     - UMOVoxelReadinessSubsystem reached Ready (skipped when the world has no AVoxelWorld)
 *   Vegetation  - UPCGSubsystem has had no graph executing for PcgQuietSeconds (and at least MinPcgSeconds passed in the stage), or MaxPcgSeconds passed.
 *                 Skipped when the world has no PCG subsystem.
 *   Streaming   - IStreamingManager reports <= StreamingWantMax textures still wanting resources (cap MaxStreamingSeconds: pool-limited scenes
 *                 never reach zero)
 *   Ready       - OnWorldReady broadcast once
 * The whole cycle is capped by MaxTotalSeconds (force-ready + a warning in the log).
 *
 * OWNERSHIP:
 *   Whoever owns the world's boot calls BeginTracking(): AMOGameMode (new game / load, right after the voxel poll starts) and
 *   AMOMainMenuPlayerController (the flyby backdrop; it also starts the voxel poll, since nothing else does on that map).
 *   Consumers: UMOGameInstance (holds the loading overlay until ready) and the menu reveal.
 *
 * TUNING (console variables, all read live):
 *   mo.Readiness.Enabled 0|1          0 = every cycle is ready immediately (autonomous harnesses that do not want the extra wait)
 *   mo.Readiness.PcgQuietSeconds      1.5
 *   mo.Readiness.MinPcgSeconds        2.0
 *   mo.Readiness.MaxPcgSeconds        15  (vegetation stage cap: the PCG scheduler works nearest-first, so the far field may still fill behind the reveal)
 *   mo.Readiness.MaxStreamingSeconds  4.0
 *   mo.Readiness.StreamingWantMax     8
 *   mo.Readiness.MaxTotalSeconds      60
 *   MO.Readiness.Dump                 print the current stage / progress / raw signals
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOWorldReadinessSubsystem.generated.h"

UENUM(BlueprintType)
enum class EMOWorldReadyStage : uint8
{
	/** BeginTracking has not been called (or the cycle was cancelled). */
	Inactive,
	/** Waiting for the voxel runtime + collision. */
	Terrain,
	/** Waiting for PCG runtime generation to go quiet. */
	Vegetation,
	/** Waiting for texture / mesh streaming to catch up with the view. */
	Streaming,
	/** Everything the player will see is in. */
	Ready,
};

UCLASS()
class MOFRAMEWORK_API UMOWorldReadinessSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FMOOnWorldReady);
	/** Fired once per tracking cycle when the stage reaches Ready. Bind before BeginTracking, or check IsReady() if late. */
	FMOOnWorldReady OnWorldReady;

	/**
	 * Start (or restart) a readiness cycle.
	 * @param bStartVoxelPolling  Also begin UMOVoxelReadinessSubsystem's poll when it is Inactive. The game mode starts that poll itself AFTER the voxel
	 *                            world is re-created with the saved seed (starting it earlier would report the cooked default world as ready), so only a
	 *                            map with no such owner (the menu backdrop) asks for it.
	 */
	void BeginTracking(bool bStartVoxelPolling = false);

	/** Drop the cycle without broadcasting (leaving the map, tests). */
	void StopTracking();

	/** True while a cycle is running (not Inactive, not Ready). */
	bool IsTracking() const { return Stage != EMOWorldReadyStage::Inactive && Stage != EMOWorldReadyStage::Ready; }

	bool IsReady() const { return Stage == EMOWorldReadyStage::Ready; }

	EMOWorldReadyStage GetStage() const { return Stage; }

	/** 0..1, monotonic within a cycle: terrain 0-0.4, vegetation 0.4-0.85, streaming 0.85-1. */
	float GetProgress01() const { return Progress; }

	/** Player-facing line for the current stage ("Raising the terrain", ...). */
	FText GetStatusText() const;

	/** Skip the remaining stages now (a debug verb, or a caller that decided the player has waited enough). */
	void ForceReady(const TCHAR* Reason);

	static UMOWorldReadinessSubsystem* Get(const UObject* WorldContextObject);

	/** One line with the stage and every raw signal (the MO.Readiness.Dump verb prints it). */
	FString DescribeState() const;

protected:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

private:
	EMOWorldReadyStage Stage = EMOWorldReadyStage::Inactive;
	float Progress = 0.0f;
	bool bStartedVoxelPolling = false;

	double CycleStartSeconds = 0.0;
	double StageStartSeconds = 0.0;
	/** Seconds the PCG subsystem has had nothing executing, within the Vegetation stage. */
	double PcgQuietSeconds = 0.0;
	double LastPollSeconds = 0.0;
	bool bPcgSeenBusy = false;
	double NextTraceSeconds = 0.0;

	FTimerHandle PollTimerHandle;

	void Poll();
	void EnterStage(EMOWorldReadyStage NewStage);
	void SetProgress(float NewProgress);
	void Finish(const TCHAR* Reason);

	bool WorldHasVoxel() const;
	bool IsPcgBusy(bool& bOutHasPcg) const;
};
