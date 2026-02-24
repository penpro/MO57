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

/**
 * AI Controller for recruited survivors.
 *
 * Extends AMOAIController with survivor-specific behaviors:
 * - Follow target (player or other pawn)
 * - Stay at location
 * - Go to home
 * - Process job queue
 *
 * The controller coordinates with UMOSurvivorJobQueueComponent on the pawn
 * to execute assigned jobs.
 */
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

	/** Timer for job actions (like digging duration). */
	UPROPERTY(BlueprintReadOnly, Category = "MO|Survivor|State")
	float SimpleJobTimer = 0.0f;

	/** Duration for simple job actions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Survivor|Config")
	float SimpleJobActionDuration = 4.0f;

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

	/** Find nearest HISM resource matching the job type for gather jobs. */
	bool FindNearestGatherResource(EMOSurvivorJobType JobType);

	/** Find nearest harvestable target (trees, etc.) using harvest recipes. */
	bool FindNearestHarvestTarget(FName RequiredTag, float SearchRadius, FName& OutRecipeId);

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
