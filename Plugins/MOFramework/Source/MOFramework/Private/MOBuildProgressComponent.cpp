#include "MOBuildProgressComponent.h"
#include "MOInventoryComponent.h"
#include "MORecipeDefinitionRow.h"
#include "MOFramework.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

UMOBuildProgressComponent::UMOBuildProgressComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void UMOBuildProgressComponent::InitializeFromRecipe(const FMORecipeDefinitionRow& Recipe)
{
	BuildParts = Recipe.BuildParts;
	Progress.TotalBuildTime = Recipe.TotalBuildTime;
	Progress.GatherRange = Recipe.BuildRange;
	Progress.State = EMOBuildState::Ghost;
	Progress.ElapsedTime = 0.0f;
	Progress.CurrentPartIndex = 0;
	Progress.CurrentPartProgress = 0.0f;

	// Initialize consumed tracking
	Progress.ConsumedPerPart.SetNum(BuildParts.Num());
	for (int32& Count : Progress.ConsumedPerPart)
	{
		Count = 0;
	}

	CalculateTotalWeight();

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Initialized with %d parts, total time: %.1fs"),
		BuildParts.Num(), Progress.TotalBuildTime);
}

void UMOBuildProgressComponent::CalculateTotalWeight()
{
	TotalWeight = 0;
	for (const FMOBuildPart& Part : BuildParts)
	{
		TotalWeight += Part.Quantity * Part.Weight;
	}

	if (TotalWeight > 0 && Progress.TotalBuildTime > 0.0f)
	{
		TimePerWeight = Progress.TotalBuildTime / static_cast<float>(TotalWeight);
	}
	else
	{
		TimePerWeight = 0.0f;
	}
}

// ============================================================================
// CONSTRUCTION CONTROL
// ============================================================================

void UMOBuildProgressComponent::StartConstruction(const FMOBuildProgress& Options)
{
	if (Progress.State == EMOBuildState::Constructing)
	{
		return;
	}

	// Apply material source options
	Progress.bDrawFromInventory = Options.bDrawFromInventory;
	Progress.bDrawFromNearbyContainers = Options.bDrawFromNearbyContainers;
	Progress.bDrawFromSurroundingArea = Options.bDrawFromSurroundingArea;
	Progress.GatherRange = Options.GatherRange;

	// Start construction
	Progress.State = EMOBuildState::Constructing;

	// Enable tick
	SetComponentTickEnabled(true);

	OnConstructionStarted.Broadcast();

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Construction started"));
}

void UMOBuildProgressComponent::PauseConstruction()
{
	if (Progress.State != EMOBuildState::Constructing)
	{
		return;
	}

	Progress.State = EMOBuildState::Paused;
	SetComponentTickEnabled(false);

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Construction paused at %.1f%%"),
		Progress.GetOverallProgress() * 100.0f);
}

void UMOBuildProgressComponent::ResumeConstruction()
{
	if (Progress.State != EMOBuildState::Paused)
	{
		return;
	}

	Progress.State = EMOBuildState::Constructing;
	SetComponentTickEnabled(true);

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Construction resumed"));
}

void UMOBuildProgressComponent::CancelConstruction(bool bRefundMaterials)
{
	if (Progress.State == EMOBuildState::Complete)
	{
		return;
	}

	// TODO: Implement material refund if bRefundMaterials is true

	Progress.State = EMOBuildState::Ghost;
	Progress.ElapsedTime = 0.0f;
	Progress.CurrentPartIndex = 0;
	Progress.CurrentPartProgress = 0.0f;

	for (int32& Count : Progress.ConsumedPerPart)
	{
		Count = 0;
	}

	SetComponentTickEnabled(false);

	OnConstructionCancelled.Broadcast();

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Construction cancelled"));
}

// ============================================================================
// MATERIAL GATHERING
// ============================================================================

bool UMOBuildProgressComponent::TryGatherNextMaterial()
{
	if (Progress.CurrentPartIndex >= BuildParts.Num())
	{
		return false;
	}

	const FMOBuildPart& CurrentPart = BuildParts[Progress.CurrentPartIndex];
	return TryConsumeMaterial(CurrentPart);
}

TArray<AActor*> UMOBuildProgressComponent::FindMaterialSources(float Range) const
{
	TArray<AActor*> Sources;

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return Sources;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return Sources;
	}

	FVector Origin = Owner->GetActorLocation();

	// Find nearby actors that could provide materials
	// This includes:
	// - World items (AMOWorldItem)
	// - Containers with inventory
	// - Other buildable actors with inventory

	TArray<AActor*> OverlappingActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

	// Use a sphere overlap to find potential sources
	UKismetSystemLibrary::SphereOverlapActors(
		World,
		Origin,
		Range,
		ObjectTypes,
		nullptr,
		TArray<AActor*>(),
		OverlappingActors
	);

	for (AActor* Actor : OverlappingActors)
	{
		// Check if actor could provide materials
		// For now, just add all actors - subclasses can filter further
		Sources.Add(Actor);
	}

	return Sources;
}

// ============================================================================
// SAVE/LOAD
// ============================================================================

void UMOBuildProgressComponent::BuildSaveData(FMOBuildProgress& OutData) const
{
	OutData = Progress;
}

void UMOBuildProgressComponent::ApplySaveData(const FMOBuildProgress& InData)
{
	Progress = InData;

	// Resume tick if was constructing
	if (Progress.State == EMOBuildState::Constructing)
	{
		SetComponentTickEnabled(true);
	}
}

// ============================================================================
// TICK
// ============================================================================

void UMOBuildProgressComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Progress.State == EMOBuildState::Constructing)
	{
		ProcessConstruction(DeltaTime);
	}
}

// ============================================================================
// INTERNAL
// ============================================================================

void UMOBuildProgressComponent::ProcessConstruction(float DeltaTime)
{
	if (BuildParts.Num() == 0 || TotalWeight == 0)
	{
		// No parts - instantly complete
		FinalizeConstruction();
		return;
	}

	// Advance time
	Progress.ElapsedTime += DeltaTime;

	// Find which part we're on
	float AccumulatedTime = 0.0f;

	for (int32 i = 0; i < BuildParts.Num(); ++i)
	{
		const FMOBuildPart& Part = BuildParts[i];
		float PartTime = Part.Quantity * Part.Weight * TimePerWeight;

		if (Progress.ElapsedTime < AccumulatedTime + PartTime)
		{
			// We're in this part
			if (Progress.CurrentPartIndex != i)
			{
				// Moved to a new part
				if (i > 0)
				{
					CompleteCurrentPart();
				}
				Progress.CurrentPartIndex = i;
			}

			// Calculate progress within this part
			Progress.CurrentPartProgress = (Progress.ElapsedTime - AccumulatedTime) / PartTime;
			Progress.CurrentPartProgress = FMath::Clamp(Progress.CurrentPartProgress, 0.0f, 1.0f);

			// Check if we need to consume materials
			CheckMaterialConsumption();

			break;
		}

		AccumulatedTime += PartTime;
	}

	// Check for completion
	if (Progress.ElapsedTime >= Progress.TotalBuildTime)
	{
		FinalizeConstruction();
		return;
	}

	// Broadcast progress update
	OnConstructionProgress.Broadcast(Progress.GetOverallProgress());
}

void UMOBuildProgressComponent::CheckMaterialConsumption()
{
	if (Progress.CurrentPartIndex >= BuildParts.Num())
	{
		return;
	}

	const FMOBuildPart& CurrentPart = BuildParts[Progress.CurrentPartIndex];

	// Calculate how many items should have been consumed at current progress
	int32 RequiredConsumed = FMath::FloorToInt(Progress.CurrentPartProgress * CurrentPart.Quantity);
	int32 CurrentConsumed = Progress.ConsumedPerPart[Progress.CurrentPartIndex];

	if (RequiredConsumed > CurrentConsumed)
	{
		// Need to consume more
		int32 ToConsume = RequiredConsumed - CurrentConsumed;
		for (int32 i = 0; i < ToConsume; ++i)
		{
			if (!TryConsumeMaterial(CurrentPart))
			{
				// Failed to consume - pause construction
				OnMaterialNeeded.Broadcast(
					CurrentPart.IsItemPart() ? CurrentPart.ItemDefinitionId : CurrentPart.ActionId,
					1
				);
				PauseConstruction();
				return;
			}
		}
	}
}

bool UMOBuildProgressComponent::TryConsumeMaterial(const FMOBuildPart& Part)
{
	if (Part.IsItemPart())
	{
		// Try to consume from available sources
		// TODO: Implement proper material gathering from inventory/containers/world

		// For now, just pretend we consumed it
		// In production, this would check BuilderInventory, nearby containers, etc.

		if (Progress.CurrentPartIndex < Progress.ConsumedPerPart.Num())
		{
			Progress.ConsumedPerPart[Progress.CurrentPartIndex]++;
		}

		UE_LOG(LogMOFramework, Verbose, TEXT("[MOBuildProgressComponent] Consumed item: %s"),
			*Part.ItemDefinitionId.ToString());
		return true;
	}
	else if (Part.IsActionPart())
	{
		// Action parts don't require material consumption
		// They represent labor like digging, hammering, etc.
		if (Progress.CurrentPartIndex < Progress.ConsumedPerPart.Num())
		{
			Progress.ConsumedPerPart[Progress.CurrentPartIndex]++;
		}

		UE_LOG(LogMOFramework, Verbose, TEXT("[MOBuildProgressComponent] Performed action: %s"),
			*Part.ActionId.ToString());
		return true;
	}

	return false;
}

void UMOBuildProgressComponent::CompleteCurrentPart()
{
	if (Progress.CurrentPartIndex < BuildParts.Num())
	{
		OnPartCompleted.Broadcast(Progress.CurrentPartIndex, BuildParts[Progress.CurrentPartIndex]);

		UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Completed part %d"),
			Progress.CurrentPartIndex);
	}
}

void UMOBuildProgressComponent::FinalizeConstruction()
{
	// Complete any remaining parts
	for (int32 i = Progress.CurrentPartIndex; i < BuildParts.Num(); ++i)
	{
		Progress.CurrentPartIndex = i;
		CompleteCurrentPart();
	}

	Progress.State = EMOBuildState::Complete;
	Progress.ElapsedTime = Progress.TotalBuildTime;
	Progress.CurrentPartProgress = 1.0f;

	SetComponentTickEnabled(false);

	OnConstructionCompleted.Broadcast();

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildProgressComponent] Construction complete!"));
}
