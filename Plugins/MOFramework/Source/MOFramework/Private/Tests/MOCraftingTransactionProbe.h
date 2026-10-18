#pragma once

#include "CoreMinimal.h"
#include "MOCraftingQueueComponent.h"
#include "MOCraftingSubsystem.h"
#include "MOCraftingTransactionProbe.generated.h"

/** Synchronous completion observer used by the reservation regression gate. */
UCLASS()
class UMOCraftingTransactionProbe : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<UMOCraftingQueueComponent> Queue;
	FMOCraftingQueueSaveData AtCompletion;
	FMOCraftingQueueSaveData AfterCancellation;
	int32 Calls = 0;
	UFUNCTION()
	void HandleCompleted(const FGuid& EntryId, const FMOCraftResult& Result)
	{
		++Calls;
		Queue->BuildSaveData(AtCompletion);
		Queue->CancelAllCrafts(true);
		Queue->BuildSaveData(AfterCancellation);
	}
};
