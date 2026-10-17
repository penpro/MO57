// How light-adapted the viewer's eyes are near a flame. A camera meters the whole frame, so a torch in a black cave pulls its exposure down to the
// darkness and the torch-lit floor blows out white. An eye does the opposite: a flame near you keeps the pupil small and the eye light-adapted, so
// what the flame lights reads normally and the darkness past it is black. The auto exposure may not adapt darker than the surroundings the
// nearby flames light.
//
// Local and cosmetic: only on the pawn a local player views through. It reads the flames registered in UMOAmbientEnvironmentRegistry
// (IMOLocalLightSource: a carried torch, a campfire), sums their illuminance at the camera (inverse square), and writes the camera's auto exposure
// minimum. The camera's post process blends over the sky's volume (Ultra Dynamic Sky sets EV100 -10..20). With no flame near, it writes nothing.
// Docs/Light_Sources.md ("What the camera must do").

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOEyeAdaptationComponent.generated.h"

class UCameraComponent;

UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOEyeAdaptationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOEyeAdaptationComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Reflectance of the surroundings the flame lights (rock, earth): ~0.2-0.4. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Eye", meta=(ClampMin="0.02", ClampMax="1"))
	float SurroundAlbedo = 0.3f;

	/**
	 * Added to the floor (EV): the surroundings the flame lights sit this much under middle grey at the floor. The camera is behind and above the
	 * pawn, so the illuminance at the camera is lower than on the ground round the flame.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Eye", meta=(ClampMin="-4", ClampMax="4"))
	float FloorBiasEV = -1.0f;

	/** Under this much flame light at the eye (lux) no floor is set and the sky's exposure range applies (a distant fire does not light-adapt you). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Eye", meta=(ClampMin="0"))
	float MinIlluminanceLux = 0.05f;

	/** How fast the floor follows (1/s): smooths the flicker; the camera's own adaptation speed does the rest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Eye", meta=(ClampMin="0.1"))
	float FloorFollowSpeed = 2.0f;

	/** The floor in force now (EV100), or a large negative number when none is set. For dev verbs. */
	float GetCurrentFloorEV100() const { return bFloorActive ? CurrentFloorEV100 : -100.0f; }
	float GetLastIlluminanceLux() const { return LastIlluminanceLux; }

	/**
	 * The exposure floor (EV100) for an illuminance at the eye: the luminance of a surround of Albedo lit by it (L = E x albedo / pi, cd/m2), as
	 * EV100 = log2(L x 100 / 12.5), plus BiasEV. Pure, for the tests.
	 */
	static float AdaptationFloorEV100(float IlluminanceLux, float Albedo, float BiasEV);

private:
	TWeakObjectPtr<UCameraComponent> Camera;
	float CurrentFloorEV100 = -10.0f;
	float LastIlluminanceLux = 0.0f;
	bool bFloorActive = false;

	UCameraComponent* FindCamera();
	void ClearFloor(UCameraComponent* Cam);
};
