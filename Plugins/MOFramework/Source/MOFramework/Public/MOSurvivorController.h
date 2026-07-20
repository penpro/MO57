/**
 * =============================================================================
 * MOSurvivorController.h - Recruited Survivor AI Controller
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * AI Controller for recruited survivors. Extends MOAIController with survivor-
 * specific commands (Follow, Stay, GoHome) and job queue processing. Coordinates
 * with UMOSurvivorJobQueueComponent for task execution.
 *
 * KEY RESPONSIBILITIES:
 * 1. Execute immediate commands (Follow, Stay, GoHome)
 * 2. Process job queue from UMOSurvivorJobQueueComponent
 * 3. Execute simple jobs without full behavior tree
 * 4. Award XP for completed jobs
 * 5. Find and harvest HISM/ISM resources
 *
 * OWNERSHIP:
 * - Owner: Possesses recruited survivor pawns
 * - Lifespan: Exists while survivor is under AI control
 *
 * COMMAND PRIORITY:
 * Commands override job processing:
 * 1. Follow target (highest) - follows until StopFollowing()
 * 2. Stay at location - idles at position
 * 3. Go home - moves to home location once
 * 4. Job queue (lowest) - process when no commands active
 *
 * JOB EXECUTION FLOW:
 * ProcessNextJob()
 * -> GetJobQueue()->GetCurrentJob()
 * -> If CanExecuteSimply(): StartSimpleJobExecution()
 * -> Else: Run behavior tree
 * -> On complete: AwardJobExperience() + CompleteSimpleJob()
 *
 * SIMPLE JOB EXECUTION:
 * For basic jobs (Forage, Gather) that don't need full BT:
 * - SimpleJobState: 0=idle, 1=moving, 2=performing
 * - UpdateSimpleJobExecution() runs in Tick
 * - PerformSimpleJobAction() executes the work
 *
 * GATHER JOB TARGETING:
 * FindNearestGatherResource() - Search for ISM/HISM instances
 * FindNearestHarvestTarget() - Search for harvestable actors
 * Uses GatherSearchRadius for range
 * Stores target in GatherTargetHISMComponent + GatherTargetInstanceIndex
 *
 * CRITICAL PATTERNS:
 * 1. COMMAND CHANGE: Always call BroadcastCommandChange() when state changes
 * 2. JOB QUEUE BINDING: Bind to OnQueueChanged in OnPossess
 * 3. CACHED COMPONENTS: Use CachedJobQueue and CachedSkillsComponent
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] NULL JOB QUEUE: Pawn must have UMOSurvivorJobQueueComponent.
 *   GetJobQueue() returns cached pointer, null if component missing.
 *   ProcessNextJob() checks for null and returns early.
 *
 * [2024-02] GATHER TARGET STALE: GatherTargetHISMComponent is TWeakObjectPtr.
 *   HISM instance may be removed mid-job (harvested by player). Check
 *   .IsValid() before accessing. PerformSimpleJobAction() handles this.
 *
 * [2024-02] HOME LOCATION: GoToHome() reads home from pawn's identity
 *   component. If no home location set, logs warning and does nothing.
 *   Check HasHomeLocation() before calling.
 *
 * [2024-02] COMMAND PRIORITY: Commands override job processing. If bIsFollowing
 *   or bShouldStay is true, ProcessNextJob() won't execute. Clear commands
 *   with ClearAllCommands() before expecting job execution.
 *
 * [2024-02] SIMPLE JOB STATE: SimpleJobState values: 0=idle, 1=moving to
 *   target, 2=performing action. UpdateSimpleJobExecution() in Tick handles
 *   state transitions. Don't modify SimpleJobState directly.
 *
 * [2024-02] COMPONENT CACHING: CachedJobQueue and CachedSkillsComponent are
 *   populated in OnPossess(). If accessing before possession, they're null.
 *   OnUnPossess() clears these caches.
 *
 * [2024-02] BROADCAST PATTERN: Always call BroadcastCommandChange() when
 *   command state changes. UI binds to OnCommandChanged to update display.
 *
 * [2024-02] XP AWARD: AwardJobExperience() maps EMOSurvivorJobType to skill
 *   category and grants XP. Requires CachedSkillsComponent to be valid.
 *
 * RELATED FILES:
 * - MOAIController.h - Base class
 * - MOSurvivorJobQueueComponent.h - Job queue on pawn
 * - MOSurvivorJobTypes.h - Job entry struct and enums
 * - MOSystemMenuUIController.h - Shows survivor menus
 *
 * TESTING CHECKLIST:
 * [ ] Follow command tracks player movement
 * [ ] Stay command keeps survivor at location
 * [ ] GoHome moves to home location
 * [ ] Job queue processes when no commands
 * [ ] Gather jobs find and harvest resources
 * [ ] XP awarded on job completion
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOAIController.h"
#include "MOSurvivorJobTypes.h"
#include "MOSurvivorController.generated.h"

class UMOSurvivorJobQueueComponent;
class UMOSkillsComponent;
class UInstancedStaticMeshComponent;

/**
 * Delegate fired when survivor command state changes.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOSurvivorCommandChanged, FName, CommandName);
UCLASS()
class MOFRAMEWORK_API AMOSurvivorController : public AMOAIController
{
	GENERATED_BODY()

public:
	AMOSurvivorController();

	// ============================================================================
	// COMMANDS (Immediate actions, not queued)
	// ============================================================================

	/**
	 * Set the survivor to follow a target actor.
	 * @param Target - Actor to follow (usually the player)
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Commands")
	void SetFollowTarget(AActor* Target);

	/**
	 * Stop following the current target.
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Commands")
	void StopFollowing();

	/**
	 * Set the survivor to stay at a location.
	 * @param Location - World location to stay at
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Commands")
	void SetStayAtLocation(FVector Location);

	/**
	 * Clear the stay-at-location command.
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Commands")
	void ClearStayLocation();

	/**
	 * Command the survivor to go to their home location.
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Commands")
	void GoToHome();

	/**
	 * Clear all active commands (follow, stay, etc.) and return to idle.
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Commands")
	void ClearAllCommands();

	// ============================================================================
	// STATE QUERIES
	// ============================================================================

	/** Check if currently following a target. */
	UFUNCTION(BlueprintPure, Category = "MO|Survivor|State")
	bool IsFollowing() const { return bIsFollowing; }

	/** Get the current follow target. */
	UFUNCTION(BlueprintPure, Category = "MO|Survivor|State")
	AActor* GetFollowTarget() const { return FollowTarget.Get(); }

	/** Check if staying at a location. */
	UFUNCTION(BlueprintPure, Category = "MO|Survivor|State")
	bool IsStaying() const { return bShouldStay; }

	/** Get the stay location. */
	UFUNCTION(BlueprintPure, Category = "MO|Survivor|State")
	FVector GetStayLocation() const { return StayLocation; }

	/** Check if going home. */
	UFUNCTION(BlueprintPure, Category = "MO|Survivor|State")
	bool IsGoingHome() const { return bIsGoingHome; }

	/** Get the job queue component from the possessed pawn. */
	UFUNCTION(BlueprintPure, Category = "MO|Survivor|State")
	UMOSurvivorJobQueueComponent* GetJobQueue() const;

	/** Get the skills component from the possessed pawn. */
	UFUNCTION(BlueprintPure, Category = "MO|Survivor|State")
	UMOSkillsComponent* GetSkillsComponent() const;

	/** Check if currently processing a job. */
	UFUNCTION(BlueprintPure, Category = "MO|Survivor|State")
	bool IsProcessingJob() const { return bIsProcessingJob; }

	/** Get the current active command name for UI. */
	UFUNCTION(BlueprintPure, Category = "MO|Survivor|State")
	FName GetActiveCommandName() const;

	// ============================================================================
	// JOB PROCESSING
	// ============================================================================

	/**
	 * Start processing the next job in the queue.
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Jobs")
	void ProcessNextJob();

	/**
	 * Abort the current job and return to idle.
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Jobs")
	void AbortCurrentJob();

	/**
	 * Award XP for completing a job.
	 * @param JobType - Type of job that was completed
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Survivor|Jobs")
	void AwardJobExperience(EMOSurvivorJobType JobType);

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Distance to maintain when following. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Survivor|Config")
	float FollowDistance = 200.0f;

	/** Distance threshold before starting to follow again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Survivor|Config")
	float FollowStartDistance = 400.0f;

	/** Default behavior tree for survivors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Survivor|Config")
	TSoftObjectPtr<UBehaviorTree> DefaultSurvivorBehaviorTree;

	// ============================================================================
	// EVENTS
	// ============================================================================

	/** Fired when a command changes (follow, stay, etc.). */
	UPROPERTY(BlueprintAssignable, Category = "MO|Survivor|Events")
	FMOSurvivorCommandChanged OnCommandChanged;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	// ============================================================================
	// FOLLOW STATE
	// ============================================================================

	/** Currently following a target. */
	UPROPERTY(BlueprintReadOnly, Category = "MO|Survivor|State")
	bool bIsFollowing = false;

	/** Target to follow. */
	UPROPERTY(BlueprintReadOnly, Category = "MO|Survivor|State")
	TWeakObjectPtr<AActor> FollowTarget;

	// ============================================================================
	// STAY STATE
	// ============================================================================

	/** Should stay at a location. */
	UPROPERTY(BlueprintReadOnly, Category = "MO|Survivor|State")
	bool bShouldStay = false;

	/** Location to stay at. */
	UPROPERTY(BlueprintReadOnly, Category = "MO|Survivor|State")
	FVector StayLocation = FVector::ZeroVector;

	// ============================================================================
	// GO HOME STATE
	// ============================================================================

	/** Currently going home. */
	UPROPERTY(BlueprintReadOnly, Category = "MO|Survivor|State")
	bool bIsGoingHome = false;

	// ============================================================================
	// JOB STATE
	// ============================================================================

	/** Currently processing a job. */
	UPROPERTY(BlueprintReadOnly, Category = "MO|Survivor|State")
	bool bIsProcessingJob = false;

	/** Current job being processed. */
	UPROPERTY(BlueprintReadOnly, Category = "MO|Survivor|State")
	FMOSurvivorJobEntry CurrentJob;

	/** Simple job execution state: 0=idle, 1=moving, 2=performing */
	UPROPERTY(BlueprintReadOnly, Category = "MO|Survivor|State")
	uint8 SimpleJobState = 0;

	/** Target location for simple job execution. */
	UPROPERTY(BlueprintReadOnly, Category = "MO|Survivor|State")
	FVector SimpleJobTargetLocation = FVector::ZeroVector;

	/** Move-leg no-progress watchdog (wedged-pawn failsafe, B1). */
	float SimpleJobTotalSeconds = 0.0f;
	float MoveLegBestDistance = FLT_MAX;
	float MoveLegNoProgressSeconds = 0.0f;

	/** Timer for job actions (like digging duration). */
	UPROPERTY(BlueprintReadOnly, Category = "MO|Survivor|State")
	float SimpleJobTimer = 0.0f;

	/** Duration for simple job actions (default fallback for dig/forage). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Survivor|Config")
	float SimpleJobActionDuration = 4.0f;

	/**
	 * (H24) Effective action duration resolved per-job. For recipe-based gather
	 * harvests this is read from the resource definition's BaseActionTime (matching
	 * the player path) instead of the flat SimpleJobActionDuration constant. Set in
	 * StartSimpleJobExecution; defaults to SimpleJobActionDuration for non-harvest jobs.
	 */
	float ActiveJobActionDuration = 4.0f;

	/** Search radius for finding gather resources. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Survivor|Config")
	float GatherSearchRadius = 500.0f;

	// ============================================================================
	// GATHER JOB STATE (for ISM/HISM harvesting)
	// ============================================================================

	/** ISM/HISM component containing the target gather resource. */
	TWeakObjectPtr<UInstancedStaticMeshComponent> GatherTargetHISMComponent;

	/** Instance index within the ISM/HISM to harvest. */
	int32 GatherTargetInstanceIndex = INDEX_NONE;

	/** Item ID being gathered (for logging). */
	FName GatherTargetItemId = NAME_None;

	/** Harvest recipe ID to execute (for KeepOnHarvest targets like trees). */
	FName HarvestRecipeId = NAME_None;

	/** Whether we're doing a recipe-based harvest vs simple pickup. */
	bool bIsRecipeHarvest = false;

private:
	/** Update follow behavior during tick. */
	void UpdateFollowBehavior(float DeltaTime);

	/** Update simple job execution during tick. */
	void UpdateSimpleJobExecution(float DeltaTime);

	/** Start simple job execution (for jobs that don't need full BT). */
	void StartSimpleJobExecution();

	/** Complete the current simple job. */
	void CompleteSimpleJob(bool bSuccess);

	/** Check if a job type can be executed simply (without BT). */
	bool CanExecuteSimply(EMOSurvivorJobType JobType) const;

	/** Perform the actual job action (digging, foraging, gathering). */
	void PerformSimpleJobAction();

	// =========================================================================
	// CRAFT-AT-STATION JOB (V0 village vertical slice)
	// =========================================================================
	// Extends the simple-job state machine with craft legs. SimpleJobState:
	//   10 = moving to storage (withdraw leg)
	//   11 = withdrawing ingredients (timed handling action)
	//   12 = moving to station
	//   13 = crafting — the survivor's REAL UMOCraftingQueueComponent runs the
	//        recipe at its real duration; we poll for completion and resume if
	//        an interrupt paused it
	//   14 = moving back to storage (deposit leg)
	//   15 = depositing outputs (timed handling action)

	/** Resolve actors + start the withdraw leg. */
	void StartCraftJobExecution();

	/** Tick the craft-leg state machine (states 10-15). */
	void UpdateCraftJobExecution(float DeltaTime);

	/** Move recipe ingredients storage -> pawn. False if storage lacks them. */
	bool TransferCraftIngredients();

	/** Move recipe outputs pawn -> storage. */
	void DepositCraftOutputs();

	/** Inventory of a container/station actor (holder interface or component). */
	class UMOInventoryComponent* GetActorInventory(AActor* Actor) const;

	/** Station actor the craft job works at. */
	TWeakObjectPtr<AActor> CraftStationActor;

	/** Storage actor the craft job withdraws from / deposits to. */
	TWeakObjectPtr<AActor> CraftStorageActor;

	/** Recipe the craft job runs. */
	FName CraftJobRecipeId;

	/** Seconds of handling time for the withdraw/deposit legs (one combined
	 *  gesture for the batch — QoL batching, not an instant transfer). */
	UPROPERTY(EditAnywhere, Category = "MO|Survivor|Config")
	float CraftHandlingDuration = 2.0f;

	// =========================================================================
	// REFUEL-STATION JOB (F1/T3: keep the home fire burning)
	// =========================================================================
	// SimpleJobState:
	//   20 = moving to storage (fuel withdraw leg)
	//   21 = withdrawing fuel (timed handling action)
	//   22 = moving to station
	//   23 = loading the fuel tank + relighting a dead fire (timed handling)

	/** Resolve actors + start the fuel withdraw leg. */
	void StartRefuelJobExecution();

	/** Tick the refuel-leg state machine (states 20-23). */
	void UpdateRefuelJobExecution(float DeltaTime);

	/** Shared move-leg step for multi-leg jobs: arrival check + no-progress
	 *  and stall watchdogs (B1). Returns 1 arrived, 0 moving, -1 failed. */
	int32 UpdateJobMoveLeg(float DeltaTime, float ArriveDist, float StallDist);

	/** Fuel item the refuel job hauls. */
	FName RefuelItemId;

	/** Fuel items actually withdrawn from storage this trip. */
	int32 RefuelCarried = 0;

	// =========================================================================
	// EXCAVATE-AND-HAUL JOB (unit 3: pawn-automated dig -> haul -> dump)
	// =========================================================================
	// SimpleJobState:
	//   30 = moving to the Dig zone
	//   31 = digging one bounded bite (timed, volume-based) -> produces spoil
	//   32 = moving to the dump target (Fill zone center, or container)
	//   33 = depositing (Fill zone: raise terrain consuming spoil; Container: transfer)

	/** Resolve zones + start the move-to-dig leg. */
	void StartExcavateJobExecution();

	/** Tick the excavate-leg state machine (states 30-33). */
	void UpdateExcavateJobExecution(float DeltaTime);

	/** Dig zone GUID this job excavates (a UMODesignationSubsystem zone). */
	FGuid ExcavateDigZoneId;

	/** Fill zone GUID for FillZone dumps (a UMODesignationSubsystem zone). */
	FGuid ExcavateDumpZoneId;

	/** Where the dug earth goes this job. */
	EMOExcavateDumpMode ExcavateDumpMode = EMOExcavateDumpMode::FillZone;

	/** Item the dug earth becomes. */
	FName ExcavateSpoilItemId;

	/** Spoil items carried this trip (produced at dig, consumed/deposited at dump). */
	int32 ExcavateCarried = 0;

	/** Earth volume (m³) moved by one bite — the conserved quantity dug then filled. */
	float ExcavateBiteVolumeM3 = 0.0f;

	/** Real seconds one dig/deposit bite takes (volume × rate), computed at start. */
	float ExcavateBiteDuration = 4.0f;

	/** Per-job wedge watchdog, sized to the (possibly long) bite duration. */
	float ExcavateWatchdogSeconds = 120.0f;

	/** World location of the dump target (Fill zone center, or container). */
	FVector ExcavateDumpLocation = FVector::ZeroVector;

	/** Find nearest HISM resource matching the job type for gather jobs. */
	bool FindNearestGatherResource(EMOSurvivorJobType JobType);

	/** Find nearest harvestable target (trees, etc.) using harvest recipes. */
	bool FindNearestHarvestTarget(FName RequiredTag, float SearchRadius, FName& OutRecipeId);

	/**
	 * (H24) Resolve the real per-action harvest duration for the current recipe
	 * gather target from its resource definition's BaseActionTime (with the same
	 * missing-tool time penalty BeginHarvest applies). Returns the flat
	 * SimpleJobActionDuration fallback if the definition/action can't be resolved.
	 */
	float ResolveGatherActionDuration() const;

	/** Get item tags that match the job type for filtering. */
	TArray<FName> GetItemTagsForJobType(EMOSurvivorJobType JobType) const;

	/** Check if an item definition matches the job type. */
	bool DoesItemMatchJobType(FName ItemId, EMOSurvivorJobType JobType) const;

	/** Check if this is a gather-type job. */
	bool IsGatherJob(EMOSurvivorJobType JobType) const;

	/** Setup blackboard keys for current command/job. */
	void SetupBlackboardForSurvivor();

	/** Clear all survivor-specific blackboard keys. */
	void ClearSurvivorBlackboardData();

	/** Broadcast command change. */
	void BroadcastCommandChange(FName CommandName);

	/** Handle job queue changes - start processing if idle. */
	UFUNCTION()
	void HandleJobQueueChanged();

	/** Cached job queue component. */
	TWeakObjectPtr<UMOSurvivorJobQueueComponent> CachedJobQueue;

	/** Cached skills component. */
	TWeakObjectPtr<UMOSkillsComponent> CachedSkillsComponent;
};
