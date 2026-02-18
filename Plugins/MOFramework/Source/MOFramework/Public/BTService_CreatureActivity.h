#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_CreatureActivity.generated.h"

/**
 * BT Service: Manages creature activity states (rest, sleep) based on time of day.
 *
 * - During daytime: Randomly decides to rest based on RestChancePerMinute
 * - During nighttime: Sets creature to sleep (if bSleepsAtNight)
 * - Wakes up creatures when threats are detected
 */
UCLASS()
class MOFRAMEWORK_API UBTService_CreatureActivity : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_CreatureActivity();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual FString GetStaticDescription() const override;

	/** Chance per minute to start resting during daytime. */
	UPROPERTY(EditAnywhere, Category="Activity", meta=(ClampMin="0", ClampMax="1"))
	float RestChancePerMinute = 0.1f;

	/** Min duration of rest in seconds. */
	UPROPERTY(EditAnywhere, Category="Activity")
	float RestDurationMin = 10.f;

	/** Max duration of rest in seconds. */
	UPROPERTY(EditAnywhere, Category="Activity")
	float RestDurationMax = 30.f;

	/** Whether creature sleeps at night. */
	UPROPERTY(EditAnywhere, Category="Activity")
	bool bSleepsAtNight = true;

private:
	/** Timer tracking how long we've been in current rest/sleep state. */
	float CurrentStateDuration = 0.f;

	/** Target duration for current rest. */
	float TargetRestDuration = 0.f;

	/** Accumulated time for rest chance calculation. */
	float AccumulatedTime = 0.f;
};
