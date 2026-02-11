#include "MOPCGItemSpawnerSettings.h"
#include "MOFramework.h"
#include "MOItemDefinitionRow.h"

#include "PCGContext.h"
#include "PCGPoint.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Helpers/PCGHelpers.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"

#define LOCTEXT_NAMESPACE "MOPCGItemSpawner"

// Attribute names for item metadata
namespace MOPCGAttributes
{
	const FName ItemId = TEXT("MOItemId");
	const FName QuantityMin = TEXT("MOQuantityMin");
	const FName QuantityMax = TEXT("MOQuantityMax");
	const FName StaticMesh = TEXT("StaticMesh");
}

// ============================================================================
// SETTINGS
// ============================================================================

UMOPCGItemSpawnerSettings::UMOPCGItemSpawnerSettings()
{
#if WITH_EDITOR
	Category = LOCTEXT("Category", "MO");
#endif
}

FPCGElementPtr UMOPCGItemSpawnerSettings::CreateElement() const
{
	return MakeShared<FMOPCGItemSpawnerElement>();
}

#if WITH_EDITOR
FText UMOPCGItemSpawnerSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Assigns item metadata to input points.\n\n"
		"For each input point, selects an item based on weighted random and sets:\n"
		"- MOItemId: The item definition ID\n"
		"- MOQuantityMin/Max: Harvest quantity range\n"
		"- StaticMesh: The mesh from the item's WorldVisual\n\n"
		"Output can be fed into a Static Mesh Spawner for HISM creation.");
}
#endif

TArray<FPCGPinProperties> UMOPCGItemSpawnerSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point, true, true);
	return Properties;
}

TArray<FPCGPinProperties> UMOPCGItemSpawnerSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point, false, false);
	return Properties;
}

// ============================================================================
// ELEMENT
// ============================================================================

bool FMOPCGItemSpawnerElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMOPCGItemSpawnerElement::Execute);

	const UMOPCGItemSpawnerSettings* Settings = Context->GetInputSettings<UMOPCGItemSpawnerSettings>();
	check(Settings);

	// Validate settings
	if (!Settings->ItemDataTable)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGItemSpawner] No ItemDataTable specified"));
		return true;
	}

	if (Settings->ItemsToSpawn.Num() == 0)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGItemSpawner] No items specified in ItemsToSpawn"));
		return true;
	}

	// Calculate total weight
	float TotalWeight = 0.0f;
	for (const FMOPCGItemSpawnEntry& Entry : Settings->ItemsToSpawn)
	{
		TotalWeight += FMath::Max(0.0f, Entry.Weight);
	}

	if (TotalWeight <= 0.0f)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGItemSpawner] Total weight is zero or negative"));
		return true;
	}

	// Get input points
	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGPointData* InputPointData = Cast<UPCGPointData>(Input.Data);
		if (!InputPointData)
		{
			continue;
		}

		const TArray<FPCGPoint>& InputPoints = InputPointData->GetPoints();
		if (InputPoints.Num() == 0)
		{
			continue;
		}

		// Create output point data
		UPCGPointData* OutputPointData = NewObject<UPCGPointData>();
		OutputPointData->InitializeFromData(InputPointData);
		TArray<FPCGPoint>& OutputPoints = OutputPointData->GetMutablePoints();
		OutputPoints.Reserve(InputPoints.Num());

		// Get/create metadata attributes using FindOrCreateAttribute (proper PCG API)
		UPCGMetadata* Metadata = OutputPointData->MutableMetadata();

		FPCGMetadataAttribute<FString>* ItemIdAttr = Metadata->FindOrCreateAttribute<FString>(
			MOPCGAttributes::ItemId, FString(), false, false, false);
		FPCGMetadataAttribute<int32>* QuantityMinAttr = Metadata->FindOrCreateAttribute<int32>(
			MOPCGAttributes::QuantityMin, 1, false, false, false);
		FPCGMetadataAttribute<int32>* QuantityMaxAttr = Metadata->FindOrCreateAttribute<int32>(
			MOPCGAttributes::QuantityMax, 1, false, false, false);
		FPCGMetadataAttribute<FSoftObjectPath>* MeshAttr = Metadata->FindOrCreateAttribute<FSoftObjectPath>(
			MOPCGAttributes::StaticMesh, FSoftObjectPath(), false, false, false);

		if (!ItemIdAttr || !QuantityMinAttr || !QuantityMaxAttr || !MeshAttr)
		{
			UE_LOG(LogMOFramework, Error, TEXT("[MOPCGItemSpawner] Failed to create metadata attributes"));
			continue;
		}

		// Process each point
		FRandomStream RandomStream(Context->GetSeed() + Settings->SeedOffset);

		for (const FPCGPoint& InputPoint : InputPoints)
		{
			// Select item for this point
			const FMOPCGItemSpawnEntry* SelectedItem = SelectWeightedItem(
				Settings->ItemsToSpawn, TotalWeight, RandomStream);

			if (!SelectedItem)
			{
				continue;
			}

			// Get mesh path for this item (no sync load - PCG runs on worker threads)
			FSoftObjectPath MeshPath = GetMeshPathForItem(Settings->ItemDataTable, SelectedItem->ItemId);

			if (!MeshPath.IsValid() && Settings->bDiscardInvalidPoints)
			{
				UE_LOG(LogMOFramework, Verbose, TEXT("[MOPCGItemSpawner] Discarding point - no mesh for item '%s'"),
					*SelectedItem->ItemId.ToString());
				continue;
			}

			// Create output point
			FPCGPoint& OutputPoint = OutputPoints.Add_GetRef(InputPoint);

			// Create new metadata entry for this point
			OutputPoint.MetadataEntry = Metadata->AddEntry();

			// Set attributes
			ItemIdAttr->SetValue(OutputPoint.MetadataEntry, SelectedItem->ItemId.ToString());
			QuantityMinAttr->SetValue(OutputPoint.MetadataEntry, SelectedItem->MinQuantity);
			QuantityMaxAttr->SetValue(OutputPoint.MetadataEntry, SelectedItem->MaxQuantity);

			if (MeshPath.IsValid())
			{
				MeshAttr->SetValue(OutputPoint.MetadataEntry, MeshPath);
			}
		}

		UE_LOG(LogMOFramework, Log, TEXT("[MOPCGItemSpawner] Generated %d item points from %d input points"),
			OutputPoints.Num(), InputPoints.Num());

		// Add to outputs
		FPCGTaggedData& Output = Outputs.Add_GetRef(Input);
		Output.Data = OutputPointData;
	}

	return true;
}

const FMOPCGItemSpawnEntry* FMOPCGItemSpawnerElement::SelectWeightedItem(
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

FSoftObjectPath FMOPCGItemSpawnerElement::GetMeshPathForItem(
	const UDataTable* DataTable,
	FName ItemId) const
{
	if (!DataTable || ItemId.IsNone())
	{
		return FSoftObjectPath();
	}

	const FMOItemDefinitionRow* Row = DataTable->FindRow<FMOItemDefinitionRow>(ItemId, TEXT("MOPCGItemSpawner"));
	if (!Row)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGItemSpawner] Item '%s' not found in datatable"),
			*ItemId.ToString());
		return FSoftObjectPath();
	}

	// Return the path without loading - PCG runs on worker threads
	if (Row->WorldVisual.StaticMesh.IsNull())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGItemSpawner] Item '%s' has no StaticMesh defined"),
			*ItemId.ToString());
		return FSoftObjectPath();
	}

	return Row->WorldVisual.StaticMesh.ToSoftObjectPath();
}

#undef LOCTEXT_NAMESPACE
