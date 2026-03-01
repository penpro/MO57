#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_SurvivorGoHome.generated.h"

class UMOSurvivorJobQueueComponent;

/**
 * Memory structure for the go home task.
 */
USTRUCT()
struct FBTTask_SurvivorGoHomeMemory
{
	GENERATED_BODY()

	/** Target home location. */
	FVector TargetLocation = FVector::ZeroVector;

	/** Whether we've started moving. */
	bool bStartedMoving = false;

	// Stuck detection
	FVector LastPosition = FVector::ZeroVector;
	float StuckTime = 0.0f;
	float LastStuckCheckTime = 0.0f;
};

/**
 * BT Task that makes a survivor go to their assigned home location.
 *
 * This task:
 * 1. Gets the home location from JobQueueComponent
 * 2. Navigates to that location
 * 3. Succeeds when arrived, fails if no home assigned
 *
 * Add this task after job tasks to make survivor return home when done.
 */
UCLASS()
class MOFRAMEWORK_API UBTTask_SurvivorGoHome : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SurvivorGoHome();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTTask_SurvivorGoHomeMemory); }
	virtual FString GetStaticDescription() const override;

protected:
	/** How close to home before we consider arrival (cm). */
	UPROPERTY(EditAnywhere, Category = "GoHome", meta = (ClampMin = "50.0"))
	float AcceptanceRadius = 150.0f;

	/** How long without progress before considered stuck (seconds). */
	UPROPERTY(EditAnywhere, Category = "GoHome|StuckRecovery")
	float StuckThresholdTime = 3.0f;

	/** Minimum distance to move per stuck check to not be considered stuck (cm). */
	UPROPERTY(EditAnywhere, Category = "GoHome|StuckRecovery")
	float StuckMinMovement = 30.0f;

	/** How often to check for stuck condition (seconds). */
	UPROPERTY(EditAnywhere, Category = "GoHome|StuckRecovery")
	float StuckCheckInterval = 0.5f;

private:
	/**
	 * Get the job queue component from the pawn.
	 */
	UMOSurvivorJobQueueComponent* GetJobQueueComponent(APawn* Pawn) const;
};
