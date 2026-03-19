// Copyright Penumbra Group Inc. All Rights Reserved.

#include "EnvQueryTest_EscapeRoute.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "NavigationSystem.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

UEnvQueryTest_EscapeRoute::UEnvQueryTest_EscapeRoute()
{
	Cost = EEnvTestCost::Medium;
	ValidItemType = UEnvQueryItemType_Point::StaticClass();
	SetWorkOnFloatValues(true);

	MinEscapeDistance.DefaultValue = 500.0f; // 5 meters minimum
	DistanceWeight = 0.5f;
	DirectionWeight = 0.5f;
	bRequireNavigablePath = true;
}

void UEnvQueryTest_EscapeRoute::RunTest(FEnvQueryInstance& QueryInstance) const
{
	UObject* QueryOwner = QueryInstance.Owner.Get();
	if (!QueryOwner)
	{
		return;
	}

	// Get querier (creature) location
	FVector QuerierLocation = FVector::ZeroVector;
	AActor* QuerierActor = nullptr;

	if (AActor* OwnerActor = Cast<AActor>(QueryOwner))
	{
		QuerierLocation = OwnerActor->GetActorLocation();
		QuerierActor = OwnerActor;
	}
	else if (AAIController* AIController = Cast<AAIController>(QueryOwner))
	{
		if (APawn* Pawn = AIController->GetPawn())
		{
			QuerierLocation = Pawn->GetActorLocation();
			QuerierActor = Pawn;
		}
	}

	if (!QuerierActor)
	{
		return;
	}

	// Get threat location from context
	// Note: Caller should set up a context that provides the threat actor
	// For now, we'll use the querier's perceived threat from blackboard
	// TODO: Add proper threat context parameter
	FVector ThreatLocation = QuerierLocation + FVector(100.0f, 0.0f, 0.0f); // Fallback

	// Try to get threat from AI controller's blackboard
	AAIController* OwnerController = Cast<AAIController>(QueryOwner);
	if (!OwnerController)
	{
		if (APawn* Pawn = Cast<APawn>(QueryOwner))
		{
			OwnerController = Cast<AAIController>(Pawn->GetController());
		}
	}

	if (OwnerController)
	{
		if (UBlackboardComponent* BB = OwnerController->GetBlackboardComponent())
		{
			if (AActor* ThreatActor = Cast<AActor>(BB->GetValueAsObject(TEXT("ThreatActor"))))
			{
				ThreatLocation = ThreatActor->GetActorLocation();
			}
		}
	}

	// Direction away from threat (normalized)
	FVector AwayDirection = (QuerierLocation - ThreatLocation).GetSafeNormal2D();
	float MinDist = MinEscapeDistance.GetValue();
	float MinDistSq = MinDist * MinDist;

	// Get navigation system if path checking enabled
	UNavigationSystemV1* NavSys = nullptr;
	if (bRequireNavigablePath)
	{
		NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(QueryOwner->GetWorld());
	}

	// Process each point
	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		FVector PointLocation = GetItemLocation(QueryInstance, It.GetIndex());

		// Calculate distance from querier to escape point
		float DistanceToPoint = FVector::Dist(QuerierLocation, PointLocation);

		// Calculate distance from threat to escape point
		float DistanceFromThreat = FVector::Dist(ThreatLocation, PointLocation);

		// Reject points too close to querier
		if (DistanceToPoint < MinDist)
		{
			It.ForceItemState(EEnvItemStatus::Failed);
			continue;
		}

		// Calculate direction score (how well this point aligns with "away from threat")
		FVector ToPointDirection = (PointLocation - QuerierLocation).GetSafeNormal2D();
		float DirectionDot = FVector::DotProduct(AwayDirection, ToPointDirection);
		// Remap from [-1, 1] to [0, 1] where 1 = directly away from threat
		float DirectionScore = (DirectionDot + 1.0f) * 0.5f;

		// Calculate distance score (normalized by MinDist, capped at 2x MinDist)
		float DistanceScore = FMath::Clamp(DistanceToPoint / (MinDist * 2.0f), 0.0f, 1.0f);

		// Check navmesh path if required
		if (NavSys && bRequireNavigablePath)
		{
			FNavLocation NavLoc;
			FVector SearchExtent(200.0f, 200.0f, 500.0f);
			if (!NavSys->ProjectPointToNavigation(PointLocation, NavLoc, SearchExtent))
			{
				// Point not on navmesh
				It.ForceItemState(EEnvItemStatus::Failed);
				continue;
			}
		}

		// Combined score
		float FinalScore = (DirectionScore * DirectionWeight) + (DistanceScore * DistanceWeight);

		// Normalize to 0-1 range
		FinalScore = FMath::Clamp(FinalScore / (DirectionWeight + DistanceWeight), 0.0f, 1.0f);

		// SetScore with min/max thresholds for float scoring
		It.SetScore(TestPurpose, FilterType, FinalScore, 0.0f, 1.0f);
	}
}

FText UEnvQueryTest_EscapeRoute::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Escape Route Viability"));
}

FText UEnvQueryTest_EscapeRoute::GetDescriptionDetails() const
{
	FString Details = FString::Printf(
		TEXT("Min distance: %.0f\nDirection weight: %.1f\nDistance weight: %.1f"),
		MinEscapeDistance.DefaultValue, DirectionWeight, DistanceWeight);
	if (bRequireNavigablePath)
	{
		Details += TEXT("\nRequires navigable path");
	}
	return FText::FromString(Details);
}
