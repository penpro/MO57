#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MOMaterialSourceInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UMOMaterialSourceInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for actors that can provide materials for the building system.
 * Implemented by inventory holders, world items, and containers.
 *
 * Priority system determines gather order:
 * - Player Inventory: 100 (checked first)
 * - Containers: 50
 * - World Items: 25 (checked last)
 *
 * Implementers: AMOCharacter, AMOContainerActor, AMOCraftingStationActor, AMOWorldItem
 */
class MOFRAMEWORK_API IMOMaterialSourceInterface
{
	GENERATED_BODY()

public:
	/**
	 * Check if this source can provide a specific material.
	 * @param MaterialId The material item definition ID
	 * @param Quantity The quantity needed
	 * @return True if this source can provide the requested amount
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Building|Materials")
	bool CanProvideMaterial(FName MaterialId, int32 Quantity) const;

	/**
	 * Gather/consume material from this source.
	 * @param MaterialId The material item definition ID
	 * @param Quantity The quantity to gather
	 * @return The actual quantity gathered (may be less than requested)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Building|Materials")
	int32 GatherMaterial(FName MaterialId, int32 Quantity);

	/**
	 * Get the priority of this material source (higher = checked first).
	 * Default priorities: Inventory = 100, Container = 50, WorldItem = 25
	 * @return Priority value
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Building|Materials")
	int32 GetMaterialSourcePriority() const;
};
