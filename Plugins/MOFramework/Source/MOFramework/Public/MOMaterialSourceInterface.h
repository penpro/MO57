/**
 * =============================================================================
 * MOMaterialSourceInterface.h - Building Material Provider Contract
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Contract for actors that can provide materials for the building system.
 * The building system queries nearby material sources by priority to gather
 * required materials for construction.
 *
 * WHO IMPLEMENTS THIS:
 * - AMOCharacter - Priority 100 (checked first)
 * - AMOContainerActor - Priority 50
 * - AMOCraftingStationActor - Priority 50
 * - AMOWorldItem - Priority 25 (checked last)
 *
 * WHO USES THIS:
 * - Building system - Gathers materials for construction
 * - MOBuildProgressComponent - Consumes materials during build
 *
 * PRIORITY SYSTEM:
 * Higher priority sources are checked FIRST. This ensures:
 * 1. Player inventory is used before containers
 * 2. Containers are used before loose world items
 * 3. Can customize priority for special cases
 *
 * =============================================================================
 * IMPLEMENTATION REQUIREMENTS
 * =============================================================================
 *
 * 1. CanProvideMaterial: Return true if source has enough of the material.
 *    Don't consume anything - this is a query only.
 *
 * 2. GatherMaterial: Actually remove materials from source.
 *    Return actual quantity gathered (may be less than requested).
 *    MUST be authority-only for replicated sources.
 *
 * 3. GetMaterialSourcePriority: Return priority (higher = checked first).
 *    Standard values: Inventory=100, Container=50, WorldItem=25
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] PARTIAL GATHER: GatherMaterial may return less than requested if
 *   source doesn't have enough. Caller must handle partial fulfillment and
 *   query next source for remainder.
 *
 * [2024-02] AUTHORITY CHECK: GatherMaterial modifies state. For replicated
 *   actors, ensure it only runs on server/authority.
 *
 * [2024-02] WORLD ITEM DESTRUCTION: When AMOWorldItem provides all its
 *   materials, it should destroy itself. Handle this in GatherMaterial.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOInventoryHolderInterface.h - Related interface for inventory access
 * - MOBuildProgressComponent.h - Uses this to gather build materials
 * - MOWorldItem.h - Implements this for dropped items
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

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
 * Interface for material providers in the building system.
 * See file header for implementation requirements and pitfalls.
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
