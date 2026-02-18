#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_CreatureRest.generated.h"

/**
 * BT Task: Makes creature rest/sleep for a duration.
 * The animation is handled by the Animation Blueprint reading the ActivityState.
 */
UCLASS()
class MOFRAMEWORK_API UBTTask_CreatureRest : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_CreatureRest();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;
	virtual uint16 GetInstanceMemorySize() const override;

protected:
	/** Whether this is a sleep task (longer duration, deeper sleep). */
	UPROPERTY(EditAnywhere, Category="Rest")
	bool bIsSleeping = false;

	/** Minimum rest duration in seconds. */
	UPROPERTY(EditAnywhere, Category="Rest", meta=(EditCondition="!bIsSleeping"))
	float MinDuration = 10.f;

	/** Maximum rest duration in seconds. */
	UPROPERTY(EditAnywhere, Category="Rest", meta=(EditCondition="!bIsSleeping"))
	float MaxDuration = 30.f;
};

/** Memory struct for rest task. */
struct FBTRestMemory
{
	float ElapsedTime;
	float TargetDuration;
};
