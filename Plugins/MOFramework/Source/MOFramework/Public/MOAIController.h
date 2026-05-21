/**
 * =============================================================================
 * MOAIController.h - Base AI Controller for Framework Pawns
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Base AI controller for all autonomous pawns in the MO Framework. Provides
 * behavior tree execution, task state management, blackboard integration,
 * and movement helpers. Extended by MOSurvivorController and MOCreatureController.
 *
 * KEY RESPONSIBILITIES:
 * 1. Execute behavior trees for autonomous task execution
 * 2. Track and report task state (Idle, Moving, Performing, Failed)
 * 3. Manage blackboard data for behavior tree decisions
 * 4. Provide movement commands with configurable acceptance radius
 * 5. Broadcast task state changes and completions
 *
 * OWNERSHIP:
 * - Owner: Possesses autonomous pawns (not player-controlled)
 * - Lifespan: Exists while pawn is under AI control
 *
 * TASK STATE MACHINE:
 * Idle -> MovingToTarget -> PerformingTask -> Returning -> Idle
 *                      \-> Failed (on error)
 *
 * BEHAVIOR TREE INTEGRATION:
 * - BehaviorTreeComponent: Executes behavior trees
 * - BlackboardComponent: Shared memory for BT decisions
 * - DefaultBehaviorTree: Loaded on BeginPlay if set
 * - SetupBlackboardForTask(): Writes task data to blackboard
 *
 * TASK ASSIGNMENT FLOW:
 * AssignTask(Name, Target, Location, BT)
 * -> SetupBlackboardForTask()
 * -> RunBehaviorTree(BT or Default)
 * -> BT nodes update state via SetTaskState()
 * -> ReportTaskComplete() when done
 * -> OnTaskCompleted broadcast
 *
 * CRITICAL PATTERNS:
 * 1. TASK STATE: Always update via SetTaskState() for broadcasts
 * 2. MOVEMENT: Use MoveToLocationWithRadius/MoveToActorWithRadius
 * 3. COMPLETION: Call ReportTaskComplete(success) from BT tasks
 *
 * KNOWN PITFALLS:
 * 1. BLACKBOARD SETUP: Must call SetupBlackboardForTask() before BT runs
 * 2. MOVEMENT CALLBACK: OnMoveCompleted may fire multiple times
 * 3. BEHAVIOR TREE: BT must exist and be properly configured
 *
 * RELATED FILES:
 * - MOSurvivorController.h - Extends for recruited survivors
 * - MOCreatureController.h - Extends for creatures with perception
 * - BTTask_*.h - Behavior tree tasks using this controller
 *
 * TESTING CHECKLIST:
 * [ ] Task assignment runs behavior tree
 * [ ] Task state changes broadcast correctly
 * [ ] Movement completion triggers state change
 * [ ] CancelCurrentTask stops BT and clears state
 * [ ] Task completion broadcasts with success/fail
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "MOAIController.generated.h"

class UBehaviorTreeComponent;
class UBlackboardComponent;
class UBehaviorTree;

/**
 * AI task state for pawns under AI control.
 */
UENUM(BlueprintType)
enum class EMOAITaskState : uint8
{
	Idle,			// No current task
	MovingToTarget,	// Moving towards a target location
	PerformingTask,	// Executing a task (chopping, gathering, etc.)
	Returning,		// Returning to a location (home, storage, etc.)
	Waiting,		// Waiting for something (resource respawn, etc.)
	Failed			// Task failed
};

/**
 * Delegate fired when AI task state changes.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnAITaskStateChanged, EMOAITaskState, OldState, EMOAITaskState, NewState);

/**
 * Delegate fired when AI completes or fails a task.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnAITaskCompleted, bool, bSuccess, FString, TaskName);

/**
 * Base AI controller for autonomous pawns in the MO Framework.
 * Used for pawns that the player has assigned to perform tasks (cut trees, gather, patrol, etc.).
 *
 * Responsibilities:
 * - Execute behavior trees for autonomous tasks
 * - Report task status back to player/management systems
 * - Handle movement and action execution
 */
UCLASS()
class MOFRAMEWORK_API AMOAIController : public AAIController
{
	GENERATED_BODY()

public:
	AMOAIController();

	// ============================================================================
	// COMPONENTS
	// ============================================================================

	/** Behavior tree component for task execution. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|AI")
	TObjectPtr<UBehaviorTreeComponent> BehaviorTreeComponent;

	/** Blackboard component for AI data storage. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|AI")
	TObjectPtr<UBlackboardComponent> BlackboardComponent;

	// ============================================================================
	// STATE
	// ============================================================================

	/** Current task state. */
	UPROPERTY(BlueprintReadOnly, Category="MO|AI|State")
	EMOAITaskState CurrentTaskState = EMOAITaskState::Idle;

	/** Name of the current task being executed. */
	UPROPERTY(BlueprintReadOnly, Category="MO|AI|State")
	FString CurrentTaskName;

	/** Target actor for the current task (if any). */
	UPROPERTY(BlueprintReadOnly, Category="MO|AI|State")
	TWeakObjectPtr<AActor> CurrentTaskTarget;

	/** Target location for the current task. */
	UPROPERTY(BlueprintReadOnly, Category="MO|AI|State")
	FVector CurrentTaskLocation;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Default behavior tree to run when no specific task is assigned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|AI|Config")
	TSoftObjectPtr<UBehaviorTree> DefaultBehaviorTree;

	/** Cached loaded behavior tree (resolved once in OnPossess, reused). */
	UPROPERTY(Transient)
	TObjectPtr<UBehaviorTree> CachedDefaultBehaviorTree;

	/** Acceptance radius for reaching target locations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|AI|Config")
	float AcceptanceRadius = 100.0f;

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Fired when task state changes. */
	UPROPERTY(BlueprintAssignable, Category="MO|AI|Events")
	FMOOnAITaskStateChanged OnTaskStateChanged;

	/** Fired when a task completes or fails. */
	UPROPERTY(BlueprintAssignable, Category="MO|AI|Events")
	FMOOnAITaskCompleted OnTaskCompleted;

	// ============================================================================
	// TASK MANAGEMENT
	// ============================================================================

	/**
	 * Assign a task to this AI controller.
	 * @param TaskName - Name of the task to execute
	 * @param TargetActor - Optional target actor for the task
	 * @param TargetLocation - Optional target location for the task
	 * @param TaskBehaviorTree - Optional specific behavior tree to run for this task
	 * @return True if the task was successfully assigned
	 */
	UFUNCTION(BlueprintCallable, Category="MO|AI|Tasks")
	bool AssignTask(const FString& TaskName, AActor* TargetActor = nullptr,
		FVector TargetLocation = FVector::ZeroVector, UBehaviorTree* TaskBehaviorTree = nullptr);

	/**
	 * Cancel the current task.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|AI|Tasks")
	void CancelCurrentTask();

	/**
	 * Set the task state.
	 * @param NewState - The new task state
	 */
	UFUNCTION(BlueprintCallable, Category="MO|AI|Tasks")
	void SetTaskState(EMOAITaskState NewState);

	/**
	 * Report task completion (called by behavior tree tasks).
	 * @param bSuccess - Whether the task succeeded
	 */
	UFUNCTION(BlueprintCallable, Category="MO|AI|Tasks")
	void ReportTaskComplete(bool bSuccess);

	/**
	 * Check if the AI is currently idle.
	 */
	UFUNCTION(BlueprintPure, Category="MO|AI|Query")
	bool IsIdle() const { return CurrentTaskState == EMOAITaskState::Idle; }

	/**
	 * Check if the AI is currently executing a task.
	 */
	UFUNCTION(BlueprintPure, Category="MO|AI|Query")
	bool IsExecutingTask() const { return CurrentTaskState != EMOAITaskState::Idle && CurrentTaskState != EMOAITaskState::Failed; }

	// ============================================================================
	// MOVEMENT
	// ============================================================================

	/**
	 * Command the AI to move to a location.
	 * @param Location - Target location
	 * @param AcceptanceRadiusOverride - Optional override for acceptance radius (-1 to use default)
	 */
	void MoveToLocationWithRadius(FVector Location, float AcceptanceRadiusOverride = -1.0f);

	/**
	 * Command the AI to move to an actor.
	 * @param TargetActor - Target actor to move to
	 * @param AcceptanceRadiusOverride - Optional override for acceptance radius (-1 to use default)
	 */
	void MoveToActorWithRadius(AActor* TargetActor, float AcceptanceRadiusOverride = -1.0f);

	/**
	 * Stop current movement (calls parent StopMovement).
	 */
	void StopCurrentMovement();

protected:
	// ============================================================================
	// OVERRIDES
	// ============================================================================

	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/** Called when movement completes. */
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	// ============================================================================
	// BLACKBOARD
	// ============================================================================

	/**
	 * Set up the blackboard with task data.
	 */
	void SetupBlackboardForTask();

	/**
	 * Clear task data from the blackboard.
	 */
	void ClearBlackboardTaskData();
};
