// Makes an actor's fire light count as a flame the eye adapts to (IMOLocalLightSource in UMOAmbientEnvironmentRegistry): add it to a campfire,
// forge, kiln or brazier Blueprint and the point lights on that actor -- while visible -- light-adapt a viewer standing near them, as a carried torch
// does (UMOEyeAdaptationComponent). Nothing about the actor's own code changes; it reads the lights it already has.
//
// The intensity is read from the light itself in whatever units it was authored in (unitless, candelas, lumens) and given in candelas, so a
// campfire's unitless 5000 counts as the 8 cd it renders as. Docs/Light_Sources.md.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOAmbientEnvironmentProvider.h"
#include "MOFlameLightComponent.generated.h"

class ULocalLightComponent;

UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOFlameLightComponent : public UActorComponent, public IMOLocalLightSource
{
	GENERATED_BODY()

public:
	UMOFlameLightComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// IMOLocalLightSource: the brightest visible light's position; the sum of the visible lights' candelas (0 while the fire is out).
	virtual FVector GetLightLocation() const override;
	virtual float GetLuminousIntensityCd() const override;

	/** A local light's luminous intensity in candelas from its authored units (unitless x16 / 10000, lumens / 4 pi), or 0 when it is hidden or off. Pure-ish, for the tests. */
	static float CandelasOf(const ULocalLightComponent* Light);

	/** The light's intensity in candelas for an authored value in given units (unitless: x16 / 10000; candelas: as is; lumens: / 4 pi; others 0). Pure, for the tests. */
	static float CandelasFromUnits(float Intensity, uint8 Units);

private:
	/** The owner's local lights, found at BeginPlay. */
	TArray<TWeakObjectPtr<ULocalLightComponent>> Lights;
};
