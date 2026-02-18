#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_CreatureAttack.generated.h"

/**
 * BT Task: Execute a creature attack on the current target.
 * Uses the creature's combat component to perform the attack.
 */
UCLASS()
class MOFRAMEWORK_API UBTTask_CreatureAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_CreatureAttack();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	/** Blackboard key for the target actor. */
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector TargetActorKey;

	/** Damage to apply (if no combat component). */
	UPROPERTY(EditAnywhere, Category="Attack")
	float BaseDamage = 10.f;

	/** Attack range check before executing. */
	UPROPERTY(EditAnywhere, Category="Attack")
	float AttackRange = 150.f;
};
