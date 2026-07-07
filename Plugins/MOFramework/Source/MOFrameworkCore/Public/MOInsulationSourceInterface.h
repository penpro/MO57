/**
 * =============================================================================
 * MOInsulationSourceInterface.h - clothing insulation contract (codex review)
 * =============================================================================
 *
 * PURPOSE:
 * Vitals needs one number — how much insulation the pawn's clothing adds —
 * but must not know the equipment component (it lives in the gameplay layer
 * above the medical module). UMOEquipmentComponent implements this by
 * summing equipped items' "Warmth" scalar properties; vitals finds it by
 * iterating owner components for the interface. Vitals consumes the final
 * value; it does not own clothing policy.
 *
 * =============================================================================
 * RELATED FILES: MOVitalsComponent.h (consumer), MOEquipmentComponent.h (impl),
 *   MOItemDefinitionRow.h (FMOItemScalarProperty "Warmth")
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MOInsulationSourceInterface.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UMOInsulationSource : public UInterface
{
	GENERATED_BODY()
};

class MOFRAMEWORKCORE_API IMOInsulationSource
{
	GENERATED_BODY()

public:
	/** Additional insulation from clothing, 0..1 scale (summed with the
	 *  wearer's baseline and shelter bonus by the consumer). */
	virtual float GetClothingInsulationBonus01() const = 0;
};
