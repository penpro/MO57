#include "MOForagingSubsystem.h"
#include "MOPCGInteractionSubsystem.h"
#include "MOWorldItem.h"
#include "MOItemComponent.h"
#include "MOSkillsComponent.h"
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
	int32 TotalHISMComponentsFound = 0;
	int32 TotalHISMComponentsWithMesh = 0;
	int32 TotalHarvestableComponents = 0;

	// Iterate all actors looking for HISM components
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		// Get all HISM components on this actor
		TArray<UHierarchicalInstancedStaticMeshComponent*> HISMComponents;
		Actor->GetComponents<UHierarchicalInstancedStaticMeshComponent>(HISMComponents);

		for (UHierarchicalInstancedStaticMeshComponent* HISM : HISMComponents)
		{
			TotalHISMComponentsFound++;

			if (!HISM || !HISM->GetStaticMesh())
			{
				continue;
			}

			TotalHISMComponentsWithMesh++;
			UStaticMesh* HISMMesh = HISM->GetStaticMesh();

			// Check if this mesh is harvestable
			FName ItemId = PCGSubsystem->GetItemIdForMesh(HISMMesh);
			if (ItemId.IsNone())
			{
				// Try tag-based lookup
				ItemId = PCGSubsystem->GetItemIdForComponentTags(HISM);
			}

			if (ItemId.IsNone())
			{
				// Not a harvestable mesh - log the mesh path for debugging
				UE_LOG(LogMOForaging, Verbose, TEXT("[MOForagingSubsystem] HISM on '%s' has non-harvestable mesh: %s"),
					*GetNameSafe(Actor), *HISMMesh->GetPathName());
				continue;
			}

			TotalHarvestableComponents++;

			// Check each instance
			const int32 InstanceCount = HISM->GetInstanceCount();
			for (int32 i = 0; i < InstanceCount; ++i)
			{
				FTransform InstanceTransform;
				if (!HISM->GetInstanceTransform(i, InstanceTransform, true))
				{
					continue;
				}

				const FVector InstanceLocation = InstanceTransform.GetLocation();
				const float DistSq = FVector::DistSquared(Origin, InstanceLocation);

				if (DistSq <= RadiusSq)
				{
					FMOHISMInstanceData Data;
					Data.HISMComponent = HISM;
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

	UE_LOG(LogMOForaging, Log, TEXT("[MOForagingSubsystem] HISM Query: %d components found, %d with mesh, %d harvestable, %d instances within %.0f units"),
		TotalHISMComponentsFound, TotalHISMComponentsWithMesh, TotalHarvestableComponents, Result.Num(), Radius);

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

	// Cap the number of items
	const int32 ItemsToReveal = FMath::Min(Instances.Num(), MaxItemsPerSearch);

	// Group instances by HISM component for efficient removal
	// Process in reverse index order per component to avoid index shifting
	TMap<UHierarchicalInstancedStaticMeshComponent*, TArray<int32>> ComponentToIndices;

	for (int32 i = 0; i < ItemsToReveal; ++i)
	{
		const FMOHISMInstanceData& Data = Instances[i];
		if (!Data.IsValid())
		{
			continue;
		}

		UHierarchicalInstancedStaticMeshComponent* HISM = Data.HISMComponent.Get();
		if (HISM)
		{
			ComponentToIndices.FindOrAdd(HISM).Add(Data.InstanceIndex);
		}

		// Spawn the world item
		AMOWorldItem* WorldItem = SpawnWorldItem(
			Data.ItemId,
			1, // Default quantity, could be randomized
			Data.InstanceTransform.GetLocation(),
			Data.InstanceTransform.GetRotation().Rotator()
		);

		if (WorldItem)
		{
			SpawnedItems.Add(WorldItem);
		}
	}

	// Remove instances from HISM components (in reverse order to avoid index shifting)
	for (auto& Pair : ComponentToIndices)
	{
		UHierarchicalInstancedStaticMeshComponent* HISM = Pair.Key;
		TArray<int32>& Indices = Pair.Value;

		if (!HISM)
		{
			continue;
		}

		// Sort indices in descending order
		Indices.Sort([](int32 A, int32 B) { return A > B; });

		// Remove each instance
		for (int32 Index : Indices)
		{
			HISM->RemoveInstance(Index);
		}

		UE_LOG(LogMOForaging, Verbose, TEXT("[MOForagingSubsystem] Removed %d instances from HISM"), Indices.Num());
	}

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

		// Spawn the item
		AMOWorldItem* WorldItem = SpawnWorldItem(Entry.ItemId, Quantity, SpawnLocation, SpawnRotation);
		if (WorldItem)
		{
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

AMOWorldItem* UMOForagingSubsystem::SpawnWorldItem(FName ItemId, int32 Quantity, FVector Location, FRotator Rotation)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AMOWorldItem* WorldItem = World->SpawnActor<AMOWorldItem>(AMOWorldItem::StaticClass(), Location, Rotation, SpawnParams);
	if (!WorldItem)
	{
		UE_LOG(LogMOForaging, Warning, TEXT("[MOForagingSubsystem] Failed to spawn world item for %s"), *ItemId.ToString());
		return nullptr;
	}

	// Set item data
	UMOItemComponent* ItemComp = WorldItem->GetItemComponent();
	if (ItemComp)
	{
		ItemComp->ItemDefinitionId = ItemId;
		ItemComp->Quantity = Quantity;
	}

	// Apply visuals from item definition
	WorldItem->ApplyItemDefinitionToWorldMesh();

	return WorldItem;
}

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
