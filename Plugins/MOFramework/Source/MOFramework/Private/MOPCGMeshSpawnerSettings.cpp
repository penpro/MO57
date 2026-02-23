#include "MOPCGMeshSpawnerSettings.h"
#include "MOFramework.h"
#include "MOItemDefinitionRow.h"
#include "MOPCGInteractionSubsystem.h"

#include "PCGContext.h"
#include "PCGPoint.h"
#include "Data/PCGPointData.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "MOPCGMeshSpawner"

// ============================================================================
// SETTINGS
// ============================================================================

UMOPCGMeshSpawnerSettings::UMOPCGMeshSpawnerSettings()
{
#if WITH_EDITOR
	Category = LOCTEXT("Category", "MO");
#endif
}

FPCGElementPtr UMOPCGMeshSpawnerSettings::CreateElement() const
{
	return MakeShared<FMOPCGMeshSpawnerElement>();
}

#if WITH_EDITOR
FText UMOPCGMeshSpawnerSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"All-in-one node that spawns item meshes as tagged HISM components.\n\n"
		"For each input point:\n"
		"1. Selects an item based on weighted random\n"
		"2. Spawns the mesh as HISM instance\n"
		"3. Tags the HISM with 'MOItem_<ItemId>'\n\n"
		"Spawned items are immediately discoverable by the foraging system.");
}
#endif

TArray<FPCGPinProperties> UMOPCGMeshSpawnerSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point, true, true);
	return Properties;
}

TArray<FPCGPinProperties> UMOPCGMeshSpawnerSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	// No output - this is a terminal spawner node
	return Properties;
}

// ============================================================================
// ELEMENT
// ============================================================================

bool FMOPCGMeshSpawnerElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMOPCGMeshSpawnerElement::Execute);

	const UMOPCGMeshSpawnerSettings* Settings = Context->GetInputSettings<UMOPCGMeshSpawnerSettings>();
	check(Settings);

	// Validate settings
	if (!Settings->ItemDataTable)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMeshSpawner] No ItemDataTable specified"));
		return true;
	}

	if (Settings->ItemsToSpawn.Num() == 0)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMeshSpawner] No items specified in ItemsToSpawn"));
		return true;
	}

	// Get target actor
	AActor* TargetActor = Context->GetTargetActor(nullptr);
	if (!TargetActor)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMeshSpawner] No target actor found"));
		return true;
	}

	// Build item data map (loads meshes)
	TMap<FName, FItemSpawnData> ItemDataMap = BuildItemDataMap(Settings->ItemDataTable, Settings->ItemsToSpawn);

	// Calculate total weight
	float TotalWeight = 0.0f;
	for (const FMOPCGItemSpawnEntry& Entry : Settings->ItemsToSpawn)
	{
		// Only count items that have valid meshes
		if (ItemDataMap.Contains(Entry.ItemId) && ItemDataMap[Entry.ItemId].Mesh)
		{
			TotalWeight += FMath::Max(0.0f, Entry.Weight);
		}
	}

	if (TotalWeight <= 0.0f)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMeshSpawner] No valid items with meshes found"));
		return true;
	}

	// Get input points
	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	// Process each point and group by item type
	FRandomStream RandomStream(Context->GetSeed() + Settings->SeedOffset);
	int32 TotalPointsProcessed = 0;
	int32 TotalPointsDiscarded = 0;

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGPointData* InputPointData = Cast<UPCGPointData>(Input.Data);
		if (!InputPointData)
		{
			continue;
		}

		const TArray<FPCGPoint>& InputPoints = InputPointData->GetPoints();

		for (const FPCGPoint& Point : InputPoints)
		{
			// Select item for this point
			const FMOPCGItemSpawnEntry* SelectedEntry = SelectWeightedItem(
				Settings->ItemsToSpawn, TotalWeight, RandomStream);

			if (!SelectedEntry)
			{
				TotalPointsDiscarded++;
				continue;
			}

			// Get item data
			FItemSpawnData* ItemData = ItemDataMap.Find(SelectedEntry->ItemId);
			if (!ItemData || !ItemData->Mesh)
			{
				if (Settings->bDiscardInvalidPoints)
				{
					TotalPointsDiscarded++;
					continue;
				}
			}

			if (ItemData && ItemData->Mesh)
			{
				// Add transform to this item's list
				ItemData->Transforms.Add(Point.Transform);
				TotalPointsProcessed++;
			}
		}
	}

	// Get PCG interaction subsystem for registering tag mappings
	UMOPCGInteractionSubsystem* PCGSubsystem = nullptr;
	if (Settings->bRegisterWithSubsystem)
	{
		UWorld* World = TargetActor->GetWorld();
		if (World)
		{
			PCGSubsystem = World->GetSubsystem<UMOPCGInteractionSubsystem>();
		}
	}

	// Create HISM components for each item type
	int32 ComponentsCreated = 0;
	int32 InstancesAdded = 0;

	for (auto& Pair : ItemDataMap)
	{
		FItemSpawnData& ItemData = Pair.Value;

		if (ItemData.Transforms.Num() == 0 || !ItemData.Mesh)
		{
			continue;
		}

		// Get or create HISM component
		UHierarchicalInstancedStaticMeshComponent* HISM = GetOrCreateHISMComponent(
			TargetActor, ItemData.Mesh, Settings, ItemData.ItemId);

		if (!HISM)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOMeshSpawner] Failed to create HISM for item '%s'"),
				*ItemData.ItemId.ToString());
			continue;
		}

		// Add tag
		const FName TagName(*FString::Printf(TEXT("%s%s"), *Settings->TagPrefix, *ItemData.ItemId.ToString()));
		if (!HISM->ComponentHasTag(TagName))
		{
			HISM->ComponentTags.Add(TagName);

			// Register with subsystem
			if (PCGSubsystem)
			{
				PCGSubsystem->RegisterTagItemMapping(TagName, ItemData.ItemId);
			}
		}

		// Add instances
		const int32 PreviousCount = HISM->GetInstanceCount();
		HISM->AddInstances(ItemData.Transforms, false, true);
		InstancesAdded += ItemData.Transforms.Num();

		UE_LOG(LogMOFramework, Log, TEXT("[MOMeshSpawner] Added %d instances of '%s' (mesh: %s, tag: %s)"),
			ItemData.Transforms.Num(), *ItemData.ItemId.ToString(), *ItemData.Mesh->GetName(), *TagName.ToString());

		ComponentsCreated++;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOMeshSpawner] Spawned %d instances across %d components (%d points processed, %d discarded)"),
		InstancesAdded, ComponentsCreated, TotalPointsProcessed, TotalPointsDiscarded);

	return true;
}

const FMOPCGItemSpawnEntry* FMOPCGMeshSpawnerElement::SelectWeightedItem(
	const TArray<FMOPCGItemSpawnEntry>& Items,
	float TotalWeight,
	FRandomStream& RandomStream) const
{
	float RandomValue = RandomStream.FRand() * TotalWeight;
	float AccumulatedWeight = 0.0f;

	for (const FMOPCGItemSpawnEntry& Entry : Items)
	{
		AccumulatedWeight += FMath::Max(0.0f, Entry.Weight);
		if (RandomValue <= AccumulatedWeight)
		{
			return &Entry;
		}
	}

	// Fallback to last item
	return Items.Num() > 0 ? &Items.Last() : nullptr;
}

TMap<FName, FMOPCGMeshSpawnerElement::FItemSpawnData> FMOPCGMeshSpawnerElement::BuildItemDataMap(
	const UDataTable* DataTable,
	const TArray<FMOPCGItemSpawnEntry>& Items) const
{
	TMap<FName, FItemSpawnData> Result;

	if (!DataTable)
	{
		return Result;
	}

	for (const FMOPCGItemSpawnEntry& Entry : Items)
	{
		if (Entry.ItemId.IsNone())
		{
			continue;
		}

		// Look up item in datatable
		const FMOItemDefinitionRow* Row = DataTable->FindRow<FMOItemDefinitionRow>(Entry.ItemId, TEXT("MOMeshSpawner"));
		if (!Row)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOMeshSpawner] Item '%s' not found in datatable"),
				*Entry.ItemId.ToString());
			continue;
		}

		// Get mesh
		if (Row->WorldVisual.StaticMesh.IsNull())
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOMeshSpawner] Item '%s' has no StaticMesh defined"),
				*Entry.ItemId.ToString());
			continue;
		}

		// Load mesh synchronously (we're on main thread)
		UStaticMesh* Mesh = Row->WorldVisual.StaticMesh.LoadSynchronous();
		if (!Mesh)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOMeshSpawner] Failed to load mesh for item '%s'"),
				*Entry.ItemId.ToString());
			continue;
		}

		FItemSpawnData& ItemData = Result.Add(Entry.ItemId);
		ItemData.ItemId = Entry.ItemId;
		ItemData.Mesh = Mesh;

		UE_LOG(LogMOFramework, Verbose, TEXT("[MOMeshSpawner] Loaded mesh '%s' for item '%s'"),
			*Mesh->GetName(), *Entry.ItemId.ToString());
	}

	return Result;
}

UHierarchicalInstancedStaticMeshComponent* FMOPCGMeshSpawnerElement::GetOrCreateHISMComponent(
	AActor* TargetActor,
	UStaticMesh* Mesh,
	const UMOPCGMeshSpawnerSettings* Settings,
	FName ItemId) const
{
	if (!TargetActor || !Mesh)
	{
		return nullptr;
	}

	// Look for existing HISM with same mesh
	TArray<UHierarchicalInstancedStaticMeshComponent*> ExistingHISMs;
	TargetActor->GetComponents<UHierarchicalInstancedStaticMeshComponent>(ExistingHISMs);

	for (UHierarchicalInstancedStaticMeshComponent* Existing : ExistingHISMs)
	{
		if (Existing && Existing->GetStaticMesh() == Mesh)
		{
			return Existing;
		}
	}

	// Create new HISM component
	const FName ComponentName(*FString::Printf(TEXT("HISM_%s"), *ItemId.ToString()));

	UHierarchicalInstancedStaticMeshComponent* NewHISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(
		TargetActor, ComponentName, RF_Transactional);

	if (!NewHISM)
	{
		return nullptr;
	}

	// Configure component
	NewHISM->SetStaticMesh(Mesh);
	NewHISM->SetCollisionProfileName(Settings->CollisionProfile.Name);
	NewHISM->SetCastShadow(Settings->bCastShadows);
	NewHISM->NumCustomDataFloats = 0;
	NewHISM->SetCanEverAffectNavigation(false);

	// Attach to actor
	NewHISM->AttachToComponent(TargetActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	NewHISM->RegisterComponent();
	TargetActor->AddInstanceComponent(NewHISM);

	return NewHISM;
}

#undef LOCTEXT_NAMESPACE
