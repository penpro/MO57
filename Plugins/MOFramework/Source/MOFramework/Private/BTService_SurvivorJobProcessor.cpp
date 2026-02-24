#include "BTService_SurvivorJobProcessor.h"
#include "MOFramework.h"
#include "MOSurvivorJobQueueComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"

UBTService_SurvivorJobProcessor::UBTService_SurvivorJobProcessor()
{
	NodeName = TEXT("Survivor Job Processor");

	// Run frequently to keep blackboard in sync
	Interval = 0.5f;
	RandomDeviation = 0.1f;

	// Always run
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;
}

void UBTService_SurvivorJobProcessor::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UMOSurvivorJobQueueComponent* JobQueue = GetJobQueueComponent(OwnerComp);
	if (!JobQueue)
	{
		// No job queue - clear blackboard
		UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
		if (Blackboard)
		{
			if (HasPendingJobKey.IsSet())
			{
				Blackboard->SetValueAsBool(HasPendingJobKey.SelectedKeyName, false);
			}
		}
		return;
	}

	UpdateBlackboardFromJobQueue(OwnerComp, JobQueue);
}

UMOSurvivorJobQueueComponent* UBTService_SurvivorJobProcessor::GetJobQueueComponent(UBehaviorTreeComponent& OwnerComp) const
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return nullptr;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		return nullptr;
	}

	return Pawn->FindComponentByClass<UMOSurvivorJobQueueComponent>();
}

void UBTService_SurvivorJobProcessor::UpdateBlackboardFromJobQueue(UBehaviorTreeComponent& OwnerComp, UMOSurvivorJobQueueComponent* JobQueue)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return;
	}

	FMOSurvivorJobEntry CurrentJob = JobQueue->GetCurrentJob();
	const bool bHasJob = CurrentJob.IsValid();

	// Update has pending job
	if (HasPendingJobKey.IsSet())
	{
		Blackboard->SetValueAsBool(HasPendingJobKey.SelectedKeyName, bHasJob);
	}

	if (!bHasJob)
	{
		// No active job - clear other keys
		if (CurrentJobTypeKey.IsSet())
		{
			Blackboard->SetValueAsEnum(CurrentJobTypeKey.SelectedKeyName, static_cast<uint8>(EMOSurvivorJobType::None));
		}
		if (JobProgressKey.IsSet())
		{
			Blackboard->SetValueAsFloat(JobProgressKey.SelectedKeyName, 0.0f);
		}
		return;
	}

	// Update current job type
	if (CurrentJobTypeKey.IsSet())
	{
		Blackboard->SetValueAsEnum(CurrentJobTypeKey.SelectedKeyName, static_cast<uint8>(CurrentJob.JobType));
	}

	// Update target location
	if (JobTargetLocationKey.IsSet())
	{
		Blackboard->SetValueAsVector(JobTargetLocationKey.SelectedKeyName, CurrentJob.TargetLocation);
	}

	// Update target actor
	if (JobTargetActorKey.IsSet())
	{
		AActor* TargetActor = CurrentJob.TargetActor.Get();
		Blackboard->SetValueAsObject(JobTargetActorKey.SelectedKeyName, TargetActor);
	}

	// Update progress
	if (JobProgressKey.IsSet())
	{
		Blackboard->SetValueAsFloat(JobProgressKey.SelectedKeyName, CurrentJob.Progress);
	}
}
