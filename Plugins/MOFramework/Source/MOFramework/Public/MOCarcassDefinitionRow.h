/**
 * =============================================================================
 * MOCarcassDefinitionRow.h - Carcass DataTable Definition
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 *
 * PURPOSE:
 * DataTable row structure for carcass definitions. Defines decay timing,
 * available parts, and visual configuration for creature carcasses.
 *
 * USAGE:
 * 1. Create DT_CarcassDefinitions DataTable
 * 2. Add rows for each creature type (Deer, Wolf, etc.)
 * 3. Configure parts array with all harvestable parts
 * 4. Link from FMOCreatureDefinitionRow via CarcassDefinitionId
 *
 * EXAMPLE DEER ROW:
 * - DisplayName: "Deer Carcass"
 * - TimeToRotting: 3600 (1 hour)
 * - TimeToSkeletal: 86400 (24 hours)
 * - Parts: [Hide, Skull, Brains, Tongue, Blood, Heart, Liver, ...]
 *
 * RELATED FILES:
 * - MOCarcassTypes.h - FMOCarcassPartEntry, EMOCarcassDecayStage
 * - MOCarcassActor.h - Uses this definition
 * - MOCreatureDefinitionRow.h - Links to this via CarcassDefinitionId
 *
 * LAST UPDATED: 2026-02-26
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MOCarcassTypes.h"
#include "MOCarcassDefinitionRow.generated.h"

/**
 * DataTable row for carcass definitions.
 * Defines what can be harvested from a dead creature and decay timing.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOCarcassDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	// ============================================================================
	// IDENTITY
	// ============================================================================

	/** Display name (e.g., "Deer Carcass"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	FText DisplayName;

	/** Description for inspection/tooltip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	FText Description;

	// ============================================================================
	// VISUAL
	// ============================================================================

	/** Skeletal mesh to display for carcass (optional - can use creature's mesh). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
	TSoftObjectPtr<USkeletalMesh> CarcassMesh;

	/** Static mesh to use when skeletal (bones pile). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
	TSoftObjectPtr<UStaticMesh> SkeletalMesh;

	/** Material to apply when rotting (optional - for color change). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
	TSoftObjectPtr<UMaterialInterface> RottingMaterial;

	// ============================================================================
	// DECAY TIMING
	// ============================================================================

	/** Time until Fresh -> Rotting (seconds, default 1 hour). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decay", meta=(ClampMin="60"))
	float TimeToRotting = 3600.0f;

	/** Time until Rotting -> Skeletal (seconds, default 24 hours). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decay", meta=(ClampMin="60"))
	float TimeToSkeletal = 86400.0f;

	/** Time until Skeletal -> Destroyed (seconds, 0 = never). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decay")
	float TimeToDestroyed = 0.0f;

	// ============================================================================
	// PARTS
	// ============================================================================

	/** All harvestable parts from this carcass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parts")
	TArray<FMOCarcassPartEntry> Parts;

	// ============================================================================
	// HELPERS
	// ============================================================================

	/** Get parts available at a given decay stage. */
	TArray<FMOCarcassPartEntry> GetPartsForStage(EMOCarcassDecayStage Stage) const
	{
		TArray<FMOCarcassPartEntry> Available;
		for (const FMOCarcassPartEntry& Part : Parts)
		{
			if (Part.IsAvailableAtStage(Stage))
			{
				Available.Add(Part);
			}
		}
		return Available;
	}

	/** Find a part by ID. */
	const FMOCarcassPartEntry* FindPart(FName PartId) const
	{
		for (const FMOCarcassPartEntry& Part : Parts)
		{
			if (Part.PartId == PartId)
			{
				return &Part;
			}
		}
		return nullptr;
	}
};
