#pragma once
#include "CoreMinimal.h"
#include "MOSurvivorJobQueueComponent.h"
#include "MOSurvivorIterationProbe.generated.h"

UCLASS()
class UMOSurvivorIterationProbe : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<UMOSurvivorJobQueueComponent> Queue;
	FGuid JobId;
	bool bFired = false;
	bool bWaitForCompletedIteration = false;
	EMOSurvivorIterationFailure NestedFailure = EMOSurvivorIterationFailure::None;
	UFUNCTION() void QueueChanged()
	{
		if (bFired) return;
		if (bWaitForCompletedIteration && (Queue->GetCurrentJob().JobId != JobId || Queue->GetCurrentJob().CompletedCount == 0)) return;
		bFired = true;
		NestedFailure = Queue->RecordCompletedIteration(JobId).Failure;
		Queue->CancelJob(JobId);
	}
};
