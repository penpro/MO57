/**
 * MOInstanceRef.h - an instanced-mesh instance named the same way on every machine
 *
 * A PCG instance (a stick on the ground, a bush, a tree) has no network identity of its own. PCG partition actors and their ISM components are spawned at
 * runtime under names that differ per machine, and an instance index shifts as instances are removed. A client RPC that passed the component pointer
 * arrived on the host as null: "Unable to resolve default guid from client: ObjectName: PCGPartitionActor_2147442263" (Wes's Steam test, Oct 18 2026),
 * and E on a stick, a stone or a flint did nothing for a joining player. Two processes on one machine name them differently too.
 *
 * What IS the same on every machine is what the removal ledger (MOWorldRemovalTypes.h) already matches on: the instance's static mesh (an asset, which the
 * net driver resolves by its path) and its world location (generation is deterministic from the shared seed). Make() on the sending side, Resolve() on the
 * receiving side: the nearest instance of that mesh within ToleranceCm.
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "MOInstanceRef.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UWorld;

USTRUCT()
struct MOFRAMEWORK_API FMOInstanceRef
{
	GENERATED_BODY()

	/** The instance's static mesh. */
	UPROPERTY()
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	/** The instance's world location (1 cm on the wire). */
	UPROPERTY()
	FVector_NetQuantize Location = FVector::ZeroVector;

	/**
	 * How far from Location the resolved instance may be. Two machines place an instance within a few millimetres of each other (the ledger allows one
	 * 10 cm cell); instances of one mesh closer together than this are one clump, where the nearest one is the right answer.
	 */
	static constexpr double ToleranceCm = 25.0;

	/** The reference to instance InstanceIndex of ISM; unset (IsSet() false) when the component, its mesh or the index is not valid. */
	static FMOInstanceRef Make(const UInstancedStaticMeshComponent* ISM, int32 InstanceIndex);

	bool IsSet() const { return Mesh != nullptr; }

	/** The receiving side: the instance of Mesh nearest Location (within ToleranceCm) among World's instanced mesh components. */
	bool Resolve(const UWorld* World, UInstancedStaticMeshComponent*& OutISM, int32& OutInstanceIndex) const;

	/** "mesh at (x, y, z)", for logs. */
	FString ToString() const;
};
