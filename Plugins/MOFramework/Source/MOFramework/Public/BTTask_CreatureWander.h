#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_CreatureWander.generated.h"

/** Memory struct for wander task. */
struct FBTWanderMemory
{
	/** Current desired heading direction (world space). */
	FVector CurrentHeading;

	/** Time remaining in this wander session. */
	float TimeRemaining;

	/** Time until next heading adjustment. */
	float NextHeadingAdjustTime;

	/** Current heading adjustment delta (degrees per second). */
	float CurrentHeadingDelta;

	/** Whether the task has been initialized. */
	bool bInitialized;
};

/**
 * BT Task: Smooth continuous wandering behavior.
 *
 * Instead of picking random destinations (which causes stop-turn-go),
 * the creature keeps moving forward while gradually adjusting its heading.
 * This creates natural, fluid wandering movement.
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

	/** How long to wander before completing the task (seconds). */
	UPROPERTY(EditAnywhere, Category="Wander")
	float MinWanderDuration = 3.f;

	UPROPERTY(EditAnywhere, Category="Wander")
	float MaxWanderDuration = 8.f;

	/** Maximum distance from home location (0 = unlimited). */
	UPROPERTY(EditAnywhere, Category="Wander")
	float MaxDistanceFromHome = 2000.f;

	/** How often to adjust heading direction (seconds). */
	UPROPERTY(EditAnywhere, Category="Wander|Heading")
	float MinHeadingAdjustInterval = 1.f;

	UPROPERTY(EditAnywhere, Category="Wander|Heading")
	float MaxHeadingAdjustInterval = 3.f;

	/** Maximum heading change per adjustment (degrees). */
	UPROPERTY(EditAnywhere, Category="Wander|Heading")
	float MaxHeadingChange = 45.f;

	/** How fast to rotate toward the desired heading (degrees/sec). */
	UPROPERTY(EditAnywhere, Category="Wander|Heading")
	float HeadingInterpSpeed = 90.f;

	/** Movement speed multiplier while wandering (1.0 = normal walk speed). */
	UPROPERTY(EditAnywhere, Category="Wander")
	float WanderSpeedMultiplier = 0.6f;

private:
	/** Initialize heading from pawn's current forward vector. */
	void InitializeHeading(APawn* Pawn, FBTWanderMemory* Memory) const;

	/** Pick a new heading adjustment. */
	void PickNewHeadingAdjustment(APawn* Pawn, FBTWanderMemory* Memory, const FVector& HomeLocation) const;

	/** Get the home location from blackboard or pawn position. */
	FVector GetHomeLocation(UBehaviorTreeComponent& OwnerComp, APawn* Pawn) const;
};
