// Copyright Penumbra Group Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "DataProviders/AIDataProvider.h"
#include "EnvQueryGenerator_HarvestTargets.generated.h"

class UMOHarvestSubsystem;

/**
 * EQS Generator that provides KeepOnHarvest target locations (trees, bushes, etc.).
 *
 * This generator queries for actors with ISM/HISM components that have the
 * KeepOnHarvest flag set. These are resource nodes that yield items when
 * harvested but aren't destroyed (e.g., trees that give sticks/branches).
 *
 * Usage in EQS:
 *   Generator: HarvestTargets
 *   Tests: Distance, PathExists, HasTag, etc.
 *
 * Combined with EnvQueryGenerator_HarvestableItems, this covers all resource
 * gathering scenarios for survivor AI.
 */
UCLASS()
class MOFRAMEWORK_API UEnvQueryGenerator_HarvestTargets : public UEnvQueryGenerator
{
	GENERATED_BODY()

public:
	UEnvQueryGenerator_HarvestTargets();

	/** Search radius around the querier */
	UPROPERTY(EditDefaultsOnly, Category = "Generator")
	FAIDataProviderFloatValue SearchRadius;

	/** Maximum number of targets to return (0 = unlimited) */
	UPROPERTY(EditDefaultsOnly, Category = "Generator")
	FAIDataProviderIntValue MaxTargets;

	/** If set, only return targets with this tag (e.g., "GivesStick", "GivesWood") */
	UPROPERTY(EditDefaultsOnly, Category = "Generator")
	FName RequiredTag;

	//~ Begin UEnvQueryGenerator Interface
	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;
	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;
	//~ End UEnvQueryGenerator Interface
};
