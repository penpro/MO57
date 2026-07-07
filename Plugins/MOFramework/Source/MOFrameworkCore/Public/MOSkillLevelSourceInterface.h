/**
 * =============================================================================
 * MOSkillLevelSourceInterface.h - skill-level query contract (C1 phase 3)
 * =============================================================================
 *
 * PURPOSE:
 * The medical layer sometimes needs a skill level (combat skill modulates
 * adrenaline response) but must not know the skills component (it lives in
 * the gameplay layer above). UMOSkillsComponent implements this; medical
 * consumers find it by iterating owner components for the interface.
 *
 * =============================================================================
 * RELATED FILES: MOAdrenalineComponent.h (consumer), MOSkillsComponent.h (impl)
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MOSkillLevelSourceInterface.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UMOSkillLevelSource : public UInterface
{
	GENERATED_BODY()
};

class MOFRAMEWORKCORE_API IMOSkillLevelSource
{
	GENERATED_BODY()

public:
	/** Current level of the given skill; 0 when unknown. */
	virtual int32 GetSkillLevelForQuery(FName SkillId) const = 0;
};
