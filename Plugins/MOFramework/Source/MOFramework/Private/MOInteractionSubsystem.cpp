#include "MOInteractionSubsystem.h"
#include "MOFramework.h"

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
	if (!IsValid(InteractorController))
	{
		return false;
	}

	APlayerController* PlayerController = Cast<APlayerController>(InteractorController);
	if (IsValid(PlayerController))
	{
		PlayerController->GetPlayerViewPoint(OutViewLocation, OutViewRotation);
		return true;
	}

	APawn* Pawn = InteractorController->GetPawn();
	if (IsValid(Pawn))
	{
		Pawn->GetActorEyesViewPoint(OutViewLocation, OutViewRotation);
		return true;
	}

	return false;
}

bool UMOInteractionSubsystem::HasServerLineOfSight(const FVector& ViewLocation, const AActor* InteractorPawnActor, const AActor* TargetActor) const
{
	if (!bRequireLineOfSight)
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!World || !IsValid(TargetActor))
	{
		return false;
	}

	const FVector TargetLocation = TargetActor->GetActorLocation();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MOInteractionLOS), true);
	QueryParams.bReturnPhysicalMaterial = false;

	if (IsValid(InteractorPawnActor))
	{
		QueryParams.AddIgnoredActor(InteractorPawnActor);
	}

	FHitResult HitResult;
	const bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		ViewLocation,
		TargetLocation,
		ValidationTraceChannel.GetValue(),
		QueryParams
	);

	if (!bHit)
	{
		return true;
	}

	AActor* HitActor = HitResult.GetActor();
	if (!IsValid(HitActor))
	{
		return false;
	}

	// Allow direct hit to the target.
	if (HitActor == TargetActor)
	{
		return true;
	}

	// Allow components attached to the target to count as visible.
	if (HitActor->IsAttachedTo(TargetActor))
	{
		return true;
	}

	return false;
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

	UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] FindServerInteractTarget: Found valid target '%s'"), *HitActor->GetName());
	OutTargetActor = HitActor;
	return true;
}

bool UMOInteractionSubsystem::ServerExecuteInteract(AController* InteractorController, AActor* TargetActor)
{
	UE_LOG(LogMOFramework, Error, TEXT("[MOInteract] *** ServerExecuteInteract ENTRY *** target='%s'"), *GetNameSafe(TargetActor));

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

	// Server-authoritative target selection.
	AActor* ServerTargetActor = nullptr;
	FHitResult ServerTargetHit;
	if (!FindServerInteractTarget(InteractorController, ServerTargetActor, ServerTargetHit))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] Reject: server trace found no target (client wanted '%s')"), *GetNameSafe(TargetActor));
		return false;
	}

	if (!IsValid(ServerTargetActor))
	{
		return false;
	}

	// If the client supplied a target actor, require it to match what the server found.
	// This is the spoofing hardening: client cannot request arbitrary actors.
	if (IsValid(TargetActor))
	{
		const bool bSameActor = (TargetActor == ServerTargetActor);
		const bool bAttachmentMatch =
			TargetActor->IsAttachedTo(ServerTargetActor) ||
			ServerTargetActor->IsAttachedTo(TargetActor);

		if (!bSameActor && !bAttachmentMatch)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] Reject: client target mismatch (client=%s server=%s)"),
				*GetNameSafe(TargetActor), *GetNameSafe(ServerTargetActor));
			return false;
		}
	}

	// Resolve viewpoint again for distance and LOS defense-in-depth (cheap).
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

	const float DistanceSquared = FVector::DistSquared(ViewLocation, ServerTargetActor->GetActorLocation());
	if (DistanceSquared > FMath::Square(MaximumInteractDistance))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] Reject: out of range"));
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

	// Distance check - target must be within interact range
	// Use bounds center instead of actor origin for more accurate distance (surface vs center)
	// Also add tolerance since client traces to surface but server checks to center
	FVector BoundsOrigin;
	FVector BoundsExtent;
	TargetActor->GetActorBounds(true, BoundsOrigin, BoundsExtent);

	const float DistanceSquared = FVector::DistSquared(ViewLocation, BoundsOrigin);
	const float MaxRangeWithTolerance = MaximumInteractDistance + BoundsExtent.Size();
	if (DistanceSquared > FMath::Square(MaxRangeWithTolerance))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] ServerExecuteSecondaryInteract: Target out of range (dist=%.1f, max=%.1f)"),
			FMath::Sqrt(DistanceSquared), MaxRangeWithTolerance);
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
