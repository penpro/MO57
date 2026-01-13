#include "MOInteractionSubsystem.h"

#include "MOInteractableComponent.h"
#include "MOInteractionInterface.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Controller.h"

DEFINE_LOG_CATEGORY_STATIC(LogMOInteractionSubsystem, Log, All);

bool UMOInteractionSubsystem::ComputeViewTrace(APlayerController* PlayerController, FVector& OutTraceStart, FVector& OutTraceEnd) const
{
	if (!PlayerController)
	{
		return false;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	OutTraceStart = ViewLocation;
	OutTraceEnd = ViewLocation + (ViewRotation.Vector() * TraceDistance);
	return true;
}

bool UMOInteractionSubsystem::QueryInteractable(APlayerController* PlayerController, FMOInteractionHit& OutHit) const
{
	UWorld* World = GetWorld();
	if (!World || !PlayerController)
	{
		return false;
	}

	FVector TraceStart = FVector::ZeroVector;
	FVector TraceEnd = FVector::ZeroVector;
	if (!ComputeViewTrace(PlayerController, TraceStart, TraceEnd))
	{
		return false;
	}

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(MOInteractionTrace), true);
	if (APawn* Pawn = PlayerController->GetPawn())
	{
		TraceParams.AddIgnoredActor(Pawn);
	}

	FHitResult HitResult;
	const bool bHit = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, TraceChannel, TraceParams);
	if (!bHit)
	{
		return false;
	}

	AActor* HitActor = HitResult.GetActor();
	if (!HitActor)
	{
		return false;
	}

	UMOInteractableComponent* InteractableComponent = HitActor->FindComponentByClass<UMOInteractableComponent>();
	if (!InteractableComponent)
	{
		return false;
	}

	OutHit.HitActor = HitActor;
	OutHit.InteractableComponent = InteractableComponent;
	OutHit.HitResult = HitResult;
	return true;
}

bool UMOInteractionSubsystem::TryInteract(APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return false;
	}

	FMOInteractionHit InteractionHit;
	if (!QueryInteractable(PlayerController, InteractionHit))
	{
		return false;
	}

	// Standalone or server can execute directly.
	if (PlayerController->HasAuthority())
	{
		return ServerExecuteInteract(PlayerController, InteractionHit.HitActor);
	}

	// Client must route through an owning actor via interface (PlayerController is ideal).
	if (!PlayerController->GetClass()->ImplementsInterface(UMOInteractionInterface::StaticClass()))
	{
		UE_LOG(LogMOInteractionSubsystem, Warning,
			TEXT("TryInteract: PlayerController does not implement MOInteractionInterface. %s"),
			*PlayerController->GetName());
		return false;
	}

	return IMOInteractionInterface::Execute_RequestServerInteract(PlayerController, InteractionHit.HitActor);
}

bool UMOInteractionSubsystem::ServerExecuteInteract(AController* InteractorController, AActor* TargetActor)
{
	UWorld* World = GetWorld();
	if (!World || !InteractorController || !TargetActor)
	{
		return false;
	}

	if (World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogMOInteractionSubsystem, Warning, TEXT("ServerExecuteInteract called on client world. Ignored."));
		return false;
	}

	UMOInteractableComponent* InteractableComponent = TargetActor->FindComponentByClass<UMOInteractableComponent>();
	if (!InteractableComponent)
	{
		return false;
	}

	if (!InteractableComponent->CanInteract(InteractorController))
	{
		return false;
	}

	return InteractableComponent->ExecuteInteraction(InteractorController);
}
