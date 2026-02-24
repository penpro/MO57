#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "MOSurvivorJobTypes.h"
#include "BTService_SurvivorJobProcessor.generated.h"

class UMOSurvivorJobQueueComponent;

/**
 * BT Service that processes the survivor's job queue.
 *
 * Runs at regular intervals on the behavior tree root to:
 * - Check for pending jobs in the queue
 * - Update blackboard keys with current job info
 * - Transition job states as needed
 *
 * Blackboard keys updated:
 * - HasPendingJob (bool)
 * - CurrentJobType (enum)
 * - JobTargetLocation (vector)
 * - JobTargetActor (object)
 * - JobProgress (float)
 */
UCLASS()
class MOFRAMEWORK_API UBTService_SurvivorJobProcessor : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_SurvivorJobProcessor();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Blackboard key for whether there's a pending job. */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector HasPendingJobKey;

	/** Blackboard key for the current job type. */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector CurrentJobTypeKey;

	/** Blackboard key for job target location. */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector JobTargetLocationKey;

	/** Blackboard key for job target actor. */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector JobTargetActorKey;

	/** Blackboard key for job progress (0-1). */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector JobProgressKey;

private:
	/**
	 * Get the job queue component from the owning pawn.
	 */
	UMOSurvivorJobQueueComponent* GetJobQueueComponent(UBehaviorTreeComponent& OwnerComp) const;

	/**
	 * Update blackboard keys based on the current job state.
	 */
	void UpdateBlackboardFromJobQueue(UBehaviorTreeComponent& OwnerComp, UMOSurvivorJobQueueComponent* JobQueue);
};
