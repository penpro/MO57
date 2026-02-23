#include "MOPCGResourceSpawnerSettings.h"
#include "MOFramework.h"
#include "MOItemDefinitionRow.h"
#include "MOPCGInteractionSubsystem.h"
#include "MOWeightedSelector.h"

#include "PCGContext.h"
#include "PCGPoint.h"
#include "Data/PCGPointData.h"
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

bool FMOPCGResourceSpawnerElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMOPCGResourceSpawnerElement::Execute);

	const UMOPCGResourceSpawnerSettings* Settings = Context->GetInputSettings<UMOPCGResourceSpawnerSettings>();
	check(Settings);

	// Validate settings
	if (!Settings->ItemDataTable)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] No ItemDataTable specified"));
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

	// Build resource data map (loads meshes)
	TMap<FName, FResourceSpawnData> ResourceDataMap = BuildResourceDataMap(Settings->ItemDataTable, Settings->ResourcesToSpawn);

	// Calculate total weight using utility (only count valid resources)
	auto HasValidMesh = [&ResourceDataMap](const FMOPCGResourceEntry& Entry) -> bool
	{
		return ResourceDataMap.Contains(Entry.ItemEntry.ItemId) && ResourceDataMap[Entry.ItemEntry.ItemId].Mesh != nullptr;
	};
	const float TotalWeight = FMOWeightedSelector::CalculateTotalWeightIf(Settings->ResourcesToSpawn, HasValidMesh);

	if (TotalWeight <= 0.0f)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] No valid resources with meshes found"));
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

			// Get resource data
			FResourceSpawnData* ResourceData = ResourceDataMap.Find(SelectedEntry->ItemEntry.ItemId);
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

	// Create HISM components for each resource type
	int32 ComponentsCreated = 0;
	int32 InstancesAdded = 0;

	// Get resource type tag
	const FName ResourceTypeTag = GetResourceTypeTag(Settings->ResourceType);

	for (auto& Pair : ResourceDataMap)
	{
		FResourceSpawnData& ResourceData = Pair.Value;

		if (ResourceData.Transforms.Num() == 0 || !ResourceData.Mesh)
		{
			continue;
		}

		// Get or create HISM component
		UHierarchicalInstancedStaticMeshComponent* HISM = GetOrCreateHISMComponent(
			TargetActor, ResourceData.Mesh, Settings, ResourceData.ItemId);

		if (!HISM)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] Failed to create HISM for resource '%s'"),
				*ResourceData.ItemId.ToString());
			continue;
		}

		// Add item tag
		const FName ItemTagName(*FString::Printf(TEXT("%s%s"), *Settings->TagPrefix, *ResourceData.ItemId.ToString()));
		if (!HISM->ComponentHasTag(ItemTagName))
		{
			HISM->ComponentTags.Add(ItemTagName);

			// Register with subsystem
			if (PCGSubsystem)
			{
				PCGSubsystem->RegisterTagItemMapping(ItemTagName, ResourceData.ItemId);
			}
		}

		// Add resource type tag
		if (!ResourceTypeTag.IsNone() && !HISM->ComponentHasTag(ResourceTypeTag))
		{
			HISM->ComponentTags.Add(ResourceTypeTag);
		}

		// Add KeepOnHarvest tag if applicable
		if (ResourceData.bKeepOnHarvest && !HISM->ComponentHasTag(TEXT("KeepOnHarvest")))
		{
			HISM->ComponentTags.Add(FName(TEXT("KeepOnHarvest")));
		}

		// Add additional custom tags
		for (const FName& AdditionalTag : Settings->AdditionalTags)
		{
			if (!AdditionalTag.IsNone() && !HISM->ComponentHasTag(AdditionalTag))
			{
				HISM->ComponentTags.Add(AdditionalTag);
			}
		}

		// Add instances
		HISM->AddInstances(ResourceData.Transforms, false, true);
		InstancesAdded += ResourceData.Transforms.Num();

		UE_LOG(LogMOFramework, Log, TEXT("[MOResourceSpawner] Added %d instances of '%s' (mesh: %s)"),
			ResourceData.Transforms.Num(), *ResourceData.ItemId.ToString(), *ResourceData.Mesh->GetName());

		ComponentsCreated++;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOResourceSpawner] Spawned %d instances across %d components (%d points processed)"),
		InstancesAdded, ComponentsCreated, TotalPointsProcessed);

	return true;
}

TMap<FName, FMOPCGResourceSpawnerElement::FResourceSpawnData> FMOPCGResourceSpawnerElement::BuildResourceDataMap(
	const UDataTable* DataTable,
	const TArray<FMOPCGResourceEntry>& Resources) const
{
	TMap<FName, FResourceSpawnData> Result;

	if (!DataTable)
	{
		return Result;
	}

	for (const FMOPCGResourceEntry& Entry : Resources)
	{
		if (Entry.ItemEntry.ItemId.IsNone())
		{
			continue;
		}

		// Look up item in datatable
		const FMOItemDefinitionRow* Row = DataTable->FindRow<FMOItemDefinitionRow>(Entry.ItemEntry.ItemId, TEXT("MOResourceSpawner"));
		if (!Row)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] Item '%s' not found in datatable"),
				*Entry.ItemEntry.ItemId.ToString());
			continue;
		}

		// Determine mesh (override or from datatable)
		UStaticMesh* Mesh = nullptr;
		if (!Entry.OverrideMesh.IsNull())
		{
			Mesh = Entry.OverrideMesh.LoadSynchronous();
		}
		else if (!Row->WorldVisual.StaticMesh.IsNull())
		{
			Mesh = Row->WorldVisual.StaticMesh.LoadSynchronous();
		}

		if (!Mesh)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceSpawner] No mesh for resource '%s'"),
				*Entry.ItemEntry.ItemId.ToString());
			continue;
		}

		FResourceSpawnData& ResourceData = Result.Add(Entry.ItemEntry.ItemId);
		ResourceData.ItemId = Entry.ItemEntry.ItemId;
		ResourceData.Mesh = Mesh;
		ResourceData.bKeepOnHarvest = Entry.bKeepOnHarvest;
		ResourceData.MinScale = Entry.MinScale;
		ResourceData.MaxScale = Entry.MaxScale;
	}

	return Result;
}

UHierarchicalInstancedStaticMeshComponent* FMOPCGResourceSpawnerElement::GetOrCreateHISMComponent(
	AActor* TargetActor,
	UStaticMesh* Mesh,
	const UMOPCGResourceSpawnerSettings* Settings,
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
		"Trees can be configured to:\n"
		"- Yield resources without being destroyed (KeepOnHarvest)\n"
		"- Have random scale and rotation variation\n"
		"- Be discoverable by the foraging system");
}
#endif

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
		"Bushes can be configured to:\n"
		"- Yield berries/materials without being destroyed\n"
		"- Have overlap collision for walking through\n"
		"- Be discoverable by the foraging system");
}
#endif

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
