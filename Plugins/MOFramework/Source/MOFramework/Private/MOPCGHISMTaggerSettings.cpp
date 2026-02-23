#include "MOPCGHISMTaggerSettings.h"
#include "MOFramework.h"
#include "MOItemDefinitionRow.h"
#include "MOPCGInteractionSubsystem.h"

#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "MOPCGHISMTagger"

// ============================================================================
// SETTINGS
// ============================================================================

UMOPCGHISMTaggerSettings::UMOPCGHISMTaggerSettings()
{
#if WITH_EDITOR
	Category = LOCTEXT("Category", "MO");
#endif
}

FPCGElementPtr UMOPCGHISMTaggerSettings::CreateElement() const
{
	return MakeShared<FMOPCGHISMTaggerElement>();
}

#if WITH_EDITOR
FText UMOPCGHISMTaggerSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Tags HISM components with their corresponding item IDs.\n\n"
		"Place AFTER Static Mesh Spawner in your PCG graph.\n"
		"Adds tags like 'MOItem_FlintNodule01' to HISM components,\n"
		"making them discoverable by the foraging system's Search Nearby feature.");
}
#endif

TArray<FPCGPinProperties> UMOPCGHISMTaggerSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	// Accept any data - we just need to run after components are created
	Properties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Any, true, true);
	return Properties;
}

TArray<FPCGPinProperties> UMOPCGHISMTaggerSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	// Pass through the input
	Properties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Any, false, false);
	return Properties;
}

// ============================================================================
// ELEMENT
// ============================================================================

bool FMOPCGHISMTaggerElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMOPCGHISMTaggerElement::Execute);

	const UMOPCGHISMTaggerSettings* Settings = Context->GetInputSettings<UMOPCGHISMTaggerSettings>();
	check(Settings);

	// Validate settings
	if (!Settings->ItemDataTable)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGHISMTagger] No ItemDataTable specified"));
		return true;
	}

	// Build mesh-to-item lookup
	TMap<FSoftObjectPath, FName> MeshToItemMap = BuildMeshToItemMap(Settings->ItemDataTable);
	if (MeshToItemMap.Num() == 0)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGHISMTagger] No mesh-to-item mappings found in datatable"));
		return true;
	}

	// Get the target actor that owns the PCG-generated components
	AActor* Owner = Context->GetTargetActor(nullptr);
	if (!Owner)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGHISMTagger] No target actor found"));
		return true;
	}

	// Get PCG interaction subsystem for registering tag mappings
	UMOPCGInteractionSubsystem* PCGSubsystem = nullptr;
	if (Settings->bRegisterWithSubsystem)
	{
		UWorld* World = Owner->GetWorld();
		if (World)
		{
			PCGSubsystem = World->GetSubsystem<UMOPCGInteractionSubsystem>();
		}
	}

	// Find all HISM/ISM components on the owner actor
	TArray<UInstancedStaticMeshComponent*> ISMComponents;
	Owner->GetComponents<UInstancedStaticMeshComponent>(ISMComponents);

	int32 TaggedCount = 0;

	for (UInstancedStaticMeshComponent* ISM : ISMComponents)
	{
		if (!ISM)
		{
			continue;
		}

		UStaticMesh* Mesh = ISM->GetStaticMesh();
		if (!Mesh)
		{
			continue;
		}

		// Look up item ID by mesh
		FSoftObjectPath MeshPath(Mesh);
		FName ItemId = NAME_None;

		// Try direct lookup first
		const FName* FoundItem = MeshToItemMap.Find(MeshPath);
		if (FoundItem)
		{
			ItemId = *FoundItem;
		}
		else
		{
			// Try by asset path string comparison (handles format differences)
			const FString MeshAssetPath = MeshPath.GetAssetPathString();
			for (const auto& Pair : MeshToItemMap)
			{
				if (Pair.Key.GetAssetPathString() == MeshAssetPath)
				{
					ItemId = Pair.Value;
					break;
				}
			}
		}

		if (ItemId.IsNone())
		{
			UE_LOG(LogMOFramework, Verbose, TEXT("[MOPCGHISMTagger] No item mapping for mesh '%s'"),
				*Mesh->GetPathName());
			continue;
		}

		// Create tag name
		const FName TagName(*FString::Printf(TEXT("%s%s"), *Settings->TagPrefix, *ItemId.ToString()));

		// Add tag to component if not already present
		if (!ISM->ComponentHasTag(TagName))
		{
			ISM->ComponentTags.Add(TagName);
			TaggedCount++;

			UE_LOG(LogMOFramework, Log, TEXT("[MOPCGHISMTagger] Tagged HISM component with '%s' (mesh: %s, instances: %d)"),
				*TagName.ToString(), *Mesh->GetName(), ISM->GetInstanceCount());
		}

		// Register tag mapping with subsystem
		if (PCGSubsystem)
		{
			PCGSubsystem->RegisterTagItemMapping(TagName, ItemId);
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOPCGHISMTagger] Tagged %d HISM components on '%s'"),
		TaggedCount, *GetNameSafe(Owner));

	// Pass through input data
	Context->OutputData = Context->InputData;

	return true;
}

TMap<FSoftObjectPath, FName> FMOPCGHISMTaggerElement::BuildMeshToItemMap(const UDataTable* DataTable) const
{
	TMap<FSoftObjectPath, FName> Result;

	if (!DataTable)
	{
		return Result;
	}

	static const FString ContextString(TEXT("MOPCGHISMTagger"));
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

		// Add to map
		FSoftObjectPath MeshPath = Item->WorldVisual.StaticMesh.ToSoftObjectPath();
		Result.Add(MeshPath, Item->ItemId);

		UE_LOG(LogMOFramework, Verbose, TEXT("[MOPCGHISMTagger] Mapped mesh '%s' -> item '%s'"),
			*MeshPath.GetAssetPathString(), *Item->ItemId.ToString());
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE
