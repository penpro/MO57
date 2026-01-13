#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MOInteractionInterface.generated.h"

class AActor;

UINTERFACE(BlueprintType)
class MOFRAMEWORK_API UMOInteractionInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implement this on an owning actor (usually PlayerController).
 * The plugin will call RequestServerInteract on clients, and your implementation
 * should forward to a Server RPC, then call the interaction subsystem on the server.
 */
class MOFRAMEWORK_API IMOInteractionInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="MO|Interaction")
	bool RequestServerInteract(AActor* TargetActor);
};
