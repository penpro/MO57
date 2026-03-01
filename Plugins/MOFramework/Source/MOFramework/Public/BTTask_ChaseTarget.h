#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ChaseTarget.generated.h"

/** Memory struct for chase task. */
struct FBTChaseMemory
{
	float OriginalMaxWalkSpeed;
	bool bAppliedSpeedBoost;

	// Stuck detection
	FVector LastPosition;
	float StuckTime;
	bool bIsRecovering;
	float RecoveryTime;
	FVector RecoveryDirection;
};

/**
 * BT Task: Chase the current target actor.
 * Moves toward the target with optional speed boost for predators.
 */
UCLASS()
class MOFRAMEWORK_API UBTTask_ChaseTarget : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ChaseTarget();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTChaseMemory); }
	virtual FString GetStaticDescription() const override;

protected:
	/** Blackboard key for the target actor to chase. */
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector TargetActorKey;

	/** How close to get before completing the chase (should be smaller than attack range). */
	UPROPERTY(EditAnywhere, Category="Chase")
	float AcceptanceRadius = 100.f;

	/** Speed multiplier while chasing (1.0 = normal, 1.5 = 50% faster). */
	UPROPERTY(EditAnywhere, Category="Chase")
	float ChaseSpeedMultiplier = 1.3f;

	/** Maximum time to chase before giving up (seconds). 0 = no limit. */
	UPROPERTY(EditAnywhere, Category="Chase")
	float MaxChaseTime = 30.f;

	/** Distance at which to give up the chase. 0 = no limit. */
	UPROPERTY(EditAnywhere, Category="Chase")
	float MaxChaseDistance = 5000.f;

	/** How long without progress before considered stuck (seconds). */
	UPROPERTY(EditAnywhere, Category="Chase|StuckRecovery")
	float StuckThresholdTime = 1.5f;

	/** Minimum distance to move per stuck check interval to not be considered stuck (cm). */
	UPROPERTY(EditAnywhere, Category="Chase|StuckRecovery")
	float StuckMinMovement = 50.f;

	/** How long to back up when stuck (seconds). */
	UPROPERTY(EditAnywhere, Category="Chase|StuckRecovery")
	float RecoveryDuration = 0.8f;

private:
	/** Restore original movement speed. */
	void RestoreMovementSpeed(APawn* Pawn, FBTChaseMemory* Memory) const;

	/** Check if stuck and handle recovery. Returns true if currently in recovery mode. */
	bool HandleStuckRecovery(APawn* Pawn, AActor* Target, FBTChaseMemory* Memory, float DeltaSeconds);
};
