#include "BTTask_CreatureWander.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"

UBTTask_CreatureWander::UBTTask_CreatureWander()
{
	NodeName = TEXT("Creature Wander");
	bNotifyTick = true;

	// Setup key filters
	HomeLocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_CreatureWander, HomeLocationKey));
}

EBTNodeResult::Type UBTTask_CreatureWander::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	APawn* OwnerPawn = AIController->GetPawn();
	if (!OwnerPawn)
	{
		return EBTNodeResult::Failed;
	}

	// Get home location from blackboard (or use current location)
	FVector HomeLocation = OwnerPawn->GetActorLocation();
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (BlackboardComp && HomeLocationKey.IsSet())
	{
		HomeLocation = BlackboardComp->GetValueAsVector(HomeLocationKey.SelectedKeyName);
		if (HomeLocation.IsZero())
		{
			HomeLocation = OwnerPawn->GetActorLocation();
		}
	}

	// Find wander location
	FVector WanderLocation;
	if (!FindWanderLocation(OwnerPawn, HomeLocation, WanderLocation))
	{
		return EBTNodeResult::Failed;
	}

	// Start moving to wander location
	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(WanderLocation);
	MoveRequest.SetAcceptanceRadius(AcceptanceRadius);

	// Try pathfinding if navmesh available, otherwise direct movement
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(OwnerPawn->GetWorld());
	bool bHasNavMesh = false;
	if (NavSys)
	{
		FNavLocation NavLoc;
		bHasNavMesh = NavSys->ProjectPointToNavigation(OwnerPawn->GetActorLocation(), NavLoc, FVector(100.f, 100.f, 100.f));
	}
	MoveRequest.SetUsePathfinding(bHasNavMesh);

	FPathFollowingRequestResult MoveResult = AIController->MoveTo(MoveRequest);

	UE_LOG(LogTemp, Warning, TEXT("BTTask_CreatureWander: MoveTo result=%d (0=Failed, 1=AlreadyAtGoal, 2=RequestSuccessful)"),
		static_cast<int32>(MoveResult.Code));

	if (MoveResult.Code == EPathFollowingRequestResult::Failed)
	{
		// MoveTo failed - try direct movement via character movement component
		UE_LOG(LogTemp, Warning, TEXT("BTTask_CreatureWander: MoveTo failed, trying direct movement"));

		// Store target in node memory for tick-based movement
		FBTWanderMemory* MyMemory = reinterpret_cast<FBTWanderMemory*>(NodeMemory);
		MyMemory->TargetLocation = WanderLocation;
		MyMemory->bUseDirectMovement = true;

		return EBTNodeResult::InProgress;
	}

	// Store that we're using AI movement
	FBTWanderMemory* MyMemory = reinterpret_cast<FBTWanderMemory*>(NodeMemory);
	MyMemory->bUseDirectMovement = false;

	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_CreatureWander::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController)
	{
		AIController->StopMovement();
	}

	return EBTNodeResult::Aborted;
}

void UBTTask_CreatureWander::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
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

	FBTWanderMemory* MyMemory = reinterpret_cast<FBTWanderMemory*>(NodeMemory);

	if (MyMemory->bUseDirectMovement)
	{
		// Direct movement fallback - move toward target using AddMovementInput
		FVector CurrentLocation = OwnerPawn->GetActorLocation();
		FVector Direction = (MyMemory->TargetLocation - CurrentLocation).GetSafeNormal();
		float DistanceToTarget = FVector::Dist2D(CurrentLocation, MyMemory->TargetLocation);

		if (DistanceToTarget <= AcceptanceRadius)
		{
			// Reached target
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
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
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		}
	}
}

FString UBTTask_CreatureWander::GetStaticDescription() const
{
	return FString::Printf(TEXT("Wander %.0f-%.0f units"), MinWanderDistance, MaxWanderDistance);
}

bool UBTTask_CreatureWander::FindWanderLocation(APawn* OwnerPawn, const FVector& HomeLocation, FVector& OutLocation) const
{
	if (!OwnerPawn)
	{
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(OwnerPawn->GetWorld());
	FVector MyLocation = OwnerPawn->GetActorLocation();

	// Try to find a valid random location
	for (int32 Attempt = 0; Attempt < 10; ++Attempt)
	{
		// Random direction
		FVector RandomDirection = FMath::VRand();
		RandomDirection.Z = 0.f;
		RandomDirection = RandomDirection.GetSafeNormal();

		// Random distance
		float Distance = FMath::RandRange(MinWanderDistance, MaxWanderDistance);

		// Target location
		FVector TargetLocation = MyLocation + (RandomDirection * Distance);

		// Check if within max distance from home
		if (MaxDistanceFromHome > 0.f)
		{
			float DistanceFromHome = FVector::Dist2D(TargetLocation, HomeLocation);
			if (DistanceFromHome > MaxDistanceFromHome)
			{
				// Try to pull back toward home
				FVector ToHome = (HomeLocation - TargetLocation).GetSafeNormal();
				TargetLocation = HomeLocation + (ToHome * MaxDistanceFromHome * -0.8f);
			}
		}

		// Try to project to navigation mesh if available
		if (NavSys)
		{
			FNavLocation NavLocation;
			if (NavSys->ProjectPointToNavigation(TargetLocation, NavLocation, FVector(500.f, 500.f, 500.f)))
			{
				OutLocation = NavLocation.Location;
				return true;
			}
		}

		// Fallback: use target location directly without navmesh validation
		OutLocation = TargetLocation;
		UE_LOG(LogTemp, Log, TEXT("BTTask_CreatureWander: Using direct location (no navmesh)"));
		return true;
	}

	return false;
}
