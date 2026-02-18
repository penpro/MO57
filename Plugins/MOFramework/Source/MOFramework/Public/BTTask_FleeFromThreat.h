#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FleeFromThreat.generated.h"

/** Memory struct for flee task. */
struct FBTFleeMemory
{
	FVector TargetLocation;
	bool bUseDirectMovement;
	float OriginalWalkSpeed;
};

/**
 * BT Task: Flee from the current threat.
 * Finds a location away from the threat and moves there.
 */
UCLASS()
class MOFRAMEWORK_API UBTTask_FleeFromThreat : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FleeFromThreat();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTFleeMemory); }
	virtual FString GetStaticDescription() const override;

protected:
	/** Blackboard key for the threat actor to flee from. */
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector ThreatActorKey;

	/** Blackboard key to store the flee destination. */
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector FleeDestinationKey;

	/** Minimum distance to flee from threat. */
	UPROPERTY(EditAnywhere, Category="Flee")
	float MinFleeDistance = 1000.f;

	/** Maximum distance to flee. */
	UPROPERTY(EditAnywhere, Category="Flee")
	float MaxFleeDistance = 2000.f;

	/** Acceptance radius for reaching flee destination. */
	UPROPERTY(EditAnywhere, Category="Flee")
	float AcceptanceRadius = 100.f;

	/** Number of attempts to find a valid flee location. */
	UPROPERTY(EditAnywhere, Category="Flee")
	int32 MaxFindAttempts = 10;

private:
	/** Find a valid flee location away from the threat. */
	bool FindFleeLocation(APawn* OwnerPawn, AActor* ThreatActor, FVector& OutLocation) const;
};
