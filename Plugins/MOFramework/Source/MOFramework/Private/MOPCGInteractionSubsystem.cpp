#include "MOPCGInteractionSubsystem.h"
#include "MOFramework.h"
#include "MOItemDatabaseSettings.h"
#include "MOInventoryComponent.h"
#include "MONotificationComponent.h"
#include "MOHISMHarvestHelper.h"
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

		UE_LOG(LogMOFramework, Verbose, TEXT("[MOPCGInteraction] Mapped mesh '%s' (path: %s) -> item '%s'"),
			*Item->WorldVisual.StaticMesh.GetAssetName(),
			*Item->WorldVisual.StaticMesh.GetLongPackageName(),
			*Item->ItemId.ToString());
	}

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOPCGInteraction] Built mesh lookup cache with %d entries"), MeshToItemCache.Num());
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
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOPCGInteraction] Found mesh '%s' -> item '%s'"),
			*MeshPath, *FoundItem->ToString());
		return *FoundItem;
	}

	// Try by pointer comparison (if datatable mesh is loaded)
	for (const auto& Pair : MeshToItemCache)
	{
		if (Pair.Key.Get() == Mesh)
		{
			UE_LOG(LogMOFramework, Verbose, TEXT("[MOPCGInteraction] Found mesh '%s' -> item '%s' (by pointer)"),
				*MeshPath, *Pair.Value.ToString());
			return Pair.Value;
		}
	}

	// Try by path comparison (handles soft reference format differences)
	// Get the mesh's full path for string comparison
	const FSoftObjectPath MeshSoftPath(Mesh);
	const FString MeshAssetPath = MeshSoftPath.GetAssetPathString();

	for (const auto& Pair : MeshToItemCache)
	{
		const FString CachedAssetPath = Pair.Key.ToSoftObjectPath().GetAssetPathString();
		if (MeshAssetPath == CachedAssetPath)
		{
			UE_LOG(LogMOFramework, Verbose, TEXT("[MOPCGInteraction] Found mesh '%s' -> item '%s' (by path string)"),
				*MeshPath, *Pair.Value.ToString());
			return Pair.Value;
		}
	}

	// Log cache contents for debugging (only on first miss to reduce spam)
	static TSet<FString> LoggedMisses;
	if (!LoggedMisses.Contains(MeshPath))
	{
		LoggedMisses.Add(MeshPath);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] No item mapping for mesh '%s' (asset: %s). Cache has %d entries:"),
			*MeshPath, *MeshAssetPath, MeshToItemCache.Num());
		for (const auto& Pair : MeshToItemCache)
		{
			const FString CachedPath = Pair.Key.ToSoftObjectPath().GetAssetPathString();
			UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction]   - '%s' -> '%s'"),
				*CachedPath, *Pair.Value.ToString());
		}
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

	// Check tag-based mapping first
	if (!GetItemIdForComponentTags(HISMComponent).IsNone())
	{
		return true;
	}

	// Fall back to mesh-based check
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

	// First try tag-based lookup (for things like trees that give sticks)
	FName ItemId = GetItemIdForComponentTags(HISMComponent);

	// Fall back to mesh-based lookup
	if (ItemId.IsNone())
	{
		UStaticMesh* Mesh = HISMComponent->GetStaticMesh();
		ItemId = GetItemIdForMesh(Mesh);
		if (ItemId.IsNone())
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] HarvestHISMInstance: No item mapping for mesh '%s' and no matching tags"),
				*GetNameSafe(Mesh));
			return false;
		}
	}

	// Find harvester's inventory
	UMOInventoryComponent* Inventory = FindHarvesterInventory(Harvester);
	if (!IsValid(Inventory))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] HarvestHISMInstance: Harvester has no inventory"));
		return false;
	}

	// Get instance transform using helper
	FTransform InstanceTransform;
	if (!FMOHISMHarvestHelper::GetInstanceTransform(HISMComponent, InstanceIndex, InstanceTransform))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] HarvestHISMInstance: Failed to get instance transform"));
		return false;
	}

	// CRITICAL: Check if inventory has space BEFORE removing the instance
	// Otherwise we destroy world items but can't pick them up
	const FGuid NewItemGuid = FGuid::NewGuid();
	if (!Inventory->CanAddItem(NewItemGuid))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] HarvestHISMInstance: Inventory is full, cannot harvest"));

		// Show notification to player
		if (APawn* HarvesterPawn = Cast<APawn>(Harvester))
		{
			if (AController* Controller = HarvesterPawn->GetController())
			{
				if (UMONotificationComponent* NotificationComp = Controller->FindComponentByClass<UMONotificationComponent>())
				{
					NotificationComp->ShowInventoryFullNotification();
				}
			}
		}
		return false;
	}

	// Remove instance using helper (handles KeepOnHarvest check automatically)
	if (!FMOHISMHarvestHelper::RemoveInstance(HISMComponent, InstanceIndex, true))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] HarvestHISMInstance: Failed to remove instance %d"), InstanceIndex);
		return false;
	}

	// Add item to inventory (we already verified space exists)
	const int32 Quantity = DefaultHarvestQuantity;
	const bool bAdded = Inventory->AddItemByGuid(NewItemGuid, ItemId, Quantity);

	OutItemId = ItemId;

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOPCGInteraction] Harvested HISM instance %d: %s x%d (added: %s) at %s"),
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

	// First try tag-based lookup (for things like trees that give sticks)
	FName ItemId = GetItemIdForComponentTags(ISMComponent);

	// Fall back to mesh-based lookup
	if (ItemId.IsNone())
	{
		UStaticMesh* Mesh = ISMComponent->GetStaticMesh();
		ItemId = GetItemIdForMesh(Mesh);
		if (ItemId.IsNone())
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] HarvestISMInstance: No item mapping for mesh '%s' and no matching tags"),
				*GetNameSafe(Mesh));
			return false;
		}
	}

	// Find harvester's inventory
	UMOInventoryComponent* Inventory = FindHarvesterInventory(Harvester);
	if (!IsValid(Inventory))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] HarvestISMInstance: Harvester has no inventory"));
		return false;
	}

	// Get instance transform using helper
	FTransform InstanceTransform;
	if (!FMOHISMHarvestHelper::GetInstanceTransform(ISMComponent, InstanceIndex, InstanceTransform))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] HarvestISMInstance: Failed to get instance transform"));
		return false;
	}

	// CRITICAL: Check if inventory has space BEFORE removing the instance
	// Otherwise we destroy world items but can't pick them up
	const FGuid NewItemGuid = FGuid::NewGuid();
	if (!Inventory->CanAddItem(NewItemGuid))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] HarvestISMInstance: Inventory is full, cannot harvest"));

		// Show notification to player
		if (APawn* HarvesterPawn = Cast<APawn>(Harvester))
		{
			if (AController* Controller = HarvesterPawn->GetController())
			{
				if (UMONotificationComponent* NotificationComp = Controller->FindComponentByClass<UMONotificationComponent>())
				{
					NotificationComp->ShowInventoryFullNotification();
				}
			}
		}
		return false;
	}

	// Remove instance using helper (handles KeepOnHarvest check automatically)
	if (!FMOHISMHarvestHelper::RemoveInstance(ISMComponent, InstanceIndex, true))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] HarvestISMInstance: Failed to remove instance %d"), InstanceIndex);
		return false;
	}

	// Add item to inventory (we already verified space exists)
	const int32 Quantity = DefaultHarvestQuantity;
	const bool bAdded = Inventory->AddItemByGuid(NewItemGuid, ItemId, Quantity);

	OutItemId = ItemId;

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOPCGInteraction] Harvested ISM instance %d: %s x%d (added: %s) at %s"),
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

void UMOPCGInteractionSubsystem::RegisterTagItemMapping(FName Tag, FName ItemId)
{
	if (Tag.IsNone() || ItemId.IsNone())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGInteraction] RegisterTagItemMapping: Invalid tag or item ID"));
		return;
	}

	TagToItemMap.Add(Tag, ItemId);
	UE_LOG(LogMOFramework, Verbose, TEXT("[MOPCGInteraction] Registered tag mapping: '%s' -> '%s'"),
		*Tag.ToString(), *ItemId.ToString());
}

FName UMOPCGInteractionSubsystem::GetItemIdForComponentTags(UActorComponent* Component) const
{
	if (!IsValid(Component))
	{
		return NAME_None;
	}

	// Check component tags against our tag-to-item map
	for (const FName& Tag : Component->ComponentTags)
	{
		const FName* FoundItem = TagToItemMap.Find(Tag);
		if (FoundItem)
		{
			UE_LOG(LogMOFramework, Verbose, TEXT("[MOPCGInteraction] Found tag '%s' -> item '%s'"),
				*Tag.ToString(), *FoundItem->ToString());
			return *FoundItem;
		}
	}

	// Also check owner actor tags (PCG might put tags there)
	AActor* Owner = Component->GetOwner();
	if (Owner)
	{
		for (const FName& Tag : Owner->Tags)
		{
			const FName* FoundItem = TagToItemMap.Find(Tag);
			if (FoundItem)
			{
				UE_LOG(LogMOFramework, Verbose, TEXT("[MOPCGInteraction] Found actor tag '%s' -> item '%s'"),
					*Tag.ToString(), *FoundItem->ToString());
				return *FoundItem;
			}
		}
	}

	return NAME_None;
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
