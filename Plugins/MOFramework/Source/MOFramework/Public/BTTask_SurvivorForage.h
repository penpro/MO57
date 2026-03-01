#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "MOSurvivorJobTypes.h"
#include "MOForagingSubsystem.h"
#include "BTTask_SurvivorForage.generated.h"

class UMOSurvivorJobQueueComponent;

/**
 * Memory structure for the forage task.
 */
USTRUCT()
struct FBTTask_SurvivorForageMemory
{
	GENERATED_BODY()

	/** Current state: 0 = finding item, 1 = moving to item, 2 = picking up item */
	uint8 State = 0;

	/** Target item location. */
	FVector TargetLocation = FVector::ZeroVector;

	/** Target item data. */
	FMOHISMInstanceData TargetItem;

	/** Time spent picking up current item. */
	float PickupTime = 0.0f;

	/** Number of items picked up this task execution. */
	int32 ItemsPickedUp = 0;

	// Stuck detection
	FVector LastPosition = FVector::ZeroVector;
	float StuckTime = 0.0f;
};

/**
 * BT Task that makes a survivor forage ground items.
 *
 * This task:
 * 1. Queries nearby HISM instances (ground items like sticks, stones)
 * 2. Picks the closest valid item
 * 3. Navigates to that item's location
 * 4. Picks up the item and adds it to inventory
 * 5. Repeats for the job's RepeatCount
 *
 * Unlike the wander-based approach, this actually navigates to items.
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
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTTask_SurvivorForageMemory); }
	virtual FString GetStaticDescription() const override;

protected:
	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Radius to search for ground items (how far survivor looks for items). */
	UPROPERTY(EditAnywhere, Category = "Forage", meta = (ClampMin = "100.0"))
	float SearchRadius = 3000.0f;

	/** Time to pick up a single item (seconds). */
	UPROPERTY(EditAnywhere, Category = "Forage", meta = (ClampMin = "0.1"))
	float PickupDuration = 1.5f;

	/** How close to item before we can pick it up. */
	UPROPERTY(EditAnywhere, Category = "Forage", meta = (ClampMin = "50.0"))
	float PickupRadius = 150.0f;

	/** How long without progress before considered stuck (seconds). */
	UPROPERTY(EditAnywhere, Category = "Forage|StuckRecovery")
	float StuckThresholdTime = 3.0f;

	/** Minimum distance to move to not be considered stuck (cm). */
	UPROPERTY(EditAnywhere, Category = "Forage|StuckRecovery")
	float StuckMinMovement = 50.0f;

private:
	/**
	 * Find the nearest ground item to pick up.
	 */
	bool FindNearestItem(APawn* Pawn, FMOHISMInstanceData& OutItemData);

	/**
	 * Pick up the target item and add to inventory.
	 */
	bool PickupItem(APawn* Pawn, const FMOHISMInstanceData& ItemData);

	/**
	 * Get the job queue component from the pawn.
	 */
	UMOSurvivorJobQueueComponent* GetJobQueueComponent(APawn* Pawn) const;
};
