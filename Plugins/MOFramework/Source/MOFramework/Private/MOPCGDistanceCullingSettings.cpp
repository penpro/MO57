#include "MOPCGDistanceCullingSettings.h"
#include "MOFramework.h"

#include "PCGContext.h"
#include "PCGPoint.h"
#include "PCGComponent.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

// PCG Runtime Generation includes for gen source access
#include "Subsystems/PCGSubsystem.h"
#include "RuntimeGen/PCGGenSourceManager.h"
#include "RuntimeGen/GenSources/PCGGenSourceBase.h"
#include "PCGWorldActor.h"

#define LOCTEXT_NAMESPACE "MOPCGDistanceCulling"

// Output pin names
namespace MOPCGDistancePins
{
	const FName Near = TEXT("Near");
	const FName Far = TEXT("Far");
}

// ============================================================================
// SETTINGS
// ============================================================================

UMOPCGDistanceCullingSettings::UMOPCGDistanceCullingSettings()
{
#if WITH_EDITOR
	Category = LOCTEXT("Category", "MO");
#endif
}

FPCGElementPtr UMOPCGDistanceCullingSettings::CreateElement() const
{
	return MakeShared<FMOPCGDistanceCullingElement>();
}

#if WITH_EDITOR
FText UMOPCGDistanceCullingSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Splits input points based on distance from the player or generation source.\n\n"
		"Near output: Points within NearDistance (full detail)\n"
		"Far output: Points beyond FarDistance (reduced detail)\n"
		"Points in between are probabilistically distributed if falloff is enabled.\n\n"
		"Use this after voxel sampling to split trees into full meshes vs. billboards.");
}
#endif

TArray<FPCGPinProperties> UMOPCGDistanceCullingSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point, true, true);
	return Properties;
}

TArray<FPCGPinProperties> UMOPCGDistanceCullingSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;

	FPCGPinProperties& NearPin = Properties.Emplace_GetRef(MOPCGDistancePins::Near, EPCGDataType::Point, false, false);
	FPCGPinProperties& FarPin = Properties.Emplace_GetRef(MOPCGDistancePins::Far, EPCGDataType::Point, false, false);

#if WITH_EDITOR
	NearPin.Tooltip = LOCTEXT("NearPinTooltip", "Points within near distance - use for full detail meshes");
	FarPin.Tooltip = LOCTEXT("FarPinTooltip", "Points beyond far distance - use for reduced detail/billboards");
#endif

	return Properties;
}

// ============================================================================
// ELEMENT
// ============================================================================

bool FMOPCGDistanceCullingElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMOPCGDistanceCullingElement::Execute);

	const UMOPCGDistanceCullingSettings* Settings = Context->GetInputSettings<UMOPCGDistanceCullingSettings>();
	check(Settings);

	// Get reference position
	FVector ReferencePosition;
	if (!GetReferencePosition(Context, Settings, ReferencePosition))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGDistanceCulling] Could not determine reference position, passing all points to Near output"));

		// Pass through all points to Near output
		TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
		for (const FPCGTaggedData& Input : Inputs)
		{
			FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
			Output.Pin = MOPCGDistancePins::Near;
		}
		return true;
	}

	// Validate distance settings
	const float NearDistSq = FMath::Square(Settings->NearDistance);
	const float FarDistSq = FMath::Square(Settings->FarDistance);
	const float TransitionRange = Settings->FarDistance - Settings->NearDistance;
	const bool bHasTransition = TransitionRange > KINDA_SMALL_NUMBER;

	// Create random stream for falloff
	FRandomStream RandomStream(Context->GetSeed() + Settings->SeedOffset);

	// Get input points
	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	int32 TotalNear = 0;
	int32 TotalFar = 0;

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
		UPCGPointData* NearOutput = NewObject<UPCGPointData>();
		NearOutput->InitializeFromData(InputPointData);
		TArray<FPCGPoint>& NearPoints = NearOutput->GetMutablePoints();
		NearPoints.Reserve(InputPoints.Num() / 2);

		UPCGPointData* FarOutput = NewObject<UPCGPointData>();
		FarOutput->InitializeFromData(InputPointData);
		TArray<FPCGPoint>& FarPoints = FarOutput->GetMutablePoints();
		FarPoints.Reserve(InputPoints.Num() / 2);

		// Optional distance attribute
		FPCGMetadataAttribute<float>* DistanceAttrNear = nullptr;
		FPCGMetadataAttribute<float>* DistanceAttrFar = nullptr;
		if (Settings->bOutputDistanceAttribute)
		{
			DistanceAttrNear = NearOutput->Metadata->CreateAttribute<float>(TEXT("MODistance"), 0.0f, true, true);
			DistanceAttrFar = FarOutput->Metadata->CreateAttribute<float>(TEXT("MODistance"), 0.0f, true, true);
		}

		// Process each point
		for (const FPCGPoint& InputPoint : InputPoints)
		{
			const FVector PointLocation = InputPoint.Transform.GetLocation();

			// Calculate distance
			float DistSq;
			if (Settings->bUse2DDistance)
			{
				DistSq = FVector::DistSquaredXY(PointLocation, ReferencePosition);
			}
			else
			{
				DistSq = FVector::DistSquared(PointLocation, ReferencePosition);
			}

			// Determine which output this point goes to
			bool bOutputToNear = false;

			if (DistSq <= NearDistSq)
			{
				// Definitely near
				bOutputToNear = true;
			}
			else if (DistSq >= FarDistSq)
			{
				// Definitely far
				bOutputToNear = false;
			}
			else if (bHasTransition && Settings->bUseFalloff)
			{
				// In transition zone - probabilistic
				const float Dist = FMath::Sqrt(DistSq);
				const float T = (Dist - Settings->NearDistance) / TransitionRange;
				// T goes from 0 (at NearDistance) to 1 (at FarDistance)
				// Probability of being "near" decreases as we get further
				bOutputToNear = RandomStream.FRand() > T;
			}
			else
			{
				// No falloff, use midpoint as threshold
				bOutputToNear = DistSq < (NearDistSq + FarDistSq) * 0.5f;
			}

			// Add point to appropriate output
			if (bOutputToNear)
			{
				FPCGPoint& NewPoint = NearPoints.Add_GetRef(InputPoint);
				if (DistanceAttrNear)
				{
					NewPoint.MetadataEntry = NearOutput->Metadata->AddEntry();
					DistanceAttrNear->SetValue(NewPoint.MetadataEntry, FMath::Sqrt(DistSq));
				}
			}
			else
			{
				FPCGPoint& NewPoint = FarPoints.Add_GetRef(InputPoint);
				if (DistanceAttrFar)
				{
					NewPoint.MetadataEntry = FarOutput->Metadata->AddEntry();
					DistanceAttrFar->SetValue(NewPoint.MetadataEntry, FMath::Sqrt(DistSq));
				}
			}
		}

		// Add outputs to context
		{
			FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
			Output.Data = NearOutput;
			Output.Pin = MOPCGDistancePins::Near;
		}
		{
			FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
			Output.Data = FarOutput;
			Output.Pin = MOPCGDistancePins::Far;
		}

		TotalNear += NearPoints.Num();
		TotalFar += FarPoints.Num();
	}

	return true;
}

bool FMOPCGDistanceCullingElement::GetReferencePosition(FPCGContext* Context, const UMOPCGDistanceCullingSettings* Settings, FVector& OutPosition) const
{
	bool bSuccess = false;

	switch (Settings->DistanceSource)
	{
	case EMOPCGDistanceSource::FirstLocalPlayer:
		bSuccess = GetFirstLocalPlayerPosition(Context, OutPosition);
		break;

	case EMOPCGDistanceSource::NearestGenSource:
		bSuccess = GetNearestGenSourcePosition(Context, OutPosition);
		break;

	case EMOPCGDistanceSource::ManualLocation:
		OutPosition = Settings->ManualReferenceLocation;
		bSuccess = true;
		break;

	default:
		break;
	}

	// Final fallback to execution source position
	if (!bSuccess)
	{
		bSuccess = GetExecutionSourcePosition(Context, OutPosition);
	}

	return bSuccess;
}

bool FMOPCGDistanceCullingElement::GetFirstLocalPlayerPosition(FPCGContext* Context, FVector& OutPosition) const
{
	// Get world from execution source
	UWorld* World = nullptr;
	if (Context->ExecutionSource.IsValid())
	{
		World = Context->ExecutionSource->GetExecutionState().GetWorld();
	}

	if (!World)
	{
		// Fallback to execution source transform (PCG component location)
		return GetExecutionSourcePosition(Context, OutPosition);
	}

	// Get first local player controller
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		// No player controller (editor mode) - use execution source position
		return GetExecutionSourcePosition(Context, OutPosition);
	}

	// Get pawn location (preferred) or controller view location
	if (APawn* Pawn = PC->GetPawn())
	{
		OutPosition = Pawn->GetActorLocation();
		return true;
	}

	// Fallback to view location
	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	OutPosition = ViewLocation;
	return true;
}

bool FMOPCGDistanceCullingElement::GetExecutionSourcePosition(FPCGContext* Context, FVector& OutPosition) const
{
	if (Context->ExecutionSource.IsValid())
	{
		// Use the execution source's transform (PCG component's world location)
		OutPosition = Context->ExecutionSource->GetExecutionState().GetTransform().GetLocation();
		return true;
	}
	return false;
}

bool FMOPCGDistanceCullingElement::GetNearestGenSourcePosition(FPCGContext* Context, FVector& OutPosition) const
{
	// Get world from execution source
	UWorld* World = nullptr;
	if (Context->ExecutionSource.IsValid())
	{
		World = Context->ExecutionSource->GetExecutionState().GetWorld();
	}

	if (!World)
	{
		return GetExecutionSourcePosition(Context, OutPosition);
	}

	// Get PCG subsystem
	UPCGSubsystem* PCGSubsystem = World->GetSubsystem<UPCGSubsystem>();
	if (!PCGSubsystem)
	{
		return GetFirstLocalPlayerPosition(Context, OutPosition);
	}

	// Get generation source manager and world actor
	FPCGGenSourceManager* GenSourceManager = PCGSubsystem->GetGenSourceManager();
	APCGWorldActor* PCGWorldActor = PCGSubsystem->GetPCGWorldActor();

	if (!GenSourceManager || !PCGWorldActor)
	{
		return GetFirstLocalPlayerPosition(Context, OutPosition);
	}

	// Get all generation sources
	TSet<IPCGGenSourceBase*> GenSources = GenSourceManager->GetAllGenSources(PCGWorldActor);

	if (GenSources.Num() == 0)
	{
		return GetFirstLocalPlayerPosition(Context, OutPosition);
	}

	// Find the position of the first valid gen source
	// For now we just use the first one - in multiplayer you might want the nearest
	for (IPCGGenSourceBase* GenSource : GenSources)
	{
		if (GenSource)
		{
			TOptional<FVector> Position = GenSource->GetPosition();
			if (Position.IsSet())
			{
				OutPosition = Position.GetValue();
				return true;
			}
		}
	}

	return GetFirstLocalPlayerPosition(Context, OutPosition);
}

#undef LOCTEXT_NAMESPACE
