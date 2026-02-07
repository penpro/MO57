#include "MOPCGInteractionSubsystem.h"
#include "MOFramework.h"
#include "MOItemDatabaseSettings.h"
#include "MOInventoryComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/StaticMesh.h"

void UMOPCGInteractionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RebuildMeshLookupCache();

	UE_LOG(LogMOFramework, Log, TEXT("[MOPCGInteraction] Initialized with %d mesh-to-item mappings"), MeshToItemCache.Num());
}

void UMOPCGInteractionSubsystem::Deinitialize()
{
	MeshToItemCache.Empty();
	Super::Deinitialize();
}

void UMOPCGInteractionSubsystem::RebuildMeshLookupCache()
{
	MeshToItemCache.Empty();

	// Get the item database settings
	const UMOItemDatabaseSettings* ItemDatabase = GetDefault<UMOItemDatabaseSettings>();
	if (!ItemDatabase)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] No item database settings found"));
		return;
	}

	UDataTable* DataTable = ItemDatabase->GetItemDefinitionsDataTable();
	if (!DataTable)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] No item data table configured"));
		return;
	}

	// Iterate all item definitions and build reverse lookup
	static const FString ContextString(TEXT("MOPCGInteractionSubsystem"));
	TArray<FMOItemDefinitionRow*> AllItems;
	DataTable->GetAllRows<FMOItemDefinitionRow>(ContextString, AllItems);

	for (const FMOItemDefinitionRow* Item : AllItems)
	{
		if (!Item)
		{
			continue;
		}

		// Skip items without a mesh
		if (Item->WorldVisual.StaticMesh.IsNull())
		{
			continue;
		}

		// Add to cache
		MeshToItemCache.Add(Item->WorldVisual.StaticMesh, Item->ItemId);

		UE_LOG(LogMOFramework, Log, TEXT("[MOPCGInteraction] Mapped mesh '%s' (path: %s) -> item '%s'"),
			*Item->WorldVisual.StaticMesh.GetAssetName(),
			*Item->WorldVisual.StaticMesh.GetLongPackageName(),
			*Item->ItemId.ToString());
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOPCGInteraction] Built mesh lookup cache with %d entries"), MeshToItemCache.Num());
}

FName UMOPCGInteractionSubsystem::GetItemIdForMesh(UStaticMesh* Mesh) const
{
	if (!IsValid(Mesh))
	{
		return NAME_None;
	}

	// Create soft pointer for lookup
	TSoftObjectPtr<UStaticMesh> SoftMesh(Mesh);
	const FString MeshPath = Mesh->GetPathName();

	const FName* FoundItem = MeshToItemCache.Find(SoftMesh);
	if (FoundItem)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOPCGInteraction] Found mesh '%s' -> item '%s'"),
			*MeshPath, *FoundItem->ToString());
		return *FoundItem;
	}

	// Also try by path in case the mesh was loaded differently
	for (const auto& Pair : MeshToItemCache)
	{
		if (Pair.Key.Get() == Mesh)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOPCGInteraction] Found mesh '%s' -> item '%s' (by pointer)"),
				*MeshPath, *Pair.Value.ToString());
			return Pair.Value;
		}
	}

	// Log cache contents for debugging
	UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] No item mapping for mesh '%s'. Cache has %d entries:"),
		*MeshPath, MeshToItemCache.Num());
	for (const auto& Pair : MeshToItemCache)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction]   - '%s' -> '%s'"),
			*Pair.Key.GetLongPackageName(), *Pair.Value.ToString());
	}

	return NAME_None;
}

bool UMOPCGInteractionSubsystem::IsMeshHarvestable(UStaticMesh* Mesh) const
{
	return !GetItemIdForMesh(Mesh).IsNone();
}

bool UMOPCGInteractionSubsystem::IsHISMHarvestable(UHierarchicalInstancedStaticMeshComponent* HISMComponent) const
{
	if (!IsValid(HISMComponent))
	{
		return false;
	}

	return IsMeshHarvestable(HISMComponent->GetStaticMesh());
}

bool UMOPCGInteractionSubsystem::HarvestHISMInstance(UHierarchicalInstancedStaticMeshComponent* HISMComponent, int32 InstanceIndex, AActor* Harvester, FName& OutItemId)
{
	OutItemId = NAME_None;

	if (!IsValid(HISMComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] HarvestHISMInstance: Invalid HISM component"));
		return false;
	}

	if (InstanceIndex < 0 || InstanceIndex >= HISMComponent->GetInstanceCount())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] HarvestHISMInstance: Invalid instance index %d (count: %d)"),
			InstanceIndex, HISMComponent->GetInstanceCount());
		return false;
	}

	// Get the mesh and find corresponding item
	UStaticMesh* Mesh = HISMComponent->GetStaticMesh();
	FName ItemId = GetItemIdForMesh(Mesh);
	if (ItemId.IsNone())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] HarvestHISMInstance: No item mapping for mesh '%s'"),
			*GetNameSafe(Mesh));
		return false;
	}

	// Find harvester's inventory
	UMOInventoryComponent* Inventory = FindHarvesterInventory(Harvester);
	if (!IsValid(Inventory))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] HarvestHISMInstance: Harvester has no inventory"));
		return false;
	}

	// Get instance transform for logging/effects
	FTransform InstanceTransform;
	HISMComponent->GetInstanceTransform(InstanceIndex, InstanceTransform, true);

	// Remove the instance from HISM
	const bool bRemoved = HISMComponent->RemoveInstance(InstanceIndex);
	if (!bRemoved)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] HarvestHISMInstance: Failed to remove instance %d"), InstanceIndex);
		return false;
	}

	// Add item to inventory
	const int32 Quantity = DefaultHarvestQuantity;
	const FGuid NewItemGuid = FGuid::NewGuid();
	const bool bAdded = Inventory->AddItemByGuid(NewItemGuid, ItemId, Quantity);

	OutItemId = ItemId;

	UE_LOG(LogMOFramework, Log, TEXT("[MOPCGInteraction] Harvested HISM instance %d: %s x%d (added: %s) at %s"),
		InstanceIndex,
		*ItemId.ToString(),
		Quantity,
		bAdded ? TEXT("yes") : TEXT("no"),
		*InstanceTransform.GetLocation().ToString());

	return true;
}

bool UMOPCGInteractionSubsystem::HarvestISMInstance(UInstancedStaticMeshComponent* ISMComponent, int32 InstanceIndex, AActor* Harvester, FName& OutItemId)
{
	OutItemId = NAME_None;

	if (!IsValid(ISMComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] HarvestISMInstance: Invalid ISM component"));
		return false;
	}

	if (InstanceIndex < 0 || InstanceIndex >= ISMComponent->GetInstanceCount())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] HarvestISMInstance: Invalid instance index %d (count: %d)"),
			InstanceIndex, ISMComponent->GetInstanceCount());
		return false;
	}

	// Get the mesh and find corresponding item
	UStaticMesh* Mesh = ISMComponent->GetStaticMesh();
	FName ItemId = GetItemIdForMesh(Mesh);
	if (ItemId.IsNone())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] HarvestISMInstance: No item mapping for mesh '%s'"),
			*GetNameSafe(Mesh));
		return false;
	}

	// Find harvester's inventory
	UMOInventoryComponent* Inventory = FindHarvesterInventory(Harvester);
	if (!IsValid(Inventory))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] HarvestISMInstance: Harvester has no inventory"));
		return false;
	}

	// Get instance transform for logging/effects
	FTransform InstanceTransform;
	ISMComponent->GetInstanceTransform(InstanceIndex, InstanceTransform, true);

	// Remove the instance from ISM
	const bool bRemoved = ISMComponent->RemoveInstance(InstanceIndex);
	if (!bRemoved)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] HarvestISMInstance: Failed to remove instance %d"), InstanceIndex);
		return false;
	}

	// Add item to inventory
	const int32 Quantity = DefaultHarvestQuantity;
	const FGuid NewItemGuid = FGuid::NewGuid();
	const bool bAdded = Inventory->AddItemByGuid(NewItemGuid, ItemId, Quantity);

	OutItemId = ItemId;

	UE_LOG(LogMOFramework, Log, TEXT("[MOPCGInteraction] Harvested ISM instance %d: %s x%d (added: %s) at %s"),
		InstanceIndex,
		*ItemId.ToString(),
		Quantity,
		bAdded ? TEXT("yes") : TEXT("no"),
		*InstanceTransform.GetLocation().ToString());

	return true;
}

FText UMOPCGInteractionSubsystem::GetInteractionPromptForMesh(UStaticMesh* Mesh) const
{
	FName ItemId = GetItemIdForMesh(Mesh);
	if (ItemId.IsNone())
	{
		return FText::GetEmpty();
	}

	// Try to get the item's display name via static helper
	FText DisplayName = UMOItemDatabaseSettings::GetItemDisplayName(ItemId);
	if (!DisplayName.IsEmpty())
	{
		return FText::Format(NSLOCTEXT("MO", "PickUpFormat", "Pick Up {0}"), DisplayName);
	}

	return DefaultInteractionPrompt;
}

UMOInventoryComponent* UMOPCGInteractionSubsystem::FindHarvesterInventory(AActor* Harvester) const
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

	return nullptr;
}
