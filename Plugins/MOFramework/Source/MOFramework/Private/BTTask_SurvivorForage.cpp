#include "BTTask_SurvivorForage.h"
#include "MOFramework.h"
#include "MOSurvivorJobQueueComponent.h"
#include "MOSurvivorController.h"
#include "MOForagingSubsystem.h"
#include "MOSkillsComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"

UBTTask_SurvivorForage::UBTTask_SurvivorForage()
{
	NodeName = TEXT("Survivor Forage Ground");
	bNotifyTick = true;
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_SurvivorForage::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBTTask_SurvivorForageMemory* Memory = reinterpret_cast<FBTTask_SurvivorForageMemory*>(NodeMemory);
	Memory->State = 0; // Start by picking a point
	Memory->ForageTime = 0.0f;
	Memory->ForagesCompleted = 0;

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
		UE_LOG(LogMOFramework, Warning, TEXT("[BTTask_SurvivorForage] No job queue component on pawn"));
		return EBTNodeResult::Failed;
	}

	// Pick a forage point
	FVector ForageLocation;
	if (!PickForagePoint(Pawn, ForageLocation))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[BTTask_SurvivorForage] Could not find valid forage point"));
		return EBTNodeResult::Failed;
	}

	Memory->TargetLocation = ForageLocation;
	Memory->State = 1; // Moving to point

	// Start moving to forage point
	AIController->MoveToLocation(ForageLocation, 50.0f);

	UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorForage] Moving to forage point at %s"), *ForageLocation.ToString());

	return EBTNodeResult::InProgress;
}

void UBTTask_SurvivorForage::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FBTTask_SurvivorForageMemory* Memory = reinterpret_cast<FBTTask_SurvivorForageMemory*>(NodeMemory);

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

	UMOSurvivorJobQueueComponent* JobQueue = GetJobQueueComponent(Pawn);
	if (!JobQueue)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	FMOSurvivorJobEntry CurrentJob = JobQueue->GetCurrentJob();
	if (!CurrentJob.IsValid())
	{
		// Job was cancelled or completed externally
		FinishLatentTask(OwnerComp, EBTNodeResult::Aborted);
		return;
	}

	switch (Memory->State)
	{
	case 1: // Moving to forage point
		{
			// Check if we've arrived
			float DistanceToTarget = FVector::Dist(Pawn->GetActorLocation(), Memory->TargetLocation);
			if (DistanceToTarget <= MinMoveDistance)
			{
				// Stop moving and start foraging
				AIController->StopMovement();
				Memory->State = 2;
				Memory->ForageTime = 0.0f;
				UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorForage] Arrived at forage point, starting forage"));
			}
			else
			{
				// Check if movement failed (not actively moving)
				EPathFollowingStatus::Type MoveStatus = AIController->GetMoveStatus();
				if (MoveStatus != EPathFollowingStatus::Moving)
				{
					// Movement finished - try picking a new point
					FVector NewLocation;
					if (PickForagePoint(Pawn, NewLocation))
					{
						Memory->TargetLocation = NewLocation;
						AIController->MoveToLocation(NewLocation, 50.0f);
					}
					else
					{
						// Can't find any forage point
						JobQueue->CompleteCurrentJob(false);
						FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
					}
				}
			}
			break;
		}

	case 2: // Foraging
		{
			Memory->ForageTime += DeltaSeconds;

			// Update job progress
			float Progress = FMath::Clamp(Memory->ForageTime / ForageDuration, 0.0f, 1.0f);
			JobQueue->UpdateJobProgress(CurrentJob.JobId, Progress);

			if (Memory->ForageTime >= ForageDuration)
			{
				// Forage complete
				Memory->ForagesCompleted++;

				UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorForage] Forage complete (%d/%d)"),
					Memory->ForagesCompleted, CurrentJob.RepeatCount);

				// Award XP through the controller
				if (AMOSurvivorController* SurvivorController = Cast<AMOSurvivorController>(AIController))
				{
					SurvivorController->AwardJobExperience(CurrentJob.JobType);
				}

				// Perform the actual foraging/digging action
				PerformForageAction(Pawn, CurrentJob.JobType, Memory->TargetLocation);

				// Check if we need to repeat
				if (Memory->ForagesCompleted >= CurrentJob.RepeatCount)
				{
					// All repeats done - job complete
					JobQueue->CompleteCurrentJob(true);
					FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
					return;
				}

				// Need more forages - pick next point
				FVector NewLocation;
				if (PickForagePoint(Pawn, NewLocation))
				{
					Memory->TargetLocation = NewLocation;
					Memory->State = 1;
					Memory->ForageTime = 0.0f;
					AIController->MoveToLocation(NewLocation, 50.0f);
				}
				else
				{
					// No more valid points - complete with what we have
					JobQueue->CompleteCurrentJob(true);
					FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
				}
			}
			break;
		}

	default:
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		break;
	}
}

EBTNodeResult::Type UBTTask_SurvivorForage::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController)
	{
		AIController->StopMovement();
	}

	return EBTNodeResult::Aborted;
}

bool UBTTask_SurvivorForage::PickForagePoint(APawn* Pawn, FVector& OutLocation)
{
	if (!Pawn)
	{
		return false;
	}

	UWorld* World = Pawn->GetWorld();
	if (!World)
	{
		return false;
	}

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
	if (!NavSystem)
	{
		return false;
	}

	FVector PawnLocation = Pawn->GetActorLocation();
	FNavLocation NavLocation;

	// Try a few times to find a valid point
	for (int32 Attempt = 0; Attempt < 5; ++Attempt)
	{
		if (NavSystem->GetRandomReachablePointInRadius(PawnLocation, ForageRadius, NavLocation))
		{
			// Make sure it's far enough away
			float Distance = FVector::Dist(PawnLocation, NavLocation.Location);
			if (Distance >= MinMoveDistance)
			{
				OutLocation = NavLocation.Location;
				return true;
			}
		}
	}

	// Fallback - try getting any random point
	if (NavSystem->GetRandomPointInNavigableRadius(PawnLocation, ForageRadius, NavLocation))
	{
		OutLocation = NavLocation.Location;
		return true;
	}

	return false;
}

UMOSurvivorJobQueueComponent* UBTTask_SurvivorForage::GetJobQueueComponent(APawn* Pawn) const
{
	if (!Pawn)
	{
		return nullptr;
	}

	return Pawn->FindComponentByClass<UMOSurvivorJobQueueComponent>();
}

void UBTTask_SurvivorForage::PerformForageAction(APawn* Pawn, EMOSurvivorJobType JobType, const FVector& Location)
{
	if (!Pawn)
	{
		return;
	}

	UWorld* World = Pawn->GetWorld();
	if (!World)
	{
		return;
	}

	UMOForagingSubsystem* ForagingSubsystem = World->GetSubsystem<UMOForagingSubsystem>();
	if (!ForagingSubsystem)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[BTTask_SurvivorForage] No ForagingSubsystem found"));
		return;
	}

	// Get foraging skill level
	int32 ForagingLevel = ForagingSubsystem->GetForagingLevel(Pawn);

	// Perform appropriate action based on job type
	switch (JobType)
	{
	case EMOSurvivorJobType::DigForSupplies:
		{
			// Dig at the current location
			TArray<AMOWorldItem*> DugItems = ForagingSubsystem->DigForSupplies(Location, ForagingLevel, Pawn);
			UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorForage] Dug up %d items at %s"),
				DugItems.Num(), *Location.ToString());
			break;
		}

	case EMOSurvivorJobType::ForageNearby:
		{
			// Reveal HISM instances (ground items like sticks, rocks, etc.)
			float SearchRadius = ForagingSubsystem->CalculateSearchRadius(ForagingLevel);
			TArray<AMOWorldItem*> RevealedItems = ForagingSubsystem->RevealHISMInstancesInRadius(
				Location, SearchRadius, Pawn);
			UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorForage] Revealed %d items in radius %.0f at %s"),
				RevealedItems.Num(), SearchRadius, *Location.ToString());
			break;
		}

	default:
		UE_LOG(LogMOFramework, Warning, TEXT("[BTTask_SurvivorForage] Unhandled job type: %d"),
			static_cast<int32>(JobType));
		break;
	}
}
