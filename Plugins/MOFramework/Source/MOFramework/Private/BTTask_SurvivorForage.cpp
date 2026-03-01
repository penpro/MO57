#include "BTTask_SurvivorForage.h"
#include "MOFramework.h"
#include "MOSurvivorJobQueueComponent.h"
#include "MOSurvivorController.h"
#include "MOForagingSubsystem.h"
#include "MOInventoryComponent.h"
#include "MOHISMHarvestHelper.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/Pawn.h"

UBTTask_SurvivorForage::UBTTask_SurvivorForage()
{
	NodeName = TEXT("Survivor Forage Ground Items");
	bNotifyTick = true;
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_SurvivorForage::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBTTask_SurvivorForageMemory* Memory = reinterpret_cast<FBTTask_SurvivorForageMemory*>(NodeMemory);
	Memory->State = 0; // Start by finding an item
	Memory->PickupTime = 0.0f;
	Memory->ItemsPickedUp = 0;
	Memory->StuckTime = 0.0f;

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

	Memory->LastPosition = Pawn->GetActorLocation();

	// Find the nearest item to pick up
	FMOHISMInstanceData ItemData;
	if (!FindNearestItem(Pawn, ItemData))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorForage] No ground items found within %.0f units"), SearchRadius);
		return EBTNodeResult::Failed;
	}

	Memory->TargetItem = ItemData;
	Memory->TargetLocation = ItemData.InstanceTransform.GetLocation();
	Memory->State = 1; // Moving to item

	// Start moving to the item
	AIController->MoveToLocation(Memory->TargetLocation, PickupRadius * 0.5f);

	UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorForage] Moving to %s at %s (%.0f units away)"),
		*ItemData.ItemId.ToString(), *Memory->TargetLocation.ToString(), ItemData.Distance);

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

	FVector CurrentLocation = Pawn->GetActorLocation();

	switch (Memory->State)
	{
	case 1: // Moving to item
		{
			float DistanceToTarget = FVector::Dist(CurrentLocation, Memory->TargetLocation);

			// Check if we've arrived
			if (DistanceToTarget <= PickupRadius)
			{
				// Stop moving and start picking up
				AIController->StopMovement();
				Memory->State = 2;
				Memory->PickupTime = 0.0f;
				UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorForage] Arrived at item, picking up..."));
			}
			else
			{
				// Check stuck detection
				float DistanceMoved = FVector::Dist(CurrentLocation, Memory->LastPosition);
				if (DistanceMoved < StuckMinMovement)
				{
					Memory->StuckTime += DeltaSeconds;
					if (Memory->StuckTime >= StuckThresholdTime)
					{
						// Stuck - try finding a different item
						UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorForage] Stuck, finding different item"));

						FMOHISMInstanceData NewItemData;
						if (FindNearestItem(Pawn, NewItemData))
						{
							Memory->TargetItem = NewItemData;
							Memory->TargetLocation = NewItemData.InstanceTransform.GetLocation();
							Memory->StuckTime = 0.0f;
							AIController->MoveToLocation(Memory->TargetLocation, PickupRadius * 0.5f);
						}
						else
						{
							// No more items - complete with what we have
							JobQueue->CompleteCurrentJob(Memory->ItemsPickedUp > 0);
							FinishLatentTask(OwnerComp, Memory->ItemsPickedUp > 0 ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
							return;
						}
					}
				}
				else
				{
					Memory->StuckTime = 0.0f;
					Memory->LastPosition = CurrentLocation;
				}

				// Check if movement failed
				EPathFollowingStatus::Type MoveStatus = AIController->GetMoveStatus();
				if (MoveStatus == EPathFollowingStatus::Idle)
				{
					// Movement finished but we're not close enough
					// Could be path was blocked - try finding another item
					FMOHISMInstanceData NewItemData;
					if (FindNearestItem(Pawn, NewItemData))
					{
						Memory->TargetItem = NewItemData;
						Memory->TargetLocation = NewItemData.InstanceTransform.GetLocation();
						AIController->MoveToLocation(Memory->TargetLocation, PickupRadius * 0.5f);
					}
					else
					{
						// No more items
						JobQueue->CompleteCurrentJob(Memory->ItemsPickedUp > 0);
						FinishLatentTask(OwnerComp, Memory->ItemsPickedUp > 0 ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
					}
				}
			}
			break;
		}

	case 2: // Picking up item
		{
			Memory->PickupTime += DeltaSeconds;

			// Update job progress
			float Progress = FMath::Clamp(
				(Memory->ItemsPickedUp + (Memory->PickupTime / PickupDuration)) / CurrentJob.RepeatCount,
				0.0f, 1.0f);
			JobQueue->UpdateJobProgress(CurrentJob.JobId, Progress);

			if (Memory->PickupTime >= PickupDuration)
			{
				// Pickup complete
				if (PickupItem(Pawn, Memory->TargetItem))
				{
					Memory->ItemsPickedUp++;
					UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorForage] Picked up %s (%d/%d)"),
						*Memory->TargetItem.ItemId.ToString(), Memory->ItemsPickedUp, CurrentJob.RepeatCount);

					// Award XP through the controller
					if (AMOSurvivorController* SurvivorController = Cast<AMOSurvivorController>(AIController))
					{
						SurvivorController->AwardJobExperience(CurrentJob.JobType);
					}
				}

				// Check if we need more items
				if (Memory->ItemsPickedUp >= CurrentJob.RepeatCount)
				{
					// All items picked up - job complete
					JobQueue->CompleteCurrentJob(true);
					FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
					return;
				}

				// Find next item
				FMOHISMInstanceData NextItemData;
				if (FindNearestItem(Pawn, NextItemData))
				{
					Memory->TargetItem = NextItemData;
					Memory->TargetLocation = NextItemData.InstanceTransform.GetLocation();
					Memory->State = 1;
					Memory->PickupTime = 0.0f;
					Memory->StuckTime = 0.0f;
					Memory->LastPosition = Pawn->GetActorLocation();
					AIController->MoveToLocation(Memory->TargetLocation, PickupRadius * 0.5f);
				}
				else
				{
					// No more items - complete with what we have
					JobQueue->CompleteCurrentJob(Memory->ItemsPickedUp > 0);
					FinishLatentTask(OwnerComp, Memory->ItemsPickedUp > 0 ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
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

FString UBTTask_SurvivorForage::GetStaticDescription() const
{
	return FString::Printf(TEXT("Forage ground items within %.0f units"), SearchRadius);
}

bool UBTTask_SurvivorForage::FindNearestItem(APawn* Pawn, FMOHISMInstanceData& OutItemData)
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

	UMOForagingSubsystem* ForagingSubsystem = World->GetSubsystem<UMOForagingSubsystem>();
	if (!ForagingSubsystem)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[BTTask_SurvivorForage] No ForagingSubsystem found"));
		return false;
	}

	FVector PawnLocation = Pawn->GetActorLocation();

	// Query for nearby HISM instances (returns sorted by distance)
	TArray<FMOHISMInstanceData> Items = ForagingSubsystem->QueryHISMInstancesInRadius(PawnLocation, SearchRadius);

	if (Items.Num() == 0)
	{
		return false;
	}

	// Return the closest valid item
	for (const FMOHISMInstanceData& Item : Items)
	{
		if (Item.IsValid())
		{
			OutItemData = Item;
			return true;
		}
	}

	return false;
}

bool UBTTask_SurvivorForage::PickupItem(APawn* Pawn, const FMOHISMInstanceData& ItemData)
{
	if (!Pawn || !ItemData.IsValid())
	{
		return false;
	}

	UMOInventoryComponent* Inventory = Pawn->FindComponentByClass<UMOInventoryComponent>();
	if (!Inventory)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[BTTask_SurvivorForage] Pawn has no inventory"));
		return false;
	}

	UInstancedStaticMeshComponent* ISM = ItemData.HISMComponent.Get();
	if (!ISM)
	{
		return false;
	}

	// Add item to inventory
	FGuid NewItemGuid = FGuid::NewGuid();
	if (!Inventory->AddItemByGuid(NewItemGuid, ItemData.ItemId, 1))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[BTTask_SurvivorForage] Failed to add %s to inventory"), *ItemData.ItemId.ToString());
		return false;
	}

	// Remove the ISM instance
	if (!FMOHISMHarvestHelper::RemoveInstance(ISM, ItemData.InstanceIndex, true))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[BTTask_SurvivorForage] Failed to remove ISM instance"));
		// Item was added to inventory, so still return success
	}

	return true;
}

UMOSurvivorJobQueueComponent* UBTTask_SurvivorForage::GetJobQueueComponent(APawn* Pawn) const
{
	if (!Pawn)
	{
		return nullptr;
	}

	return Pawn->FindComponentByClass<UMOSurvivorJobQueueComponent>();
}
