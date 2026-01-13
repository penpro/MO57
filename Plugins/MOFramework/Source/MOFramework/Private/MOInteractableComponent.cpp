#include "MOInteractableComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogMOInteractable, Log, All);

UMOInteractableComponent::UMOInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMOInteractableComponent::BeginPlay()
{
	Super::BeginPlay();

	if (DisplayName.IsEmpty())
	{
		if (const AActor* OwnerActor = GetOwner())
		{
			DisplayName = FText::FromString(OwnerActor->GetName());
		}
	}
}

bool UMOInteractableComponent::CanInteract(AController* InteractorController) const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !InteractorController)
	{
		return false;
	}

	const APawn* InteractorPawn = InteractorController->GetPawn();
	if (!InteractorPawn)
	{
		return false;
	}

	const float DistanceToInteractor = FVector::Dist(
		InteractorPawn->GetActorLocation(),
		OwnerActor->GetActorLocation()
	);

	return DistanceToInteractor <= MaxInteractionDistance;
}

bool UMOInteractableComponent::ExecuteInteraction(AController* InteractorController)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !InteractorController)
	{
		return false;
	}

	if (!OwnerActor->HasAuthority())
	{
		UE_LOG(LogMOInteractable, Warning, TEXT("ExecuteInteraction called without authority on %s"), *OwnerActor->GetName());
		return false;
	}

	if (!CanInteract(InteractorController))
	{
		return false;
	}

	const bool bSuccess = K2_OnInteract(InteractorController);
	if (!bSuccess)
	{
		return false;
	}

	OnInteracted.Broadcast(InteractorController, Verb);

	if (bDestroyOwnerOnInteract)
	{
		OwnerActor->Destroy();
	}

	return true;
}

bool UMOInteractableComponent::K2_OnInteract_Implementation(AController* InteractorController)
{
	return true;
}
