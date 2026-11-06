// Large rocks dressing the generator's features (N-330, N-331): under a floating island, set into a ravine's wall. Closed grey scans the biomes already plant, so they cook. Placed from the seed alone,
// so every machine has the same ones and nothing is replicated. One place for the meshes and the components, so the next feature that wants rocks adds only its placement.
#pragma once

#include "CoreMinimal.h"

class AActor;
class UStaticMesh;
class UWorld;

namespace MOFeatureRocks
{
	/** What a placement needs of a mesh: thread-safe to read, so placement can run off the game thread. */
	struct FMeshSize
	{
		FVector Origin = FVector::ZeroVector;
		FVector Extent = FVector(100.0);
	};

	/** The rock meshes (those that load) and their sizes, index for index. */
	void LoadMeshes(TArray<UStaticMesh*>& OutMeshes, TArray<FMeshSize>& OutSizes);

	/** The instance transform that makes a mesh HalfWidth across and HalfHeight tall, its middle at Center. */
	FTransform MakeInstance(const FMeshSize& Size, const FVector& Center, const FRotator& Rotation, double HalfWidth, double HalfHeight);

	/** A transient actor holding one instanced component per mesh with instances (world space). Null when there is nothing to hold. Climbable, solid, out of Lumen's surface cache as every plant and rock. */
	AActor* SpawnHolder(UWorld* World, const TArray<UStaticMesh*>& Meshes, const TArray<TArray<FTransform>>& ByMesh);
}
