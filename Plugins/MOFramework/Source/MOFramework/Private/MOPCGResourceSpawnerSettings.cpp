#include "MOPCGResourceSpawnerSettings.h"
#include "MOFramework.h"
#include "MOPCGInteractionSubsystem.h"
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

#define LOCTEXT_NAMESPACE "MOPCGResourceSpawner"

// ============================================================================
// BASE RESOURCE SPAWNER SETTINGS
// ============================================================================

UMOPCGResourceSpawnerSettings::UMOPCGResourceSpawnerSettings()
{
#if WITH_EDITOR
	Category = LOCTEXT("Category", "MO");
#endif
}

FPCGElementPtr UMOPCGResourceSpawnerSettings::CreateElement() const
{
	return MakeShared<FMOPCGResourceSpawnerElement>();
}

#if WITH_EDITOR
FText UMOPCGResourceSpawnerSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Spawns harvestable resources (trees, bushes, rocks, etc.) as tagged HISM components.\n\n"
		"DataTable-Driven:\n"
		"- Select resources from DT_ResourceNodes via dropdown\n"
		"- All tags, meshes, and yields come from the DataTable\n"
		"- Node-level AdditionalTags apply to ALL resources\n\n"
		"Auto-Generated Tags:\n"
		"- 'Name {DisplayName}' for context menu display\n"
		"- 'MOResource_{Type}' for type classification\n"
		"- 'Gives_{ItemId}' for each yield item\n"
		"- 'KeepOnHarvest' for renewable resources");
}
#endif

TArray<FPCGPinProperties> UMOPCGResourceSpawnerSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point, true, true);
	return Properties;
}

TArray<FPCGPinProperties> UMOPCGResourceSpawnerSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	// No output - this is a terminal spawner node
	return Properties;
}

// ============================================================================
// ELEMENT IMPLEMENTATION
// ============================================================================

bool FMOPCGResourceSpawnerElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMOPCGResourceSpawnerElement::Execute);

	const UMOPCGResourceSpawnerSettings* Settings = Context->GetInputSettings<UMOPCGResourceSpawnerSettings>();
	check(Settings);

	// Validate settings
	if (!Settings->ResourceNodeDataTable)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] No ResourceNodeDataTable specified"));
		return true;
	}

	if (Settings->ResourcesToSpawn.Num() == 0)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] No resources specified"));
		return true;
	}

	// Get target actor
	AActor* TargetActor = Context->GetTargetActor(nullptr);
	if (!TargetActor)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] No target actor found"));
		return true;
	}

	// Initialize random stream
	FRandomStream RandomStream(Context->GetSeed() + Settings->SeedOffset);

	// Build resource data map (loads meshes from DataTable)
	const UDataTable* DataTable = Settings->ResourceNodeDataTable;
	TMap<FName, FResourceSpawnData> ResourceDataMap = BuildResourceDataMap(Settings->ResourcesToSpawn, DataTable, RandomStream);

	// Calculate total weight using utility (only count valid resources with meshes)
	auto HasValidResource = [&ResourceDataMap, DataTable](const FMOPCGResourceEntry& Entry) -> bool
	{
		const FMOResourceNodeDefinitionRow* Def = Entry.GetResourceDefinition(DataTable);
		if (!Def)
		{
			return false;
		}
		// Check that the row name is valid and has data in our map
		const FName RowName = Entry.ResourceRowName;
		return !RowName.IsNone() && ResourceDataMap.Contains(RowName) && ResourceDataMap[RowName].Mesh != nullptr;
	};

	const float TotalWeight = FMOWeightedSelector::CalculateTotalWeightIf(Settings->ResourcesToSpawn, HasValidResource);

	if (TotalWeight <= 0.0f)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] No valid resources with meshes found (map has %d entries)"), ResourceDataMap.Num());
		return true;
	}

	// Get input points
	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	// Process each point
	int32 TotalPointsProcessed = 0;

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
			// Select resource using weighted selector
			const FMOPCGResourceEntry* SelectedEntry = FMOWeightedSelector::SelectWeightedIf(
				Settings->ResourcesToSpawn, TotalWeight, RandomStream, HasValidResource);

			if (!SelectedEntry)
			{
				continue;
			}

			// Get resource data
			const FName RowName = SelectedEntry->ResourceRowName;
			FResourceSpawnData* ResourceData = ResourceDataMap.Find(RowName);
			if (!ResourceData || !ResourceData->Mesh)
			{
				if (Settings->bDiscardInvalidPoints)
				{
					continue;
				}
			}

			if (ResourceData && ResourceData->Mesh)
			{
				// Create transform with optional randomization
				FTransform InstanceTransform = Point.Transform;

				// Random rotation on Z (from DataTable definition)
				if (ResourceData->bRandomizeRotation)
				{
					FRotator CurrentRotation = InstanceTransform.GetRotation().Rotator();
					CurrentRotation.Yaw = RandomStream.FRandRange(0.0f, 360.0f);
					InstanceTransform.SetRotation(CurrentRotation.Quaternion());
				}

				// Random scale (from DataTable definition or entry override)
				const FVector RandomScale = FVector(
					RandomStream.FRandRange(ResourceData->MinScale.X, ResourceData->MaxScale.X),
					RandomStream.FRandRange(ResourceData->MinScale.Y, ResourceData->MaxScale.Y),
					RandomStream.FRandRange(ResourceData->MinScale.Z, ResourceData->MaxScale.Z)
				);
				InstanceTransform.SetScale3D(InstanceTransform.GetScale3D() * RandomScale);

				ResourceData->Transforms.Add(InstanceTransform);
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

	// Create managed ISM components for each resource type
	int32 ComponentsCreated = 0;
	int32 InstancesAdded = 0;

	for (auto& Pair : ResourceDataMap)
	{
		FResourceSpawnData& ResourceData = Pair.Value;

		if (ResourceData.Transforms.Num() == 0 || !ResourceData.Mesh)
		{
			continue;
		}

		// Build component tags - start with all tags from DataTable
		TArray<FName> ComponentTags = ResourceData.AllTags;

		// Add node-level additional tags
		for (const FName& AdditionalTag : Settings->AdditionalTags)
		{
			if (!AdditionalTag.IsNone() && !ComponentTags.Contains(AdditionalTag))
			{
				ComponentTags.Add(AdditionalTag);
			}
		}

		// Get or create managed ISM component with tags (PCG handles cleanup on re-execution)
		UInstancedStaticMeshComponent* ISM = GetOrCreateManagedISMC(
			Context, TargetActor, ResourceData.Mesh, Settings, ResourceData.ResourceNodeId, ComponentTags);

		if (!ISM)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] Failed to create ISM for resource '%s'"),
				*ResourceData.ResourceNodeId.ToString());
			continue;
		}

		// Register tag mapping with subsystem (for runtime lookup)
		if (PCGSubsystem)
		{
			// Register the resource node ID as a tag mapping
			const FName ResourceTag = FName(*FString::Printf(TEXT("MOResource_%s"), *ResourceData.ResourceNodeId.ToString()));
			PCGSubsystem->RegisterTagItemMapping(ResourceTag, ResourceData.ResourceNodeId);
		}

		// Add instances (bWorldSpace=true to handle world-space transforms correctly)
		ISM->AddInstances(ResourceData.Transforms, false, true);
		InstancesAdded += ResourceData.Transforms.Num();

		UE_LOG(LogMOFramework, Verbose, TEXT("[MOResourceSpawner] Added %d instances of '%s' (mesh: %s, tags: %d)"),
			ResourceData.Transforms.Num(), *ResourceData.ResourceNodeId.ToString(),
			*ResourceData.Mesh->GetName(), ComponentTags.Num());

		ComponentsCreated++;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOResourceSpawner] Spawned %d instances across %d components (%d points processed)"),
		InstancesAdded, ComponentsCreated, TotalPointsProcessed);

	return true;
}

TMap<FName, FMOPCGResourceSpawnerElement::FResourceSpawnData> FMOPCGResourceSpawnerElement::BuildResourceDataMap(
	const TArray<FMOPCGResourceEntry>& Resources,
	const UDataTable* DataTable,
	FRandomStream& RandomStream) const
{
	TMap<FName, FResourceSpawnData> Result;

	if (!DataTable)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] No DataTable provided to BuildResourceDataMap"));
		return Result;
	}

	for (const FMOPCGResourceEntry& Entry : Resources)
	{
		// Get the resource definition from DataTable
		const FMOResourceNodeDefinitionRow* Definition = Entry.GetResourceDefinition(DataTable);
		if (!Definition)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] Resource row '%s' not found in DataTable '%s'"),
				*Entry.ResourceRowName.ToString(),
				*DataTable->GetName());
			continue;
		}

		const FName RowName = Entry.ResourceRowName;

		// Select a mesh variation
		const FMOResourceMeshVariation* SelectedVariation = Definition->SelectMeshVariation(RandomStream);
		if (!SelectedVariation)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] No mesh variations defined for resource '%s'"),
				*RowName.ToString());
			continue;
		}

		// Load the mesh
		UStaticMesh* Mesh = SelectedVariation->Mesh.LoadSynchronous();
		if (!Mesh)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] Failed to load mesh for resource '%s'"),
				*RowName.ToString());
			continue;
		}

		// Create spawn data
		FResourceSpawnData& ResourceData = Result.Add(RowName);
		ResourceData.ResourceNodeId = RowName;
		ResourceData.DisplayName = Definition->DisplayName;
		ResourceData.Mesh = Mesh;
		ResourceData.ResourceType = Definition->ResourceType;
		ResourceData.MinScale = Entry.GetEffectiveMinScale(DataTable);
		ResourceData.MaxScale = Entry.GetEffectiveMaxScale(DataTable);
		ResourceData.bRandomizeRotation = Definition->bRandomizeRotation;

		// Load material override if specified
		if (!SelectedVariation->MaterialOverride.IsNull())
		{
			ResourceData.MaterialOverride = SelectedVariation->MaterialOverride.LoadSynchronous();
		}

		// Get all auto-generated tags from the DataTable definition
		ResourceData.AllTags = Definition->GetAllTags();

		UE_LOG(LogMOFramework, Verbose, TEXT("[MOResourceSpawner] Loaded resource '%s' with %d tags, mesh '%s'"),
			*RowName.ToString(), ResourceData.AllTags.Num(), *Mesh->GetName());
	}

	return Result;
}

UInstancedStaticMeshComponent* FMOPCGResourceSpawnerElement::GetOrCreateManagedISMC(
	FPCGContext* Context,
	AActor* TargetActor,
	UStaticMesh* Mesh,
	const UMOPCGResourceSpawnerSettings* Settings,
	FName ResourceNodeId,
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
		UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] No source PCG component found"));
		return nullptr;
	}

	// Build ISM component descriptor with tags
	FPCGISMComponentBuilderParams Params;
	Params.Descriptor.StaticMesh = Mesh;
	Params.Descriptor.ComponentClass = UHierarchicalInstancedStaticMeshComponent::StaticClass();
	Params.Descriptor.BodyInstance.SetCollisionProfileName(Settings->CollisionProfile.Name);
	Params.Descriptor.bCastShadow = Settings->bCastShadows;
	Params.Descriptor.bAffectDistanceFieldLighting = false;
	Params.Descriptor.bAffectDynamicIndirectLighting = false;
	Params.Descriptor.ComponentTags = ComponentTags;
	Params.NumCustomDataFloats = 0;

	// Use PCG's managed component system - handles cleanup automatically on re-execution
	UInstancedStaticMeshComponent* ISM = UPCGActorHelpers::GetOrCreateISMC(
		TargetActor, SourceComponent, Params, Context);

	return ISM;
}

// ============================================================================
// SPECIALIZED SPAWNERS
// ============================================================================

UMOPCGTreeSpawnerSettings::UMOPCGTreeSpawnerSettings()
{
	CollisionProfile = FCollisionProfileName(TEXT("BlockAll"));
	bCastShadows = true;
}

#if WITH_EDITOR
FText UMOPCGTreeSpawnerSettings::GetNodeTooltipText() const
{
	return LOCTEXT("TreeTooltip",
		"Spawns trees as harvestable HISM components.\n\n"
		"All yields and tags are now defined in DT_ResourceNodes.\n"
		"Select tree resources via the dropdown in ResourcesToSpawn.\n\n"
		"Trees typically:\n"
		"- Yield bark, sticks, wood, leaves\n"
		"- Keep mesh on harvest (KeepOnHarvest)\n"
		"- Have random scale and rotation");
}
#endif

UMOPCGBushSpawnerSettings::UMOPCGBushSpawnerSettings()
{
	CollisionProfile = FCollisionProfileName(TEXT("OverlapAllDynamic"));
	bCastShadows = true;
}

#if WITH_EDITOR
FText UMOPCGBushSpawnerSettings::GetNodeTooltipText() const
{
	return LOCTEXT("BushTooltip",
		"Spawns bushes and shrubs as harvestable HISM components.\n\n"
		"All yields and tags are now defined in DT_ResourceNodes.\n"
		"Select bush resources via the dropdown in ResourcesToSpawn.\n\n"
		"Bushes typically:\n"
		"- Yield berries, twigs, leaves\n"
		"- Have overlap collision for walking through\n"
		"- Keep mesh on harvest (KeepOnHarvest)");
}
#endif

UMOPCGRockSpawnerSettings::UMOPCGRockSpawnerSettings()
{
	CollisionProfile = FCollisionProfileName(TEXT("BlockAll"));
	bCastShadows = true;
}

#if WITH_EDITOR
FText UMOPCGRockSpawnerSettings::GetNodeTooltipText() const
{
	return LOCTEXT("RockTooltip",
		"Spawns rocks and mineral deposits as harvestable HISM components.\n\n"
		"All yields and tags are now defined in DT_ResourceNodes.\n"
		"Select rock resources via the dropdown in ResourcesToSpawn.\n\n"
		"Rocks typically:\n"
		"- Yield stone, ore, minerals\n"
		"- Are destroyed on harvest (no KeepOnHarvest)\n"
		"- Have random scale and rotation");
}
#endif

#undef LOCTEXT_NAMESPACE
