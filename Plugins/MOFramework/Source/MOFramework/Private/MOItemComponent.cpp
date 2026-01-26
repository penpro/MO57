#include "MOItemComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerState.h"

#include "MOIdentityComponent.h"
#include "MOInventoryComponent.h"

UMOItemComponent::UMOItemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMOItemComponent::BeginPlay()
{
	Super::BeginPlay();

	// Ensure correct visual/collision state on both server and clients.
	ApplyWorldItemActiveState();

	// Make the initial definition available to listeners.
	OnItemDefinitionIdChanged.Broadcast(ItemDefinitionId);
}

void UMOItemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMOItemComponent, ItemDefinitionId);
	DOREPLIFETIME(UMOItemComponent, Quantity);
	DOREPLIFETIME(UMOItemComponent, bWorldItemActive);
}

UMOIdentityComponent* UMOItemComponent::FindIdentityComponent() const
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return nullptr;
	}

	return OwnerActor->FindComponentByClass<UMOIdentityComponent>();
}

UMOInventoryComponent* UMOItemComponent::FindInventoryComponentForController(AController* InteractorController) const
{
	if (!IsValid(InteractorController))
	{
		return nullptr;
	}

	APawn* InteractorPawn = InteractorController->GetPawn();
	if (!IsValid(InteractorPawn))
	{
		return nullptr;
	}

	return InteractorPawn->FindComponentByClass<UMOInventoryComponent>();
}

bool UMOItemComponent::GiveToInteractorInventory(AController* InteractorController)
{
	UE_LOG(LogTemp, Warning, TEXT("[MOItem] GiveToInteractorInventory called"));

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOItem] GiveToInteractorInventory: Invalid owner"));
		return false;
	}

	if (!OwnerActor->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOItem] GiveToInteractorInventory called without authority"));
		return false;
	}

	if (!bWorldItemActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOItem] GiveToInteractorInventory: Item not active"));
		return false;
	}

	if (ItemDefinitionId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOItem] GiveToInteractorInventory: ItemDefinitionId is None!"));
		return false;
	}

	if (Quantity <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOItem] GiveToInteractorInventory: Quantity is %d"), Quantity);
		return false;
	}

	UMOIdentityComponent* IdentityComponent = FindIdentityComponent();
	if (!IsValid(IdentityComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOItem] Missing UMOIdentityComponent on %s"), *GetNameSafe(OwnerActor));
		return false;
	}

	// Ensure we have a stable GUID on the server.
	const FGuid ItemGuid = IdentityComponent->GetOrCreateGuid();
	if (!ItemGuid.IsValid())
	{
		return false;
	}

	UMOInventoryComponent* InventoryComponent = FindInventoryComponentForController(InteractorController);
	if (!IsValid(InventoryComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOItem] No inventory component found for controller %s"), *GetNameSafe(InteractorController));
		return false;
	}

	const bool bAdded = InventoryComponent->AddItemByGuid(ItemGuid, ItemDefinitionId, Quantity);
	if (!bAdded)
	{
		return false;
	}

	// Do not destroy yet. We replicate "inactive" so everyone sees it gone.
	SetWorldItemActive(false);
	return true;
}

void UMOItemComponent::SetWorldItemActive(bool bNewWorldItemActive)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	// Authority sets replicated state.
	if (OwnerActor->HasAuthority())
	{
		if (bWorldItemActive == bNewWorldItemActive)
		{
			return;
		}

		bWorldItemActive = bNewWorldItemActive;
		ApplyWorldItemActiveState();
		OnWorldItemActiveChanged.Broadcast(bWorldItemActive);
	}
	else
	{
		// Non-authority should not drive replicated state.
	}
}

void UMOItemComponent::OnRep_WorldItemActive()
{
	ApplyWorldItemActiveState();
	OnWorldItemActiveChanged.Broadcast(bWorldItemActive);
}

void UMOItemComponent::ApplyWorldItemActiveState()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	OwnerActor->SetActorHiddenInGame(!bWorldItemActive);
	OwnerActor->SetActorEnableCollision(bWorldItemActive);
	OwnerActor->SetActorTickEnabled(bWorldItemActive);
}

void UMOItemComponent::OnRep_ItemDefinitionId()
{
	OnItemDefinitionIdChanged.Broadcast(ItemDefinitionId);
}
