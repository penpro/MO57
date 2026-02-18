#include "BTTask_FleeFromThreat.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"

UBTTask_FleeFromThreat::UBTTask_FleeFromThreat()
{
	NodeName = TEXT("Flee From Threat");
	bNotifyTick = true;

	// Setup key filters
	ThreatActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FleeFromThreat, ThreatActorKey), AActor::StaticClass());
	FleeDestinationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FleeFromThreat, FleeDestinationKey));
}

EBTNodeResult::Type UBTTask_FleeFromThreat::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UE_LOG(LogTemp, Warning, TEXT("BTTask_FleeFromThreat: ExecuteTask called!"));

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		UE_LOG(LogTemp, Warning, TEXT("BTTask_FleeFromThreat: No AIController!"));
		return EBTNodeResult::Failed;
	}

	APawn* OwnerPawn = AIController->GetPawn();
	if (!OwnerPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("BTTask_FleeFromThreat: No OwnerPawn!"));
		return EBTNodeResult::Failed;
	}

	// Get threat from blackboard
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("BTTask_FleeFromThreat: No BlackboardComp!"));
		return EBTNodeResult::Failed;
	}

	AActor* ThreatActor = Cast<AActor>(BlackboardComp->GetValueAsObject(ThreatActorKey.SelectedKeyName));
	if (!ThreatActor)
	{
		// No threat, nothing to flee from
		UE_LOG(LogTemp, Warning, TEXT("BTTask_FleeFromThreat: No ThreatActor in blackboard key '%s'!"), *ThreatActorKey.SelectedKeyName.ToString());
		return EBTNodeResult::Failed;
	}

	UE_LOG(LogTemp, Warning, TEXT("BTTask_FleeFromThreat: Fleeing from %s"), *ThreatActor->GetName());

	// Find flee location
	FVector FleeLocation;
	if (!FindFleeLocation(OwnerPawn, ThreatActor, FleeLocation))
	{
		UE_LOG(LogTemp, Warning, TEXT("BTTask_FleeFromThreat: Could not find valid flee location for %s"),
			*OwnerPawn->GetName());
		return EBTNodeResult::Failed;
	}

	// Store flee destination in blackboard
	BlackboardComp->SetValueAsVector(FleeDestinationKey.SelectedKeyName, FleeLocation);

	// Start moving to flee location
	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(FleeLocation);
	MoveRequest.SetAcceptanceRadius(AcceptanceRadius);

	// Try pathfinding first, fall back to direct movement
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(OwnerPawn->GetWorld());
	bool bHasNavMesh = false;
	if (NavSys)
	{
		FNavLocation NavLoc;
		bHasNavMesh = NavSys->ProjectPointToNavigation(OwnerPawn->GetActorLocation(), NavLoc, FVector(100.f, 100.f, 100.f));
	}
	MoveRequest.SetUsePathfinding(bHasNavMesh);

	FPathFollowingRequestResult MoveResult = AIController->MoveTo(MoveRequest);

	FBTFleeMemory* MyMemory = reinterpret_cast<FBTFleeMemory*>(NodeMemory);
	MyMemory->TargetLocation = FleeLocation;

	if (MoveResult.Code == EPathFollowingRequestResult::Failed)
	{
		// MoveTo failed - use direct movement
		UE_LOG(LogTemp, Warning, TEXT("BTTask_FleeFromThreat: MoveTo failed, using direct movement"));
		MyMemory->bUseDirectMovement = true;
	}
	else
	{
		MyMemory->bUseDirectMovement = false;
	}

	// Set sprint speed for fleeing
	if (ACharacter* Character = Cast<ACharacter>(OwnerPawn))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			MyMemory->OriginalWalkSpeed = Movement->MaxWalkSpeed;

			// Try to get sprint speed from creature, otherwise use 1.5x walk speed
			float FleeSpeed = MyMemory->OriginalWalkSpeed * 1.5f;

			// Check if this is an MOCreature with SprintSpeed defined
			if (UFunction* GetSprintSpeedFunc = OwnerPawn->FindFunction(TEXT("GetSprintSpeed")))
			{
				// Has GetSprintSpeed function - but we can just check for the property
			}

			// Use a property lookup for SprintSpeed if available
			if (FProperty* SprintProp = OwnerPawn->GetClass()->FindPropertyByName(TEXT("SprintSpeed")))
			{
				if (FFloatProperty* FloatProp = CastField<FFloatProperty>(SprintProp))
				{
					FleeSpeed = FloatProp->GetPropertyValue_InContainer(OwnerPawn);
				}
			}

			Movement->MaxWalkSpeed = FleeSpeed;
			UE_LOG(LogTemp, Warning, TEXT("BTTask_FleeFromThreat: Set flee speed to %.1f (was %.1f)"),
				FleeSpeed, MyMemory->OriginalWalkSpeed);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("BTTask_FleeFromThreat: %s fleeing to %s (direct=%d)"),
		*OwnerPawn->GetName(), *FleeLocation.ToString(), MyMemory->bUseDirectMovement ? 1 : 0);

	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_FleeFromThreat::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController)
	{
		AIController->StopMovement();

		// Restore original walk speed
		if (APawn* OwnerPawn = AIController->GetPawn())
		{
			if (ACharacter* Character = Cast<ACharacter>(OwnerPawn))
			{
				if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
				{
					FBTFleeMemory* MyMemory = reinterpret_cast<FBTFleeMemory*>(NodeMemory);
					if (MyMemory->OriginalWalkSpeed > 0.f)
					{
						Movement->MaxWalkSpeed = MyMemory->OriginalWalkSpeed;
					}
				}
			}
		}
	}

	return EBTNodeResult::Aborted;
}

void UBTTask_FleeFromThreat::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	APawn* OwnerPawn = AIController->GetPawn();
	if (!OwnerPawn)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	FBTFleeMemory* MyMemory = reinterpret_cast<FBTFleeMemory*>(NodeMemory);

	auto RestoreSpeedAndFinish = [&](EBTNodeResult::Type Result)
	{
		// Restore original walk speed
		if (ACharacter* Character = Cast<ACharacter>(OwnerPawn))
		{
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				if (MyMemory->OriginalWalkSpeed > 0.f)
				{
					Movement->MaxWalkSpeed = MyMemory->OriginalWalkSpeed;
				}
			}
		}
		FinishLatentTask(OwnerComp, Result);
	};

	if (MyMemory->bUseDirectMovement)
	{
		// Direct movement fallback - move toward target using AddMovementInput
		FVector CurrentLocation = OwnerPawn->GetActorLocation();
		FVector Direction = (MyMemory->TargetLocation - CurrentLocation).GetSafeNormal();
		float DistanceToTarget = FVector::Dist2D(CurrentLocation, MyMemory->TargetLocation);

		if (DistanceToTarget <= AcceptanceRadius)
		{
			// Reached target
			RestoreSpeedAndFinish(EBTNodeResult::Succeeded);
			return;
		}

		// Apply movement input
		OwnerPawn->AddMovementInput(Direction, 1.0f);

		// Face movement direction
		FRotator TargetRotation = Direction.Rotation();
		OwnerPawn->SetActorRotation(FMath::RInterpTo(OwnerPawn->GetActorRotation(), TargetRotation, DeltaSeconds, 5.0f));
	}
	else
	{
		// Standard AI pathfinding - check if movement completed
		UPathFollowingComponent* PathComp = AIController->GetPathFollowingComponent();
		if (!PathComp || PathComp->GetStatus() == EPathFollowingStatus::Idle)
		{
			// Movement completed or no path component
			RestoreSpeedAndFinish(EBTNodeResult::Succeeded);
		}
	}
}

FString UBTTask_FleeFromThreat::GetStaticDescription() const
{
	return FString::Printf(TEXT("Flee %.0f-%.0f units from threat"), MinFleeDistance, MaxFleeDistance);
}

bool UBTTask_FleeFromThreat::FindFleeLocation(APawn* OwnerPawn, AActor* ThreatActor, FVector& OutLocation) const
{
	if (!OwnerPawn || !ThreatActor)
	{
		return false;
	}

	FVector MyLocation = OwnerPawn->GetActorLocation();
	FVector ThreatLocation = ThreatActor->GetActorLocation();

	// Direction away from threat
	FVector AwayDirection = (MyLocation - ThreatLocation).GetSafeNormal();

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(OwnerPawn->GetWorld());

	// Try multiple times to find a valid location
	for (int32 Attempt = 0; Attempt < MaxFindAttempts; ++Attempt)
	{
		// Add some randomness to the flee direction
		FVector RandomOffset = FMath::VRand();
		RandomOffset.Z = 0.f;
		RandomOffset = RandomOffset.GetSafeNormal() * 0.3f; // 30% deviation

		FVector FleeDirection = (AwayDirection + RandomOffset).GetSafeNormal();

		// Calculate flee distance with some randomness
		float FleeDistance = FMath::RandRange(MinFleeDistance, MaxFleeDistance);

		// Target location
		FVector TargetLocation = MyLocation + (FleeDirection * FleeDistance);

		// Try to project to navigation mesh if available
		if (NavSys)
		{
			FNavLocation NavLocation;
			if (NavSys->ProjectPointToNavigation(TargetLocation, NavLocation, FVector(500.f, 500.f, 500.f)))
			{
				// Verify the location is actually farther from the threat
				float NewDistanceToThreat = FVector::Dist(NavLocation.Location, ThreatLocation);
				float CurrentDistanceToThreat = FVector::Dist(MyLocation, ThreatLocation);

				if (NewDistanceToThreat > CurrentDistanceToThreat)
				{
					OutLocation = NavLocation.Location;
					return true;
				}
				continue; // Try another direction
			}
		}

		// Fallback: use target location directly without navmesh validation
		// Just verify it's farther from threat
		float NewDistanceToThreat = FVector::Dist(TargetLocation, ThreatLocation);
		float CurrentDistanceToThreat = FVector::Dist(MyLocation, ThreatLocation);

		if (NewDistanceToThreat > CurrentDistanceToThreat)
		{
			OutLocation = TargetLocation;
			UE_LOG(LogTemp, Warning, TEXT("BTTask_FleeFromThreat: Using direct location (no navmesh)"));
			return true;
		}
	}

	return false;
}
