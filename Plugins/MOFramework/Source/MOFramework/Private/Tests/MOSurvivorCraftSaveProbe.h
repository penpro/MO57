#pragma once
#include "CoreMinimal.h"
#include "MOCraftingQueueComponent.h"
#include "MOInventoryComponent.h"
#include "MOSurvivorJobQueueComponent.h"
#include "MOSurvivorCraftSaveProbe.generated.h"

/** Capture the real save payload synchronously inside notifications, where half-transitions used to escape. */
UCLASS()
class UMOSurvivorCraftSaveProbe : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<UMOSurvivorJobQueueComponent> Jobs;
	UPROPERTY() TObjectPtr<UMOCraftingQueueComponent> Craft;
	UPROPERTY() TObjectPtr<UMOInventoryComponent> PawnInventory;
	UPROPERTY() TObjectPtr<UMOInventoryComponent> Storage;
	FMOSurvivorJobQueueSaveData JobSave;
	FMOCraftingQueueSaveData CraftSave;
	FMOInventorySaveData PawnSave, StorageSave;
	uint8 ExpectedPhase = 12;
	int32 ExpectedCount = 0;
	bool bCaptured = false;
	UFUNCTION() void Capture()
	{
		const auto Job = Jobs->GetCurrentJob();
		if (bCaptured || !Job.IsValid() || Job.CraftExecution.Phase != ExpectedPhase || Job.CompletedCount != ExpectedCount) return;
		bCaptured = true;
		Jobs->BuildSaveData(JobSave);
		Craft->BuildSaveData(CraftSave);
		PawnInventory->BuildSaveData(PawnSave);
		Storage->BuildSaveData(StorageSave);
	}
};
