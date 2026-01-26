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
	UE_LOG(LogTemp, Warning, TEXT("[MOInteractable] ServerInteract called on '%s'"), *GetNameSafe(OwnerActor));

	if (!IsValid(OwnerActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInteractable] ServerInteract: Invalid owner"));
		return false;
	}

	if (!OwnerActor->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInteractable] ServerInteract called without authority"));
		return false;
	}

	if (!CanInteract(InteractorController))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOInteractable] ServerInteract: CanInteract returned false"));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[MOInteractable] ServerInteract: Calling HandleInteract"));
	const bool bHandled = HandleInteract(InteractorController);
	UE_LOG(LogTemp, Warning, TEXT("[MOInteractable] ServerInteract: HandleInteract returned %s"), bHandled ? TEXT("true") : TEXT("false"));

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
	// If a C++ handler is bound, use it
	if (OnHandleInteract.IsBound())
	{
		return OnHandleInteract.Execute(InteractorController);
	}

	// Default: treat as successful interaction.
	return true;
}
