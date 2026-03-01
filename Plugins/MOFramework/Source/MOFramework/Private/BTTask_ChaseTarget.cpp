#include "BTTask_ChaseTarget.h"
#include "MOFramework.h"
#include "MOCreature.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Navigation/PathFollowingComponent.h"

UBTTask_ChaseTarget::UBTTask_ChaseTarget()
{
	NodeName = "Chase Target";
	bNotifyTick = true;
	bNotifyTaskFinished = true;

	// Default blackboard key
	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_ChaseTarget, TargetActorKey), AActor::StaticClass());
}

EBTNodeResult::Type UBTTask_ChaseTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	APawn* OwnerPawn = AIController->GetPawn();
	if (!OwnerPawn)
	{
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	AActor* TargetActor = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetActorKey.SelectedKeyName));
	if (!TargetActor)
	{
		return EBTNodeResult::Failed;
	}

	// Initialize memory
	FBTChaseMemory* Memory = reinterpret_cast<FBTChaseMemory*>(NodeMemory);
	Memory->OriginalMaxWalkSpeed = 0.f;
	Memory->bAppliedSpeedBoost = false;
	Memory->LastPosition = OwnerPawn->GetActorLocation();
	Memory->StuckTime = 0.f;
	Memory->bIsRecovering = false;
	Memory->RecoveryTime = 0.f;
	Memory->RecoveryDirection = FVector::ZeroVector;

	// Apply run speed for chasing
	if (ACharacter* Character = Cast<ACharacter>(OwnerPawn))
	{
		if (UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement())
		{
			Memory->OriginalMaxWalkSpeed = MovementComp->MaxWalkSpeed;

			// Use creature's RunSpeed if available, otherwise fall back to multiplier
			if (AMOCreature* Creature = Cast<AMOCreature>(OwnerPawn))
			{
				MovementComp->MaxWalkSpeed = Creature->GetRunSpeed();
				UE_LOG(LogMOFramework, Verbose, TEXT("[BTChaseTarget] Using creature RunSpeed: %.1f"), Creature->GetRunSpeed());
			}
			else
			{
				MovementComp->MaxWalkSpeed *= ChaseSpeedMultiplier;
			}
			Memory->bAppliedSpeedBoost = true;
		}
	}

	// Start moving toward target
	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalActor(TargetActor);
	MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetAllowPartialPath(true);
	MoveRequest.SetProjectGoalLocation(true);

	AIController->MoveTo(MoveRequest);

	UE_LOG(LogMOFramework, Verbose, TEXT("[BTChaseTarget] Started chasing %s"), *TargetActor->GetName());

	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_ChaseTarget::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController)
	{
		AIController->StopMovement();

		if (APawn* OwnerPawn = AIController->GetPawn())
		{
			FBTChaseMemory* Memory = reinterpret_cast<FBTChaseMemory*>(NodeMemory);
			RestoreMovementSpeed(OwnerPawn, Memory);
		}
	}

	return EBTNodeResult::Aborted;
}

void UBTTask_ChaseTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	APawn* OwnerPawn = AIController->GetPawn();
	if (!OwnerPawn)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	AActor* TargetActor = BlackboardComp ? Cast<AActor>(BlackboardComp->GetValueAsObject(TargetActorKey.SelectedKeyName)) : nullptr;

	// Check if target is lost
	if (!TargetActor)
	{
		FBTChaseMemory* Memory = reinterpret_cast<FBTChaseMemory*>(NodeMemory);
		RestoreMovementSpeed(OwnerPawn, Memory);
		AIController->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	float DistanceToTarget = FVector::Dist(OwnerPawn->GetActorLocation(), TargetActor->GetActorLocation());

	// Water avoidance - stop chasing if target goes into water or we're approaching water
	const float WaterLevel = 0.f;
	FVector CurrentLocation = OwnerPawn->GetActorLocation();
	FVector TargetLocation = TargetActor->GetActorLocation();

	if (TargetLocation.Z < WaterLevel - 50.f)
	{
		// Target is in water - give up chase
		FBTChaseMemory* Memory = reinterpret_cast<FBTChaseMemory*>(NodeMemory);
		RestoreMovementSpeed(OwnerPawn, Memory);
		AIController->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (CurrentLocation.Z < WaterLevel + 50.f)
	{
		// We're in or near water - stop and let wander task handle escape
		FBTChaseMemory* Memory = reinterpret_cast<FBTChaseMemory*>(NodeMemory);
		RestoreMovementSpeed(OwnerPawn, Memory);
		AIController->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// Check if we're close enough (in attack range)
	if (DistanceToTarget <= AcceptanceRadius)
	{
		FBTChaseMemory* Memory = reinterpret_cast<FBTChaseMemory*>(NodeMemory);
		RestoreMovementSpeed(OwnerPawn, Memory);
		AIController->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	// Check if target is too far (give up)
	if (MaxChaseDistance > 0.f && DistanceToTarget > MaxChaseDistance)
	{
		FBTChaseMemory* Memory = reinterpret_cast<FBTChaseMemory*>(NodeMemory);
		RestoreMovementSpeed(OwnerPawn, Memory);
		AIController->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// Handle stuck detection and recovery
	FBTChaseMemory* Memory = reinterpret_cast<FBTChaseMemory*>(NodeMemory);
	if (HandleStuckRecovery(OwnerPawn, TargetActor, Memory, DeltaSeconds))
	{
		// Currently in recovery mode - don't do normal pathfinding
		return;
	}

	// Check movement status
	UPathFollowingComponent* PathComp = AIController->GetPathFollowingComponent();
	if (PathComp)
	{
		EPathFollowingStatus::Type Status = PathComp->GetStatus();
		if (Status == EPathFollowingStatus::Idle)
		{
			// Movement finished but we're not in range - re-issue move command
			FAIMoveRequest MoveRequest;
			MoveRequest.SetGoalActor(TargetActor);
			MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
			MoveRequest.SetUsePathfinding(true);
			MoveRequest.SetAllowPartialPath(true);

			AIController->MoveTo(MoveRequest);
		}
	}
}

void UBTTask_ChaseTarget::RestoreMovementSpeed(APawn* Pawn, FBTChaseMemory* Memory) const
{
	if (!Memory->bAppliedSpeedBoost)
	{
		return;
	}

	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement())
		{
			MovementComp->MaxWalkSpeed = Memory->OriginalMaxWalkSpeed;
		}
	}
	Memory->bAppliedSpeedBoost = false;
}

bool UBTTask_ChaseTarget::HandleStuckRecovery(APawn* Pawn, AActor* Target, FBTChaseMemory* Memory, float DeltaSeconds)
{
	if (!Pawn || !Target)
	{
		return false;
	}

	FVector CurrentPosition = Pawn->GetActorLocation();

	// If currently in recovery mode, continue backing up
	if (Memory->bIsRecovering)
	{
		Memory->RecoveryTime += DeltaSeconds;

		if (Memory->RecoveryTime >= RecoveryDuration)
		{
			// Recovery complete - reset and try normal movement again
			Memory->bIsRecovering = false;
			Memory->StuckTime = 0.f;
			Memory->LastPosition = CurrentPosition;

			UE_LOG(LogMOFramework, Verbose, TEXT("[BTChaseTarget] Recovery complete, resuming chase"));
			return false;
		}

		// Continue moving in recovery direction (away from target)
		Pawn->AddMovementInput(Memory->RecoveryDirection, 1.0f);

		// Face recovery direction
		FRotator TargetRotation = Memory->RecoveryDirection.Rotation();
		Pawn->SetActorRotation(FMath::RInterpTo(Pawn->GetActorRotation(), TargetRotation, DeltaSeconds, 5.0f));

		return true;
	}

	// Check if we've moved enough since last check
	float DistanceMoved = FVector::Dist(CurrentPosition, Memory->LastPosition);

	if (DistanceMoved < StuckMinMovement)
	{
		// Not moving much - accumulate stuck time
		Memory->StuckTime += DeltaSeconds;

		if (Memory->StuckTime >= StuckThresholdTime)
		{
			// We're stuck - start recovery
			Memory->bIsRecovering = true;
			Memory->RecoveryTime = 0.f;

			// Calculate recovery direction - back up and slightly to the side
			FVector ToTarget = (Target->GetActorLocation() - CurrentPosition).GetSafeNormal();
			FVector BackwardDirection = -ToTarget;

			// Add some sideways movement to escape corners
			float SideSign = FMath::RandBool() ? 1.f : -1.f;
			FVector SideDirection = FVector::CrossProduct(BackwardDirection, FVector::UpVector) * SideSign;
			Memory->RecoveryDirection = (BackwardDirection + SideDirection * 0.5f).GetSafeNormal();
			Memory->RecoveryDirection.Z = 0.f;

			UE_LOG(LogMOFramework, Log, TEXT("[BTChaseTarget] %s stuck while chasing, backing up"), *Pawn->GetName());

			// Stop current movement
			if (AAIController* AIController = Cast<AAIController>(Pawn->GetController()))
			{
				AIController->StopMovement();
			}

			return true;
		}
	}
	else
	{
		// Moving normally - reset stuck detection
		Memory->StuckTime = 0.f;
		Memory->LastPosition = CurrentPosition;
	}

	return false;
}

FString UBTTask_ChaseTarget::GetStaticDescription() const
{
	return FString::Printf(TEXT("Chase: %s\nRadius: %.0f, Speed: %.1fx"),
		*TargetActorKey.SelectedKeyName.ToString(),
		AcceptanceRadius,
		ChaseSpeedMultiplier);
}
