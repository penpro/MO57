#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_CreatureWander.generated.h"

/** Memory struct for wander task. */
struct FBTWanderMemory
{
	FVector TargetLocation;
	bool bUseDirectMovement;
};

/**
 * BT Task: Wander to a random nearby location.
 * Used for idle behavior when no threats are present.
 */
UCLASS()
class MOFRAMEWORK_API UBTTask_CreatureWander : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_CreatureWander();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTWanderMemory); }
	virtual FString GetStaticDescription() const override;

protected:
	/** Blackboard key for home/spawn location (optional, to stay near home). */
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector HomeLocationKey;

	/** Minimum wander distance. */
	UPROPERTY(EditAnywhere, Category="Wander")
	float MinWanderDistance = 200.f;

	/** Maximum wander distance. */
	UPROPERTY(EditAnywhere, Category="Wander")
	float MaxWanderDistance = 800.f;

	/** Maximum distance from home location (0 = unlimited). */
	UPROPERTY(EditAnywhere, Category="Wander")
	float MaxDistanceFromHome = 2000.f;

	/** Acceptance radius for reaching destination. */
	UPROPERTY(EditAnywhere, Category="Wander")
	float AcceptanceRadius = 50.f;

private:
	/** Find a random wander location. */
	bool FindWanderLocation(APawn* OwnerPawn, const FVector& HomeLocation, FVector& OutLocation) const;
};
