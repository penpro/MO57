#include "MOPCGResourceSpawnerSettings.h"
#include "MOFramework.h"
#include "MOItemDefinitionRow.h"
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
		"Features:\n"
		"- Weighted random resource selection\n"
		"- Scale randomization per instance\n"
		"- KeepOnHarvest support for renewable resources\n"
		"- Automatic tag registration for foraging discovery");
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

// Helper to generate consistent resource key (matches BuildResourceDataMap logic)
static FName GetResourceKey(const FMOPCGResourceEntry& Entry)
{
	// If ItemId is provided, use that
	if (!Entry.ItemEntry.ItemId.IsNone())
	{
		return Entry.ItemEntry.ItemId;
	}

	// Otherwise, use mesh name if override mesh is set
	if (!Entry.OverrideMesh.IsNull())
	{
		if (UStaticMesh* Mesh = Entry.OverrideMesh.LoadSynchronous())
		{
			return FName(*Mesh->GetName());
		}
	}

	return NAME_None;
}

bool FMOPCGResourceSpawnerElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMOPCGResourceSpawnerElement::Execute);

	const UMOPCGResourceSpawnerSettings* Settings = Context->GetInputSettings<UMOPCGResourceSpawnerSettings>();
	check(Settings);

	// Validate settings
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

	// Build resource data map (loads meshes)
	TMap<FName, FResourceSpawnData> ResourceDataMap = BuildResourceDataMap(Settings->ItemDataTable, Settings->ResourcesToSpawn);

	// Calculate total weight using utility (only count valid resources)
	// Use GetResourceKey to match the key generation logic in BuildResourceDataMap
	auto HasValidMesh = [&ResourceDataMap](const FMOPCGResourceEntry& Entry) -> bool
	{
		const FName ResourceKey = GetResourceKey(Entry);
		return !ResourceKey.IsNone() && ResourceDataMap.Contains(ResourceKey) && ResourceDataMap[ResourceKey].Mesh != nullptr;
	};
	const float TotalWeight = FMOWeightedSelector::CalculateTotalWeightIf(Settings->ResourcesToSpawn, HasValidMesh);

	if (TotalWeight <= 0.0f)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] No valid resources with meshes found (map has %d entries)"), ResourceDataMap.Num());
		return true;
	}

	// Get input points
	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	// Process each point
	FRandomStream RandomStream(Context->GetSeed() + Settings->SeedOffset);
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
				Settings->ResourcesToSpawn, TotalWeight, RandomStream, HasValidMesh);

			if (!SelectedEntry)
			{
				continue;
			}

			// Get resource data using consistent key generation
			const FName ResourceKey = GetResourceKey(*SelectedEntry);
			FResourceSpawnData* ResourceData = ResourceDataMap.Find(ResourceKey);
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

				// Random rotation on Z
				if (Settings->bRandomizeRotation)
				{
					FRotator CurrentRotation = InstanceTransform.GetRotation().Rotator();
					CurrentRotation.Yaw = RandomStream.FRandRange(0.0f, 360.0f);
					InstanceTransform.SetRotation(CurrentRotation.Quaternion());
				}

				// Random scale
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

	// Get resource type tag
	const FName ResourceTypeTag = GetResourceTypeTag(Settings->ResourceType);

	// Get yield tags if this is a specialized spawner (Tree or Bush)
	// Yield tags use "GivesX" format to match harvest recipe RequiredTargetTag
	TArray<FName> YieldTags;
	if (const UMOPCGTreeSpawnerSettings* TreeSettings = Cast<UMOPCGTreeSpawnerSettings>(Settings))
	{
		YieldTags = TreeSettings->GetYieldTags();
	}
	else if (const UMOPCGBushSpawnerSettings* BushSettings = Cast<UMOPCGBushSpawnerSettings>(Settings))
	{
		YieldTags = BushSettings->GetYieldTags();
	}

	for (auto& Pair : ResourceDataMap)
	{
		FResourceSpawnData& ResourceData = Pair.Value;

		if (ResourceData.Transforms.Num() == 0 || !ResourceData.Mesh)
		{
			continue;
		}

		// Build component tags for this resource
		TArray<FName> ComponentTags;

		// Item tag (e.g., "MOItem_Stick")
		const FName ItemTagName(*FString::Printf(TEXT("%s%s"), *Settings->TagPrefix, *ResourceData.ItemId.ToString()));
		ComponentTags.Add(ItemTagName);

		// Resource type tag (e.g., "MOResource_Tree")
		if (!ResourceTypeTag.IsNone())
		{
			ComponentTags.Add(ResourceTypeTag);
		}

		// KeepOnHarvest tag
		if (ResourceData.bKeepOnHarvest)
		{
			ComponentTags.Add(FName(TEXT("KeepOnHarvest")));
		}

		// Additional custom tags
		for (const FName& AdditionalTag : Settings->AdditionalTags)
		{
			if (!AdditionalTag.IsNone())
			{
				ComponentTags.Add(AdditionalTag);
			}
		}

		// Add yield tags (from Tree/Bush specialized spawners)
		for (const FName& YieldTag : YieldTags)
		{
			if (!YieldTag.IsNone() && !ComponentTags.Contains(YieldTag))
			{
				ComponentTags.Add(YieldTag);
			}
		}

		// Get or create managed ISM component with tags (PCG handles cleanup on re-execution)
		UInstancedStaticMeshComponent* ISM = GetOrCreateManagedISMC(
			Context, TargetActor, ResourceData.Mesh, Settings, ResourceData.ItemId, ComponentTags);

		if (!ISM)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] Failed to create ISM for resource '%s'"),
				*ResourceData.ItemId.ToString());
			continue;
		}

		// Register tag mapping with subsystem (for runtime lookup)
		// Note: Yield tags (GivesX) are semantic tags for recipe matching, not item tags
		if (PCGSubsystem)
		{
			PCGSubsystem->RegisterTagItemMapping(ItemTagName, ResourceData.ItemId);
		}

		// Add instances (bWorldSpace=true to handle world-space transforms correctly)
		ISM->AddInstances(ResourceData.Transforms, false, true);
		InstancesAdded += ResourceData.Transforms.Num();

		UE_LOG(LogMOFramework, Verbose, TEXT("[MOResourceSpawner] Added %d instances of '%s' (mesh: %s, tags: %d)"),
			ResourceData.Transforms.Num(), *ResourceData.ItemId.ToString(), *ResourceData.Mesh->GetName(), ComponentTags.Num());

		ComponentsCreated++;
	}

	// Log summary with yield info
	if (YieldTags.Num() > 0)
	{
		FString YieldTagsStr;
		for (const FName& Tag : YieldTags)
		{
			if (!YieldTagsStr.IsEmpty()) YieldTagsStr += TEXT(", ");
			YieldTagsStr += Tag.ToString();
		}
		UE_LOG(LogMOFramework, Log, TEXT("[MOResourceSpawner] Spawned %d instances across %d components with yields: %s"),
			InstancesAdded, ComponentsCreated, *YieldTagsStr);
	}
	else
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOResourceSpawner] Spawned %d instances across %d components (%d points processed)"),
			InstancesAdded, ComponentsCreated, TotalPointsProcessed);
	}

	return true;
}

TMap<FName, FMOPCGResourceSpawnerElement::FResourceSpawnData> FMOPCGResourceSpawnerElement::BuildResourceDataMap(
	const UDataTable* DataTable,
	const TArray<FMOPCGResourceEntry>& Resources) const
{
	TMap<FName, FResourceSpawnData> Result;

	for (const FMOPCGResourceEntry& Entry : Resources)
	{
		// Generate a unique key for this entry
		// Use ItemId if provided, otherwise generate from mesh name
		FName ResourceKey = Entry.ItemEntry.ItemId;

		// Determine mesh (override takes priority)
		UStaticMesh* Mesh = nullptr;
		if (!Entry.OverrideMesh.IsNull())
		{
			Mesh = Entry.OverrideMesh.LoadSynchronous();

			// If no ItemId but we have an override mesh, use mesh name as key
			if (ResourceKey.IsNone() && Mesh)
			{
				ResourceKey = FName(*Mesh->GetName());
			}
		}
		else if (DataTable && !Entry.ItemEntry.ItemId.IsNone())
		{
			// Try to get mesh from datatable
			const FMOItemDefinitionRow* Row = DataTable->FindRow<FMOItemDefinitionRow>(Entry.ItemEntry.ItemId, TEXT("MOResourceSpawner"));
			if (Row && !Row->WorldVisual.StaticMesh.IsNull())
			{
				Mesh = Row->WorldVisual.StaticMesh.LoadSynchronous();
			}
			else if (!Row)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] Item '%s' not found in datatable (set OverrideMesh to spawn without datatable entry)"),
					*Entry.ItemEntry.ItemId.ToString());
			}
		}

		if (!Mesh)
		{
			if (!Entry.OverrideMesh.IsNull())
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] Failed to load override mesh"));
			}
			else
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] No mesh for resource (set OverrideMesh or ensure ItemId has WorldVisual.StaticMesh in datatable)"));
			}
			continue;
		}

		if (ResourceKey.IsNone())
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] No valid key for resource entry"));
			continue;
		}

		FResourceSpawnData& ResourceData = Result.Add(ResourceKey);
		ResourceData.ItemId = ResourceKey;
		ResourceData.Mesh = Mesh;
		ResourceData.bKeepOnHarvest = Entry.bKeepOnHarvest;
		ResourceData.MinScale = Entry.MinScale;
		ResourceData.MaxScale = Entry.MaxScale;
	}

	return Result;
}

UInstancedStaticMeshComponent* FMOPCGResourceSpawnerElement::GetOrCreateManagedISMC(
	FPCGContext* Context,
	AActor* TargetActor,
	UStaticMesh* Mesh,
	const UMOPCGResourceSpawnerSettings* Settings,
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
		UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] No source PCG component found"));
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

FName FMOPCGResourceSpawnerElement::GetResourceTypeTag(EMOResourceType Type) const
{
	switch (Type)
	{
	case EMOResourceType::Tree:
		return FName(TEXT("MOResource_Tree"));
	case EMOResourceType::Bush:
		return FName(TEXT("MOResource_Bush"));
	case EMOResourceType::Rock:
		return FName(TEXT("MOResource_Rock"));
	case EMOResourceType::Ore:
		return FName(TEXT("MOResource_Ore"));
	case EMOResourceType::Plant:
		return FName(TEXT("MOResource_Plant"));
	default:
		return NAME_None;
	}
}

// ============================================================================
// SPECIALIZED SPAWNERS
// ============================================================================

UMOPCGTreeSpawnerSettings::UMOPCGTreeSpawnerSettings()
{
	ResourceType = EMOResourceType::Tree;
	CollisionProfile = FCollisionProfileName(TEXT("BlockAll"));
	bCastShadows = true;
	bRandomizeRotation = true;

	// Trees typically keep on harvest (yield wood without being destroyed)
	// Can be overridden per-resource entry
}

#if WITH_EDITOR
FText UMOPCGTreeSpawnerSettings::GetNodeTooltipText() const
{
	return LOCTEXT("TreeTooltip",
		"Spawns trees as harvestable HISM components.\n\n"
		"Tree Yields section configures what ALL trees spawned by this node yield:\n"
		"- Bark, Sticks, Wood, Leaves\n\n"
		"Features:\n"
		"- Yield resources without being destroyed (KeepOnHarvest)\n"
		"- Have random scale and rotation variation\n"
		"- Be discoverable by the foraging system");
}
#endif

TArray<FName> UMOPCGTreeSpawnerSettings::GetYieldTags() const
{
	TArray<FName> YieldTags;

	// Yield tags use "Gives<ItemType>" format to match harvest recipe RequiredTargetTag
	// e.g., "GivesBark" matches HarvestBark recipe's RequiredTargetTag
	if (bYieldsBark)
	{
		YieldTags.Add(FName(TEXT("GivesBark")));
	}
	if (bYieldsSticks)
	{
		YieldTags.Add(FName(TEXT("GivesStick")));
	}
	if (bYieldsWood)
	{
		YieldTags.Add(FName(TEXT("GivesWood")));
	}
	if (bYieldsLeaves)
	{
		YieldTags.Add(FName(TEXT("GivesLeaves")));
	}

	return YieldTags;
}

UMOPCGBushSpawnerSettings::UMOPCGBushSpawnerSettings()
{
	ResourceType = EMOResourceType::Bush;
	CollisionProfile = FCollisionProfileName(TEXT("OverlapAllDynamic"));
	bCastShadows = true;
	bRandomizeRotation = true;

	// Bushes often have berries that can be harvested without destroying the bush
}

#if WITH_EDITOR
FText UMOPCGBushSpawnerSettings::GetNodeTooltipText() const
{
	return LOCTEXT("BushTooltip",
		"Spawns bushes and shrubs as harvestable HISM components.\n\n"
		"Bush Yields section configures what ALL bushes spawned by this node yield:\n"
		"- Berries, Twigs, Leaves\n\n"
		"Features:\n"
		"- Yield resources without being destroyed\n"
		"- Have overlap collision for walking through\n"
		"- Be discoverable by the foraging system");
}
#endif

TArray<FName> UMOPCGBushSpawnerSettings::GetYieldTags() const
{
	TArray<FName> YieldTags;

	// Yield tags use "Gives<ItemType>" format to match harvest recipe RequiredTargetTag
	if (bYieldsBerries)
	{
		YieldTags.Add(FName(TEXT("GivesBerries")));
	}
	if (bYieldsTwigs)
	{
		YieldTags.Add(FName(TEXT("GivesTwig")));
	}
	if (bYieldsLeaves)
	{
		YieldTags.Add(FName(TEXT("GivesLeaves")));
	}

	return YieldTags;
}

UMOPCGRockSpawnerSettings::UMOPCGRockSpawnerSettings()
{
	ResourceType = EMOResourceType::Rock;
	CollisionProfile = FCollisionProfileName(TEXT("BlockAll"));
	bCastShadows = true;
	bRandomizeRotation = true;

	// Rocks typically are destroyed when harvested
}

#if WITH_EDITOR
FText UMOPCGRockSpawnerSettings::GetNodeTooltipText() const
{
	return LOCTEXT("RockTooltip",
		"Spawns rocks and mineral deposits as harvestable HISM components.\n\n"
		"Rocks can be configured to:\n"
		"- Be destroyed on harvest (for stone/ore)\n"
		"- Have random scale and rotation variation\n"
		"- Be discoverable by the foraging/mining system");
}
#endif

#undef LOCTEXT_NAMESPACE
