// Copyright Penumbra Group Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "DataProviders/AIDataProvider.h"
#include "EnvQueryTest_EscapeRoute.generated.h"

/**
 * EQS Test that evaluates escape route viability.
 *
 * Scores points based on how effectively they lead away from a threat.
 * Used by prey creatures to determine if they're cornered (no good escape routes).
 *
 * Scoring factors:
 * - Distance from threat (farther = better)
 * - Direction away from threat (directly away = better)
 * - Path availability (must be navigable)
 * - Not blocked by obstacles
 *
 * Usage in EQS:
 *   Generator: Points around querier
 *   Context: Threat actor
 *   Test: EscapeRoute (this test)
 */
UCLASS()
class MOFRAMEWORK_API UEnvQueryTest_EscapeRoute : public UEnvQueryTest
{
	GENERATED_BODY()

public:
	UEnvQueryTest_EscapeRoute();

	/** Minimum distance to consider a valid escape (closer points are rejected) */
	UPROPERTY(EditDefaultsOnly, Category = "Test")
	FAIDataProviderFloatValue MinEscapeDistance;

	/** Weight for distance scoring (how much being farther helps) */
	UPROPERTY(EditDefaultsOnly, Category = "Test")
	float DistanceWeight;

	/** Weight for direction scoring (how much being in the opposite direction helps) */
	UPROPERTY(EditDefaultsOnly, Category = "Test")
	float DirectionWeight;

	/** If true, require a valid navmesh path to the escape point */
	UPROPERTY(EditDefaultsOnly, Category = "Test")
	bool bRequireNavigablePath;

	//~ Begin UEnvQueryTest Interface
	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;
	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;
	//~ End UEnvQueryTest Interface
};
