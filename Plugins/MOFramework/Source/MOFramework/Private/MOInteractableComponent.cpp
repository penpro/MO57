#include "MOInteractableComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"

UMOInteractableComponent::UMOInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UMOInteractableComponent::CanInteract(AController* InteractorController) const
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return false;
	}

	if (OwnerActor->IsActorBeingDestroyed())
	{
		return false;
	}

	// Extend later (teams, locks, ownership, etc.)
	return IsValid(InteractorController);
}

bool UMOInteractableComponent::ServerInteract(AController* InteractorController)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return false;
	}

	if (!OwnerActor->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInteract] ServerInteract called without authority"));
		return false;
	}

	if (!CanInteract(InteractorController))
	{
		return false;
	}

	const bool bHandled = HandleInteract(InteractorController);
	if (!bHandled)
	{
		return false;
	}

	OnInteracted.Broadcast(OwnerActor, InteractorController);

	if (bDestroyOwnerOnInteract)
	{
		OwnerActor->Destroy();
	}

	return true;
}

bool UMOInteractableComponent::HandleInteract_Implementation(AController* InteractorController)
{
	// Default: treat as successful interaction.
	return true;
}
