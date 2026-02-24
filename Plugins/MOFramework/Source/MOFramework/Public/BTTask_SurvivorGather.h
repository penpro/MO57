#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "MOSurvivorJobTypes.h"
#include "BTTask_SurvivorGather.generated.h"

class UMOSurvivorJobQueueComponent;
class UInstancedStaticMeshComponent;

/**
 * Memory structure for the gather task.
 */
USTRUCT()
struct FBTTask_SurvivorGatherMemory
{
	GENERATED_BODY()

	/** Current state: 0 = searching, 1 = moving, 2 = gathering */
	uint8 State = 0;

	/** Target resource location. */
	FVector TargetLocation = FVector::ZeroVector;

	/** Time spent gathering at current location. */
	float GatherTime = 0.0f;

	/** Number of gathers completed this task execution. */
	int32 GathersCompleted = 0;

	/** ISM/HISM component containing the current target instance. */
	TWeakObjectPtr<UInstancedStaticMeshComponent> TargetHISMComponent;

	/** Instance index within the ISM/HISM to harvest. */
	int32 TargetInstanceIndex = INDEX_NONE;

	/** Item ID being gathered (for logging/debugging). */
	FName TargetItemId = NAME_None;
};

/**
 * BT Task that makes a survivor gather resources.
 *
 * This task:
 * 1. Finds the nearest harvestable HISM instance matching the job type
 * 2. Moves to the resource
 * 3. Performs the gather action
 * 4. Harvests the HISM instance and awards XP
 * 5. Repeats for RepeatCount in the job
 *
 * The task handles gathering wood, stone, fiber, etc.
 * Resource detection uses MOForagingSubsystem to find HISM instances,
 * then filters by item definition tags (Wood, Stone, Fiber).
 */
UCLASS()
class MOFRAMEWORK_API UBTTask_SurvivorGather : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SurvivorGather();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTTask_SurvivorGatherMemory); }

protected:
	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Search radius for finding resources. */
	UPROPERTY(EditAnywhere, Category = "Gather", meta = (ClampMin = "100.0"))
	float SearchRadius = 500.0f;

	/** Time to complete a single gather action (seconds). */
	UPROPERTY(EditAnywhere, Category = "Gather", meta = (ClampMin = "0.5"))
	float GatherDuration = 3.0f;

	/** Distance at which to start gathering. */
	UPROPERTY(EditAnywhere, Category = "Gather", meta = (ClampMin = "50.0"))
	float GatherDistance = 150.0f;

	/** Blackboard key containing the current job type. */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector JobTypeKey;

private:
	/**
	 * Find the nearest ISM/HISM instance matching the job type.
	 * Uses ForagingSubsystem to query instances, then filters by item tags.
	 * @param Pawn - The pawn doing the search
	 * @param JobType - Type of resource to find
	 * @param OutISMComponent - The ISM/HISM component containing the instance
	 * @param OutInstanceIndex - The instance index to harvest
	 * @param OutLocation - Location of the found resource
	 * @return true if a resource was found
	 */
	bool FindNearestHISMResource(APawn* Pawn, EMOSurvivorJobType JobType,
		UInstancedStaticMeshComponent*& OutISMComponent,
		int32& OutInstanceIndex, FVector& OutLocation);

	/**
	 * Get item tags that match the job type.
	 * @param JobType - The gather job type
	 * @return Array of item tags to match (e.g., "Wood", "Stick" for GatherWood)
	 */
	TArray<FName> GetItemTagsForJobType(EMOSurvivorJobType JobType) const;

	/**
	 * Check if an item definition matches the job type.
	 * @param ItemId - The item definition ID to check
	 * @param JobType - The gather job type
	 * @return true if item should be gathered for this job type
	 */
	bool DoesItemMatchJobType(FName ItemId, EMOSurvivorJobType JobType) const;

	/**
	 * Perform the actual harvest of the current ISM/HISM instance.
	 * @param Pawn - The pawn doing the harvesting
	 * @param ISMComponent - The ISM/HISM component
	 * @param InstanceIndex - The instance to harvest
	 * @return true if harvest was successful
	 */
	bool HarvestCurrentResource(APawn* Pawn, UInstancedStaticMeshComponent* ISMComponent, int32 InstanceIndex);

	/**
	 * Get the job queue component from the pawn.
	 */
	UMOSurvivorJobQueueComponent* GetJobQueueComponent(APawn* Pawn) const;
};
