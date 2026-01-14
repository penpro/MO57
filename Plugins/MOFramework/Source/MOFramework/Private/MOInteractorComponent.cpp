#include "MOInteractorComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "MOInteractionSubsystem.h"
#include "MOInteractableComponent.h"

UMOInteractorComponent::UMOInteractorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMOInteractorComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool UMOInteractorComponent::ResolveViewpoint(FVector& OutViewLocation, FRotator& OutViewRotation) const
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return false;
	}

	APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	AController* OwnerController = OwnerPawn ? OwnerPawn->GetController() : nullptr;

	APlayerController* PlayerController = Cast<APlayerController>(OwnerController);
	if (IsValid(PlayerController))
	{
		PlayerController->GetPlayerViewPoint(OutViewLocation, OutViewRotation);
		return true;
	}

	if (IsValid(OwnerPawn))
	{
		OwnerPawn->GetActorEyesViewPoint(OutViewLocation, OutViewRotation);
		return true;
	}

	return false;
}

void UMOInteractorComponent::BuildTrace(const FVector& ViewLocation, const FRotator& ViewRotation, FVector& OutTraceStart, FVector& OutTraceEnd) const
{
	const FVector ViewForwardVector = ViewRotation.Vector();
	OutTraceStart = ViewLocation + (ViewForwardVector * TraceConfig.ViewStartForwardOffset);
	OutTraceEnd = OutTraceStart + (ViewForwardVector * TraceConfig.TraceDistance);
}

bool UMOInteractorComponent::TraceForHit(const FVector& TraceStart, const FVector& TraceEnd, FHitResult& OutHitResult) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	AActor* OwnerActor = GetOwner();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MOInteractorTrace), TraceConfig.bTraceComplex);
	QueryParams.bReturnPhysicalMaterial = false;

	if (IsValid(OwnerActor))
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}

	const ECollisionChannel TraceChannel = TraceConfig.TraceChannel.GetValue();

	if (TraceConfig.TraceRadius > 0.0f)
	{
		const FCollisionShape SphereShape = FCollisionShape::MakeSphere(TraceConfig.TraceRadius);
		return World->SweepSingleByChannel(OutHitResult, TraceStart, TraceEnd, FQuat::Identity, TraceChannel, SphereShape, QueryParams);
	}

	return World->LineTraceSingleByChannel(OutHitResult, TraceStart, TraceEnd, TraceChannel, QueryParams);
}

bool UMOInteractorComponent::FindInteractTarget(AActor*& OutTargetActor, FHitResult& OutHitResult) const
{
	OutTargetActor = nullptr;
	OutHitResult = FHitResult();

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	if (!ResolveViewpoint(ViewLocation, ViewRotation))
	{
		return false;
	}

	FVector TraceStart = FVector::ZeroVector;
	FVector TraceEnd = FVector::ZeroVector;
	BuildTrace(ViewLocation, ViewRotation, TraceStart, TraceEnd);

	if (!TraceForHit(TraceStart, TraceEnd, OutHitResult))
	{
		return false;
	}

	AActor* HitActor = OutHitResult.GetActor();
	if (!IsValid(HitActor))
	{
		return false;
	}

	UMOInteractableComponent* InteractableComponent = HitActor->FindComponentByClass<UMOInteractableComponent>();
	if (!IsValid(InteractableComponent))
	{
		return false;
	}

	OutTargetActor = HitActor;
	return true;
}

bool UMOInteractorComponent::TryInteract()
{
	AActor* OwnerActor = GetOwner();
	APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	if (!IsValid(OwnerPawn))
	{
		return false;
	}

	if (!OwnerPawn->IsLocallyControlled())
	{
		return false;
	}

	AActor* TargetActor = nullptr;
	FHitResult HitResult;
	const bool bFoundTarget = FindInteractTarget(TargetActor, HitResult);

	LastTracedActor = bFoundTarget ? TargetActor : nullptr;

	if (!bFoundTarget || !IsValid(TargetActor))
	{
		return false;
	}

	ServerRequestInteract(TargetActor);
	return true;
}

void UMOInteractorComponent::ServerRequestInteract_Implementation(AActor* TargetActor)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AController* InteractorController = OwnerPawn ? OwnerPawn->GetController() : nullptr;

	if (!IsValid(InteractorController) || !IsValid(TargetActor))
	{
		return;
	}

	UMOInteractionSubsystem* InteractionSubsystem = World->GetSubsystem<UMOInteractionSubsystem>();
	if (!InteractionSubsystem)
	{
		return;
	}

	InteractionSubsystem->ServerExecuteInteract(InteractorController, TargetActor);
}
