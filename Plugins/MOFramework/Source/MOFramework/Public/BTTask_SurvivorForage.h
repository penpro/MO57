#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "MOSurvivorJobTypes.h"
#include "BTTask_SurvivorForage.generated.h"

class UMOSurvivorJobQueueComponent;

/**
 * BT Task that makes a survivor forage the ground.
 *
 * This task:
 * 1. Picks a random point near the pawn
 * 2. Moves to that point
 * 3. Performs a forage action (digging animation)
 * 4. Awards XP and spawns items
 * 5. Repeats for the job's RepeatCount
 *
 * Unlike gathering, foraging doesn't require finding a specific resource.
 * Items spawned depend on biome/location.
 */
UCLASS()
class MOFRAMEWORK_API UBTTask_SurvivorForage : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SurvivorForage();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Radius to search for a forage point. */
	UPROPERTY(EditAnywhere, Category = "Forage", meta = (ClampMin = "100.0"))
	float ForageRadius = 300.0f;

	/** Time to complete a single forage action (seconds). */
	UPROPERTY(EditAnywhere, Category = "Forage", meta = (ClampMin = "0.5"))
	float ForageDuration = 4.0f;

	/** Minimum distance to move before foraging. */
	UPROPERTY(EditAnywhere, Category = "Forage", meta = (ClampMin = "50.0"))
	float MinMoveDistance = 100.0f;

private:
	/**
	 * Pick a random forage point near the pawn.
	 */
	bool PickForagePoint(APawn* Pawn, FVector& OutLocation);

	/**
	 * Get the job queue component from the pawn.
	 */
	UMOSurvivorJobQueueComponent* GetJobQueueComponent(APawn* Pawn) const;

	/**
	 * Perform the actual forage/dig action and spawn items.
	 */
	void PerformForageAction(APawn* Pawn, EMOSurvivorJobType JobType, const FVector& Location);
};

/**
 * Memory structure for the forage task.
 */
USTRUCT()
struct FBTTask_SurvivorForageMemory
{
	GENERATED_BODY()

	/** Current state: 0 = picking point, 1 = moving, 2 = foraging */
	uint8 State = 0;

	/** Target forage location. */
	FVector TargetLocation = FVector::ZeroVector;

	/** Time spent foraging at current location. */
	float ForageTime = 0.0f;

	/** Number of forages completed this task execution. */
	int32 ForagesCompleted = 0;
};
