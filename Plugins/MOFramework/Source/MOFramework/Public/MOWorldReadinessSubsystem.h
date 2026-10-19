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
 * FAR-GRID HOLD (2026-10-13): the PCG runtime scheduler always generates the LARGEST hi-gen grid first (UE 5.8 PCGRuntimeGenScheduler: priority = grid size
 * + a 0..1 distance term), nearest-first only within a grid. A far tier on a big grid (the canopy clumps: 2 km cells out to 7.5 km, ~50 cells) is therefore
 * generated in full before the first near cell, and the reveal waited for it (READY 29 s -> 46 s in the packaged build). While a cycle runs, every grid of
 * mo.Readiness.HoldGridsFrom (the UNSCALED size, as the HiGenGridSize node names it) and larger gets no generation radius on each runtime-generated PCG
 * component; Finish / StopTracking / Deinitialize put the authored radii back, and the far field fills in behind the reveal.
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
 *   mo.Readiness.MaxPcgSeconds        15  (vegetation stage cap; past it the stage holds on for the near-instance count, up to MaxPcgHardSeconds 32)
 *   mo.Readiness.HoldGridsFrom        512 (FAR-GRID HOLD, below; 0 = off)
 *   mo.Readiness.MaxStreamingSeconds  4.0
 *   mo.Readiness.StreamingWantMax     8
 *   mo.Readiness.MaxTotalSeconds      60
 *   MO.Readiness.Dump                 print the current stage / progress / raw signals
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "PCGCommon.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOWorldReadinessSubsystem.generated.h"

class UPCGComponent;

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

	/** FAR-GRID HOLD on one radii set: every grid of HoldFromUnscaledGrid (512 = GRID512) and larger gets GetHeldRadiusCm(), default and per-quality values alike. */
	static void HoldRadii(FPCGRuntimeGenerationRadii& Radii, int32 HoldFromUnscaledGrid);
	/** Write the authored values of every grid back into Radii, in place (the per-quality maps keep their allocation). */
	static void RestoreRadii(FPCGRuntimeGenerationRadii& Radii, const FPCGRuntimeGenerationRadii& Authored);
	/** The radius of a held grid: negative by more than any cell, so the scheduler walks no cell of it (0 would still generate the viewer's own cell). */
	static float GetHeldRadiusCm();
	/** The radius field of an UNSCALED hi-gen grid (512 -> Radius51200; the fields are protected, reached through reflection). Null for a grid with no field of its own. */
	static FPerQualityLevelFloat* GetMutableRadiusField(FPCGRuntimeGenerationRadii& Radii, int32 UnscaledGrid);
	static const FPerQualityLevelFloat* GetRadiusField(const FPCGRuntimeGenerationRadii& Radii, int32 UnscaledGrid);

	/**
	 * The "WorldBuild.*" fields of a bug report (registered with UMOBugReportSubsystem on world begin play): the readiness stage, how the last load
	 * ended (and whether the vegetation stage was capped), held grids, PCG busy, PCG components generated / generating, near instances. Shipping
	 * writes no log, so a player's "the world never filled in" report (Steam playtest Oct 16 2026: one Electric Dreams spire and nothing else) can
	 * only be diagnosed from these.
	 */
	void DescribeForBugReport(TArray<TPair<FString, FString>>& Out) const;

protected:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
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
	/** How the last load ended (Finish's reason) and when, for the bug report. */
	FString LastFinishReason;
	double LastFinishAfterSeconds = 0.0;
	/** The last load's vegetation stage hit its cap with PCG still working, and how much stood near the camera then. */
	bool bLastVegetationCapped = false;
	int32 LastCappedNearCount = 0;
	/** The biome spawner's process-wide run totals when this load's vegetation stage began (MOPCGBiomeSpawner::GetRunTotals): the "nothing grew" report gives this load's share. */
	int32 VegetationStartSpawnerRuns = 0;
	int64 VegetationStartSpawnerAccepted = 0;
	double NextTraceSeconds = 0.0;

	FTimerHandle PollTimerHandle;

	void Poll();
	void EnterStage(EMOWorldReadyStage NewStage);
	void SetProgress(float NewProgress);
	void Finish(const TCHAR* Reason);

	bool WorldHasVoxel() const;
	bool IsPcgBusy(bool& bOutHasPcg) const;

	/**
	 * Spawned vegetation / rock instances (every PCG tier except the far ones) within mo.Readiness.NearInstanceRadiusCm of the local camera. The PCG scheduler generates the largest grids
	 * first, so the ground cover right around the player is the LAST thing to arrive: at the old 15 s cap the overlay lifted on a bare spot and the grass grew in during the reveal. Throttled to ~1 Hz.
	 */
	int32 CountNearPcgInstances() const;

	mutable double NextNearCountSeconds = 0.0;
	mutable int32 LastNearCount = 0;

	/** FAR-GRID HOLD (see the file header): take the radii of the held grids away from every runtime-generated PCG component, remembering the authored ones. Idempotent. */
	void HoldFarGrids();
	/** Put the remembered radii back (no-op when nothing is held). */
	void ReleaseFarGrids();

	struct FHeldGenerationRadii
	{
		TWeakObjectPtr<UPCGComponent> Component;
		FPCGRuntimeGenerationRadii Authored;
	};
	TArray<FHeldGenerationRadii> HeldRadii;

	/** VEGETATION BUDGET: pcg.FrameTime raised to mo.Readiness.PcgFrameTimeMs while the vegetation stage runs behind the overlay (the spawners are time-sliced), restored when it ends. */
	void RaisePcgBudget();
	void RestorePcgBudget();
	bool bPcgBudgetRaised = false;
	float SavedPcgFrameTimeMs = 0.0f;
};
