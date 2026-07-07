/**
 * =============================================================================
 * MOSurvivorJobQueueComponent.h - Survivor Job Queue Management
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Manages a survivor's job queue. Each recruited survivor has this component
 * to track queued work tasks (gather wood, forage, etc.), home location
 * assignment, and job progress/state. Similar pattern to MOCraftingQueueComponent.
 *
 * JOB QUEUE FEATURES:
 * - EnqueueJob: Add job by type with repeat count
 * - EnqueueJobAtLocation: Add location-based job
 * - EnqueueJobWithTarget: Add actor-targeted job
 * - CancelJob/CancelAllJobs: Remove jobs
 * - ReorderJob: Move job in queue
 * - SetJobState/UpdateJobProgress: State management
 * - CompleteCurrentJob/CompleteCurrentIteration: Completion handling
 *
 * HOME LOCATION:
 * - HomeLocation: World position
 * - HomeBuildingGuid: Optional building assignment
 * - SetHome/ClearHome/HasHome: Home management
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] FAST ARRAY: JobQueue uses FastArraySerializer for efficient
 *   network replication. Call MarkQueueDirty() after modifications.
 *
 * [2024-02] JOB DEFINITIONS: Uses UMOSurvivorJobDatabaseSettings for lookups.
 *   GetJobDefinitionPtr() returns nullptr if job type not in DataTable.
 *
 * [2024-02] ITERATION TRACKING: RepeatCount vs CompletedCount determines
 *   remaining iterations. Use ShouldRepeat() to check.
 *
 * =============================================================================
 * RELATED FILES: MOSurvivorJobTypes.h, MOSurvivorController.h, MOSurvivorTaskMenu.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOSurvivorJobTypes.h"
#include "MOSurvivorJobQueueComponent.generated.h"

class UDataTable;

/**
 * Delegate fired when the job queue changes (add, remove, reorder).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOJobQueueChangedDelegate);

/**
 * Delegate fired when a job's state changes.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOJobStateChangedDelegate, FGuid, JobId, EMOSurvivorJobState, NewState);

/**
 * Delegate fired when a job completes.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOJobCompletedDelegate, FGuid, JobId, bool, bSuccess);

/**
 * Component that manages a survivor's job queue.
 *
 * Each recruited survivor has this component to track:
 * - Queued work tasks (gather wood, forage, etc.)
 * - Home location assignment
 * - Job progress and state
 *
 * This follows the same pattern as UMOCraftingQueueComponent.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOSurvivorJobQueueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOSurvivorJobQueueComponent();

	// ============================================================================
	// QUEUE MANAGEMENT
	// ============================================================================

	/**
	 * Add a job to the queue.
	 * @param JobType - Type of job to perform
	 * @param RepeatCount - Number of times to repeat (default 1)
	 * @return GUID of the new job entry
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Jobs")
	FGuid EnqueueJob(EMOSurvivorJobType JobType, int32 RepeatCount = 1);

	/**
	 * Add a location-based job to the queue.
	 * @param JobType - Type of job to perform
	 * @param Location - Target location for the job
	 * @param RepeatCount - Number of times to repeat
	 * @return GUID of the new job entry
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Jobs")
	FGuid EnqueueJobAtLocation(EMOSurvivorJobType JobType, FVector Location, int32 RepeatCount = 1);

	/**
	 * Add an actor-targeted job to the queue.
	 * @param JobType - Type of job to perform
	 * @param Target - Target actor for the job
	 * @param RepeatCount - Number of times to repeat
	 * @return GUID of the new job entry
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Jobs")
	FGuid EnqueueJobWithTarget(EMOSurvivorJobType JobType, AActor* Target, int32 RepeatCount = 1);

	/**
	 * Add a CraftAtStation job (V0): withdraw the recipe's ingredients from
	 * Storage, craft at Station via the survivor's REAL crafting queue, and
	 * deposit the output back to Storage.
	 * @param RecipeId - Recipe to craft (must exist in DT_Recipes)
	 * @param Station - Crafting station actor to work at
	 * @param Storage - Container actor holding ingredients / receiving output
	 * @param RepeatCount - Number of crafts
	 * @return GUID of the new job entry
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Jobs")
	FGuid EnqueueCraftJob(FName RecipeId, AActor* Station, AActor* Storage, int32 RepeatCount = 1);

	/**
	 * Add a RefuelStation job (F1/T3): withdraw fuel items from Storage, carry
	 * them to Station, load its fuel tank, and relight it if it burned out.
	 * @param Station - Fuel-burning crafting station (campfire, forge)
	 * @param Storage - Container actor holding the fuel
	 * @param FuelItemId - Fuel item to haul (must be accepted by the station)
	 * @param Quantity - Items to withdraw per trip (capped by storage stock)
	 * @return GUID of the new job entry
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Jobs")
	FGuid EnqueueRefuelJob(AActor* Station, AActor* Storage, FName FuelItemId, int32 Quantity = 5);

	/**
	 * Cancel a specific job.
	 * @param JobId - GUID of the job to cancel
	 * @return True if job was found and cancelled
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Jobs")
	bool CancelJob(const FGuid& JobId);

	/**
	 * Cancel all jobs in the queue.
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Jobs")
	void CancelAllJobs();

	/**
	 * Move a job to a new position in the queue.
	 * @param JobId - GUID of the job to move
	 * @param NewIndex - Target index in the queue
	 * @return True if reorder was successful
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Jobs")
	bool ReorderJob(const FGuid& JobId, int32 NewIndex);

	// ============================================================================
	// JOB STATE
	// ============================================================================

	/**
	 * Get the current job (first active or queued job).
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Jobs")
	FMOSurvivorJobEntry GetCurrentJob() const;

	/**
	 * Get all jobs in the queue.
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Jobs")
	TArray<FMOSurvivorJobEntry> GetAllJobs() const;

	/**
	 * Get the number of jobs in the queue.
	 */
	UFUNCTION(BlueprintPure, Category = "MO|Survivor|Jobs")
	int32 GetJobCount() const { return JobQueue.Jobs.Num(); }

	/**
	 * Check if the queue has any jobs.
	 */
	UFUNCTION(BlueprintPure, Category = "MO|Survivor|Jobs")
	bool HasJobs() const { return JobQueue.Jobs.Num() > 0; }

	/**
	 * Set the state of a job.
	 * @param JobId - GUID of the job
	 * @param NewState - New state to set
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Jobs")
	void SetJobState(const FGuid& JobId, EMOSurvivorJobState NewState);

	/**
	 * Update the progress of a job.
	 * @param JobId - GUID of the job
	 * @param Progress - New progress value (0.0 - 1.0)
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Jobs")
	void UpdateJobProgress(const FGuid& JobId, float Progress);

	/**
	 * Complete the current job.
	 * @param bSuccess - Whether the job succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Jobs")
	void CompleteCurrentJob(bool bSuccess);

	/**
	 * Mark an iteration of the current job as complete.
	 * If more iterations remain, resets progress for the next iteration.
	 * If all iterations are done, removes the job from queue.
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Jobs")
	void CompleteCurrentIteration();

	// ============================================================================
	// HOME LOCATION
	// ============================================================================

	/** Home location for this survivor. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "MO|Survivor|Home")
	FVector HomeLocation = FVector::ZeroVector;

	/** GUID of the home building (campfire, shelter, etc.). */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "MO|Survivor|Home")
	FGuid HomeBuildingGuid;

	/**
	 * Set the home location for this survivor.
	 * @param Location - World location of the home
	 * @param BuildingGuid - Optional GUID of the building being assigned as home
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Home")
	void SetHome(FVector Location, FGuid BuildingGuid = FGuid());

	/**
	 * Clear the home assignment.
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Home")
	void ClearHome();

	/**
	 * Check if this survivor has a home assigned.
	 */
	UFUNCTION(BlueprintPure, Category = "MO|Survivor|Home")
	bool HasHome() const { return !HomeLocation.IsZero() || HomeBuildingGuid.IsValid(); }

	// ============================================================================
	// JOB DEFINITIONS
	// ============================================================================

	/**
	 * Get the job definition for a job type.
	 * Uses centralized UMOSurvivorJobDatabaseSettings.
	 * @param JobType - Type of job to look up
	 * @param OutDefinition - The definition if found
	 * @return true if the definition was found
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Jobs")
	bool GetJobDefinition(EMOSurvivorJobType JobType, FMOSurvivorJobDefinitionRow& OutDefinition) const;

	/**
	 * Get the job definition for a job type (C++ only, returns pointer).
	 * Uses centralized UMOSurvivorJobDatabaseSettings.
	 * @param JobType - Type of job to look up
	 * @return Pointer to the definition row, or nullptr if not found
	 */
	const FMOSurvivorJobDefinitionRow* GetJobDefinitionPtr(EMOSurvivorJobType JobType) const;

	/**
	 * Get all available job definitions.
	 * Uses centralized UMOSurvivorJobDatabaseSettings.
	 * @param bExcludeCommands - If true, excludes command-type jobs (Follow, Stay, etc.)
	 * @return Array of job definition rows
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Jobs")
	TArray<FMOSurvivorJobDefinitionRow> GetAllJobDefinitions(bool bExcludeCommands = true) const;

	// ============================================================================
	// EVENTS
	// ============================================================================

	/** Fired when the job queue changes (add, remove, reorder). */
	UPROPERTY(BlueprintAssignable, Category = "MO|Survivor|Events")
	FMOJobQueueChangedDelegate OnQueueChanged;

	/** Fired when a job's state changes. */
	UPROPERTY(BlueprintAssignable, Category = "MO|Survivor|Events")
	FMOJobStateChangedDelegate OnJobStateChanged;

	/** Fired when a job completes. */
	UPROPERTY(BlueprintAssignable, Category = "MO|Survivor|Events")
	FMOJobCompletedDelegate OnJobCompleted;

	// ============================================================================
	// SAVE/LOAD
	// ============================================================================

	/**
	 * Build save data for persistence.
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Save")
	void BuildSaveData(FMOSurvivorJobQueueSaveData& OutSaveData) const;

	/**
	 * Apply save data (authority only).
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Save")
	bool ApplySaveDataAuthority(const FMOSurvivorJobQueueSaveData& InSaveData);

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** The job queue (replicated via FastArray). */
	UPROPERTY(Replicated)
	FMOSurvivorJobList JobQueue;

	/** Create a new job entry with defaults. */
	FMOSurvivorJobEntry CreateJobEntry(EMOSurvivorJobType JobType, int32 RepeatCount);

	/** Remove completed/failed jobs from the queue. */
	void CleanupFinishedJobs();

	/** Mark queue as dirty for replication. */
	void MarkQueueDirty();
};
