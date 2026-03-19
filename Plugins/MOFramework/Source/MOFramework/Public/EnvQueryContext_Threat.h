// Copyright Penumbra Group Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvQueryContext_Threat.generated.h"

/**
 * EQS Context that provides the current threat actor.
 *
 * Reads "ThreatActor" from the AI controller's blackboard.
 * Used for escape route calculations and flee behavior.
 */
UCLASS()
class MOFRAMEWORK_API UEnvQueryContext_Threat : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	/** Blackboard key name for the threat actor */
	UPROPERTY(EditDefaultsOnly, Category = "Context")
	FName ThreatBlackboardKey;

	UEnvQueryContext_Threat();

	//~ Begin UEnvQueryContext Interface
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
	//~ End UEnvQueryContext Interface
};
