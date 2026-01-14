#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MOInteractionInterface.generated.h"

UINTERFACE(BlueprintType)
class MOFRAMEWORK_API UMOInteractionInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface implemented by "interactors" (typically PlayerController).
 * It provides a viewpoint for traces and a client entry point to request a server interaction.
 */
class MOFRAMEWORK_API IMOInteractionInterface
{
	GENERATED_BODY()

public:
	/**
	 * Provide the world-space viewpoint used for interaction traces.
	 * Return true if OutWorldLocation/OutWorldRotation are valid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Interaction")
	bool GetInteractionViewpoint(FVector& OutWorldLocation, FRotator& OutWorldRotation) const;

	/**
	 * Client-side entry point. Implementors typically forward to a Server RPC.
	 * Return true if the request was accepted for processing.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Interaction")
	bool RequestServerInteract(AActor* TargetActor);
};
