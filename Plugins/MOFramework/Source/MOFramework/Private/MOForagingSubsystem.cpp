#include "MOForagingSubsystem.h"
#include "Engine/StaticMesh.h"
#include "MOInteractorComponent.h"
#include "UObject/UObjectIterator.h"
#include "MOItemDatabaseSettings.h"
#include "MONotificationWidget.h"
#include "MOPlayerController.h"
#include "MOTimedActionComponent.h"
#include "MOPCGWaterMask.h"
#include "MOCoverForageSettings.h"
#include "MOPCGInteractionSubsystem.h"
#include "MOGatheringSettingsActor.h"
#include "MOWorldItem.h"
#include "MOItemComponent.h"
#include "MOInventoryComponent.h"
#include "MOSkillsComponent.h"
#include "MOWorldItemFactory.h"
#include "MOHISMHarvestHelper.h"
#include "MOBiomeDatabaseSettings.h"
#include "MOBiomeDefinitionRow.h"
#include "MOWorldSeedSubsystem.h"
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
	if (GetWorld() && GetWorld()->GetNetMode() == NM_Client)
	{
		// World items and the removal ledger are the server's: a client asks through AMOPlayerController::ServerForage (H22 residual: the menu used to run this on the client).
		UE_LOG(LogMOForaging, Warning, TEXT("[MOForagingSubsystem] %s refused on a client: foraging runs on the server"), TEXT(__FUNCTION__));
		return {};
	}
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

	// Get settings from world actor
	FMOForageDetectionSettings DetectionSettings = AMOGatheringSettingsActor::GetForageDetectionSettings(this);

	// Cap the number of items
	const int32 ItemsToReveal = FMath::Min(Instances.Num(), DetectionSettings.MaxItemsPerSearch);

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
			// Through the helper: the revealed instance became an item actor, so it is recorded as removed for good (it must not regrow with its PCG cell).
			if (FMOHISMHarvestHelper::RemoveInstance(ISM, Index, /*bRespectKeepOnHarvest=*/false))
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
		FMOForageXPSettings XPSettings = AMOGatheringSettingsActor::GetForageXPSettings(this);
		AwardForagingXP(ForagingPawn, XPSettings.XPPerRevealedItem * SpawnedItems.Num());
	}

	UE_LOG(LogMOForaging, Log, TEXT("[MOForagingSubsystem] Revealed %d items (capped from %d found)"),
		SpawnedItems.Num(), Instances.Num());

	return SpawnedItems;
}

// ============================================================================
// DIG FOR SUPPLIES
// ============================================================================

// ============================================================================
// GATHERING GROUND COVER (UMOCoverForageSettings)
// ============================================================================

const FName UMOForagingSubsystem::GatherCoverActionId(TEXT("GatherCover"));

namespace
{
	/** The biome spawner's water-edge band (FMOBiomeDefinitionRow::WaterEdgeBandCm is 250 by default), a little generous: cover this close above the water
	 *  is the shore variant. */
	constexpr float CoverShoreBandCm = 300.0f;
}

FText FMOGatherableCover::Label() const
{
	return Entry ? Entry->LabelFor(bAtShore) : FText::GetEmpty();
}

const TArray<FMODigDropEntry>& FMOGatherableCover::Yields() const
{
	static const TArray<FMODigDropEntry> Nothing;
	return Entry ? Entry->YieldsFor(bAtShore) : Nothing;
}

bool UMOForagingSubsystem::FindGatherableCover(const FVector& Location, float RadiusCm, FMOGatherableCover& Out, const APawn* ReachableBy, int32 OnlyEntryIndex) const
{
	TArray<FMOGatherableCover> Kinds;
	FindGatherableCoverKinds(Location, RadiusCm, Kinds, ReachableBy, OnlyEntryIndex);
	Out = Kinds.Num() > 0 ? Kinds[0] : FMOGatherableCover();
	return Kinds.Num() > 0;
}

void UMOForagingSubsystem::FindGatherableCoverKinds(const FVector& Location, float RadiusCm, TArray<FMOGatherableCover>& Out, const APawn* ReachableBy,
	int32 OnlyEntryIndex) const
{
	Out.Reset();
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	TMap<int32, TPair<float, FMOGatherableCover>> NearestByKind;
	for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
	{
		UInstancedStaticMeshComponent* Ism = *It;
		if (!Ism || Ism->GetWorld() != World || Ism->GetInstanceCount() == 0 || !UMOInteractorComponent::IsWorldCoverComponent(Ism)
			|| FVector::DistSquared(Ism->Bounds.Origin, Location) > FMath::Square(Ism->Bounds.SphereRadius + RadiusCm))
		{
			continue;
		}
		const int32 EntryIndex = UMOCoverForageSettings::FindIndexForMeshName(Ism->GetStaticMesh() ? Ism->GetStaticMesh()->GetName() : FString());
		if (EntryIndex == INDEX_NONE || (OnlyEntryIndex != INDEX_NONE && EntryIndex != OnlyEntryIndex))
		{
			continue;
		}
		// Measured to the instance's BOUNDS, not its origin: a sandstone ground patch or a fallen log is metres long and its pivot may be buried metres
		// from where someone stands on it.
		const FBox MeshBox = Ism->GetStaticMesh()->GetBoundingBox();
		for (const int32 Index : Ism->GetInstancesOverlappingSphere(Location, RadiusCm, /*bSphereInWorldSpace=*/true))
		{
			FTransform Xf;
			if (!Ism->GetInstanceTransform(Index, Xf, /*bWorldSpace=*/true) || Xf.GetScale3D().IsNearlyZero())
			{
				continue;
			}
			const FBox Box = MeshBox.TransformBy(Xf);
			const float DistSq = Box.ComputeSquaredDistanceToPoint(Location);
			if (DistSq > FMath::Square(RadiusCm)
				|| (ReachableBy && Box.ComputeSquaredDistanceToPoint(ReachableBy->GetActorLocation()) > FMath::Square(GatherReachCm)))
			{
				continue;
			}
			TPair<float, FMOGatherableCover>* Best = NearestByKind.Find(EntryIndex);
			if (!Best || DistSq < Best->Key)
			{
				FMOGatherableCover Cover;
				Cover.Component = Ism;
				Cover.InstanceIndex = Index;
				Cover.Location = Xf.GetLocation();
				Cover.EntryIndex = EntryIndex;
				Cover.Entry = UMOCoverForageSettings::GetEntry(EntryIndex);
				NearestByKind.Add(EntryIndex, TPair<float, FMOGatherableCover>(DistSq, Cover));
			}
		}
	}
	TArray<TPair<float, FMOGatherableCover>> Sorted;
	NearestByKind.GenerateValueArray(Sorted);
	Sorted.Sort([](const TPair<float, FMOGatherableCover>& A, const TPair<float, FMOGatherableCover>& B) { return A.Key < B.Key; });
	for (TPair<float, FMOGatherableCover>& Pair : Sorted)
	{
		FMOGatherableCover& Cover = Pair.Value;
		if (Cover.Entry->HasShoreVariant())
		{
			// The same test the spawner planted the water-edge species by.
			const FMOPCGWaterMask Mask = FMOPCGWaterMask::ForActor(Cover.Component->GetOwner());
			Cover.bAtShore = Mask.IsActive() && Mask.ShoreFactor(Cover.Location.X, Cover.Location.Y, Cover.Location.Z, CoverShoreBandCm) > 0.0f;
		}
		Out.Add(Cover);
	}
}

bool UMOForagingSubsystem::BeginGatherCover(APawn* Pawn, const FVector& Location, int32 EntryIndex, FText* OutReason)
{
	UMOTimedActionComponent* Timed = Pawn ? Pawn->FindComponentByClass<UMOTimedActionComponent>() : nullptr;
	if (!Pawn || !Pawn->HasAuthority() || !Timed)
	{
		return false;
	}
	FMOGatherableCover Cover;
	if (!FindGatherableCover(Location, 150.0f, Cover, Pawn, EntryIndex))   // the kind that was asked for (the menu's button), not just the nearest
	{
		// Two different failures, told apart: the clump is there but out of reach, or there is nothing to gather.
		const bool bOutOfReach = FindGatherableCover(Location, 150.0f, Cover, nullptr, EntryIndex);
		if (OutReason)
		{
			*OutReason = bOutOfReach ? NSLOCTEXT("MOForaging", "GatherTooFar", "Too far away: step closer.") : NSLOCTEXT("MOForaging", "NothingToGather", "Nothing to gather there.");
		}
		UE_LOG(LogMOForaging, Warning, TEXT("[MOForagingSubsystem] %s: gather refused near (%.0f, %.0f, %.0f): %s"), *Pawn->GetName(), Location.X, Location.Y, Location.Z,
			bOutOfReach ? *FString::Printf(TEXT("the clump is %.0f cm away (reach %.0f)"), FVector::Dist(Pawn->GetActorLocation(), Cover.Location), GatherReachCm) : TEXT("no cover"));
		return false;
	}
	FText ToolReason;
	if (!Cover.Entry->CanBeWorkedBy(Pawn->FindComponentByClass<UMOInventoryComponent>(), &ToolReason))
	{
		if (OutReason)
		{
			*OutReason = ToolReason;
		}
		UE_LOG(LogMOForaging, Warning, TEXT("[MOForagingSubsystem] %s: %s refused: %s"), *Pawn->GetName(), *Cover.Label().ToString(), *ToolReason.ToString());
		return false;
	}
	TWeakObjectPtr<APawn> WeakPawn(Pawn);
	TWeakObjectPtr<UMOForagingSubsystem> WeakThis(this);
	return Timed->Begin(GatherCoverActionId, Cover.Label(), Cover.Entry->Seconds, [WeakPawn, WeakThis, Cover]()
		{
			APawn* P = WeakPawn.Get();
			UMOForagingSubsystem* Self = WeakThis.Get();
			// The SAME clump, still there: its component alive, its index still at that place (another gatherer's removal can take or shift it).
			UInstancedStaticMeshComponent* Ism = Cover.Component.Get();
			FTransform Xf;
			const bool bStill = Ism && Ism->GetInstanceTransform(Cover.InstanceIndex, Xf, /*bWorldSpace=*/true) && !Xf.GetScale3D().IsNearlyZero()
				&& FVector::DistSquared(Xf.GetLocation(), Cover.Location) < 1.0f;
			if (!P || !Self || !bStill)
			{
				UE_LOG(LogMOForaging, Warning, TEXT("[MOForagingSubsystem] gather: the clump at (%.0f, %.0f) is gone"), Cover.Location.X, Cover.Location.Y);
				return;
			}
			Self->CompleteGatherCover(P, Cover);
		}, nullptr, 100.0f);
}

void UMOForagingSubsystem::CompleteGatherCover(APawn* Pawn, const FMOGatherableCover& Cover)
{
	UWorld* World = GetWorld();
	UMOInventoryComponent* Inventory = Pawn->FindComponentByClass<UMOInventoryComponent>();
	UMOWorldItemFactory* Factory = World ? World->GetSubsystem<UMOWorldItemFactory>() : nullptr;
	const int32 Level = GetForagingLevel(Pawn);
	TArray<FString> Got;
	int32 Pieces = 0;
	for (const FMODigDropEntry& Entry : Cover.Yields())
	{
		if (Level < Entry.MinForagingLevel || FMath::FRand() > Entry.DropChance)
		{
			continue;
		}
		const int32 Quantity = FMath::RandRange(Entry.MinQuantity, Entry.MaxQuantity);
		if (!(Inventory && Inventory->AddItemByGuid(FGuid::NewGuid(), Entry.ItemId, Quantity)) && Factory)
		{
			// The pack is full: it lands at the gatherer's feet.
			if (AMOWorldItem* Dropped = Factory->SpawnWorldItemSimple(Entry.ItemId, Quantity, Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 60.0f, FRotator::ZeroRotator))
			{
				Dropped->EnableDropPhysics();
			}
		}
		Got.Add(FString::Printf(TEXT("%d %s"), Quantity, *UMOItemDatabaseSettings::GetItemDisplayName(Entry.ItemId).ToString()));
		Pieces += Quantity;
	}
	if (Cover.Entry->bRemovesInstance)
	{
		if (UHierarchicalInstancedStaticMeshComponent* Hism = Cast<UHierarchicalInstancedStaticMeshComponent>(Cover.Component.Get()))
		{
			FMOHISMHarvestHelper::RemoveInstance(Hism, Cover.InstanceIndex, /*bRespectKeepOnHarvest=*/false);
		}
		else
		{
			FMOHISMHarvestHelper::RemoveInstance(Cover.Component.Get(), Cover.InstanceIndex, /*bRespectKeepOnHarvest=*/false);
		}
	}
	if (Pieces > 0)
	{
		FMOForageXPSettings XPSettings = AMOGatheringSettingsActor::GetForageXPSettings(this);
		AwardForagingXP(Pawn, XPSettings.XPPerRevealedItem * Pieces);
	}
	if (AMOPlayerController* PC = Cast<AMOPlayerController>(Pawn->GetController()))
	{
		PC->ClientShowNotification(Got.Num() > 0
			? FText::Format(NSLOCTEXT("MOForaging", "Gathered", "Gathered {0}"), FText::FromString(FString::Join(Got, TEXT(", "))))
			: NSLOCTEXT("MOForaging", "GatheredNothing", "Nothing worth keeping."), 3.0f, EMONotificationType::Info);
	}
	UE_LOG(LogMOForaging, Log, TEXT("[MOForagingSubsystem] %s gathered %s at (%.0f, %.0f)%s: %s"), *Pawn->GetName(), *Cover.Label().ToString(), Cover.Location.X,
		Cover.Location.Y, Cover.bAtShore ? TEXT(" (shore)") : TEXT(""), Got.Num() > 0 ? *FString::Join(Got, TEXT(", ")) : TEXT("nothing"));
}

const TArray<FMODigDropEntry>& UMOForagingSubsystem::GetDigTableAt(const FVector& Location, FName* OutBiomeId) const
{
	if (OutBiomeId)
	{
		*OutBiomeId = NAME_None;
	}
	const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(GetWorld());
	if (Seeds && Seeds->HasActiveSeed())
	{
		// The biome the ground SHOWS here (the surface pick the ground material and footsteps use): dig on a strand's sand and you get sand.
		const FName BiomeId = UMOBiomeDatabaseSettings::PickSurfaceBiomeAtWorld(Location, 0.0f, Seeds->GetActiveTerrainSeed());
		if (OutBiomeId)
		{
			*OutBiomeId = BiomeId;
		}
		const FMOBiomeDefinitionRow* Biome = BiomeId.IsNone() ? nullptr : UMOBiomeDatabaseSettings::GetBiomeDefinition(BiomeId);
		if (Biome && Biome->DigYields.Num() > 0)
		{
			return Biome->DigYields;
		}
	}
	return DigDropTable;
}

TArray<AMOWorldItem*> UMOForagingSubsystem::DigForSupplies(FVector Location, int32 ForagingLevel, APawn* ForagingPawn)
{
	if (GetWorld() && GetWorld()->GetNetMode() == NM_Client)
	{
		// World items and the removal ledger are the server's: a client asks through AMOPlayerController::ServerForage (H22 residual: the menu used to run this on the client).
		UE_LOG(LogMOForaging, Warning, TEXT("[MOForagingSubsystem] %s refused on a client: foraging runs on the server"), TEXT(__FUNCTION__));
		return {};
	}
	TArray<AMOWorldItem*> SpawnedItems;

	FName BiomeId;
	const TArray<FMODigDropEntry>& DigTable = GetDigTableAt(Location, &BiomeId);
	if (DigTable.Num() == 0)
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

	// Get settings from world actor
	FMODigSettings DigSettingsData = AMOGatheringSettingsActor::GetDigSettings(this);

	// Roll each drop entry
	for (const FMODigDropEntry& Entry : DigTable)
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
		const FVector2D RandomOffset = FMath::RandPointInCircle(DigSettingsData.SpawnRadius);
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
		FMOForageXPSettings XPSettings = AMOGatheringSettingsActor::GetForageXPSettings(this);
		AwardForagingXP(ForagingPawn, XPSettings.XPPerDugItem * SpawnedItems.Num());
	}

	UE_LOG(LogMOForaging, Log, TEXT("[MOForagingSubsystem] Dug up %d items at skill level %d (biome %s)"),
		SpawnedItems.Num(), ForagingLevel, *BiomeId.ToString());

	return SpawnedItems;
}

int32 UMOForagingSubsystem::DigForSuppliesToInventory(FVector Location, int32 ForagingLevel, APawn* TargetPawn)
{
	if (GetWorld() && GetWorld()->GetNetMode() == NM_Client)
	{
		// World items and the removal ledger are the server's: a client asks through AMOPlayerController::ServerForage (H22 residual: the menu used to run this on the client).
		UE_LOG(LogMOForaging, Warning, TEXT("[MOForagingSubsystem] %s refused on a client: foraging runs on the server"), TEXT(__FUNCTION__));
		return {};
	}
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

	FName BiomeId;
	const TArray<FMODigDropEntry>& DigTable = GetDigTableAt(Location, &BiomeId);
	if (DigTable.Num() == 0)
	{
		UE_LOG(LogMOForaging, Warning, TEXT("[MOForagingSubsystem] Dig drop table is empty"));
		return 0;
	}

	int32 ItemsAdded = 0;

	// Roll each drop entry
	for (const FMODigDropEntry& Entry : DigTable)
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
		FMOForageXPSettings XPSettings = AMOGatheringSettingsActor::GetForageXPSettings(this);
		AwardForagingXP(TargetPawn, XPSettings.XPPerDugItem * ItemsAdded);
	}

	UE_LOG(LogMOForaging, Log, TEXT("[MOForagingSubsystem] Dug %d items directly to inventory at skill level %d"),
		ItemsAdded, ForagingLevel);

	return ItemsAdded;
}

int32 UMOForagingSubsystem::ForageToInventory(FVector Origin, float Radius, APawn* TargetPawn)
{
	if (GetWorld() && GetWorld()->GetNetMode() == NM_Client)
	{
		// World items and the removal ledger are the server's: a client asks through AMOPlayerController::ServerForage (H22 residual: the menu used to run this on the client).
		UE_LOG(LogMOForaging, Warning, TEXT("[MOForagingSubsystem] %s refused on a client: foraging runs on the server"), TEXT(__FUNCTION__));
		return {};
	}
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

	// Get settings from world actor
	FMOForageDetectionSettings DetectionSettings = AMOGatheringSettingsActor::GetForageDetectionSettings(this);

	// Cap the number of items
	const int32 ItemsToReveal = FMath::Min(Instances.Num(), DetectionSettings.MaxItemsPerSearch);

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
		FMOForageXPSettings XPSettings = AMOGatheringSettingsActor::GetForageXPSettings(this);
		AwardForagingXP(TargetPawn, XPSettings.XPPerRevealedItem * ItemsAdded);
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
	// Get settings from world actor if available
	FMOForageDetectionSettings DetectionSettings = AMOGatheringSettingsActor::GetForageDetectionSettings(this);

	const float CalculatedRadius = DetectionSettings.BaseSearchRadius + (ForagingLevel * DetectionSettings.RadiusPerSkillLevel);
	return FMath::Min(CalculatedRadius, DetectionSettings.MaxSearchRadius);
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
