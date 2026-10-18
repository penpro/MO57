#pragma once
#include "CoreMinimal.h"
#include "MOEquipmentComponent.h"
#include "MOEquipmentWearProbe.generated.h"

UCLASS()
class UMOEquipmentWearProbe : public UObject
{
	GENERATED_BODY()
public:
	int32 EquipmentEvents = 0;
	int32 WearEvents = 0;
	UFUNCTION() void EquipmentChanged(EMOEquipmentSlot Slot, const FMOEquippedItem& Item) { ++EquipmentEvents; }
	UFUNCTION() void DurabilityChanged(EMOEquipmentSlot Slot, int32 Durability) { ++WearEvents; }
};
