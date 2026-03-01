#include "BTTask_CreatureWander.h"
#include "MOFramework.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"

UBTTask_CreatureWander::UBTTask_CreatureWander()
{
	NodeName = TEXT("Creature Wander (Smooth)");
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

	// Initialize memory
	FBTWanderMemory* Memory = reinterpret_cast<FBTWanderMemory*>(NodeMemory);
	Memory->bInitialized = false;
	Memory->TimeRemaining = FMath::RandRange(MinWanderDuration, MaxWanderDuration);
	Memory->NextHeadingAdjustTime = 0.f; // Adjust immediately on first tick
	Memory->LastPosition = OwnerPawn->GetActorLocation();
	Memory->StuckTime = 0.f;
	Memory->LastStuckCheckTime = 0.f;

	// Initialize heading from current direction
	InitializeHeading(OwnerPawn, Memory);

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

	FBTWanderMemory* Memory = reinterpret_cast<FBTWanderMemory*>(NodeMemory);

	// Update time remaining
	Memory->TimeRemaining -= DeltaSeconds;
	if (Memory->TimeRemaining <= 0.f)
	{
		// Wander session complete
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	// Water avoidance - check if we're in or heading toward water (z < 0)
	FVector CurrentLocation = OwnerPawn->GetActorLocation();
	const float WaterLevel = 0.f;
	const float WaterAvoidanceMargin = 200.f; // Start avoiding before hitting water

	bool bNeedsWaterAvoidance = false;
	if (CurrentLocation.Z < WaterLevel + WaterAvoidanceMargin)
	{
		// We're close to or in water - need to head uphill (positive Z gradient)
		bNeedsWaterAvoidance = true;

		// Force heading adjustment toward higher ground
		// Find direction that leads uphill by checking multiple directions
		FVector BestDirection = -Memory->CurrentHeading; // Default: turn around
		float BestZGain = -FLT_MAX;

		for (int32 i = 0; i < 8; ++i)
		{
			float Angle = (i / 8.f) * 360.f;
			FVector TestDir = FRotator(0.f, Angle, 0.f).Vector();
			FVector TestLocation = CurrentLocation + TestDir * 500.f;

			// Trace to find ground height in that direction
			FHitResult Hit;
			FVector TraceStart = TestLocation + FVector(0, 0, 1000.f);
			FVector TraceEnd = TestLocation - FVector(0, 0, 2000.f);

			if (OwnerPawn->GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility))
			{
				float ZGain = Hit.ImpactPoint.Z - CurrentLocation.Z;
				if (ZGain > BestZGain && Hit.ImpactPoint.Z > WaterLevel)
				{
					BestZGain = ZGain;
					BestDirection = TestDir;
				}
			}
		}

		// Override heading to escape water
		Memory->CurrentHeading = BestDirection;
		Memory->CurrentHeading.Z = 0.f;
		Memory->CurrentHeading.Normalize();
	}

	// Stuck detection - check every 0.5 seconds to avoid overhead
	Memory->LastStuckCheckTime += DeltaSeconds;
	if (Memory->LastStuckCheckTime >= 0.5f)
	{
		float DistanceMoved = FVector::Dist(CurrentLocation, Memory->LastPosition);

		if (DistanceMoved < StuckMinMovement)
		{
			// Not moving much - accumulate stuck time
			Memory->StuckTime += Memory->LastStuckCheckTime;

			if (Memory->StuckTime >= StuckThresholdTime)
			{
				// We're stuck - reverse heading and add randomness
				FRotator HeadingRotation = Memory->CurrentHeading.Rotation();
				HeadingRotation.Yaw += 180.f + FMath::RandRange(-60.f, 60.f);
				Memory->CurrentHeading = HeadingRotation.Vector();
				Memory->CurrentHeading.Z = 0.f;
				Memory->CurrentHeading.Normalize();

				Memory->StuckTime = 0.f;
				Memory->NextHeadingAdjustTime = FMath::RandRange(MinHeadingAdjustInterval, MaxHeadingAdjustInterval);

				UE_LOG(LogMOFramework, Verbose, TEXT("[BTWander] %s stuck, reversing heading"), *OwnerPawn->GetName());
			}
		}
		else
		{
			// Moving normally - reset stuck detection
			Memory->StuckTime = 0.f;
		}

		Memory->LastPosition = CurrentLocation;
		Memory->LastStuckCheckTime = 0.f;
	}

	// Check if we need a new heading adjustment (skip if doing water avoidance)
	if (!bNeedsWaterAvoidance)
	{
		Memory->NextHeadingAdjustTime -= DeltaSeconds;
		if (Memory->NextHeadingAdjustTime <= 0.f)
		{
			FVector HomeLocation = GetHomeLocation(OwnerComp, OwnerPawn);
			PickNewHeadingAdjustment(OwnerPawn, Memory, HomeLocation);
		}
	}

	// Get current forward direction
	FVector CurrentForward = OwnerPawn->GetActorForwardVector();
	CurrentForward.Z = 0.f;
	CurrentForward.Normalize();

	// Smoothly interpolate toward desired heading
	FVector DesiredHeading = Memory->CurrentHeading;
	DesiredHeading.Z = 0.f;
	DesiredHeading.Normalize();

	// Calculate rotation needed
	FRotator CurrentRotation = CurrentForward.Rotation();
	FRotator DesiredRotation = DesiredHeading.Rotation();

	// Interpolate rotation (faster when avoiding water)
	float InterpSpeed = bNeedsWaterAvoidance ? HeadingInterpSpeed * 3.f : HeadingInterpSpeed;
	float InterpAlpha = FMath::Clamp(InterpSpeed * DeltaSeconds / 180.f, 0.f, 1.f);
	FRotator NewRotation = FMath::Lerp(CurrentRotation, DesiredRotation, InterpAlpha);

	// Calculate new heading direction from interpolated rotation
	FVector NewHeading = NewRotation.Vector();
	NewHeading.Z = 0.f;
	NewHeading.Normalize();

	// Apply movement input (keep walking forward while turning)
	OwnerPawn->AddMovementInput(NewHeading, WanderSpeedMultiplier);

	// Update the pawn's rotation to face movement direction
	// Note: bOrientRotationToMovement should handle this, but we ensure smooth rotation
	if (ACharacter* Character = Cast<ACharacter>(OwnerPawn))
	{
		// The CharacterMovementComponent with bOrientRotationToMovement will handle rotation
		// We just need to provide movement input
	}
	else
	{
		// For non-Character pawns, manually set rotation
		OwnerPawn->SetActorRotation(NewRotation);
	}
}

FString UBTTask_CreatureWander::GetStaticDescription() const
{
	return FString::Printf(TEXT("Wander smoothly for %.0f-%.0f seconds"), MinWanderDuration, MaxWanderDuration);
}

void UBTTask_CreatureWander::InitializeHeading(APawn* Pawn, FBTWanderMemory* Memory) const
{
	if (!Pawn)
	{
		Memory->CurrentHeading = FVector::ForwardVector;
		return;
	}

	// Start with current facing direction
	Memory->CurrentHeading = Pawn->GetActorForwardVector();
	Memory->CurrentHeading.Z = 0.f;

	if (Memory->CurrentHeading.IsNearlyZero())
	{
		Memory->CurrentHeading = FVector::ForwardVector;
	}
	else
	{
		Memory->CurrentHeading.Normalize();
	}

	Memory->CurrentHeadingDelta = 0.f;
	Memory->bInitialized = true;
}

void UBTTask_CreatureWander::PickNewHeadingAdjustment(APawn* Pawn, FBTWanderMemory* Memory, const FVector& HomeLocation) const
{
	if (!Pawn)
	{
		return;
	}

	// Set next adjustment time
	Memory->NextHeadingAdjustTime = FMath::RandRange(MinHeadingAdjustInterval, MaxHeadingAdjustInterval);

	FVector CurrentLocation = Pawn->GetActorLocation();
	const float WaterLevel = 0.f;
	const float WaterAvoidanceMargin = 300.f;

	float HeadingChange = 0.f;
	bool bNeedsWaterAvoidance = false;

	// Check if current heading would lead toward water
	FVector ProposedLocation = CurrentLocation + Memory->CurrentHeading * 500.f;

	// Trace to find ground height in proposed direction
	UWorld* World = Pawn->GetWorld();
	if (World)
	{
		FHitResult Hit;
		FVector TraceStart = ProposedLocation + FVector(0, 0, 1000.f);
		FVector TraceEnd = ProposedLocation - FVector(0, 0, 2000.f);

		if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility))
		{
			if (Hit.ImpactPoint.Z < WaterLevel)
			{
				// Proposed direction leads to water - need to turn away
				bNeedsWaterAvoidance = true;
			}
		}
	}

	// Check if we're too far from home - if so, bias toward home
	float DistanceFromHome = FVector::Dist2D(CurrentLocation, HomeLocation);

	if (bNeedsWaterAvoidance)
	{
		// Turn away from water - reverse heading with some randomness
		HeadingChange = 180.f + FMath::RandRange(-45.f, 45.f);
	}
	else if (MaxDistanceFromHome > 0.f && DistanceFromHome > MaxDistanceFromHome * 0.7f)
	{
		// Getting far from home - turn back toward it
		FVector ToHome = (HomeLocation - CurrentLocation).GetSafeNormal2D();
		FVector CurrentHeading2D = Memory->CurrentHeading.GetSafeNormal2D();

		// Calculate angle to home
		float DotToHome = FVector::DotProduct(CurrentHeading2D, ToHome);
		FVector CrossToHome = FVector::CrossProduct(CurrentHeading2D, ToHome);
		float AngleToHome = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotToHome, -1.f, 1.f)));

		// Determine turn direction
		float TurnSign = (CrossToHome.Z >= 0.f) ? 1.f : -1.f;

		// Turn toward home (stronger correction as we get farther)
		float CorrectionStrength = FMath::GetMappedRangeValueClamped(
			FVector2D(MaxDistanceFromHome * 0.7f, MaxDistanceFromHome),
			FVector2D(0.3f, 1.0f),
			DistanceFromHome
		);

		HeadingChange = TurnSign * FMath::Min(AngleToHome, MaxHeadingChange * 2.f) * CorrectionStrength;
	}
	else
	{
		// Normal wandering - random heading adjustment
		HeadingChange = FMath::RandRange(-MaxHeadingChange, MaxHeadingChange);
	}

	// Apply heading change
	FRotator HeadingRotation = Memory->CurrentHeading.Rotation();
	HeadingRotation.Yaw += HeadingChange;
	Memory->CurrentHeading = HeadingRotation.Vector();
	Memory->CurrentHeading.Z = 0.f;
	Memory->CurrentHeading.Normalize();
}

FVector UBTTask_CreatureWander::GetHomeLocation(UBehaviorTreeComponent& OwnerComp, APawn* Pawn) const
{
	// Try to get from blackboard
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (BlackboardComp && HomeLocationKey.IsSet())
	{
		FVector HomeLocation = BlackboardComp->GetValueAsVector(HomeLocationKey.SelectedKeyName);
		if (!HomeLocation.IsZero())
		{
			return HomeLocation;
		}
	}

	// Fall back to pawn's current location
	if (Pawn)
	{
		return Pawn->GetActorLocation();
	}

	return FVector::ZeroVector;
}
