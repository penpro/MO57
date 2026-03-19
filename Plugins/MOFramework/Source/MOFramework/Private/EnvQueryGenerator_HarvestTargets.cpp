// Copyright Penumbra Group Inc. All Rights Reserved.

#include "EnvQueryGenerator_HarvestTargets.h"
#include "MOHarvestSubsystem.h"
#include "MOHISMHarvestHelper.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "AIController.h"
#include "EngineUtils.h"

UEnvQueryGenerator_HarvestTargets::UEnvQueryGenerator_HarvestTargets()
{
	ItemType = UEnvQueryItemType_Point::StaticClass();
	SearchRadius.DefaultValue = 5000.0f; // 50 meters default
	MaxTargets.DefaultValue = 10;
	RequiredTag = NAME_None;
}

void UEnvQueryGenerator_HarvestTargets::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	UObject* QueryOwner = QueryInstance.Owner.Get();
	if (!QueryOwner)
	{
		return;
	}

	UWorld* World = QueryOwner->GetWorld();
	if (!World)
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
	float RadiusSq = Radius * Radius;
	int32 Max = MaxTargets.GetValue();

	// Get HarvestSubsystem for tag collection
	UMOHarvestSubsystem* HarvestSubsystem = World->GetSubsystem<UMOHarvestSubsystem>();

	// Collect harvest target locations
	TArray<FVector> ResultLocations;
	ResultLocations.Reserve(Max > 0 ? Max : 50);

	// Iterate actors looking for KeepOnHarvest ISM/HISM components
	// TODO: Replace with spatial index query when available
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		// Quick distance check on actor location
		float ActorDistSq = FVector::DistSquared(Actor->GetActorLocation(), QuerierLocation);
		if (ActorDistSq > RadiusSq)
		{
			continue;
		}

		// Check all ISM components on this actor
		TArray<UInstancedStaticMeshComponent*> ISMComponents;
		Actor->GetComponents<UInstancedStaticMeshComponent>(ISMComponents);

		for (UInstancedStaticMeshComponent* ISM : ISMComponents)
		{
			if (!IsValid(ISM) || ISM->GetInstanceCount() == 0)
			{
				continue;
			}

			// Must be KeepOnHarvest
			if (!FMOHISMHarvestHelper::ShouldKeepOnHarvest(ISM))
			{
				continue;
			}

			// Check tag requirement if specified
			if (!RequiredTag.IsNone() && HarvestSubsystem)
			{
				TArray<FName> TargetTags = HarvestSubsystem->CollectTargetTags(ISM);
				if (!TargetTags.Contains(RequiredTag))
				{
					continue;
				}
			}

			// Find closest instance within radius
			float BestDistSq = RadiusSq;
			FVector BestLocation = FVector::ZeroVector;
			bool bFoundInstance = false;

			for (int32 i = 0; i < ISM->GetInstanceCount(); i++)
			{
				FTransform InstanceTransform;
				if (ISM->GetInstanceTransform(i, InstanceTransform, true))
				{
					FVector InstanceLoc = InstanceTransform.GetLocation();
					float InstDistSq = FVector::DistSquared(InstanceLoc, QuerierLocation);
					if (InstDistSq < BestDistSq)
					{
						BestDistSq = InstDistSq;
						BestLocation = InstanceLoc;
						bFoundInstance = true;
					}
				}
			}

			if (bFoundInstance)
			{
				ResultLocations.Add(BestLocation);

				// Stop if we hit the max
				if (Max > 0 && ResultLocations.Num() >= Max)
				{
					break;
				}
			}
		}

		// Early exit if we have enough results
		if (Max > 0 && ResultLocations.Num() >= Max)
		{
			break;
		}
	}

	// Sort by distance
	ResultLocations.Sort([&QuerierLocation](const FVector& A, const FVector& B)
	{
		return FVector::DistSquared(A, QuerierLocation) < FVector::DistSquared(B, QuerierLocation);
	});

	// Store results
	QueryInstance.ReserveItemData(ResultLocations.Num());
	for (const FVector& Location : ResultLocations)
	{
		QueryInstance.AddItemData<UEnvQueryItemType_Point>(Location);
	}

	UE_LOG(LogTemp, Verbose, TEXT("EQS HarvestTargets: Generated %d targets within %.0f radius (tag: %s)"),
		ResultLocations.Num(), Radius, RequiredTag.IsNone() ? TEXT("any") : *RequiredTag.ToString());
}

FText UEnvQueryGenerator_HarvestTargets::GetDescriptionTitle() const
{
	return FText::FromString(FString::Printf(TEXT("Harvest Targets: %.0f radius"),
		SearchRadius.DefaultValue));
}

FText UEnvQueryGenerator_HarvestTargets::GetDescriptionDetails() const
{
	FString Details = TEXT("Queries KeepOnHarvest ISM/HISM components (trees, bushes)");
	if (!RequiredTag.IsNone())
	{
		Details += FString::Printf(TEXT("\nRequired tag: %s"), *RequiredTag.ToString());
	}
	return FText::FromString(Details);
}
