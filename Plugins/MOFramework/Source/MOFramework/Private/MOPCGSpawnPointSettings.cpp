#include "MOPCGSpawnPointSettings.h"
#include "MOFramework.h"
#include "MOSpawnPoint.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGPoint.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGActorHelpers.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "MOPCGSpawnPointPlacer"

// ============================================================================
// SETTINGS
// ============================================================================

UMOPCGSpawnPointSettings::UMOPCGSpawnPointSettings()
{
	SpawnPointClass = AMOSpawnPoint::StaticClass();

#if WITH_EDITOR
	Category = LOCTEXT("Category", "MO");
#endif
}

FPCGElementPtr UMOPCGSpawnPointSettings::CreateElement() const
{
	return MakeShared<FMOPCGSpawnPointElement>();
}

#if WITH_EDITOR
FText UMOPCGSpawnPointSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Places spawn point actors at input point locations.\n\n"
		"Spawn points are used by the MOSpawnManagerSubsystem to:\n"
		"- Spawn mobs (prey, predators, ambient)\n"
		"- Spawn survivors for recruitment\n\n"
		"Configure category, radius, and cooldown per point.");
}
#endif

TArray<FPCGPinProperties> UMOPCGSpawnPointSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point, true, true);
	return Properties;
}

TArray<FPCGPinProperties> UMOPCGSpawnPointSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	// No output - this is a terminal spawner node
	return Properties;
}

// ============================================================================
// ELEMENT
// ============================================================================

bool FMOPCGSpawnPointElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMOPCGSpawnPointElement::Execute);

	const UMOPCGSpawnPointSettings* Settings = Context->GetInputSettings<UMOPCGSpawnPointSettings>();
	check(Settings);

	// Validate spawn point class
	TSubclassOf<AMOSpawnPoint> SpawnPointClass = Settings->SpawnPointClass;
	if (!SpawnPointClass)
	{
		SpawnPointClass = AMOSpawnPoint::StaticClass();
	}

	// Get target actor
	AActor* TargetActor = Context->GetTargetActor(nullptr);
	if (!TargetActor)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSpawnPointPlacer] No target actor found"));
		return true;
	}

	UWorld* World = TargetActor->GetWorld();
	if (!World)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSpawnPointPlacer] No world found"));
		return true;
	}

	// Get input points
	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	int32 SpawnPointsCreated = 0;

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
			// Spawn the spawn point actor
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			// Extract location and rotation from point transform
			const FVector Location = Point.Transform.GetLocation();
			const FRotator Rotation = Point.Transform.GetRotation().Rotator();

			AMOSpawnPoint* SpawnPoint = World->SpawnActor<AMOSpawnPoint>(
				SpawnPointClass,
				Location,
				Rotation,
				SpawnParams);

			if (SpawnPoint)
			{
				// Configure the spawn point
				SpawnPoint->SpawnCategory = Settings->SpawnCategory;
				SpawnPoint->SpawnRadius = Settings->SpawnRadius;
				SpawnPoint->PointCooldownSeconds = Settings->PointCooldownSeconds;
				SpawnPoint->SelectionWeight = Settings->SelectionWeight;

				SpawnPointsCreated++;
			}
		}
	}

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOSpawnPointPlacer] Created %d spawn points for category %d"),
		SpawnPointsCreated, (int32)Settings->SpawnCategory);

	return true;
}

#undef LOCTEXT_NAMESPACE
