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
		UE_LOG(LogTemp, Warning, TEXT("[MOInteractor] FindInteractTarget: Failed to resolve viewpoint"));
		return false;
	}

	FVector TraceStart = FVector::ZeroVector;
	FVector TraceEnd = FVector::ZeroVector;
	BuildTrace(ViewLocation, ViewRotation, TraceStart, TraceEnd);

	if (!TraceForHit(TraceStart, TraceEnd, OutHitResult))
	{
		UE_LOG(LogTemp, Log, TEXT("[MOInteractor] FindInteractTarget: No hit"));
		return false;
	}

	AActor* HitActor = OutHitResult.GetActor();
	if (!IsValid(HitActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInteractor] FindInteractTarget: Hit but no valid actor"));
		return false;
	}

	UMOInteractableComponent* InteractableComponent = HitActor->FindComponentByClass<UMOInteractableComponent>();
	if (!IsValid(InteractableComponent))
	{
		UE_LOG(LogTemp, Log, TEXT("[MOInteractor] FindInteractTarget: Hit actor '%s' has no InteractableComponent"), *HitActor->GetName());
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[MOInteractor] FindInteractTarget: Found target '%s'"), *HitActor->GetName());
	OutTargetActor = HitActor;
	return true;
}

bool UMOInteractorComponent::TryInteract()
{
	UE_LOG(LogTemp, Warning, TEXT("[MOInteractor] TryInteract called"));

	AActor* OwnerActor = GetOwner();
	APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	if (!IsValid(OwnerPawn))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInteractor] TryInteract: Owner is not a valid pawn"));
		return false;
	}

	if (!OwnerPawn->IsLocallyControlled())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInteractor] TryInteract: Pawn is not locally controlled"));
		return false;
	}

	AActor* TargetActor = nullptr;
	FHitResult HitResult;
	const bool bFoundTarget = FindInteractTarget(TargetActor, HitResult);

	LastTracedActor = bFoundTarget ? TargetActor : nullptr;

	if (!bFoundTarget || !IsValid(TargetActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInteractor] TryInteract: No valid target found"));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[MOInteractor] TryInteract: Sending ServerRequestInteract for '%s'"), *TargetActor->GetName());
	ServerRequestInteract(TargetActor);
	return true;
}

void UMOInteractorComponent::ServerRequestInteract_Implementation(AActor* TargetActor)
{
	UE_LOG(LogTemp, Warning, TEXT("[MOInteractor] ServerRequestInteract for '%s'"), *GetNameSafe(TargetActor));

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInteractor] ServerRequestInteract: No world"));
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AController* InteractorController = OwnerPawn ? OwnerPawn->GetController() : nullptr;

	if (!IsValid(InteractorController) || !IsValid(TargetActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInteractor] ServerRequestInteract: Invalid controller or target"));
		return;
	}

	UMOInteractionSubsystem* InteractionSubsystem = World->GetSubsystem<UMOInteractionSubsystem>();
	if (!InteractionSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[MOInteractor] ServerRequestInteract: No InteractionSubsystem!"));
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("[MOInteractor] ServerRequestInteract: About to call ServerExecuteInteract, Subsystem=%p"), InteractionSubsystem);
	const bool bResult = InteractionSubsystem->ServerExecuteInteract(InteractorController, TargetActor);
	UE_LOG(LogTemp, Error, TEXT("[MOInteractor] ServerRequestInteract: ServerExecuteInteract returned %s"), bResult ? TEXT("true") : TEXT("false"));
}
