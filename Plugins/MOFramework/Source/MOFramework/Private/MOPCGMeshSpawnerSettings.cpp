#include "MOPCGMeshSpawnerSettings.h"
#include "MOFramework.h"
#include "MOItemDefinitionRow.h"
#include "MOPCGInteractionSubsystem.h"
#include "MOTerrainModificationSubsystem.h"
#include "MOHarvestDebugSubsystem.h"
#include "MOWeightedSelector.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGPoint.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGActorHelpers.h"
#include "Components/InstancedStaticMeshComponent.h"
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

	// Calculate total weight using utility (only count items with valid meshes)
	auto HasValidMesh = [&ItemDataMap](const FMOPCGItemSpawnEntry& Entry) -> bool
	{
		return ItemDataMap.Contains(Entry.ItemId) && ItemDataMap[Entry.ItemId].Mesh != nullptr;
	};
	const float TotalWeight = FMOWeightedSelector::CalculateTotalWeightIf(Settings->ItemsToSpawn, HasValidMesh);

	if (TotalWeight <= 0.0f)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMeshSpawner] No valid items with meshes found"));
		return true;
	}

	// Get input points
	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	// Cache the terrain-modification subsystem ONCE for the whole point loop.
	// Per-point lookup would still be cheap (one TMap and a sphere math) but
	// hoisting the resolve to the outer scope is free and clearer. Null is
	// fine — IsLocationModified would also return false in that case, but
	// skipping the call entirely is simplest.
	UMOTerrainModificationSubsystem* TerrainMod = nullptr;
	if (Settings->bRespectTerrainModifications)
	{
		if (UWorld* World = TargetActor->GetWorld())
		{
			TerrainMod = World->GetSubsystem<UMOTerrainModificationSubsystem>();
		}
	}

	// Count total input points across all inputs so the log line shows the
	// "denominator" of the filter — useful for confirming the spawner is
	// even processing points.
	int32 TotalInputPoints = 0;
	for (const FPCGTaggedData& Input : Inputs)
	{
		if (const UPCGPointData* InputPointData = Cast<UPCGPointData>(Input.Data))
		{
			TotalInputPoints += InputPointData->GetPoints().Num();
		}
	}
	MOHARVEST_LOG(TargetActor, "PCG-MeshSpawner",
		"Execute START: bRespectTerrainMods=%d terrainSubsystem=%s totalInputPoints=%d zoneCount=%d",
		Settings->bRespectTerrainModifications ? 1 : 0,
		TerrainMod ? TEXT("FOUND") : TEXT("NULL"),
		TotalInputPoints,
		TerrainMod ? TerrainMod->GetZoneCount() : -1);

	// Process each point and group by item type
	FRandomStream RandomStream(Context->GetSeed() + Settings->SeedOffset);
	int32 TotalPointsProcessed = 0;
	int32 TotalPointsDiscarded = 0;
	int32 TotalPointsSuppressedByTerrainMod = 0;

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
			// REACTIVE TERRAIN-MOD FILTER: skip points in modified zones so
			// PCG never creates instances on worked ground. This is the
			// permanent fix for "PCG respawns foliage after a terraform
			// action" — no polling, no flicker, no race.
			//
			// IsLocationModified is an O(zones-per-cell) grid lookup —
			// effectively constant time for typical zone counts.
			if (TerrainMod && TerrainMod->IsLocationModified(Point.Transform.GetLocation()))
			{
				++TotalPointsSuppressedByTerrainMod;
				continue;
			}

			// Select item for this point using weighted selector utility
			const FMOPCGItemSpawnEntry* SelectedEntry = FMOWeightedSelector::SelectWeightedIf(
				Settings->ItemsToSpawn, TotalWeight, RandomStream, HasValidMesh);

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

	MOHARVEST_LOG(TargetActor, "PCG-MeshSpawner",
		"Execute END: processed=%d discarded=%d suppressedByTerrainMod=%d",
		TotalPointsProcessed, TotalPointsDiscarded, TotalPointsSuppressedByTerrainMod);

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

	// Create managed ISM components for each item type
	int32 ComponentsCreated = 0;
	int32 InstancesAdded = 0;

	for (auto& Pair : ItemDataMap)
	{
		FItemSpawnData& ItemData = Pair.Value;

		if (ItemData.Transforms.Num() == 0 || !ItemData.Mesh)
		{
			continue;
		}

		// Build component tags for this item
		TArray<FName> ComponentTags;
		const FName TagName(*FString::Printf(TEXT("%s%s"), *Settings->TagPrefix, *ItemData.ItemId.ToString()));
		ComponentTags.Add(TagName);

		// Get or create managed ISM component with tags (PCG handles cleanup on re-execution)
		UInstancedStaticMeshComponent* ISM = GetOrCreateManagedISMC(
			Context, TargetActor, ItemData.Mesh, Settings, ItemData.ItemId, ComponentTags);

		if (!ISM)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOMeshSpawner] Failed to create ISM for item '%s'"),
				*ItemData.ItemId.ToString());
			continue;
		}

		// Register tag mapping with subsystem (for runtime lookup)
		if (PCGSubsystem)
		{
			PCGSubsystem->RegisterTagItemMapping(TagName, ItemData.ItemId);
		}

		// Add instances (bWorldSpace=true to handle world-space transforms correctly)
		ISM->AddInstances(ItemData.Transforms, false, true);
		InstancesAdded += ItemData.Transforms.Num();

		UE_LOG(LogMOFramework, Verbose, TEXT("[MOMeshSpawner] Added %d instances of '%s' (mesh: %s, tag: %s)"),
			ItemData.Transforms.Num(), *ItemData.ItemId.ToString(), *ItemData.Mesh->GetName(), *TagName.ToString());

		ComponentsCreated++;
	}

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOMeshSpawner] Spawned %d instances across %d components (%d points processed, %d discarded)"),
		InstancesAdded, ComponentsCreated, TotalPointsProcessed, TotalPointsDiscarded);

	return true;
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

UInstancedStaticMeshComponent* FMOPCGMeshSpawnerElement::GetOrCreateManagedISMC(
	FPCGContext* Context,
	AActor* TargetActor,
	UStaticMesh* Mesh,
	const UMOPCGMeshSpawnerSettings* Settings,
	FName ItemId,
	const TArray<FName>& ComponentTags) const
{
	if (!Context || !TargetActor || !Mesh)
	{
		return nullptr;
	}

	// Get the source PCG component for managed resource tracking
	UPCGComponent* SourceComponent = Cast<UPCGComponent>(Context->ExecutionSource.Get());
	if (!SourceComponent)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMeshSpawner] No source PCG component found"));
		return nullptr;
	}

	// Build ISM component descriptor with tags
	FPCGISMComponentBuilderParams Params;
	Params.Descriptor.StaticMesh = Mesh;
	Params.Descriptor.ComponentClass = UHierarchicalInstancedStaticMeshComponent::StaticClass(); // Use HISM for better performance
	Params.Descriptor.BodyInstance.SetCollisionProfileName(Settings->CollisionProfile.Name);
	Params.Descriptor.bCastShadow = Settings->bCastShadows;
	Params.Descriptor.bAffectDistanceFieldLighting = false;
	Params.Descriptor.bAffectDynamicIndirectLighting = false;
	Params.Descriptor.ComponentTags = ComponentTags; // Include tags in descriptor
	Params.NumCustomDataFloats = 0;

	// Use PCG's managed component system - handles cleanup automatically on re-execution
	UInstancedStaticMeshComponent* ISM = UPCGActorHelpers::GetOrCreateISMC(
		TargetActor, SourceComponent, Params, Context);

	return ISM;
}

#undef LOCTEXT_NAMESPACE
