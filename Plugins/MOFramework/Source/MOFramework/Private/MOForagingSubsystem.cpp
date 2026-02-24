#include "MOForagingSubsystem.h"
#include "MOPCGInteractionSubsystem.h"
#include "MOWorldItem.h"
#include "MOItemComponent.h"
#include "MOInventoryComponent.h"
#include "MOSkillsComponent.h"
#include "MOWorldItemFactory.h"
#include "MOHISMHarvestHelper.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogMOForaging, Log, All);

// ============================================================================
// SUBSYSTEM LIFECYCLE
// ============================================================================

void UMOForagingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Setup default dig drop table if empty
	// NOTE: Configure DigDropTable in Blueprint or Project Settings with your actual item IDs
	if (DigDropTable.Num() == 0)
	{
		// Edible roots - common, low level
		FMODigDropEntry Root;
		Root.ItemId = FName("EdibleRoot01");
		Root.MinQuantity = 1;
		Root.MaxQuantity = 2;
		Root.DropChance = 0.35f;
		Root.MinForagingLevel = 0;
		DigDropTable.Add(Root);

		// River cobbles - common, low level (stones found in dirt)
		FMODigDropEntry Stone;
		Stone.ItemId = FName("RiverCobble01");
		Stone.MinQuantity = 1;
		Stone.MaxQuantity = 2;
		Stone.DropChance = 0.25f;
		Stone.MinForagingLevel = 0;
		DigDropTable.Add(Stone);

		// Flint nodules - less common, slight level requirement
		FMODigDropEntry Flint;
		Flint.ItemId = FName("FlintNodule01");
		Flint.MinQuantity = 1;
		Flint.MaxQuantity = 1;
		Flint.DropChance = 0.15f;
		Flint.MinForagingLevel = 2;
		DigDropTable.Add(Flint);

		// Mushrooms - can be found underground, higher skill
		FMODigDropEntry Mushroom;
		Mushroom.ItemId = FName("Mushroom01");
		Mushroom.MinQuantity = 1;
		Mushroom.MaxQuantity = 3;
		Mushroom.DropChance = 0.2f;
		Mushroom.MinForagingLevel = 5;
		DigDropTable.Add(Mushroom);
	}

	UE_LOG(LogMOForaging, Log, TEXT("[MOForagingSubsystem] Initialized with %d dig drop entries"), DigDropTable.Num());
}

void UMOForagingSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

// ============================================================================
// HISM QUERY & REVEAL
// ============================================================================

TArray<FMOHISMInstanceData> UMOForagingSubsystem::QueryHISMInstancesInRadius(FVector Origin, float Radius) const
{
	TArray<FMOHISMInstanceData> Result;

	UWorld* World = GetWorld();
	if (!World)
	{
		return Result;
	}

	// Get the PCG interaction subsystem for mesh-to-item lookup
	UMOPCGInteractionSubsystem* PCGSubsystem = World->GetSubsystem<UMOPCGInteractionSubsystem>();
	if (!PCGSubsystem)
	{
		UE_LOG(LogMOForaging, Warning, TEXT("[MOForagingSubsystem] No PCG interaction subsystem found"));
		return Result;
	}

	const float RadiusSq = Radius * Radius;
	int32 TotalISMComponentsFound = 0;
	int32 TotalISMComponentsWithMesh = 0;
	int32 TotalHarvestableComponents = 0;

	// Iterate all actors looking for ISM/HISM components
	// Using UInstancedStaticMeshComponent as base class catches both ISM and HISM
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		// Get all ISM components on this actor (includes HISM which inherits from ISM)
		TArray<UInstancedStaticMeshComponent*> ISMComponents;
		Actor->GetComponents<UInstancedStaticMeshComponent>(ISMComponents);

		for (UInstancedStaticMeshComponent* ISM : ISMComponents)
		{
			TotalISMComponentsFound++;

			if (!ISM || !ISM->GetStaticMesh())
			{
				continue;
			}

			// Skip KeepOnHarvest components - these are resources (trees/bushes) that should be
			// harvested via recipes, not picked up directly. They yield products like sticks/bark.
			if (FMOHISMHarvestHelper::ShouldKeepOnHarvest(ISM))
			{
				UE_LOG(LogMOForaging, Verbose, TEXT("[MOForagingSubsystem] Skipping KeepOnHarvest component on '%s'"),
					*GetNameSafe(Actor));
				continue;
			}

			TotalISMComponentsWithMesh++;
			UStaticMesh* ISMMesh = ISM->GetStaticMesh();

			// Check if this mesh is harvestable
			FName ItemId = PCGSubsystem->GetItemIdForMesh(ISMMesh);
			if (ItemId.IsNone())
			{
				// Try tag-based lookup
				ItemId = PCGSubsystem->GetItemIdForComponentTags(ISM);
			}

			if (ItemId.IsNone())
			{
				// Not a harvestable mesh - log the mesh path for debugging
				UE_LOG(LogMOForaging, Verbose, TEXT("[MOForagingSubsystem] ISM on '%s' has non-harvestable mesh: %s"),
					*GetNameSafe(Actor), *ISMMesh->GetPathName());
				continue;
			}

			TotalHarvestableComponents++;

			// Check each instance
			const int32 InstanceCount = ISM->GetInstanceCount();
			for (int32 i = 0; i < InstanceCount; ++i)
			{
				FTransform InstanceTransform;
				if (!ISM->GetInstanceTransform(i, InstanceTransform, true))
				{
					continue;
				}

				const FVector InstanceLocation = InstanceTransform.GetLocation();
				const float DistSq = FVector::DistSquared(Origin, InstanceLocation);

				if (DistSq <= RadiusSq)
				{
					FMOHISMInstanceData Data;
					Data.HISMComponent = ISM;
					Data.InstanceIndex = i;
					Data.InstanceTransform = InstanceTransform;
					Data.ItemId = ItemId;
					Data.Distance = FMath::Sqrt(DistSq);
					Result.Add(Data);
				}
			}
		}
	}

	// Sort by distance (closest first)
	Result.Sort([](const FMOHISMInstanceData& A, const FMOHISMInstanceData& B)
	{
		return A.Distance < B.Distance;
	});

	UE_LOG(LogMOForaging, Log, TEXT("[MOForagingSubsystem] ISM Query: %d components found, %d with mesh, %d harvestable, %d instances within %.0f units"),
		TotalISMComponentsFound, TotalISMComponentsWithMesh, TotalHarvestableComponents, Result.Num(), Radius);

	return Result;
}

TArray<AMOWorldItem*> UMOForagingSubsystem::RevealHISMInstancesInRadius(FVector Origin, float Radius, APawn* ForagingPawn)
{
	TArray<AMOWorldItem*> SpawnedItems;

	// Query instances
	TArray<FMOHISMInstanceData> Instances = QueryHISMInstancesInRadius(Origin, Radius);

	if (Instances.Num() == 0)
	{
		UE_LOG(LogMOForaging, Log, TEXT("[MOForagingSubsystem] No items found to reveal"));
		return SpawnedItems;
	}

	// Get the world item factory
	UWorld* World = GetWorld();
	UMOWorldItemFactory* ItemFactory = World ? World->GetSubsystem<UMOWorldItemFactory>() : nullptr;
	if (!ItemFactory)
	{
		UE_LOG(LogMOForaging, Warning, TEXT("[MOForagingSubsystem] No WorldItemFactory subsystem available"));
		return SpawnedItems;
	}

	// Cap the number of items
	const int32 ItemsToReveal = FMath::Min(Instances.Num(), MaxItemsPerSearch);

	// Group instances by ISM component for efficient batch removal
	TMap<UInstancedStaticMeshComponent*, TArray<int32>> ComponentToIndices;

	for (int32 i = 0; i < ItemsToReveal; ++i)
	{
		const FMOHISMInstanceData& Data = Instances[i];
		if (!Data.IsValid())
		{
			continue;
		}

		UInstancedStaticMeshComponent* ISM = Data.HISMComponent.Get();
		if (ISM)
		{
			ComponentToIndices.FindOrAdd(ISM).Add(Data.InstanceIndex);
		}

		// Spawn the world item using factory
		AMOWorldItem* WorldItem = ItemFactory->SpawnWorldItemAtTransform(
			Data.ItemId,
			1, // Default quantity
			Data.InstanceTransform
		);

		if (WorldItem)
		{
			SpawnedItems.Add(WorldItem);
		}
	}

	// Remove instances - handles both ISM and HISM components
	// Must remove in reverse order to preserve valid indices
	int32 RemovedCount = 0;
	int32 FailedCount = 0;
	for (auto& Pair : ComponentToIndices)
	{
		UInstancedStaticMeshComponent* ISM = Pair.Key;
		TArray<int32>& Indices = Pair.Value;

		if (!ISM)
		{
			FailedCount += Indices.Num();
			continue;
		}

		// Sort indices in descending order to preserve valid indices during removal
		Indices.Sort([](int32 A, int32 B) { return A > B; });

		for (int32 Index : Indices)
		{
			if (ISM->RemoveInstance(Index))
			{
				RemovedCount++;
			}
			else
			{
				FailedCount++;
			}
		}
	}
	UE_LOG(LogMOForaging, Verbose, TEXT("[MOForagingSubsystem] ISM removal: %d removed, %d failed"),
		RemovedCount, FailedCount);

	// Award XP
	if (ForagingPawn && SpawnedItems.Num() > 0)
	{
		AwardForagingXP(ForagingPawn, XPPerRevealedItem * SpawnedItems.Num());
	}

	UE_LOG(LogMOForaging, Log, TEXT("[MOForagingSubsystem] Revealed %d items (capped from %d found)"),
		SpawnedItems.Num(), Instances.Num());

	return SpawnedItems;
}

// ============================================================================
// DIG FOR SUPPLIES
// ============================================================================

TArray<AMOWorldItem*> UMOForagingSubsystem::DigForSupplies(FVector Location, int32 ForagingLevel, APawn* ForagingPawn)
{
	TArray<AMOWorldItem*> SpawnedItems;

	if (DigDropTable.Num() == 0)
	{
		UE_LOG(LogMOForaging, Warning, TEXT("[MOForagingSubsystem] Dig drop table is empty"));
		return SpawnedItems;
	}

	// Get the world item factory
	UWorld* World = GetWorld();
	UMOWorldItemFactory* ItemFactory = World ? World->GetSubsystem<UMOWorldItemFactory>() : nullptr;
	if (!ItemFactory)
	{
		UE_LOG(LogMOForaging, Warning, TEXT("[MOForagingSubsystem] No WorldItemFactory subsystem available"));
		return SpawnedItems;
	}

	// Roll each drop entry
	for (const FMODigDropEntry& Entry : DigDropTable)
	{
		// Check level requirement
		if (ForagingLevel < Entry.MinForagingLevel)
		{
			continue;
		}

		// Roll for drop
		const float Roll = FMath::FRand();
		if (Roll > Entry.DropChance)
		{
			continue;
		}

		// Determine quantity
		const int32 Quantity = FMath::RandRange(Entry.MinQuantity, Entry.MaxQuantity);

		// Randomize spawn position within dig radius
		const FVector2D RandomOffset = FMath::RandPointInCircle(DigSpawnRadius);
		const FVector SpawnLocation = Location + FVector(RandomOffset.X, RandomOffset.Y, 10.0f);
		const FRotator SpawnRotation = FRotator(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f);

		// Spawn the item using factory
		AMOWorldItem* WorldItem = ItemFactory->SpawnWorldItemSimple(Entry.ItemId, Quantity, SpawnLocation, SpawnRotation);
		if (WorldItem)
		{
			// Enable physics so item falls to ground
			WorldItem->EnableDropPhysics();

			SpawnedItems.Add(WorldItem);
			UE_LOG(LogMOForaging, Verbose, TEXT("[MOForagingSubsystem] Dug up %s x%d"), *Entry.ItemId.ToString(), Quantity);
		}
	}

	// Award XP
	if (ForagingPawn && SpawnedItems.Num() > 0)
	{
		AwardForagingXP(ForagingPawn, XPPerDugItem * SpawnedItems.Num());
	}

	UE_LOG(LogMOForaging, Log, TEXT("[MOForagingSubsystem] Dug up %d items at skill level %d"),
		SpawnedItems.Num(), ForagingLevel);

	return SpawnedItems;
}

int32 UMOForagingSubsystem::DigForSuppliesToInventory(FVector Location, int32 ForagingLevel, APawn* TargetPawn)
{
	if (!TargetPawn)
	{
		UE_LOG(LogMOForaging, Warning, TEXT("[MOForagingSubsystem] DigForSuppliesToInventory: No target pawn"));
		return 0;
	}

	UMOInventoryComponent* Inventory = TargetPawn->FindComponentByClass<UMOInventoryComponent>();
	if (!Inventory)
	{
		UE_LOG(LogMOForaging, Warning, TEXT("[MOForagingSubsystem] DigForSuppliesToInventory: Pawn has no inventory"));
		return 0;
	}

	if (DigDropTable.Num() == 0)
	{
		UE_LOG(LogMOForaging, Warning, TEXT("[MOForagingSubsystem] Dig drop table is empty"));
		return 0;
	}

	int32 ItemsAdded = 0;

	// Roll each drop entry
	for (const FMODigDropEntry& Entry : DigDropTable)
	{
		// Check level requirement
		if (ForagingLevel < Entry.MinForagingLevel)
		{
			continue;
		}

		// Roll for drop
		const float Roll = FMath::FRand();
		if (Roll > Entry.DropChance)
		{
			continue;
		}

		// Determine quantity
		const int32 Quantity = FMath::RandRange(Entry.MinQuantity, Entry.MaxQuantity);

		// Add directly to inventory
		const FGuid NewItemGuid = FGuid::NewGuid();
		if (Inventory->AddItemByGuid(NewItemGuid, Entry.ItemId, Quantity))
		{
			ItemsAdded++;
			UE_LOG(LogMOForaging, Verbose, TEXT("[MOForagingSubsystem] Added %s x%d to inventory"), *Entry.ItemId.ToString(), Quantity);
		}
	}

	// Award XP
	if (ItemsAdded > 0)
	{
		AwardForagingXP(TargetPawn, XPPerDugItem * ItemsAdded);
	}

	UE_LOG(LogMOForaging, Log, TEXT("[MOForagingSubsystem] Dug %d items directly to inventory at skill level %d"),
		ItemsAdded, ForagingLevel);

	return ItemsAdded;
}

int32 UMOForagingSubsystem::ForageToInventory(FVector Origin, float Radius, APawn* TargetPawn)
{
	if (!TargetPawn)
	{
		UE_LOG(LogMOForaging, Warning, TEXT("[MOForagingSubsystem] ForageToInventory: No target pawn"));
		return 0;
	}

	UMOInventoryComponent* Inventory = TargetPawn->FindComponentByClass<UMOInventoryComponent>();
	if (!Inventory)
	{
		UE_LOG(LogMOForaging, Warning, TEXT("[MOForagingSubsystem] ForageToInventory: Pawn has no inventory"));
		return 0;
	}

	// Query ISM instances
	TArray<FMOHISMInstanceData> Instances = QueryHISMInstancesInRadius(Origin, Radius);

	if (Instances.Num() == 0)
	{
		UE_LOG(LogMOForaging, Log, TEXT("[MOForagingSubsystem] ForageToInventory: No items found"));
		return 0;
	}

	// Cap the number of items
	const int32 ItemsToReveal = FMath::Min(Instances.Num(), MaxItemsPerSearch);

	// Group instances by ISM component for efficient batch removal
	TMap<UInstancedStaticMeshComponent*, TArray<int32>> ComponentToIndices;
	int32 ItemsAdded = 0;

	for (int32 i = 0; i < ItemsToReveal; ++i)
	{
		const FMOHISMInstanceData& Data = Instances[i];
		if (!Data.IsValid())
		{
			continue;
		}

		UInstancedStaticMeshComponent* ISM = Data.HISMComponent.Get();
		if (ISM)
		{
			ComponentToIndices.FindOrAdd(ISM).Add(Data.InstanceIndex);
		}

		// Add directly to inventory
		const FGuid NewItemGuid = FGuid::NewGuid();
		if (Inventory->AddItemByGuid(NewItemGuid, Data.ItemId, 1))
		{
			ItemsAdded++;
			UE_LOG(LogMOForaging, Verbose, TEXT("[MOForagingSubsystem] Foraged %s to inventory"), *Data.ItemId.ToString());
		}
	}

	// Remove instances from ISM components using helper (respects KeepOnHarvest tag)
	int32 RemovedCount = 0;
	for (auto& Pair : ComponentToIndices)
	{
		UInstancedStaticMeshComponent* ISM = Pair.Key;
		TArray<int32>& Indices = Pair.Value;

		if (!ISM)
		{
			continue;
		}

		// Sort indices in descending order to preserve valid indices during removal
		Indices.Sort([](int32 A, int32 B) { return A > B; });

		for (int32 Index : Indices)
		{
			// Use helper to properly handle KeepOnHarvest components
			if (FMOHISMHarvestHelper::RemoveInstance(ISM, Index, true))
			{
				RemovedCount++;
			}
		}
	}

	// Award XP
	if (ItemsAdded > 0)
	{
		AwardForagingXP(TargetPawn, XPPerRevealedItem * ItemsAdded);
	}

	UE_LOG(LogMOForaging, Log, TEXT("[MOForagingSubsystem] Foraged %d items directly to inventory (removed %d instances)"),
		ItemsAdded, RemovedCount);

	return ItemsAdded;
}

// ============================================================================
// SKILL INTEGRATION
// ============================================================================

float UMOForagingSubsystem::CalculateSearchRadius(int32 ForagingLevel) const
{
	const float CalculatedRadius = BaseSearchRadius + (ForagingLevel * RadiusPerSkillLevel);
	return FMath::Min(CalculatedRadius, MaxSearchRadius);
}

int32 UMOForagingSubsystem::GetForagingLevel(APawn* Pawn) const
{
	if (!Pawn)
	{
		return 0;
	}

	UMOSkillsComponent* SkillsComp = Pawn->FindComponentByClass<UMOSkillsComponent>();
	if (!SkillsComp)
	{
		return 0;
	}

	return SkillsComp->GetSkillLevel(FName("Foraging"));
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

void UMOForagingSubsystem::AwardForagingXP(APawn* Pawn, float Amount)
{
	if (!Pawn || Amount <= 0.0f)
	{
		return;
	}

	UMOSkillsComponent* SkillsComp = Pawn->FindComponentByClass<UMOSkillsComponent>();
	if (!SkillsComp)
	{
		return;
	}

	SkillsComp->AddExperience(FName("Foraging"), Amount);
	UE_LOG(LogMOForaging, Verbose, TEXT("[MOForagingSubsystem] Awarded %.1f Foraging XP"), Amount);
}
