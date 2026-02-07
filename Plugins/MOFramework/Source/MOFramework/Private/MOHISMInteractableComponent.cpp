#include "MOHISMInteractableComponent.h"
#include "MOFramework.h"
#include "MOInventoryComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "GameFramework/Pawn.h"

UMOHISMInteractableComponent::UMOHISMInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UMOHISMInteractableComponent::HandlesHISMComponent(UHierarchicalInstancedStaticMeshComponent* HISMComponent) const
{
	if (!IsValid(HISMComponent))
	{
		return false;
	}

	// If a specific target is set, only handle that one
	if (IsValid(TargetHISMComponent))
	{
		return HISMComponent == TargetHISMComponent;
	}

	// Otherwise handle any HISM on our owning actor
	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return false;
	}

	return HISMComponent->GetOwner() == Owner;
}

bool UMOHISMInteractableComponent::HarvestInstance(UHierarchicalInstancedStaticMeshComponent* HISMComponent, int32 InstanceIndex, AActor* Harvester)
{
	if (!IsValid(HISMComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHISMInteractable] HarvestInstance: Invalid HISM component"));
		return false;
	}

	if (!HandlesHISMComponent(HISMComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHISMInteractable] HarvestInstance: This component doesn't handle the given HISM"));
		return false;
	}

	if (InstanceIndex < 0 || InstanceIndex >= HISMComponent->GetInstanceCount())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHISMInteractable] HarvestInstance: Invalid instance index %d (count: %d)"),
			InstanceIndex, HISMComponent->GetInstanceCount());
		return false;
	}

	if (ItemDefinitionId.IsNone())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHISMInteractable] HarvestInstance: No ItemDefinitionId set"));
		return false;
	}

	// Find harvester's inventory
	UMOInventoryComponent* Inventory = FindHarvesterInventory(Harvester);
	if (!IsValid(Inventory))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHISMInteractable] HarvestInstance: Harvester has no inventory"));
		return false;
	}

	// Calculate quantity
	const int32 Quantity = CalculateQuantity();

	// Get instance transform before removing (for effects/logging)
	FTransform InstanceTransform;
	HISMComponent->GetInstanceTransform(InstanceIndex, InstanceTransform, true);

	// Remove the instance from HISM
	// Note: RemoveInstance causes index shifting, but that's OK for one-off harvests
	const bool bRemoved = HISMComponent->RemoveInstance(InstanceIndex);
	if (!bRemoved)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHISMInteractable] HarvestInstance: Failed to remove instance %d"), InstanceIndex);
		return false;
	}

	// Add item to inventory
	const FGuid NewItemGuid = FGuid::NewGuid();
	const bool bAdded = Inventory->AddItemByGuid(NewItemGuid, ItemDefinitionId, Quantity);

	UE_LOG(LogMOFramework, Log, TEXT("[MOHISMInteractable] Harvested instance %d: %s x%d (added: %s) at %s"),
		InstanceIndex,
		*ItemDefinitionId.ToString(),
		Quantity,
		bAdded ? TEXT("yes") : TEXT("no"),
		*InstanceTransform.GetLocation().ToString());

	// Broadcast event
	OnInstanceHarvested.Broadcast(InstanceIndex, ItemDefinitionId, Quantity);

	return true;
}

int32 UMOHISMInteractableComponent::CalculateQuantity() const
{
	if (MinQuantity >= MaxQuantity)
	{
		return MinQuantity;
	}
	return FMath::RandRange(MinQuantity, MaxQuantity);
}

UMOInventoryComponent* UMOHISMInteractableComponent::FindHarvesterInventory(AActor* Harvester) const
{
	if (!IsValid(Harvester))
	{
		return nullptr;
	}

	// Try direct lookup on the actor
	UMOInventoryComponent* Inventory = Harvester->FindComponentByClass<UMOInventoryComponent>();
	if (IsValid(Inventory))
	{
		return Inventory;
	}

	// If harvester is a controller, try on the pawn
	if (AController* Controller = Cast<AController>(Harvester))
	{
		if (APawn* Pawn = Controller->GetPawn())
		{
			return Pawn->FindComponentByClass<UMOInventoryComponent>();
		}
	}

	// If harvester is a pawn, we already checked
	return nullptr;
}
