/**
 * MOPCGTerrainModificationFilterSettings.cpp - PCG point filter that drops
 * (or keeps) points inside terrain-modified zones.
 *
 * See header for design rationale. Logs to the harvest debug system under
 * the "PCG-TerrainFilter" category for diagnostics.
 */

#include "MOPCGTerrainModificationFilterSettings.h"
#include "MOFramework.h"
#include "MOTerrainModificationSubsystem.h"
#include "MOHarvestDebugSubsystem.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGPoint.h"
#include "Data/PCGPointData.h"

#define LOCTEXT_NAMESPACE "MOPCGTerrainModFilter"

UMOPCGTerrainModificationFilterSettings::UMOPCGTerrainModificationFilterSettings()
{
#if WITH_EDITOR
	Category = LOCTEXT("Category", "MO");
#endif
}

#if WITH_EDITOR
FText UMOPCGTerrainModificationFilterSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Filters input points against terrain modification zones tracked by "
		"UMOTerrainModificationSubsystem.\n\n"
		"Default mode: drops points INSIDE modified zones (so foliage doesn't "
		"spawn on worked ground).\n\n"
		"Invert mode: passes ONLY points inside modified zones (for crops, "
		"tilled-soil overlays, etc.).\n\n"
		"Insert this node between your point source (e.g. VoxelSampler) and "
		"your spawner chain. Stock UE PCG and Voxel-plugin spawners see the "
		"filtered list automatically — no per-spawner integration needed.");
}
#endif

TArray<FPCGPinProperties> UMOPCGTerrainModificationFilterSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point, /*bAllowMultiple=*/true, /*bAllowMultipleConnections=*/true);
	return Properties;
}

TArray<FPCGPinProperties> UMOPCGTerrainModificationFilterSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return Properties;
}

FPCGElementPtr UMOPCGTerrainModificationFilterSettings::CreateElement() const
{
	return MakeShared<FMOPCGTerrainModificationFilterElement>();
}

// ============================================================================
// ELEMENT
// ============================================================================

bool FMOPCGTerrainModificationFilterElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMOPCGTerrainModificationFilterElement::Execute);

	const UMOPCGTerrainModificationFilterSettings* Settings = Context->GetInputSettings<UMOPCGTerrainModificationFilterSettings>();
	check(Settings);

	AActor* TargetActor = Context->GetTargetActor(nullptr);

	// Resolve the terrain-modification subsystem. If it's missing (e.g.
	// running in a tooling context with no game world), pass everything
	// through unchanged — failing closed would silently delete content.
	UMOTerrainModificationSubsystem* TerrainMod = nullptr;
	if (TargetActor)
	{
		if (UWorld* World = TargetActor->GetWorld())
		{
			TerrainMod = World->GetSubsystem<UMOTerrainModificationSubsystem>();
		}
	}

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	int32 TotalInputPoints = 0;
	int32 TotalOutputPoints = 0;
	int32 TotalDropped = 0;

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGPointData* InputPointData = Cast<UPCGPointData>(Input.Data);
		if (!InputPointData)
		{
			continue;
		}

		const TArray<FPCGPoint>& InputPoints = InputPointData->GetPoints();
		TotalInputPoints += InputPoints.Num();

		// Output container, initialized from input (carries metadata layout etc).
		UPCGPointData* OutputPointData = NewObject<UPCGPointData>();
		OutputPointData->InitializeFromData(InputPointData);
		TArray<FPCGPoint>& OutputPoints = OutputPointData->GetMutablePoints();
		OutputPoints.Reserve(InputPoints.Num());

		for (const FPCGPoint& Point : InputPoints)
		{
			// Default behavior:
			//   bInvert=false: pass-through point if it's NOT in a zone (skip
			//                  foliage on worked ground).
			//   bInvert=true:  pass-through point if it IS in a zone (only
			//                  spawn on worked ground — for crops etc.).
			//
			// If the subsystem is unavailable, treat as "no zones exist" —
			// the bInvert=false case passes everything (safe), the
			// bInvert=true case passes nothing (safe-ish but spawn-suppressing).
			const bool bInZone = TerrainMod
				? TerrainMod->IsLocationModified(Point.Transform.GetLocation())
				: false;

			const bool bKeep = (bInZone == Settings->bInvert);
			if (bKeep)
			{
				OutputPoints.Add(Point);
			}
			else
			{
				++TotalDropped;
			}
		}

		TotalOutputPoints += OutputPoints.Num();

		FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
		Output.Data = OutputPointData;
		Output.Pin = PCGPinConstants::DefaultOutputLabel;
	}

	MOHARVEST_LOG(TargetActor, "PCG-TerrainFilter",
		"Execute: invert=%d terrainSubsystem=%s zoneCount=%d inputPoints=%d output=%d dropped=%d",
		Settings->bInvert ? 1 : 0,
		TerrainMod ? TEXT("FOUND") : TEXT("NULL"),
		TerrainMod ? TerrainMod->GetZoneCount() : -1,
		TotalInputPoints, TotalOutputPoints, TotalDropped);

	return true;
}

#undef LOCTEXT_NAMESPACE
