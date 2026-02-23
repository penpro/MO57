#include "MOWorldItemFactory.h"
#include "MOFramework.h"
#include "MOWorldItem.h"
#include "MOItemComponent.h"
#include "Engine/World.h"

// ============================================================================
// SPAWNING
// ============================================================================

AMOWorldItem* UMOWorldItemFactory::SpawnWorldItem(const FMOWorldItemSpawnParams& Params)
{
	return SpawnWorldItemInternal(Params, nullptr);
}

AMOWorldItem* UMOWorldItemFactory::SpawnWorldItemSimple(
	FName ItemDefinitionId,
	int32 Quantity,
	FVector Location,
	FRotator Rotation)
{
	FMOWorldItemSpawnParams Params;
	Params.ItemDefinitionId = ItemDefinitionId;
	Params.Quantity = FMath::Max(1, Quantity);
	Params.Location = Location;
	Params.Rotation = Rotation;
	Params.bEnableDropPhysics = false;

	return SpawnWorldItemInternal(Params, nullptr);
}

AMOWorldItem* UMOWorldItemFactory::SpawnWorldItemAtTransform(
	FName ItemDefinitionId,
	int32 Quantity,
	const FTransform& Transform)
{
	FMOWorldItemSpawnParams Params;
	Params.ItemDefinitionId = ItemDefinitionId;
	Params.Quantity = FMath::Max(1, Quantity);
	Params.Location = Transform.GetLocation();
	Params.Rotation = Transform.GetRotation().Rotator();
	Params.bEnableDropPhysics = false;

	return SpawnWorldItemInternal(Params, nullptr);
}

AMOWorldItem* UMOWorldItemFactory::SpawnDroppedItem(
	FName ItemDefinitionId,
	int32 Quantity,
	const FGuid& ItemGuid,
	FVector DropLocation,
	FRotator DropRotation,
	AActor* Owner)
{
	FMOWorldItemSpawnParams Params;
	Params.ItemDefinitionId = ItemDefinitionId;
	Params.Quantity = FMath::Max(1, Quantity);
	Params.ItemGuid = ItemGuid;
	Params.Location = DropLocation;
	Params.Rotation = DropRotation;
	Params.bEnableDropPhysics = true;
	Params.Owner = Owner;
	if (Owner)
	{
		Params.Instigator = Owner->GetInstigator();
	}

	return SpawnWorldItemInternal(Params, nullptr);
}

// ============================================================================
// BATCH SPAWNING
// ============================================================================

TArray<AMOWorldItem*> UMOWorldItemFactory::SpawnWorldItemsBatch(const TArray<FMOWorldItemSpawnParams>& ParamsArray)
{
	TArray<AMOWorldItem*> SpawnedItems;
	SpawnedItems.Reserve(ParamsArray.Num());

	for (const FMOWorldItemSpawnParams& Params : ParamsArray)
	{
		AMOWorldItem* Item = SpawnWorldItemInternal(Params, nullptr);
		SpawnedItems.Add(Item); // May be nullptr
	}

	return SpawnedItems;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void UMOWorldItemFactory::SetDefaultWorldItemClass(TSubclassOf<AMOWorldItem> InClass)
{
	DefaultWorldItemClass = InClass;
}

TSubclassOf<AMOWorldItem> UMOWorldItemFactory::GetDefaultWorldItemClass() const
{
	if (DefaultWorldItemClass)
	{
		return DefaultWorldItemClass;
	}
	return AMOWorldItem::StaticClass();
}

// ============================================================================
// INTERNAL IMPLEMENTATION
// ============================================================================

AMOWorldItem* UMOWorldItemFactory::SpawnWorldItemInternal(
	const FMOWorldItemSpawnParams& Params,
	TSubclassOf<AMOWorldItem> WorldItemClass)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[WorldItemFactory] SpawnWorldItem: No world available"));
		return nullptr;
	}

	if (Params.ItemDefinitionId.IsNone())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[WorldItemFactory] SpawnWorldItem: Invalid ItemDefinitionId"));
		return nullptr;
	}

	// Determine class to spawn
	TSubclassOf<AMOWorldItem> ClassToSpawn = WorldItemClass;
	if (!ClassToSpawn)
	{
		ClassToSpawn = GetDefaultWorldItemClass();
	}

	// Configure spawn parameters
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = Params.CollisionHandling;
	if (Params.Owner.IsValid())
	{
		SpawnParams.Owner = Params.Owner.Get();
	}
	if (Params.Instigator.IsValid())
	{
		SpawnParams.Instigator = Params.Instigator.Get();
	}

	// Spawn the actor
	const FTransform SpawnTransform(Params.Rotation, Params.Location);
	AMOWorldItem* WorldItem = World->SpawnActor<AMOWorldItem>(ClassToSpawn, SpawnTransform, SpawnParams);

	if (!WorldItem)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[WorldItemFactory] SpawnWorldItem: Failed to spawn world item for %s"),
			*Params.ItemDefinitionId.ToString());
		return nullptr;
	}

	// Apply item data
	ApplyItemData(WorldItem, Params.ItemDefinitionId, Params.Quantity, Params.ItemGuid);

	// Enable drop physics if requested
	if (Params.bEnableDropPhysics)
	{
		// Preserve location before applying physics (ApplyItemDefinitionToWorldMesh may reset it)
		const FVector PreservedLocation = WorldItem->GetActorLocation();
		const FRotator PreservedRotation = WorldItem->GetActorRotation();

		WorldItem->SetActorLocationAndRotation(PreservedLocation, PreservedRotation);
		WorldItem->EnableDropPhysics();
	}

	UE_LOG(LogMOFramework, Verbose, TEXT("[WorldItemFactory] Spawned world item '%s' x%d at %s"),
		*Params.ItemDefinitionId.ToString(), Params.Quantity, *Params.Location.ToString());

	return WorldItem;
}

void UMOWorldItemFactory::ApplyItemData(AMOWorldItem* WorldItem, FName ItemDefinitionId, int32 Quantity, const FGuid& ItemGuid)
{
	if (!WorldItem)
	{
		return;
	}

	// Set item component data
	UMOItemComponent* ItemComp = WorldItem->GetItemComponent();
	if (ItemComp)
	{
		ItemComp->ItemDefinitionId = ItemDefinitionId;
		ItemComp->Quantity = FMath::Max(1, Quantity);
	}

	// Note: ItemGuid is stored in FMOInventoryEntry when item enters inventory,
	// not on the world item itself. The GUID is passed through spawn params for
	// persistence tracking purposes only.

	// Apply visual representation from item definition
	WorldItem->ApplyItemDefinitionToWorldMesh();
}
