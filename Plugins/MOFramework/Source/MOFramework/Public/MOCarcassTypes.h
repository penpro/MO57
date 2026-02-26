/**
 * =============================================================================
 * MOCarcassTypes.h - Carcass Butchering Types
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 *
 * PURPOSE:
 * Types and enums for the carcass butchering system. Defines decay stages,
 * harvestable part entries, and related structures.
 *
 * DECAY SYSTEM:
 * Fresh (0-60 min) -> Rotting (1-24 hr) -> Skeletal (24+ hr)
 * - Fresh: All parts available, best quality meat
 * - Rotting: Meat becomes rotten, blood unavailable, organs still usable
 * - Skeletal: Only bones, skull, hooves remain
 *
 * RELATED FILES:
 * - MOCarcassActor.h - Uses these types
 * - MOCarcassDefinitionRow.h - DataTable row using FMOCarcassPartEntry
 * - MOCreature.h - Spawns carcass on death
 *
 * LAST UPDATED: 2026-02-26
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOCarcassTypes.generated.h"

/**
 * Decay stage of a carcass.
 * Determines which parts can be harvested and their quality.
 */
UENUM(BlueprintType)
enum class EMOCarcassDecayStage : uint8
{
	/** Just died - all parts available, best quality. */
	Fresh,

	/** Hours after death - meat becomes rotten, blood unavailable. */
	Rotting,

	/** Days after death - only bones, skull, hooves remain. */
	Skeletal,

	/** Nothing left (fully butchered or despawned). */
	Destroyed
};

/**
 * Defines a single harvestable part from a carcass.
 * Used in carcass definition DataTable rows.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOCarcassPartEntry
{
	GENERATED_BODY()

	/** Part identifier (e.g., "Skull", "Hindquarter_L", "Hide"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Part")
	FName PartId;

	/** Display name for UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Part")
	FText DisplayName;

	/** Item definition ID to give when harvested. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Part")
	FName ItemDefinitionId;

	/** Quantity to give. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Part", meta=(ClampMin="1"))
	int32 Quantity = 1;

	/** Time in seconds to harvest this part. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Part", meta=(ClampMin="0.5"))
	float HarvestTime = 2.0f;

	// ============================================================================
	// REQUIREMENTS
	// ============================================================================

	/** Tool type required (e.g., "Knife", "Cleaver"). Empty = no tool needed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Requirements")
	FName RequiredToolType;

	/** If true, requires empty container in inventory (for blood, bile, etc). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Requirements")
	bool bRequiresContainer = false;

	/** Container item ID to consume when harvesting (for liquids). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Requirements", meta=(EditCondition="bRequiresContainer"))
	FName ContainerItemId;

	// ============================================================================
	// DECAY
	// ============================================================================

	/** At which decay stage this part becomes unavailable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decay")
	EMOCarcassDecayStage MaxDecayStage = EMOCarcassDecayStage::Skeletal;

	/** If true, this part transforms into a different item when rotting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decay")
	bool bDecaysIntoAlternate = false;

	/** Item to give instead if carcass is rotting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decay", meta=(EditCondition="bDecaysIntoAlternate"))
	FName RottenItemDefinitionId;

	// ============================================================================
	// HELPERS
	// ============================================================================

	/** Check if this part is available at the given decay stage. */
	bool IsAvailableAtStage(EMOCarcassDecayStage Stage) const
	{
		return static_cast<int32>(Stage) <= static_cast<int32>(MaxDecayStage);
	}

	/** Get the item ID to give based on decay stage. */
	FName GetItemForStage(EMOCarcassDecayStage Stage) const
	{
		if (bDecaysIntoAlternate && Stage == EMOCarcassDecayStage::Rotting)
		{
			return RottenItemDefinitionId;
		}
		return ItemDefinitionId;
	}
};
