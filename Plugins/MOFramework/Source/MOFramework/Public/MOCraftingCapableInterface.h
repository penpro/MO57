/**
 * =============================================================================
 * MOCraftingCapableInterface.h - Crafting Capability Interface
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Interface for actors (typically pawns) that can perform crafting. Provides
 * standardized access to crafting queue, recipe discovery, and active station
 * tracking. Decouples crafting system from specific pawn implementations.
 *
 * KEY RESPONSIBILITIES:
 * 1. Provide access to crafting queue component
 * 2. Provide access to recipe discovery component
 * 3. Track active crafting station for station-specific recipes
 *
 * IMPLEMENTORS:
 * - AMOCharacter: Primary implementor with all crafting components
 * - Potentially vehicles or structures with crafting capability
 *
 * USAGE PATTERN:
 * if (Actor->Implements<UMOCraftingCapableInterface>())
 * {
 *     UMOCraftingQueueComponent* Queue =
 *         IMOCraftingCapableInterface::Execute_GetCraftingQueue(Actor);
 *     // Use queue...
 * }
 *
 * ACTIVE STATION TRACKING:
 * SetActiveCraftingStation(StationActor) - when opening station
 * GetActiveCraftingStation() - for validation and save/load
 * SetActiveCraftingStation(nullptr) - when leaving station
 *
 * CRITICAL PATTERNS:
 * 1. BLUEPRINTNATIVEEVENT: All methods have _Implementation suffix
 * 2. INTERFACE CAST: Use Implements<T>() + Execute_* pattern
 * 3. NULLABLE: All returns may be nullptr if not implemented
 *
 * KNOWN PITFALLS:
 * 1. WEAK STATION: Active station reference may become invalid
 * 2. COMPONENT NULL: Implementor may not have all components
 *
 * RELATED FILES:
 * - MOCharacter.h - Primary implementor
 * - MOCraftingQueueComponent.h - Queue management
 * - MORecipeDiscoveryComponent.h - Recipe unlocking
 * - MOCraftingStationActor.h - Crafting station actors
 *
 * TESTING CHECKLIST:
 * [ ] Character implements interface correctly
 * [ ] GetCraftingQueue returns valid component
 * [ ] GetRecipeDiscovery returns valid component
 * [ ] Active station tracks correctly
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MOCraftingCapableInterface.generated.h"

class UMOCraftingQueueComponent;
class UMORecipeDiscoveryComponent;
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
