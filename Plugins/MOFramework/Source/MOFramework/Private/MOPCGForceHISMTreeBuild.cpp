// Copyright MO57. All Rights Reserved.

#include "MOPCGForceHISMTreeBuild.h"
#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Async/Async.h"

UMOPCGForceHISMTreeBuildSettings::UMOPCGForceHISMTreeBuildSettings()
{
	// Uses UseSeed() override instead of deprecated bUseSeed
}

#if WITH_EDITOR
FText UMOPCGForceHISMTreeBuildSettings::GetNodeTooltipText() const
{
	return NSLOCTEXT("MOPCGForceHISMTreeBuild", "NodeTooltip",
		"Forces HISM tree rebuilding on all HISM components in the target actor.\n\n"
		"Place AFTER StaticMeshSpawner nodes to ensure InstanceMinDrawDistance and other "
		"tree-dependent properties are properly applied.\n\n"
		"Without this node, PCG-spawned HISM instances won't respect distance culling settings "
		"until something else triggers a tree rebuild.");
}
#endif

TArray<FPCGPinProperties> UMOPCGForceHISMTreeBuildSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	// Accept any input - we just need to be connected to execute after spawners
	PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Any);
	return PinProperties;
}

TArray<FPCGPinProperties> UMOPCGForceHISMTreeBuildSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	// Pass through input data unchanged
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Any);
	return PinProperties;
}

FPCGElementPtr UMOPCGForceHISMTreeBuildSettings::CreateElement() const
{
	return MakeShared<FPCGForceHISMTreeBuildElement>();
}

bool FPCGForceHISMTreeBuildElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGForceHISMTreeBuildElement::Execute);

	const UMOPCGForceHISMTreeBuildSettings* Settings = Context->GetInputSettings<UMOPCGForceHISMTreeBuildSettings>();
	check(Settings);

	// Get the target actor from the PCG component
	AActor* TargetActor = Context->GetTargetActor(nullptr);

	if (!TargetActor)
	{
		UE_LOG(LogPCG, Warning, TEXT("MOPCGForceHISMTreeBuild: No target actor found"));
		return true;
	}

	UWorld* World = TargetActor->GetWorld();
	if (!World)
	{
		UE_LOG(LogPCG, Warning, TEXT("MOPCGForceHISMTreeBuild: No world found"));
		return true;
	}

	// Find all HISM components on the target actor
	TArray<UHierarchicalInstancedStaticMeshComponent*> HISMComponents;
	TargetActor->GetComponents<UHierarchicalInstancedStaticMeshComponent>(HISMComponents);

	// Filter to only components with instances
	TArray<TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent>> ValidHISMs;
	for (UHierarchicalInstancedStaticMeshComponent* HISM : HISMComponents)
	{
		if (HISM && HISM->GetInstanceCount() > 0)
		{
			ValidHISMs.Add(HISM);
		}
	}

	if (ValidHISMs.Num() == 0)
	{
		return true;
	}

	// Capture settings for the lambda
	const bool bForceUpdate = Settings->bForceUpdate;
	const bool bDebugLog = Settings->bDebugLog;
	TWeakObjectPtr<UWorld> WeakWorld = World;

	// PCG executes on worker threads, so we need to dispatch to game thread
	// Then defer to next tick to avoid the post-tick component update assertion
	AsyncTask(ENamedThreads::GameThread, [ValidHISMs, bForceUpdate, bDebugLog, WeakWorld]()
	{
		UWorld* WorldPtr = WeakWorld.Get();
		if (!WorldPtr)
		{
			return;
		}

		// Now on game thread, defer to next tick
		WorldPtr->GetTimerManager().SetTimerForNextTick([ValidHISMs, bForceUpdate, bDebugLog]()
		{
			int32 ProcessedCount = 0;
			int32 TotalInstances = 0;

			for (const TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent>& WeakHISM : ValidHISMs)
			{
				if (UHierarchicalInstancedStaticMeshComponent* HISM = WeakHISM.Get())
				{
					const int32 InstanceCount = HISM->GetInstanceCount();
					TotalInstances += InstanceCount;

					// Save all instance transforms
					TArray<FTransform> InstanceTransforms;
					InstanceTransforms.SetNum(InstanceCount);
					for (int32 i = 0; i < InstanceCount; ++i)
					{
						HISM->GetInstanceTransform(i, InstanceTransforms[i], /*bWorldSpace=*/true);
					}

					// Clear and re-add instances - this is what PCG refresh does
					HISM->ClearInstances();
					HISM->AddInstances(InstanceTransforms, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true);

					// Force synchronous tree rebuild
					HISM->BuildTreeIfOutdated(/*bAsync=*/false, bForceUpdate);

					ProcessedCount++;

					if (bDebugLog)
					{
						UE_LOG(LogPCG, Log, TEXT("MOPCGForceHISMTreeBuild: Rebuilt %s (%d instances, MinDrawDist=%.0f, EndCull=%.0f)"),
							*HISM->GetName(),
							InstanceCount,
							static_cast<double>(HISM->InstanceMinDrawDistance),
							static_cast<double>(HISM->InstanceEndCullDistance));
					}
				}
			}

			if (bDebugLog && ProcessedCount > 0)
			{
				UE_LOG(LogPCG, Log, TEXT("MOPCGForceHISMTreeBuild: Processed %d HISM components with %d total instances"),
					ProcessedCount, TotalInstances);
			}
		});
	});

	// Pass through all input data unchanged
	TArray<FPCGTaggedData> Inputs = Context->InputData.GetAllInputs();
	for (const FPCGTaggedData& Input : Inputs)
	{
		FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
		Output.Pin = PCGPinConstants::DefaultOutputLabel;
	}

	return true;
}
