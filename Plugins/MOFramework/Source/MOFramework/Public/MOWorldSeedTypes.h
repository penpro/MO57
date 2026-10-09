/**
 * MOWorldSeedTypes.h - The world-generation inputs the host publishes to every client
 *
 * WHY THIS EXISTS:
 * Every machine generates its OWN voxel terrain locally (AVoxelWorld runs per machine; only
 * gameplay state replicates). The terrain is a pure function of the seed, so two machines
 * only see the same ground if they apply the same seed. The seed used to be applied only
 * inside AMOGameMode -- which exists on the host alone -- so a joining client generated
 * terrain from the cooked default seed: a pawn standing on the host's land was under (or
 * inside) the client's different terrain and "fell through the world".
 *
 * FMOWorldSeedInfo is the complete input to that function. It lives on AMOGameState as one
 * replicated struct so it arrives atomically, and UMOWorldSeedSubsystem applies it the same
 * way on host and client.
 */

#pragma once

#include "CoreMinimal.h"
#include "MOWorldSeedTypes.generated.h"

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWorldSeedInfo
{
	GENERATED_BODY()

	/** False until the host has decided the seed. A client must not generate terrain before this is true. */
	UPROPERTY(BlueprintReadOnly, Category = "MO|Voxel")
	bool bPublished = false;

	/** The integer world seed (same value the save file stores). */
	UPROPERTY(BlueprintReadOnly, Category = "MO|Voxel")
	int32 Seed = 0;

	/**
	 * Name of the voxel graph's seed parameter. Published with the seed (rather than re-read from
	 * the client's own GameMode defaults, which a client does not have) so a host-side change to
	 * it can never put host and client on different parameters.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "MO|Voxel")
	FName ParameterName = TEXT("Seed");

	bool operator==(const FMOWorldSeedInfo& Other) const
	{
		return bPublished == Other.bPublished && Seed == Other.Seed && ParameterName == Other.ParameterName;
	}
	bool operator!=(const FMOWorldSeedInfo& Other) const { return !(*this == Other); }
};
