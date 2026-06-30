// MOVoxelAlias.h — Anti-corruption layer over the Voxel Plugin sculpt API.
//
// CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH VOXEL-FACING CODE.
//
// PURPOSE
//   The Voxel Plugin reshapes its public API between versions — e.g. the
//   2.0p8 -> dev-phy (5.8) jump renamed AVoxelHeightSculptActor ->
//   AVoxelSculptHeight, AVoxelVolumeSculptActor -> AVoxelSculptVolume, moved
//   headers, and made save/load return FVoxelFuture. Left un-isolated, every
//   such change is a scattered refactor across terraforming + persistence.
//
//   This facade is the ONE place in MO that names Voxel types or includes Voxel
//   headers. The rest of the codebase speaks only AActor*/FVector(2D)/uint8
//   bytes. When Voxel changes its API again, update MOVoxelAlias.cpp ONLY —
//   the callers never change.
//
// RULES
//   - NOTHING in MOVoxelAlias.h may name a Voxel type or include a Voxel header.
//   - ALL Voxel #includes, class names, enums, futures and save structs live in
//     MOVoxelAlias.cpp.
//   - Callers store the returned sculpt actor as TWeakObjectPtr<AActor> and pass
//     it straight back in. The facade revalidates/casts internally.
//
// CONSUMERS: MOTerraformingComponent.cpp, MOPersistenceSubsystem.cpp
#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;

namespace MOVoxel
{
	// --- discovery -------------------------------------------------------------
	/** All height/volume sculpt actors in the world (persistence iterates these). */
	void GetHeightSculptActors(UWorld* World, TArray<AActor*>& OutActors);
	void GetVolumeSculptActors(UWorld* World, TArray<AActor*>& OutActors);

	/** First sculpt actor of each kind, or nullptr (terraforming caches these). */
	AActor* FindHeightSculptActor(UWorld* World);
	AActor* FindVolumeSculptActor(UWorld* World);

	/** Type checks without naming the Voxel class. */
	bool IsHeightSculptActor(const AActor* Actor);
	bool IsVolumeSculptActor(const AActor* Actor);

	// --- height sculpting (Center is XY world space) --------------------------
	void HeightSculpt(AActor* SculptActor, const FVector2D& Center, float Radius, float Strength, bool bAdd);
	void HeightFlatten(AActor* SculptActor, const FVector2D& Center, float Radius, float Falloff, float TargetHeight);
	void HeightSmooth(AActor* SculptActor, const FVector2D& Center, float Radius, float Strength);

	// --- volume sculpting (Location is world space) ---------------------------
	void VolumeSphere(AActor* SculptActor, const FVector& Location, float Radius, bool bAdd, float Smoothness);
	void VolumeFlatten(AActor* SculptActor, const FVector& Location, const FVector& Normal, float Radius, float Height, float Falloff);
	void VolumeSmooth(AActor* SculptActor, const FVector& Location, float Radius, float Strength);

	// --- persistence (opaque byte blobs; hides Voxel save structs + async) -----
	// Save* returns true and fills OutBytes only when the actor has modifications.
	// Load* returns true if the blob was valid and applied. Both block internally
	// (Voxel's get/load are async — the facade waits, since saves aren't per-frame).
	bool SaveHeightSculpt(AActor* SculptActor, TArray<uint8>& OutBytes);
	bool LoadHeightSculpt(AActor* SculptActor, const TArray<uint8>& InBytes);
	bool SaveVolumeSculpt(AActor* SculptActor, TArray<uint8>& OutBytes);
	bool LoadVolumeSculpt(AActor* SculptActor, const TArray<uint8>& InBytes);
}
