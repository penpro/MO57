#include "MOAIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Navigation/PathFollowingComponent.h"

AMOAIController::AMOAIController()
{
	// Create behavior tree component
	BehaviorTreeComponent = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorTreeComponent"));

	// Create blackboard component
	BlackboardComponent = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComponent"));

	// Set default blackboard
	Blackboard = BlackboardComponent;
}

void AMOAIController::BeginPlay()
{
	Super::BeginPlay();
}

void AMOAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	UE_LOG(LogTemp, Log, TEXT("AMOAIController: Possessed %s"), InPawn ? *InPawn->GetName() : TEXT("None"));

	// Run default behavior tree if set and no task is active
	if (IsIdle() && DefaultBehaviorTree.IsValid())
	{
		if (UBehaviorTree* BT = DefaultBehaviorTree.LoadSynchronous())
		{
			RunBehaviorTree(BT);
		}
	}
}

void AMOAIController::OnUnPossess()
{
	// Stop behavior tree
	if (BehaviorTreeComponent)
	{
		BehaviorTreeComponent->StopTree();
	}

	// Cancel any current task
	CancelCurrentTask();

	Super::OnUnPossess();
}

// ============================================================================
// TASK MANAGEMENT
// ============================================================================

bool AMOAIController::AssignTask(const FString& TaskName, AActor* TargetActor,
	FVector TargetLocation, UBehaviorTree* TaskBehaviorTree)
{
	// Cancel any existing task
	CancelCurrentTask();

	// Store task data
	CurrentTaskName = TaskName;
	CurrentTaskTarget = TargetActor;
	CurrentTaskLocation = TargetLocation;

	// If no specific behavior tree provided, use default
	UBehaviorTree* BTToRun = TaskBehaviorTree;
	if (!BTToRun && DefaultBehaviorTree.IsValid())
	{
		BTToRun = DefaultBehaviorTree.LoadSynchronous();
	}

	if (!BTToRun)
	{
		UE_LOG(LogTemp, Warning, TEXT("AMOAIController: No behavior tree available for task '%s'"), *TaskName);
		SetTaskState(EMOAITaskState::Failed);
		return false;
	}

	// Setup blackboard with task data
	SetupBlackboardForTask();

	// Run the behavior tree
	if (!RunBehaviorTree(BTToRun))
	{
		UE_LOG(LogTemp, Warning, TEXT("AMOAIController: Failed to run behavior tree for task '%s'"), *TaskName);
		SetTaskState(EMOAITaskState::Failed);
		return false;
	}

	SetTaskState(EMOAITaskState::MovingToTarget);

	UE_LOG(LogTemp, Log, TEXT("AMOAIController: Assigned task '%s' to %s"),
		*TaskName, GetPawn() ? *GetPawn()->GetName() : TEXT("None"));

	return true;
}

void AMOAIController::CancelCurrentTask()
{
	if (CurrentTaskState == EMOAITaskState::Idle)
	{
		return;
	}

	// Stop behavior tree
	if (BehaviorTreeComponent)
	{
		BehaviorTreeComponent->StopTree();
	}

	// Stop movement
	StopCurrentMovement();

	// Clear task data
	FString CancelledTask = CurrentTaskName;
	CurrentTaskName.Empty();
	CurrentTaskTarget.Reset();
	CurrentTaskLocation = FVector::ZeroVector;

	// Clear blackboard
	ClearBlackboardTaskData();

	// Update state
	SetTaskState(EMOAITaskState::Idle);

	UE_LOG(LogTemp, Log, TEXT("AMOAIController: Cancelled task '%s'"), *CancelledTask);
}

void AMOAIController::SetTaskState(EMOAITaskState NewState)
{
	if (CurrentTaskState == NewState)
	{
		return;
	}

	EMOAITaskState OldState = CurrentTaskState;
	CurrentTaskState = NewState;

	// Broadcast state change
	OnTaskStateChanged.Broadcast(OldState, NewState);
}

void AMOAIController::ReportTaskComplete(bool bSuccess)
{
	FString CompletedTask = CurrentTaskName;

	// Clear task data
	CurrentTaskName.Empty();
	CurrentTaskTarget.Reset();
	CurrentTaskLocation = FVector::ZeroVector;
	ClearBlackboardTaskData();

	// Update state
	SetTaskState(bSuccess ? EMOAITaskState::Idle : EMOAITaskState::Failed);

	// Broadcast completion
	OnTaskCompleted.Broadcast(bSuccess, CompletedTask);

	UE_LOG(LogTemp, Log, TEXT("AMOAIController: Task '%s' %s"),
		*CompletedTask, bSuccess ? TEXT("completed successfully") : TEXT("failed"));

	// If we have a default behavior tree, restart it
	if (IsIdle() && DefaultBehaviorTree.IsValid())
	{
		if (UBehaviorTree* BT = DefaultBehaviorTree.LoadSynchronous())
		{
			RunBehaviorTree(BT);
		}
	}
}

// ============================================================================
// MOVEMENT
// ============================================================================

void AMOAIController::MoveToLocationWithRadius(FVector Location, float AcceptanceRadiusOverride)
{
	const float Radius = (AcceptanceRadiusOverride >= 0.0f) ? AcceptanceRadiusOverride : AcceptanceRadius;

	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(Location);
	MoveRequest.SetAcceptanceRadius(Radius);

	MoveTo(MoveRequest);
}

void AMOAIController::MoveToActorWithRadius(AActor* TargetActor, float AcceptanceRadiusOverride)
{
	if (!TargetActor)
	{
		return;
	}

	const float Radius = (AcceptanceRadiusOverride >= 0.0f) ? AcceptanceRadiusOverride : AcceptanceRadius;

	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalActor(TargetActor);
	MoveRequest.SetAcceptanceRadius(Radius);

	MoveTo(MoveRequest);
}

void AMOAIController::StopCurrentMovement()
{
	Super::StopMovement();
}

void AMOAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	if (Result.IsSuccess())
	{
		// Movement succeeded - if we were moving to target, switch to performing task
		if (CurrentTaskState == EMOAITaskState::MovingToTarget)
		{
			SetTaskState(EMOAITaskState::PerformingTask);
		}
	}
	else if (Result.Code == EPathFollowingResult::Blocked || Result.Code == EPathFollowingResult::OffPath)
	{
		// Movement failed
		UE_LOG(LogTemp, Warning, TEXT("AMOAIController: Movement failed for task '%s'"), *CurrentTaskName);
		// Don't immediately fail - behavior tree may handle recovery
	}
}

// ============================================================================
// BLACKBOARD
// ============================================================================

void AMOAIController::SetupBlackboardForTask()
{
	if (!BlackboardComponent)
	{
		return;
	}

	// Set common task values
	// Note: Actual key names depend on your blackboard asset
	// These are common conventions
	if (CurrentTaskTarget.IsValid())
	{
		BlackboardComponent->SetValueAsObject(TEXT("TargetActor"), CurrentTaskTarget.Get());
	}

	if (!CurrentTaskLocation.IsZero())
	{
		BlackboardComponent->SetValueAsVector(TEXT("TargetLocation"), CurrentTaskLocation);
	}

	BlackboardComponent->SetValueAsString(TEXT("TaskName"), CurrentTaskName);
}

void AMOAIController::ClearBlackboardTaskData()
{
	if (!BlackboardComponent)
	{
		return;
	}

	BlackboardComponent->ClearValue(TEXT("TargetActor"));
	BlackboardComponent->ClearValue(TEXT("TargetLocation"));
	BlackboardComponent->ClearValue(TEXT("TaskName"));
}
