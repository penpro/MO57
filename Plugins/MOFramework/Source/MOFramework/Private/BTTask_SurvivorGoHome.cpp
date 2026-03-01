#include "BTTask_SurvivorGoHome.h"
#include "MOFramework.h"
#include "MOSurvivorJobQueueComponent.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/Pawn.h"

UBTTask_SurvivorGoHome::UBTTask_SurvivorGoHome()
{
	NodeName = TEXT("Survivor Go Home");
	bNotifyTick = true;
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_SurvivorGoHome::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBTTask_SurvivorGoHomeMemory* Memory = reinterpret_cast<FBTTask_SurvivorGoHomeMemory*>(NodeMemory);
	Memory->bStartedMoving = false;
	Memory->StuckTime = 0.0f;
	Memory->LastStuckCheckTime = 0.0f;

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		return EBTNodeResult::Failed;
	}

	UMOSurvivorJobQueueComponent* JobQueue = GetJobQueueComponent(Pawn);
	if (!JobQueue)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[BTTask_SurvivorGoHome] No job queue component on pawn"));
		return EBTNodeResult::Failed;
	}

	// Check if we have a home
	if (!JobQueue->HasHome())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorGoHome] No home assigned, task skipped"));
		return EBTNodeResult::Failed;
	}

	Memory->TargetLocation = JobQueue->HomeLocation;
	Memory->LastPosition = Pawn->GetActorLocation();

	// Check if we're already at home
	float DistanceToHome = FVector::Dist(Pawn->GetActorLocation(), Memory->TargetLocation);
	if (DistanceToHome <= AcceptanceRadius)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorGoHome] Already at home"));
		return EBTNodeResult::Succeeded;
	}

	// Start moving to home
	AIController->MoveToLocation(Memory->TargetLocation, AcceptanceRadius * 0.5f);
	Memory->bStartedMoving = true;

	UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorGoHome] Moving to home at %s (%.0f units away)"),
		*Memory->TargetLocation.ToString(), DistanceToHome);

	return EBTNodeResult::InProgress;
}

void UBTTask_SurvivorGoHome::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FBTTask_SurvivorGoHomeMemory* Memory = reinterpret_cast<FBTTask_SurvivorGoHomeMemory*>(NodeMemory);

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	FVector CurrentLocation = Pawn->GetActorLocation();
	float DistanceToTarget = FVector::Dist(CurrentLocation, Memory->TargetLocation);

	// Check if we've arrived
	if (DistanceToTarget <= AcceptanceRadius)
	{
		AIController->StopMovement();
		UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorGoHome] Arrived at home"));
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	// Stuck detection - check periodically to avoid overhead
	Memory->LastStuckCheckTime += DeltaSeconds;
	if (Memory->LastStuckCheckTime >= StuckCheckInterval)
	{
		float DistanceMoved = FVector::Dist(CurrentLocation, Memory->LastPosition);

		if (DistanceMoved < StuckMinMovement)
		{
			// Not moving much - accumulate stuck time
			Memory->StuckTime += Memory->LastStuckCheckTime;

			if (Memory->StuckTime >= StuckThresholdTime)
			{
				// We're stuck - just succeed anyway (close enough)
				UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorGoHome] Stuck near home, considering arrived"));
				AIController->StopMovement();
				FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
				return;
			}
		}
		else
		{
			// Moving normally - reset stuck detection
			Memory->StuckTime = 0.0f;
		}

		Memory->LastPosition = CurrentLocation;
		Memory->LastStuckCheckTime = 0.0f;
	}

	// Check if movement completed but we're not there yet
	EPathFollowingStatus::Type MoveStatus = AIController->GetMoveStatus();
	if (MoveStatus == EPathFollowingStatus::Idle && Memory->bStartedMoving)
	{
		// Path finished but we're not at acceptance radius
		// Could be partial path - try again or succeed if close enough
		if (DistanceToTarget <= AcceptanceRadius * 2.0f)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorGoHome] Close enough to home (%.0f units)"), DistanceToTarget);
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		}
		else
		{
			// Re-issue move command
			AIController->MoveToLocation(Memory->TargetLocation, AcceptanceRadius * 0.5f);
		}
	}
}

EBTNodeResult::Type UBTTask_SurvivorGoHome::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController)
	{
		AIController->StopMovement();
	}

	return EBTNodeResult::Aborted;
}

FString UBTTask_SurvivorGoHome::GetStaticDescription() const
{
	return FString::Printf(TEXT("Go to assigned home location\nAcceptance: %.0f"), AcceptanceRadius);
}

UMOSurvivorJobQueueComponent* UBTTask_SurvivorGoHome::GetJobQueueComponent(APawn* Pawn) const
{
	if (!Pawn)
	{
		return nullptr;
	}

	return Pawn->FindComponentByClass<UMOSurvivorJobQueueComponent>();
}
