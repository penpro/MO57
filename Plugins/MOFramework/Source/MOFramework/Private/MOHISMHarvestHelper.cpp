#include "MOHISMHarvestHelper.h"
#include "MOFramework.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"

// ============================================================================
// SINGLE INSTANCE OPERATIONS
// ============================================================================

bool FMOHISMHarvestHelper::RemoveInstance(
	UHierarchicalInstancedStaticMeshComponent* HISM,
	int32 InstanceIndex,
	bool bRespectKeepOnHarvest)
{
	if (!HISM)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[HISMHarvestHelper] RemoveInstance: Null HISM component"));
		return false;
	}

	if (InstanceIndex < 0 || InstanceIndex >= HISM->GetInstanceCount())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[HISMHarvestHelper] RemoveInstance: Invalid index %d (count: %d)"),
			InstanceIndex, HISM->GetInstanceCount());
		return false;
	}

	// Check keep-on-harvest tag
	if (bRespectKeepOnHarvest && ShouldKeepOnHarvest(HISM))
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[HISMHarvestHelper] RemoveInstance: Keeping instance due to KeepOnHarvest tag"));
		return true; // Return true - operation "succeeded" by keeping the instance
	}

	const bool bRemoved = HISM->RemoveInstance(InstanceIndex);
	if (!bRemoved)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[HISMHarvestHelper] RemoveInstance: Failed to remove instance %d"), InstanceIndex);
	}

	return bRemoved;
}

bool FMOHISMHarvestHelper::RemoveInstance(
	UInstancedStaticMeshComponent* ISM,
	int32 InstanceIndex,
	bool bRespectKeepOnHarvest)
{
	if (!ISM)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[HISMHarvestHelper] RemoveInstance: Null ISM component"));
		return false;
	}

	if (InstanceIndex < 0 || InstanceIndex >= ISM->GetInstanceCount())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[HISMHarvestHelper] RemoveInstance: Invalid index %d (count: %d)"),
			InstanceIndex, ISM->GetInstanceCount());
		return false;
	}

	// Check keep-on-harvest tag
	if (bRespectKeepOnHarvest && ShouldKeepOnHarvest(ISM))
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[HISMHarvestHelper] RemoveInstance: Keeping instance due to KeepOnHarvest tag"));
		return true;
	}

	const bool bRemoved = ISM->RemoveInstance(InstanceIndex);
	if (!bRemoved)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[HISMHarvestHelper] RemoveInstance: Failed to remove instance %d"), InstanceIndex);
	}

	return bRemoved;
}

bool FMOHISMHarvestHelper::ShouldKeepOnHarvest(UInstancedStaticMeshComponent* ISM)
{
	if (!ISM)
	{
		return false;
	}

	// Check component tag
	if (ISM->ComponentHasTag(TEXT("KeepOnHarvest")))
	{
		return true;
	}

	// Check owning actor tag
	AActor* Owner = ISM->GetOwner();
	if (Owner && Owner->ActorHasTag(TEXT("KeepOnHarvest")))
	{
		return true;
	}

	return false;
}

// ============================================================================
// BATCH OPERATIONS
// ============================================================================

FMOHISMRemovalResult FMOHISMHarvestHelper::RemoveInstancesBatch(
	const TMap<UHierarchicalInstancedStaticMeshComponent*, TArray<int32>>& ComponentToIndices,
	bool bRespectKeepOnHarvest)
{
	FMOHISMRemovalResult Result;

	for (const auto& Pair : ComponentToIndices)
	{
		UHierarchicalInstancedStaticMeshComponent* HISM = Pair.Key;
		if (!HISM)
		{
			Result.FailedCount += Pair.Value.Num();
			continue;
		}

		// Check keep-on-harvest for entire component
		if (bRespectKeepOnHarvest && ShouldKeepOnHarvest(HISM))
		{
			// Count as "removed" since the operation is intentionally skipped
			Result.RemovedCount += Pair.Value.Num();
			continue;
		}

		// Copy and sort indices in descending order
		TArray<int32> SortedIndices = Pair.Value;
		SortedIndices.Sort([](int32 A, int32 B) { return A > B; });

		// Remove each instance
		for (int32 Index : SortedIndices)
		{
			if (Index >= 0 && Index < HISM->GetInstanceCount())
			{
				if (HISM->RemoveInstance(Index))
				{
					Result.RemovedCount++;
				}
				else
				{
					Result.FailedCount++;
				}
			}
			else
			{
				Result.FailedCount++;
			}
		}
	}

	UE_LOG(LogMOFramework, Verbose, TEXT("[HISMHarvestHelper] RemoveInstancesBatch: Removed %d, Failed %d"),
		Result.RemovedCount, Result.FailedCount);

	return Result;
}

FMOHISMRemovalResult FMOHISMHarvestHelper::RemoveInstances(
	UHierarchicalInstancedStaticMeshComponent* HISM,
	TArray<int32> InstanceIndices,
	bool bRespectKeepOnHarvest)
{
	FMOHISMRemovalResult Result;

	if (!HISM)
	{
		Result.FailedCount = InstanceIndices.Num();
		return Result;
	}

	// Check keep-on-harvest
	if (bRespectKeepOnHarvest && ShouldKeepOnHarvest(HISM))
	{
		Result.RemovedCount = InstanceIndices.Num();
		return Result;
	}

	// Sort in descending order to avoid index shifting
	InstanceIndices.Sort([](int32 A, int32 B) { return A > B; });

	for (int32 Index : InstanceIndices)
	{
		if (Index >= 0 && Index < HISM->GetInstanceCount())
		{
			if (HISM->RemoveInstance(Index))
			{
				Result.RemovedCount++;
			}
			else
			{
				Result.FailedCount++;
			}
		}
		else
		{
			Result.FailedCount++;
		}
	}

	return Result;
}

// ============================================================================
// TRANSFORM RETRIEVAL
// ============================================================================

bool FMOHISMHarvestHelper::GetInstanceTransform(
	UHierarchicalInstancedStaticMeshComponent* HISM,
	int32 InstanceIndex,
	FTransform& OutTransform)
{
	if (!HISM)
	{
		return false;
	}

	if (InstanceIndex < 0 || InstanceIndex >= HISM->GetInstanceCount())
	{
		return false;
	}

	return HISM->GetInstanceTransform(InstanceIndex, OutTransform, true);
}

bool FMOHISMHarvestHelper::GetInstanceTransform(
	UInstancedStaticMeshComponent* ISM,
	int32 InstanceIndex,
	FTransform& OutTransform)
{
	if (!ISM)
	{
		return false;
	}

	if (InstanceIndex < 0 || InstanceIndex >= ISM->GetInstanceCount())
	{
		return false;
	}

	return ISM->GetInstanceTransform(InstanceIndex, OutTransform, true);
}
