#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_UpdateCreatureState.generated.h"

/**
 * BT Service: Updates creature state on the blackboard.
 * Runs periodically to keep blackboard keys synchronized with creature state.
 *
 * Updates:
 * - HealthPercent
 * - ShouldFlee
 * - HasTarget
 * - DistanceToTarget
 * - IsInAttackRange
 */
UCLASS()
class MOFRAMEWORK_API UBTService_UpdateCreatureState : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_UpdateCreatureState();

	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual FString GetStaticDescription() const override;

protected:
	/** Blackboard key for target actor. */
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector TargetActorKey;

	/** Blackboard key for health percentage. */
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector HealthPercentKey;

	/** Blackboard key for should flee flag. */
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector ShouldFleeKey;

	/** Blackboard key for has target flag. */
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector HasTargetKey;

	/** Blackboard key for distance to target. */
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector DistanceToTargetKey;

	/** Blackboard key for is in attack range flag. */
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector IsInAttackRangeKey;

	/** Attack range for range check. */
	UPROPERTY(EditAnywhere, Category="Combat")
	float AttackRange = 150.f;

	/** Health threshold for flee decision. */
	UPROPERTY(EditAnywhere, Category="Combat")
	float FleeHealthThreshold = 0.3f;
};
