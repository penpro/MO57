#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_CreatureAttack.generated.h"

class UAnimMontage;

/** Memory struct for attack task. */
struct FBTAttackMemory
{
	float ElapsedTime;
	bool bDamageApplied;
	bool bMontageStarted;
};

/**
 * BT Task: Execute a creature attack on the current target.
 * Has a wind-up time before damage is applied, then a cooldown.
 */
UCLASS()
class MOFRAMEWORK_API UBTTask_CreatureAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_CreatureAttack();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTAttackMemory); }
	virtual FString GetStaticDescription() const override;

protected:
	/** Blackboard key for the target actor. */
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector TargetActorKey;

	/** Damage to apply per attack. */
	UPROPERTY(EditAnywhere, Category="Attack")
	float BaseDamage = 15.f;

	/** Attack range check before executing. */
	UPROPERTY(EditAnywhere, Category="Attack")
	float AttackRange = 200.f;

	/** Time before damage is applied (wind-up). */
	UPROPERTY(EditAnywhere, Category="Attack")
	float WindUpTime = 0.3f;

	/** Total attack duration (wind-up + recovery). */
	UPROPERTY(EditAnywhere, Category="Attack")
	float AttackDuration = 1.0f;

	/** Attack animation montage to play. If not set, will try to use MOCombatComponent's montage. */
	UPROPERTY(EditAnywhere, Category="Attack|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** Play rate for the attack montage. */
	UPROPERTY(EditAnywhere, Category="Attack|Animation")
	float MontagePlayRate = 1.0f;
};
