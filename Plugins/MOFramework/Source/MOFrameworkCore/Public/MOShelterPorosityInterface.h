/**
 * =============================================================================
 * MOShelterPorosityInterface.h - what a solid lets through of rain and wind
 * =============================================================================
 *
 * The shelter rays (UMOWeatherBlueprintLibrary::TestExposureShelter) count a solid as cover. Some solids are not tight (Wes, Nov 2 2026: "take the
 * wattle walls we have and add moss dobbing for insulation"): a bare wattle panel or a stick wall lets the wind through its gaps, a roof frame
 * without its thatch lets the rain through. An actor that implements this says, for the part a ray hit, the fraction of rain and of wind that
 * passes; the ray goes on through it carrying what passed (a daubed wall behind a bare one still stops the wind), so the cover a ray reads is
 * 1 - the product of everything it passed through. An actor that does not implement it is tight (passes nothing).
 *
 * Implemented by AMOBuildableActor (MOFramework): its RainPass / WindPass, once it is built.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MOShelterPorosityInterface.generated.h"

class UPrimitiveComponent;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UMOShelterPorosity : public UInterface
{
	GENERATED_BODY()
};

class MOFRAMEWORKCORE_API IMOShelterPorosity
{
	GENERATED_BODY()

public:
	/** The fraction (0..1) of rain and of wind the hit part lets through. */
	virtual void GetShelterPass(const UPrimitiveComponent* HitComponent, float& OutRainPass, float& OutWindPass) const = 0;
};
