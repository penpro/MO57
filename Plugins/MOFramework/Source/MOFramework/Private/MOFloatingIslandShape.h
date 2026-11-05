// A floating island's rock as a voxel shape: the generator's torn clumps (FMOWorldGen::IslandRockDistanceCm) drawn as additive rock in its own bounds. The height terrain and the cave field know
// nothing of it: no rock disc on the ground under an island, no pillar at far LODs, no needles (fork F62, measured in MO_WorldLab: Docs/Terrain_Generation_Exploration.md § Sky features).
// UMOWorldFeatureSubsystem keeps two of these stamps per island round the players (MOFloatingIslandStamps.cpp).
#pragma once

#include "CoreMinimal.h"
#include "Shape/VoxelShape.h"
#include "MOWorldGen.h"
#include "MOFloatingIslandShape.generated.h"

/** One floating island's rock (or its top slab: the grass). Sampled in world space: the stamp actor stands at the origin, unturned. */
USTRUCT()
struct FMOFloatingIslandShape : public FVoxelShape
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

public:
	// UPROPERTYs: what tells two islands' shapes apart when the plugin compares stamps.
	UPROPERTY()
	int32 Seed = 0;

	UPROPERTY()
	FVector BoundsMin = FVector::ZeroVector;

	UPROPERTY()
	FVector BoundsMax = FVector::ZeroVector;

	/** > 0: only the rock's top TopSlabCm, standing FMOFloatingIsland::TopSlabProudCm proud of it (it wins the surface there: the grass top). 0: all the rock. */
	UPROPERTY()
	float TopSlabCm = 0.0f;

	/** > 0: a far chunk's stand-in for the rock (FMOWorldGen::IslandFarDistanceCm), this much deeper and half of it wider: the rock itself is thinner than that chunk's cells. */
	UPROPERTY()
	float FarThickenCm = 0.0f;

	TSharedPtr<const FMOWorldGenParams, ESPMode::ThreadSafe> Params;

	virtual FVoxelBox GetLocalBounds() const override;
	virtual void Sample(TVoxelArrayView<float> OutDistances, const FVoxelDoubleVectorBuffer& Positions) const override;
};
