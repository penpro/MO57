// Copyright Penumbra Group Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "DataProviders/AIDataProvider.h"
#include "EnvQueryGenerator_HarvestableItems.generated.h"

class UMOForagingSubsystem;

/**
 * EQS Generator that provides harvestable HISM instance locations.
 *
 * This generator queries the ForagingSubsystem for harvestable items within
 * a configurable radius around the querier. Results are returned as points
 * that can be scored by EQS tests.
 *
 * Usage in EQS:
 *   Generator: HarvestableItems
 *   Tests: Distance, PathExists, Trace (visibility), etc.
 *
 * Performance Note: The underlying ForagingSubsystem query is O(n) where n is
 * the number of actors. Future optimization will add spatial indexing.
 */
UCLASS()
class MOFRAMEWORK_API UEnvQueryGenerator_HarvestableItems : public UEnvQueryGenerator
{
	GENERATED_BODY()

public:
	UEnvQueryGenerator_HarvestableItems();

	/** Search radius around the querier */
	UPROPERTY(EditDefaultsOnly, Category = "Generator")
	FAIDataProviderFloatValue SearchRadius;

	/** Maximum number of items to return (0 = unlimited) */
	UPROPERTY(EditDefaultsOnly, Category = "Generator")
	FAIDataProviderIntValue MaxItems;

	/** If set, only return items matching this tag (e.g., "Wood", "Stone", "Fiber") */
	UPROPERTY(EditDefaultsOnly, Category = "Generator")
	FName FilterTag;

	/** If true, include KeepOnHarvest targets (trees, bushes). If false, only ground spawns. */
	UPROPERTY(EditDefaultsOnly, Category = "Generator")
	bool bIncludeKeepOnHarvest;

	//~ Begin UEnvQueryGenerator Interface
	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;
	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;
	//~ End UEnvQueryGenerator Interface

protected:
	/** Get the ForagingSubsystem from the query context */
	UMOForagingSubsystem* GetForagingSubsystem(FEnvQueryInstance& QueryInstance) const;
};
