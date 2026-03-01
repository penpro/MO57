#include "BTTask_FleeFromThreat.h"
#include "MOFramework.h"
#include "MOCreature.h"
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

	// Get threat from blackboard
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	AActor* ThreatActor = Cast<AActor>(BlackboardComp->GetValueAsObject(ThreatActorKey.SelectedKeyName));
	if (!ThreatActor)
	{
		// No threat, nothing to flee from - this is normal, not an error
		return EBTNodeResult::Failed;
	}

	UE_LOG(LogMOFramework, Verbose, TEXT("[BTTask_Flee] %s fleeing from %s"),
		*OwnerPawn->GetName(), *ThreatActor->GetName());

	// Find flee location
	FVector FleeLocation;
	if (!FindFleeLocation(OwnerPawn, ThreatActor, FleeLocation))
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[BTTask_Flee] %s: Could not find flee location"),
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
	MyMemory->LastPosition = OwnerPawn->GetActorLocation();
	MyMemory->StuckTime = 0.f;
	MyMemory->RecoveryAttempts = 0;
	MyMemory->bIsRecovering = false;
	MyMemory->RecoveryTime = 0.f;
	MyMemory->RecoveryDirection = FVector::ZeroVector;

	if (MoveResult.Code == EPathFollowingRequestResult::Failed)
	{
		// MoveTo failed - use direct movement
		MyMemory->bUseDirectMovement = true;
	}
	else
	{
		MyMemory->bUseDirectMovement = false;
	}

	// Set run speed for fleeing
	if (ACharacter* Character = Cast<ACharacter>(OwnerPawn))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			MyMemory->OriginalWalkSpeed = Movement->MaxWalkSpeed;

			// Use creature's RunSpeed if available, otherwise use 1.5x walk speed
			float FleeSpeed = MyMemory->OriginalWalkSpeed * 1.5f;
			if (AMOCreature* Creature = Cast<AMOCreature>(OwnerPawn))
			{
				FleeSpeed = Creature->GetRunSpeed();
				UE_LOG(LogMOFramework, Verbose, TEXT("[BTTask_Flee] Using creature RunSpeed: %.1f"), FleeSpeed);
			}

			Movement->MaxWalkSpeed = FleeSpeed;
		}
	}

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

	// Get threat for stuck recovery
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	AActor* ThreatActor = BlackboardComp ? Cast<AActor>(BlackboardComp->GetValueAsObject(ThreatActorKey.SelectedKeyName)) : nullptr;

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

	// Handle stuck detection and recovery
	if (HandleStuckRecovery(OwnerPawn, ThreatActor, MyMemory, DeltaSeconds))
	{
		// If we've exceeded max recovery attempts, pick a new flee location
		if (MyMemory->RecoveryAttempts >= MaxRecoveryAttempts && ThreatActor)
		{
			FVector NewFleeLocation;
			if (FindFleeLocation(OwnerPawn, ThreatActor, NewFleeLocation))
			{
				MyMemory->TargetLocation = NewFleeLocation;
				MyMemory->RecoveryAttempts = 0;
				MyMemory->bIsRecovering = false;

				if (BlackboardComp)
				{
					BlackboardComp->SetValueAsVector(FleeDestinationKey.SelectedKeyName, NewFleeLocation);
				}

				UE_LOG(LogMOFramework, Log, TEXT("[BTTask_Flee] Picked new flee location after %d recovery attempts"), MaxRecoveryAttempts);
			}
		}
		return; // In recovery mode
	}

	if (MyMemory->bUseDirectMovement)
	{
		// Direct movement fallback - move toward target using AddMovementInput
		FVector CurrentLocation = OwnerPawn->GetActorLocation();
		FVector Direction = (MyMemory->TargetLocation - CurrentLocation).GetSafeNormal();
		float DistanceToTarget = FVector::Dist2D(CurrentLocation, MyMemory->TargetLocation);

		// Water avoidance - don't move toward water
		const float WaterLevel = 0.f;
		if (MyMemory->TargetLocation.Z < WaterLevel || CurrentLocation.Z < WaterLevel + 100.f)
		{
			// Target is in water or we're in water - abort and try different path
			RestoreSpeedAndFinish(EBTNodeResult::Failed);
			return;
		}

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

bool UBTTask_FleeFromThreat::HandleStuckRecovery(APawn* Pawn, AActor* Threat, FBTFleeMemory* Memory, float DeltaSeconds)
{
	if (!Pawn)
	{
		return false;
	}

	FVector CurrentPosition = Pawn->GetActorLocation();

	// If currently in recovery mode, continue moving sideways
	if (Memory->bIsRecovering)
	{
		Memory->RecoveryTime += DeltaSeconds;

		if (Memory->RecoveryTime >= RecoveryDuration)
		{
			// Recovery complete
			Memory->bIsRecovering = false;
			Memory->StuckTime = 0.f;
			Memory->LastPosition = CurrentPosition;
			Memory->RecoveryAttempts++;

			UE_LOG(LogMOFramework, Verbose, TEXT("[BTTask_Flee] Recovery attempt %d complete"), Memory->RecoveryAttempts);
			return false;
		}

		// Continue moving in recovery direction (sideways)
		Pawn->AddMovementInput(Memory->RecoveryDirection, 1.0f);

		// Face recovery direction
		FRotator TargetRotation = Memory->RecoveryDirection.Rotation();
		Pawn->SetActorRotation(FMath::RInterpTo(Pawn->GetActorRotation(), TargetRotation, DeltaSeconds, 5.0f));

		return true;
	}

	// Check if we've moved enough since last check
	float DistanceMoved = FVector::Dist(CurrentPosition, Memory->LastPosition);

	if (DistanceMoved < StuckMinMovement)
	{
		// Not moving much - accumulate stuck time
		Memory->StuckTime += DeltaSeconds;

		if (Memory->StuckTime >= StuckThresholdTime)
		{
			// We're stuck - start recovery
			Memory->bIsRecovering = true;
			Memory->RecoveryTime = 0.f;

			// Calculate recovery direction - move sideways (perpendicular to flee direction)
			FVector FleeDirection = (Memory->TargetLocation - CurrentPosition).GetSafeNormal();
			float SideSign = FMath::RandBool() ? 1.f : -1.f;
			Memory->RecoveryDirection = FVector::CrossProduct(FleeDirection, FVector::UpVector) * SideSign;
			Memory->RecoveryDirection.Z = 0.f;
			Memory->RecoveryDirection.Normalize();

			UE_LOG(LogMOFramework, Log, TEXT("[BTTask_Flee] %s stuck while fleeing, moving sideways"), *Pawn->GetName());

			// Stop current movement
			if (AAIController* AIController = Cast<AAIController>(Pawn->GetController()))
			{
				AIController->StopMovement();
			}

			return true;
		}
	}
	else
	{
		// Moving normally - reset stuck detection
		Memory->StuckTime = 0.f;
		Memory->LastPosition = CurrentPosition;
	}

	return false;
}

bool UBTTask_FleeFromThreat::FindFleeLocation(APawn* OwnerPawn, AActor* ThreatActor, FVector& OutLocation) const
{
	if (!OwnerPawn || !ThreatActor)
	{
		return false;
	}

	FVector MyLocation = OwnerPawn->GetActorLocation();
	FVector ThreatLocation = ThreatActor->GetActorLocation();

	// Water avoidance constants
	const float WaterLevel = 0.f;

	// Direction away from threat
	FVector AwayDirection = (MyLocation - ThreatLocation).GetSafeNormal();

	// If we're currently in/near water, bias flee direction uphill
	if (MyLocation.Z < WaterLevel + 100.f)
	{
		// Add uphill bias - prefer fleeing toward higher ground
		AwayDirection.Z = 0.5f; // Bias upward
		AwayDirection.Normalize();
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(OwnerPawn->GetWorld());
	UWorld* World = OwnerPawn->GetWorld();

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

		// Trace to find actual ground height at target
		FHitResult Hit;
		FVector TraceStart = TargetLocation + FVector(0, 0, 1000.f);
		FVector TraceEnd = TargetLocation - FVector(0, 0, 2000.f);

		if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility))
		{
			TargetLocation = Hit.ImpactPoint + FVector(0, 0, 50.f); // Slightly above ground
		}

		// Reject locations below water level
		if (TargetLocation.Z < WaterLevel)
		{
			continue; // Try another direction
		}

		// Try to project to navigation mesh if available
		if (NavSys)
		{
			FNavLocation NavLocation;
			if (NavSys->ProjectPointToNavigation(TargetLocation, NavLocation, FVector(500.f, 500.f, 500.f)))
			{
				// Reject nav locations below water level
				if (NavLocation.Location.Z < WaterLevel)
				{
					continue;
				}

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
			return true;
		}
	}

	return false;
}
