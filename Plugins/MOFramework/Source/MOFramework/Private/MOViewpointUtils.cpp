#include "MOViewpointUtils.h"

#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

bool UMOViewpointUtils::ResolveViewpointForController(AController* Controller, FVector& OutViewLocation, FRotator& OutViewRotation)
{
	if (!IsValid(Controller))
	{
		return false;
	}

	// Try player controller path first (most accurate for players)
	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (IsValid(PlayerController))
	{
		PlayerController->GetPlayerViewPoint(OutViewLocation, OutViewRotation);
		return true;
	}

	// Fall back to pawn eyes
	APawn* Pawn = Controller->GetPawn();
	if (IsValid(Pawn))
	{
		Pawn->GetActorEyesViewPoint(OutViewLocation, OutViewRotation);
		return true;
	}

	return false;
}

bool UMOViewpointUtils::ResolveViewpointForPlayerController(APlayerController* PlayerController, FVector& OutViewLocation, FRotator& OutViewRotation)
{
	if (!IsValid(PlayerController))
	{
		return false;
	}

	PlayerController->GetPlayerViewPoint(OutViewLocation, OutViewRotation);
	return true;
}

bool UMOViewpointUtils::ResolveViewpointForPawn(APawn* Pawn, FVector& OutViewLocation, FRotator& OutViewRotation)
{
	if (!IsValid(Pawn))
	{
		return false;
	}

	Pawn->GetActorEyesViewPoint(OutViewLocation, OutViewRotation);
	return true;
}

bool UMOViewpointUtils::HasLineOfSight(UWorld* World, const FVector& ViewLocation, const AActor* TargetActor, const FLineOfSightOptions& Options)
{
	if (!World || !IsValid(TargetActor))
	{
		return false;
	}

	const FVector TargetLocation = TargetActor->GetActorLocation();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MOViewpointLOS), true);
	QueryParams.bReturnPhysicalMaterial = false;

	if (IsValid(Options.IgnoredActor))
	{
		QueryParams.AddIgnoredActor(Options.IgnoredActor);
	}

	FHitResult HitResult;
	const bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		ViewLocation,
		TargetLocation,
		Options.TraceChannel,
		QueryParams
	);

	// No hit means clear line of sight
	if (!bHit)
	{
		return true;
	}

	AActor* HitActor = HitResult.GetActor();
	if (!IsValid(HitActor))
	{
		return false;
	}

	// Allow direct hit to the target
	if (HitActor == TargetActor)
	{
		return true;
	}

	// Allow components attached to the target to count as visible
	if (Options.bAllowAttachedHits && HitActor->IsAttachedTo(TargetActor))
	{
		return true;
	}

	return false;
}

bool UMOViewpointUtils::HasLineOfSightSimple(UWorld* World, const FVector& ViewLocation, const FVector& TargetLocation, ECollisionChannel TraceChannel, AActor* IgnoredActor)
{
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MOViewpointLOSSimple), true);
	QueryParams.bReturnPhysicalMaterial = false;

	if (IsValid(IgnoredActor))
	{
		QueryParams.AddIgnoredActor(IgnoredActor);
	}

	FHitResult HitResult;
	const bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		ViewLocation,
		TargetLocation,
		TraceChannel,
		QueryParams
	);

	// No hit means clear line of sight
	return !bHit;
}

bool UMOViewpointUtils::IsInViewCone(const FVector& ViewLocation, const FRotator& ViewRotation, const FVector& TargetLocation, float MaxAngleDegrees)
{
	// Disable if configured out of range
	if (MaxAngleDegrees <= 0.0f || MaxAngleDegrees >= 180.0f)
	{
		return true;
	}

	const FVector ToTargetDirection = (TargetLocation - ViewLocation).GetSafeNormal();
	if (ToTargetDirection.IsNearlyZero())
	{
		return true;
	}

	const FVector ViewForward = ViewRotation.Vector().GetSafeNormal();
	const float Dot = FVector::DotProduct(ViewForward, ToTargetDirection);

	const float CosThreshold = FMath::Cos(FMath::DegreesToRadians(MaxAngleDegrees));
	return Dot >= CosThreshold;
}
