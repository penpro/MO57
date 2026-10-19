#include "MOInteractionSubsystem.h"
#include "MOInteractorComponent.h"
#include "MOFramework.h"
#include "MOViewpointUtils.h"

#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "MOInteractableComponent.h"

UMOInteractionSubsystem::UMOInteractionSubsystem()
{
}

bool UMOInteractionSubsystem::ResolveServerViewpoint(AController* InteractorController, FVector& OutViewLocation, FRotator& OutViewRotation) const
{
	// Delegate to shared utility
	return UMOViewpointUtils::ResolveViewpointForController(InteractorController, OutViewLocation, OutViewRotation);
}

bool UMOInteractionSubsystem::HasServerLineOfSight(const FVector& ViewLocation, const AActor* InteractorPawnActor, const AActor* TargetActor) const
{
	if (!bRequireLineOfSight)
	{
		return true;
	}

	// Delegate to shared utility with options
	UMOViewpointUtils::FLineOfSightOptions Options;
	Options.TraceChannel = ValidationTraceChannel.GetValue();
	Options.bAllowAttachedHits = true;
	Options.IgnoredActor = InteractorPawnActor;
	Options.SeeThrough = &UMOInteractorComponent::IsSeeThroughForInteraction;   // grass between the eye and a stick lying in it does not hide the stick

	return UMOViewpointUtils::HasLineOfSight(GetWorld(), ViewLocation, TargetActor, Options);
}

FVector UMOInteractionSubsystem::ComputeAimPoint(const AActor* TargetActor) const
{
	if (!IsValid(TargetActor))
	{
		return FVector::ZeroVector;
	}

	// Prefer bounds origin so large actors are easier to interact with.
	FVector BoundsOrigin = FVector::ZeroVector;
	FVector BoundsExtent = FVector::ZeroVector;
	TargetActor->GetActorBounds(true, BoundsOrigin, BoundsExtent);

	// If bounds are degenerate (rare), fall back to actor location.
	if (BoundsExtent.IsNearlyZero())
	{
		return TargetActor->GetActorLocation();
	}

	return BoundsOrigin;
}

bool UMOInteractionSubsystem::PassesViewCone(const FVector& ViewLocation, const FRotator& ViewRotation, const AActor* TargetActor) const
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	// Disable if configured out of range.
	if (MaxInteractAngleDegrees <= 0.0f || MaxInteractAngleDegrees >= 180.0f)
	{
		return true;
	}

	const FVector AimPoint = ComputeAimPoint(TargetActor);
	const FVector ToTargetDirection = (AimPoint - ViewLocation).GetSafeNormal();
	if (ToTargetDirection.IsNearlyZero())
	{
		return true;
	}

	const FVector ViewForward = ViewRotation.Vector().GetSafeNormal();
	const float Dot = FVector::DotProduct(ViewForward, ToTargetDirection);

	const float CosThreshold = FMath::Cos(FMath::DegreesToRadians(MaxInteractAngleDegrees));
	return Dot >= CosThreshold;
}

bool UMOInteractionSubsystem::IsWithinInteractReach(const FVector& ViewLocation, const FVector& ViewForward, const FVector& BoundsOrigin,
	const FVector& BoundsExtent, float MaxDistance, float MaxAngleDegrees)
{
	// The client's sweep stops at the target's surface; the centre may lie up to the half-diagonal further.
	const float MaxRangeWithTolerance = MaxDistance + BoundsExtent.Size();
	if (FVector::DistSquared(ViewLocation, BoundsOrigin) > FMath::Square(MaxRangeWithTolerance))
	{
		return false;
	}

	if (MaxAngleDegrees <= 0.0f || MaxAngleDegrees >= 180.0f)
	{
		return true;
	}

	const FVector ToTarget = (BoundsOrigin - ViewLocation).GetSafeNormal();
	if (ToTarget.IsNearlyZero())
	{
		return true;
	}
	return FVector::DotProduct(ViewForward.GetSafeNormal(), ToTarget) >= FMath::Cos(FMath::DegreesToRadians(MaxAngleDegrees));
}

bool UMOInteractionSubsystem::FindServerInteractTarget(AController* InteractorController, AActor*& OutTargetActor, FHitResult& OutHit) const
{
	OutTargetActor = nullptr;
	OutHit = FHitResult();

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] FindServerInteractTarget: No world"));
		return false;
	}

	if (!IsValid(InteractorController))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] FindServerInteractTarget: Invalid controller"));
		return false;
	}

	APawn* InteractorPawn = InteractorController->GetPawn();
	if (!IsValid(InteractorPawn))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] FindServerInteractTarget: No pawn"));
		return false;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	if (!ResolveServerViewpoint(InteractorController, ViewLocation, ViewRotation))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] FindServerInteractTarget: Failed to resolve viewpoint"));
		return false;
	}

	// Optional hardening: clamp viewpoint offset.
	if (MaxViewpointDistanceFromPawn > 0.0f)
	{
		const float ViewToPawnDistanceSquared = FVector::DistSquared(ViewLocation, InteractorPawn->GetActorLocation());
		if (ViewToPawnDistanceSquared > FMath::Square(MaxViewpointDistanceFromPawn))
		{
			InteractorPawn->GetActorEyesViewPoint(ViewLocation, ViewRotation);
		}
	}

	const FVector ViewForwardVector = ViewRotation.Vector();
	const FVector TraceStart = ViewLocation + (ViewForwardVector * ServerTraceForwardOffset);
	const FVector TraceEnd = TraceStart + (ViewForwardVector * MaximumInteractDistance);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MOInteractionServerTrace), bServerTraceComplex);
	QueryParams.bReturnPhysicalMaterial = false;
	QueryParams.AddIgnoredActor(InteractorPawn);

	const ECollisionChannel TraceChannel = InteractTraceChannel.GetValue();

	UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] Trace: Start=%s End=%s Channel=%d Radius=%.1f"),
		*TraceStart.ToString(), *TraceEnd.ToString(), (int32)TraceChannel, ServerTraceRadius);

	bool bHit = false;
	if (ServerTraceRadius > 0.0f)
	{
		const FCollisionShape SphereShape = FCollisionShape::MakeSphere(ServerTraceRadius);
		bHit = World->SweepSingleByChannel(OutHit, TraceStart, TraceEnd, FQuat::Identity, TraceChannel, SphereShape, QueryParams);
	}
	else
	{
		bHit = World->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, TraceChannel, QueryParams);
	}

	if (bHit)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] Trace hit: Actor=%s Location=%s Distance=%.1f"),
			*GetNameSafe(OutHit.GetActor()), *OutHit.Location.ToString(), OutHit.Distance);
	}

	if (!bHit)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] FindServerInteractTarget: Server trace found no hit (channel=%d)"), (int32)TraceChannel);
		return false;
	}

	AActor* HitActor = OutHit.GetActor();
	if (!IsValid(HitActor))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] FindServerInteractTarget: Hit but no valid actor"));
		return false;
	}

	UMOInteractableComponent* InteractableComponent = HitActor->FindComponentByClass<UMOInteractableComponent>();
	if (!IsValid(InteractableComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] FindServerInteractTarget: Hit actor '%s' has no InteractableComponent"), *HitActor->GetName());
		return false;
	}

	if (!PassesViewCone(ViewLocation, ViewRotation, HitActor))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] FindServerInteractTarget: Hit actor '%s' failed view cone check"), *HitActor->GetName());
		return false;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOInteract] FindServerInteractTarget: Found valid target '%s'"), *HitActor->GetName());
	OutTargetActor = HitActor;
	return true;
}

bool UMOInteractionSubsystem::ServerExecuteInteract(AController* InteractorController, AActor* TargetActor)
{
	// Routine (every E press): Log, not Error -- until Oct 19 this January debug line put an Error in every log and bug report for each interaction.
	UE_LOG(LogMOFramework, Log, TEXT("[MOInteract] ServerExecuteInteract target='%s'"), *GetNameSafe(TargetActor));

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] ServerExecuteInteract: No world"));
		return false;
	}

	// Must run on server authority.
	if (World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] ServerExecuteInteract: Running on client, rejected"));
		return false;
	}

	if (!IsValid(InteractorController))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] ServerExecuteInteract: Invalid controller"));
		return false;
	}

	APawn* InteractorPawn = InteractorController->GetPawn();
	if (!IsValid(InteractorPawn))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] ServerExecuteInteract: No pawn for controller"));
		return false;
	}

	// Rate limit per controller.
	const double CurrentTimeSeconds = World->GetTimeSeconds();
	const FObjectKey ControllerKey(InteractorController);

	if (const double* LastTimeSeconds = LastInteractTimeSeconds.Find(ControllerKey))
	{
		if ((CurrentTimeSeconds - *LastTimeSeconds) < MinimumSecondsBetweenInteract)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] Rate limited"));
			return false;
		}
	}
	LastInteractTimeSeconds.Add(ControllerKey, CurrentTimeSeconds);

	// The client names the actor its focus sweep hit; the server checks that claim (reach, view cone, line of sight)
	// instead of re-deriving the target with a sweep of its own. The second, separately tuned sweep (12 cm vs the
	// client's 30 cm radius, and a lagged view for a remote client) refused E on low objects the crosshair covered --
	// a freshly built campfire logged "server trace found no target". A spoofed claim still has to be in reach, in
	// front of the player and in sight. With no claimed target (Blueprint callers) the server sweep picks one.
	AActor* ServerTargetActor = TargetActor;
	if (!IsValid(ServerTargetActor))
	{
		FHitResult ServerTargetHit;
		if (!FindServerInteractTarget(InteractorController, ServerTargetActor, ServerTargetHit) || !IsValid(ServerTargetActor))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] Reject: no target named and the server trace found none"));
			return false;
		}
	}

	// Resolve viewpoint for the reach and LOS checks.
	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	if (!ResolveServerViewpoint(InteractorController, ViewLocation, ViewRotation))
	{
		return false;
	}

	// Optional viewpoint clamp (keep consistent with targeting).
	if (MaxViewpointDistanceFromPawn > 0.0f)
	{
		const float ViewToPawnDistanceSquared = FVector::DistSquared(ViewLocation, InteractorPawn->GetActorLocation());
		if (ViewToPawnDistanceSquared > FMath::Square(MaxViewpointDistanceFromPawn))
		{
			InteractorPawn->GetActorEyesViewPoint(ViewLocation, ViewRotation);
		}
	}

	FVector BoundsOrigin = FVector::ZeroVector;
	FVector BoundsExtent = FVector::ZeroVector;
	ServerTargetActor->GetActorBounds(true, BoundsOrigin, BoundsExtent);
	if (BoundsExtent.IsNearlyZero())
	{
		BoundsOrigin = ServerTargetActor->GetActorLocation();
	}
	if (!IsWithinInteractReach(ViewLocation, ViewRotation.Vector(), BoundsOrigin, BoundsExtent, MaximumInteractDistance, MaxInteractAngleDegrees))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] Reject: '%s' out of reach (dist=%.1f, max=%.1f, or outside the %.0f deg view cone) view=%s pawn=%s target=%s"),
			*ServerTargetActor->GetName(), FVector::Dist(ViewLocation, BoundsOrigin), MaximumInteractDistance + BoundsExtent.Size(), MaxInteractAngleDegrees,
			*ViewLocation.ToCompactString(), *InteractorPawn->GetActorLocation().ToCompactString(), *BoundsOrigin.ToCompactString());
		return false;
	}

	if (!HasServerLineOfSight(ViewLocation, InteractorPawn, ServerTargetActor))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] Reject: no LOS"));
		return false;
	}

	UMOInteractableComponent* InteractableComponent = ServerTargetActor->FindComponentByClass<UMOInteractableComponent>();
	if (!IsValid(InteractableComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] Reject: '%s' has no InteractableComponent"), *ServerTargetActor->GetName());
		return false;
	}

	return InteractableComponent->ServerInteract(InteractorController);
}

bool UMOInteractionSubsystem::ServerExecuteSecondaryInteract(AController* InteractorController, AActor* TargetActor)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOInteract] ServerExecuteSecondaryInteract target='%s'"), *GetNameSafe(TargetActor));

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Must run on server authority.
	if (World->GetNetMode() == NM_Client)
	{
		return false;
	}

	if (!IsValid(InteractorController))
	{
		return false;
	}

	APawn* InteractorPawn = InteractorController->GetPawn();
	if (!IsValid(InteractorPawn))
	{
		return false;
	}

	// Secondary interact (context menus) uses lighter validation than primary interact.
	// We trust the client-supplied target if it's valid, in range, and has an interactable component.
	// This avoids issues with trace blocking from floors/walls.

	if (!IsValid(TargetActor))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] ServerExecuteSecondaryInteract: Invalid target actor"));
		return false;
	}

	// Rate limit per controller (shared with primary interact).
	const double CurrentTimeSeconds = World->GetTimeSeconds();
	const FObjectKey ControllerKey(InteractorController);

	if (const double* LastTimeSeconds = LastInteractTimeSeconds.Find(ControllerKey))
	{
		if ((CurrentTimeSeconds - *LastTimeSeconds) < MinimumSecondsBetweenInteract)
		{
			return false;
		}
	}
	LastInteractTimeSeconds.Add(ControllerKey, CurrentTimeSeconds);

	// Resolve viewpoint for distance check.
	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	if (!ResolveServerViewpoint(InteractorController, ViewLocation, ViewRotation))
	{
		return false;
	}

	// Distance check - the same reach rule as primary interact, without the view cone.
	FVector BoundsOrigin;
	FVector BoundsExtent;
	TargetActor->GetActorBounds(true, BoundsOrigin, BoundsExtent);

	if (!IsWithinInteractReach(ViewLocation, ViewRotation.Vector(), BoundsOrigin, BoundsExtent, MaximumInteractDistance, /*MaxAngleDegrees=*/0.0f))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] ServerExecuteSecondaryInteract: Target out of range (dist=%.1f, max=%.1f)"),
			FVector::Dist(ViewLocation, BoundsOrigin), MaximumInteractDistance + BoundsExtent.Size());
		return false;
	}

	// Verify target has an interactable component
	UMOInteractableComponent* InteractableComponent = TargetActor->FindComponentByClass<UMOInteractableComponent>();
	if (!IsValid(InteractableComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] ServerExecuteSecondaryInteract: Target '%s' has no InteractableComponent"), *TargetActor->GetName());
		return false;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOInteract] ServerExecuteSecondaryInteract: Executing on '%s'"), *TargetActor->GetName());
	return InteractableComponent->ServerSecondaryInteract(InteractorController);
}
