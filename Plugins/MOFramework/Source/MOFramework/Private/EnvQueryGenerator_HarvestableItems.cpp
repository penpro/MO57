// Copyright Penumbra Group Inc. All Rights Reserved.

#include "EnvQueryGenerator_HarvestableItems.h"
#include "MOForagingSubsystem.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "Engine/World.h"
#include "AIController.h"

UEnvQueryGenerator_HarvestableItems::UEnvQueryGenerator_HarvestableItems()
{
	ItemType = UEnvQueryItemType_Point::StaticClass();
	SearchRadius.DefaultValue = 5000.0f; // 50 meters default
	MaxItems.DefaultValue = 20;
	FilterTag = NAME_None;
	bIncludeKeepOnHarvest = false;
}

void UEnvQueryGenerator_HarvestableItems::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	UObject* QueryOwner = QueryInstance.Owner.Get();
	if (!QueryOwner)
	{
		return;
	}

	// Get the querier's location
	FVector QuerierLocation = FVector::ZeroVector;
	if (AActor* OwnerActor = Cast<AActor>(QueryOwner))
	{
		QuerierLocation = OwnerActor->GetActorLocation();
	}
	else if (AAIController* AIController = Cast<AAIController>(QueryOwner))
	{
		if (APawn* Pawn = AIController->GetPawn())
		{
			QuerierLocation = Pawn->GetActorLocation();
		}
	}

	// Get parameter values
	float Radius = SearchRadius.GetValue();
	int32 Max = MaxItems.GetValue();

	// Get the ForagingSubsystem
	UMOForagingSubsystem* ForagingSubsystem = GetForagingSubsystem(QueryInstance);
	if (!ForagingSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("EQS HarvestableItems: No ForagingSubsystem available"));
		return;
	}

	// Query harvestable items
	TArray<FMOHISMInstanceData> Instances = ForagingSubsystem->QueryHISMInstancesInRadius(QuerierLocation, Radius);

	// Filter by tag if specified
	TArray<FVector> ResultLocations;
	ResultLocations.Reserve(Instances.Num());

	for (const FMOHISMInstanceData& Instance : Instances)
	{
		if (!Instance.IsValid())
		{
			continue;
		}

		// Filter by tag if specified
		if (!FilterTag.IsNone())
		{
			// Check if item ID contains the filter tag (simple substring match for now)
			// TODO: Use proper gameplay tags when item system migrates to tags
			if (!Instance.ItemId.ToString().Contains(FilterTag.ToString()))
			{
				continue;
			}
		}

		ResultLocations.Add(Instance.InstanceTransform.GetLocation());

		// Stop if we hit the max
		if (Max > 0 && ResultLocations.Num() >= Max)
		{
			break;
		}
	}

	// TODO: If bIncludeKeepOnHarvest, also query the HarvestSubsystem for trees/bushes
	// This would need a spatial query on KeepOnHarvest actors

	// Store results
	QueryInstance.ReserveItemData(ResultLocations.Num());
	for (const FVector& Location : ResultLocations)
	{
		QueryInstance.AddItemData<UEnvQueryItemType_Point>(Location);
	}

	UE_LOG(LogTemp, Verbose, TEXT("EQS HarvestableItems: Generated %d items within %.0f radius"),
		ResultLocations.Num(), Radius);
}

FText UEnvQueryGenerator_HarvestableItems::GetDescriptionTitle() const
{
	return FText::FromString(FString::Printf(TEXT("Harvestable Items: %.0f radius"),
		SearchRadius.DefaultValue));
}

FText UEnvQueryGenerator_HarvestableItems::GetDescriptionDetails() const
{
	FString Details = TEXT("Queries ForagingSubsystem for harvestable HISM instances");
	if (!FilterTag.IsNone())
	{
		Details += FString::Printf(TEXT("\nFilter: %s"), *FilterTag.ToString());
	}
	if (bIncludeKeepOnHarvest)
	{
		Details += TEXT("\nIncludes trees/bushes");
	}
	return FText::FromString(Details);
}

UMOForagingSubsystem* UEnvQueryGenerator_HarvestableItems::GetForagingSubsystem(FEnvQueryInstance& QueryInstance) const
{
	UObject* QueryOwner = QueryInstance.Owner.Get();
	if (!QueryOwner)
	{
		return nullptr;
	}

	UWorld* World = QueryOwner->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	return World->GetSubsystem<UMOForagingSubsystem>();
}
