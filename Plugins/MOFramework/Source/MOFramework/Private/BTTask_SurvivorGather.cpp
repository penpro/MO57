#include "BTTask_SurvivorGather.h"
#include "MOFramework.h"
#include "MOSurvivorJobQueueComponent.h"
#include "MOSurvivorController.h"
#include "MOSkillsComponent.h"
#include "MOForagingSubsystem.h"
#include "MOPCGInteractionSubsystem.h"
#include "MOItemDatabaseSettings.h"
#include "MOSurvivorJobDatabaseSettings.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "NavigationSystem.h"
#include "Components/InstancedStaticMeshComponent.h"

UBTTask_SurvivorGather::UBTTask_SurvivorGather()
{
	NodeName = TEXT("Survivor Gather Resources");
	bNotifyTick = true;
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_SurvivorGather::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBTTask_SurvivorGatherMemory* Memory = reinterpret_cast<FBTTask_SurvivorGatherMemory*>(NodeMemory);
	Memory->State = 0; // Start in searching state
	Memory->GatherTime = 0.0f;
	Memory->GathersCompleted = 0;

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
		UE_LOG(LogMOFramework, Warning, TEXT("[BTTask_SurvivorGather] No job queue component on pawn"));
		return EBTNodeResult::Failed;
	}

	// Get current job type from blackboard or job queue
	EMOSurvivorJobType JobType = EMOSurvivorJobType::None;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (Blackboard && JobTypeKey.IsSet())
	{
		JobType = static_cast<EMOSurvivorJobType>(Blackboard->GetValueAsEnum(JobTypeKey.SelectedKeyName));
	}
	else
	{
		FMOSurvivorJobEntry CurrentJob = JobQueue->GetCurrentJob();
		JobType = CurrentJob.JobType;
	}

	if (JobType == EMOSurvivorJobType::None)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[BTTask_SurvivorGather] No job type specified"));
		return EBTNodeResult::Failed;
	}

	// Find nearest HISM resource
	UInstancedStaticMeshComponent* HISMComponent = nullptr;
	int32 InstanceIndex = INDEX_NONE;
	FVector ResourceLocation;

	if (!FindNearestHISMResource(Pawn, JobType, HISMComponent, InstanceIndex, ResourceLocation))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorGather] No HISM resources found nearby for job type %d"), static_cast<int32>(JobType));
		// Mark job as failed - no resources
		JobQueue->CompleteCurrentJob(false);
		return EBTNodeResult::Failed;
	}

	Memory->TargetLocation = ResourceLocation;
	Memory->TargetHISMComponent = HISMComponent;
	Memory->TargetInstanceIndex = InstanceIndex;
	Memory->State = 1; // Moving to resource

	// Start moving to resource
	AIController->MoveToLocation(ResourceLocation, GatherDistance);

	UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorGather] Moving to HISM resource at %s (instance %d)"),
		*ResourceLocation.ToString(), InstanceIndex);

	return EBTNodeResult::InProgress;
}

void UBTTask_SurvivorGather::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FBTTask_SurvivorGatherMemory* Memory = reinterpret_cast<FBTTask_SurvivorGatherMemory*>(NodeMemory);

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
	case 1: // Moving to resource
		{
			// Check if we've arrived
			float DistanceToTarget = FVector::Dist(Pawn->GetActorLocation(), Memory->TargetLocation);
			if (DistanceToTarget <= GatherDistance)
			{
				// Stop moving and start gathering
				AIController->StopMovement();
				Memory->State = 2;
				Memory->GatherTime = 0.0f;
				UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorGather] Arrived at resource, starting gather"));
			}
			else
			{
				// Check if movement failed (not actively moving)
				EPathFollowingStatus::Type MoveStatus = AIController->GetMoveStatus();
				if (MoveStatus != EPathFollowingStatus::Moving)
				{
					// Movement finished but we're not close enough - path blocked?
					UE_LOG(LogMOFramework, Warning, TEXT("[BTTask_SurvivorGather] Movement stopped but not at destination"));

					// Try to find another resource
					UInstancedStaticMeshComponent* NewHISMComponent = nullptr;
					int32 NewInstanceIndex = INDEX_NONE;
					FVector NewLocation;

					if (FindNearestHISMResource(Pawn, CurrentJob.JobType, NewHISMComponent, NewInstanceIndex, NewLocation) &&
					    NewLocation != Memory->TargetLocation)
					{
						Memory->TargetLocation = NewLocation;
						Memory->TargetHISMComponent = NewHISMComponent;
						Memory->TargetInstanceIndex = NewInstanceIndex;
						AIController->MoveToLocation(NewLocation, GatherDistance);
					}
					else
					{
						JobQueue->CompleteCurrentJob(false);
						FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
					}
				}
			}
			break;
		}

	case 2: // Gathering
		{
			Memory->GatherTime += DeltaSeconds;

			// Update job progress
			float Progress = FMath::Clamp(Memory->GatherTime / GatherDuration, 0.0f, 1.0f);
			JobQueue->UpdateJobProgress(CurrentJob.JobId, Progress);

			if (Memory->GatherTime >= GatherDuration)
			{
				// Gather complete - harvest the HISM instance
				UInstancedStaticMeshComponent* HISMComp = Memory->TargetHISMComponent.Get();
				if (HISMComp && Memory->TargetInstanceIndex != INDEX_NONE)
				{
					HarvestCurrentResource(Pawn, HISMComp, Memory->TargetInstanceIndex);
				}

				Memory->GathersCompleted++;

				UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorGather] Gather complete (%d/%d)"),
					Memory->GathersCompleted, CurrentJob.RepeatCount);

				// Award XP through the controller
				if (AMOSurvivorController* SurvivorController = Cast<AMOSurvivorController>(AIController))
				{
					SurvivorController->AwardJobExperience(CurrentJob.JobType);
				}

				// Check if we need to repeat
				if (Memory->GathersCompleted >= CurrentJob.RepeatCount)
				{
					// All repeats done - job complete
					JobQueue->CompleteCurrentJob(true);
					FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
					return;
				}

				// Need more gathers - find next HISM resource
				UInstancedStaticMeshComponent* NewHISMComponent = nullptr;
				int32 NewInstanceIndex = INDEX_NONE;
				FVector NewLocation;

				if (FindNearestHISMResource(Pawn, CurrentJob.JobType, NewHISMComponent, NewInstanceIndex, NewLocation))
				{
					Memory->TargetLocation = NewLocation;
					Memory->TargetHISMComponent = NewHISMComponent;
					Memory->TargetInstanceIndex = NewInstanceIndex;
					Memory->State = 1;
					Memory->GatherTime = 0.0f;
					AIController->MoveToLocation(NewLocation, GatherDistance);
				}
				else
				{
					// No more resources - partial completion
					UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorGather] No more HISM resources, partial completion"));
					JobQueue->CompleteCurrentJob(true); // Still successful, just fewer items
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

EBTNodeResult::Type UBTTask_SurvivorGather::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController)
	{
		AIController->StopMovement();
	}

	return EBTNodeResult::Aborted;
}

bool UBTTask_SurvivorGather::FindNearestHISMResource(APawn* Pawn, EMOSurvivorJobType JobType,
	UInstancedStaticMeshComponent*& OutHISMComponent,
	int32& OutInstanceIndex, FVector& OutLocation)
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

	// Get the foraging subsystem to query HISM instances
	UMOForagingSubsystem* ForagingSubsystem = World->GetSubsystem<UMOForagingSubsystem>();
	if (!ForagingSubsystem)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[BTTask_SurvivorGather] No ForagingSubsystem found"));
		return false;
	}

	FVector PawnLocation = Pawn->GetActorLocation();

	// Query all harvestable HISM instances in radius
	TArray<FMOHISMInstanceData> Instances = ForagingSubsystem->QueryHISMInstancesInRadius(PawnLocation, SearchRadius);

	if (Instances.Num() == 0)
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[BTTask_SurvivorGather] No HISM instances found in radius %.0f"), SearchRadius);
		return false;
	}

	// Filter instances by job type (check item tags)
	for (const FMOHISMInstanceData& Instance : Instances)
	{
		if (!Instance.IsValid())
		{
			continue;
		}

		// Check if this item matches the job type
		if (DoesItemMatchJobType(Instance.ItemId, JobType))
		{
			OutHISMComponent = Instance.HISMComponent.Get();
			OutInstanceIndex = Instance.InstanceIndex;
			OutLocation = Instance.InstanceTransform.GetLocation();

			UE_LOG(LogMOFramework, Verbose, TEXT("[BTTask_SurvivorGather] Found matching resource: %s at %s"),
				*Instance.ItemId.ToString(), *OutLocation.ToString());
			return true;
		}
	}

	UE_LOG(LogMOFramework, Verbose, TEXT("[BTTask_SurvivorGather] No matching resources for job type %d in %d instances"),
		static_cast<int32>(JobType), Instances.Num());
	return false;
}

TArray<FName> UBTTask_SurvivorGather::GetItemTagsForJobType(EMOSurvivorJobType JobType) const
{
	// First try to get tags from job definition DataTable
	const FMOSurvivorJobDefinitionRow* JobDef = UMOSurvivorJobDatabaseSettings::GetJobDefinition(JobType);
	if (JobDef && JobDef->RequiredResourceTags.Num() > 0)
	{
		return JobDef->RequiredResourceTags;
	}

	// Fallback to hardcoded tags if DataTable not configured
	TArray<FName> MatchTags;

	switch (JobType)
	{
	case EMOSurvivorJobType::GatherWood:
		MatchTags.Add(FName(TEXT("Wood")));
		MatchTags.Add(FName(TEXT("Stick")));
		MatchTags.Add(FName(TEXT("Branch")));
		break;

	case EMOSurvivorJobType::GatherStone:
		MatchTags.Add(FName(TEXT("Stone")));
		MatchTags.Add(FName(TEXT("Rock")));
		break;

	case EMOSurvivorJobType::GatherFiber:
		MatchTags.Add(FName(TEXT("Fiber")));
		MatchTags.Add(FName(TEXT("Plant")));
		break;

	default:
		break;
	}

	return MatchTags;
}

bool UBTTask_SurvivorGather::DoesItemMatchJobType(FName ItemId, EMOSurvivorJobType JobType) const
{
	if (ItemId.IsNone())
	{
		return false;
	}

	// Get the required tags for this job type (from DataTable or fallback)
	TArray<FName> RequiredTags = GetItemTagsForJobType(JobType);
	if (RequiredTags.Num() == 0)
	{
		// No requirements = matches anything
		return true;
	}

	// First try to get item definition to check item tags
	FMOItemDefinitionRow ItemDef;
	if (UMOItemDatabaseSettings::GetItemDefinition(ItemId, ItemDef))
	{
		for (const FName& ItemTag : ItemDef.Tags)
		{
			if (RequiredTags.Contains(ItemTag))
			{
				return true;
			}
		}
	}

	// Also check the ItemId itself as a fallback (e.g., "Stick" matches "Stick" tag)
	FString ItemIdString = ItemId.ToString();
	for (const FName& RequiredTag : RequiredTags)
	{
		if (ItemIdString.Contains(RequiredTag.ToString()))
		{
			return true;
		}
	}

	return false;
}

bool UBTTask_SurvivorGather::HarvestCurrentResource(APawn* Pawn, UInstancedStaticMeshComponent* ISMComponent, int32 InstanceIndex)
{
	if (!Pawn || !ISMComponent || InstanceIndex == INDEX_NONE)
	{
		return false;
	}

	UWorld* World = Pawn->GetWorld();
	if (!World)
	{
		return false;
	}

	// Use the PCG interaction subsystem to harvest the instance
	UMOPCGInteractionSubsystem* PCGSubsystem = World->GetSubsystem<UMOPCGInteractionSubsystem>();
	if (!PCGSubsystem)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[BTTask_SurvivorGather] No PCGInteractionSubsystem found"));
		return false;
	}

	FName HarvestedItemId;
	bool bSuccess = PCGSubsystem->HarvestISMInstance(ISMComponent, InstanceIndex, Pawn, HarvestedItemId);

	if (bSuccess)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[BTTask_SurvivorGather] Harvested %s from ISM instance %d"),
			*HarvestedItemId.ToString(), InstanceIndex);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[BTTask_SurvivorGather] Failed to harvest ISM instance %d"), InstanceIndex);
	}

	return bSuccess;
}

UMOSurvivorJobQueueComponent* UBTTask_SurvivorGather::GetJobQueueComponent(APawn* Pawn) const
{
	if (!Pawn)
	{
		return nullptr;
	}

	return Pawn->FindComponentByClass<UMOSurvivorJobQueueComponent>();
}
