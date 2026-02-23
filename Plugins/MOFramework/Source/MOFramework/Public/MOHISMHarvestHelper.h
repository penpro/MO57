#pragma once

#include "CoreMinimal.h"
#include "MOHISMHarvestHelper.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UInstancedStaticMeshComponent;

/**
 * Result of a HISM instance removal operation.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOHISMRemovalResult
{
	GENERATED_BODY()

	/** Number of instances successfully removed */
	UPROPERTY(BlueprintReadOnly)
	int32 RemovedCount = 0;

	/** Number of instances that failed to remove */
	UPROPERTY(BlueprintReadOnly)
	int32 FailedCount = 0;

	/** True if all requested removals succeeded */
	bool WasFullySuccessful() const { return FailedCount == 0 && RemovedCount > 0; }
};

/**
 * Utility class for HISM/ISM instance harvesting and removal.
 * Consolidates duplicate HISM removal logic from ForagingSubsystem and PCGInteractionSubsystem.
 *
 * Key features:
 * - Batch removal with automatic reverse-order processing (avoids index shifting)
 * - Tag-based "KeepOnHarvest" support for persistent resources
 * - Safe null checks and logging
 */
class MOFRAMEWORK_API FMOHISMHarvestHelper
{
public:
	// ============================================================================
	// SINGLE INSTANCE OPERATIONS
	// ============================================================================

	/**
	 * Remove a single instance from a HISM component.
	 * @param HISM The HISM component
	 * @param InstanceIndex Index of instance to remove
	 * @param bRespectKeepOnHarvest If true, won't remove if component/actor has "KeepOnHarvest" tag
	 * @return True if removed (or kept due to tag), false on error
	 */
	static bool RemoveInstance(
		UHierarchicalInstancedStaticMeshComponent* HISM,
		int32 InstanceIndex,
		bool bRespectKeepOnHarvest = true);

	/**
	 * Remove a single instance from a regular ISM component.
	 */
	static bool RemoveInstance(
		UInstancedStaticMeshComponent* ISM,
		int32 InstanceIndex,
		bool bRespectKeepOnHarvest = true);

	/**
	 * Check if a component should keep instances on harvest.
	 * Checks for "KeepOnHarvest" tag on component or owning actor.
	 */
	static bool ShouldKeepOnHarvest(UInstancedStaticMeshComponent* ISM);

	// ============================================================================
	// BATCH OPERATIONS
	// ============================================================================

	/**
	 * Remove multiple instances from HISM components in batch.
	 * Automatically processes in reverse index order to avoid shifting.
	 *
	 * @param ComponentToIndices Map of HISM components to their instance indices to remove
	 * @param bRespectKeepOnHarvest If true, skips components with "KeepOnHarvest" tag
	 * @return Result struct with counts
	 */
	static FMOHISMRemovalResult RemoveInstancesBatch(
		const TMap<UHierarchicalInstancedStaticMeshComponent*, TArray<int32>>& ComponentToIndices,
		bool bRespectKeepOnHarvest = true);

	/**
	 * Remove multiple instances from a single HISM component.
	 * Automatically sorts indices in descending order.
	 *
	 * @param HISM The HISM component
	 * @param InstanceIndices Indices to remove (will be sorted internally)
	 * @param bRespectKeepOnHarvest If true, won't remove if tagged
	 * @return Result struct with counts
	 */
	static FMOHISMRemovalResult RemoveInstances(
		UHierarchicalInstancedStaticMeshComponent* HISM,
		TArray<int32> InstanceIndices,
		bool bRespectKeepOnHarvest = true);

	// ============================================================================
	// TRANSFORM RETRIEVAL
	// ============================================================================

	/**
	 * Get the world transform of a HISM instance.
	 * @param HISM The HISM component
	 * @param InstanceIndex Index of instance
	 * @param OutTransform Output transform
	 * @return True if transform was retrieved successfully
	 */
	static bool GetInstanceTransform(
		UHierarchicalInstancedStaticMeshComponent* HISM,
		int32 InstanceIndex,
		FTransform& OutTransform);

	/**
	 * Get the world transform of an ISM instance.
	 */
	static bool GetInstanceTransform(
		UInstancedStaticMeshComponent* ISM,
		int32 InstanceIndex,
		FTransform& OutTransform);
};
