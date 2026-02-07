#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MOCraftingCapableInterface.generated.h"

class UMOCraftingQueueComponent;
class UMORecipeDiscoveryComponent;

/**
 * Interface for actors (typically pawns) that have crafting capabilities.
 * Decouples crafting stations from specific pawn types.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UMOCraftingCapableInterface : public UInterface
{
	GENERATED_BODY()
};

class MOFRAMEWORK_API IMOCraftingCapableInterface
{
	GENERATED_BODY()

public:
	/**
	 * Get the crafting queue component for this actor.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Crafting")
	UMOCraftingQueueComponent* GetCraftingQueue() const;

	/**
	 * Get the recipe discovery component for this actor.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Crafting")
	UMORecipeDiscoveryComponent* GetRecipeDiscovery() const;

	/**
	 * Set the active crafting station for this actor.
	 * @param StationActor - The station to use (nullptr to clear)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Crafting")
	void SetActiveCraftingStation(AActor* StationActor);

	/**
	 * Get the active crafting station for this actor.
	 * @return The current station actor, or nullptr if none
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Crafting")
	AActor* GetActiveCraftingStation() const;
};
