#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MOMedicalProviderInterface.generated.h"

class UMOVitalsComponent;
class UMOAnatomyComponent;
class UMOMentalStateComponent;
class UMOAdrenalineComponent;
class UMOMetabolismComponent;

/**
 * Interface for actors that provide medical/health components.
 * Decouples medical systems from specific character types and eliminates
 * FindComponentByClass chains.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UMOMedicalProviderInterface : public UInterface
{
	GENERATED_BODY()
};

class MOFRAMEWORK_API IMOMedicalProviderInterface
{
	GENERATED_BODY()

public:
	/**
	 * Get the vitals component (health, stamina, etc.).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Medical")
	UMOVitalsComponent* GetVitals() const;

	/**
	 * Get the anatomy component (body parts, injuries).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Medical")
	UMOAnatomyComponent* GetAnatomy() const;

	/**
	 * Get the mental state component (stress, morale).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Medical")
	UMOMentalStateComponent* GetMentalState() const;

	/**
	 * Get the adrenaline component.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Medical")
	UMOAdrenalineComponent* GetAdrenaline() const;

	/**
	 * Get the metabolism component (hunger, thirst).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Medical")
	UMOMetabolismComponent* GetMetabolism() const;
};
