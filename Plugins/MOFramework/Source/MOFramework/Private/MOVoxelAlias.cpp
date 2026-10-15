// MOVoxelAlias.cpp — the ONLY MO translation unit that names Voxel types.
// See MOVoxelAlias.h. When the Voxel API changes, this file is the only one
// that should need editing.

#include "MOVoxelAlias.h"

// --- Voxel plugin headers (dev-phy / 5.8). Confined to this file. ------------
#include "VoxelMinimal.h"                                  // Voxel::ExecuteSynchronously, FVoxelFuture
#include "Sculpt/VoxelSculptSave.h"                        // FVoxel*SculptSave (IsValid/Serialize)
#include "Sculpt/Height/VoxelSculptHeight.h"               // AVoxelSculptHeight (was AVoxelHeightSculptActor)
#include "Sculpt/Height/VoxelHeightSculptBlueprintLibrary.h"
#include "VoxelFalloff.h"
#include "Sculpt/Volume/VoxelSculptVolume.h"               // AVoxelSculptVolume (was AVoxelVolumeSculptActor)
#include "Sculpt/Volume/VoxelVolumeSculptBlueprintLibrary.h"

#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

namespace
{
	FORCEINLINE AVoxelSculptHeight* AsHeight(AActor* A) { return Cast<AVoxelSculptHeight>(A); }
	FORCEINLINE AVoxelSculptVolume* AsVolume(AActor* A) { return Cast<AVoxelSculptVolume>(A); }
}

namespace MOVoxel
{
	// --- discovery ------------------------------------------------------------
	void GetHeightSculptActors(UWorld* World, TArray<AActor*>& OutActors)
	{
		OutActors.Reset();
		if (World)
		{
			UGameplayStatics::GetAllActorsOfClass(World, AVoxelSculptHeight::StaticClass(), OutActors);
		}
	}

	void GetVolumeSculptActors(UWorld* World, TArray<AActor*>& OutActors)
	{
		OutActors.Reset();
		if (World)
		{
			UGameplayStatics::GetAllActorsOfClass(World, AVoxelSculptVolume::StaticClass(), OutActors);
		}
	}

	AActor* FindHeightSculptActor(UWorld* World)
	{
		TArray<AActor*> Actors;
		GetHeightSculptActors(World, Actors);
		return Actors.Num() > 0 ? Actors[0] : nullptr;
	}

	AActor* FindVolumeSculptActor(UWorld* World)
	{
		TArray<AActor*> Actors;
		GetVolumeSculptActors(World, Actors);
		return Actors.Num() > 0 ? Actors[0] : nullptr;
	}

	bool IsHeightSculptActor(const AActor* Actor) { return Actor && Actor->IsA<AVoxelSculptHeight>(); }
	bool IsVolumeSculptActor(const AActor* Actor) { return Actor && Actor->IsA<AVoxelSculptVolume>(); }

	// --- height sculpting -----------------------------------------------------
	void HeightSculpt(AActor* SculptActor, const FVector2D& Center, float Radius, float Strength, bool bAdd)
	{
		AVoxelSculptHeight* A = AsHeight(SculptActor);
		if (!IsValid(A)) { return; }
		UVoxelHeightSculptBlueprintLibrary::SculptHeight(
			A, Center, Radius, Strength, bAdd ? EVoxelSculptMode::Add : EVoxelSculptMode::Remove);
	}

	float HeightSculptMovedVolumeM3(float Radius, float Strength)
	{
		// UVoxelHeightSculptBlueprintLibrary::SculptHeight (FVoxelSculptHeightModifierRuntime::Apply): Delta = Radius / 10 * Falloff * Strength
		// per column, Falloff from the default brush (FVoxelFalloff: Smooth, Amount 0.5 -- full depth to half the radius, smoothstep to 0 at
		// the rim). Integrated in rings; the rule is symmetric, so a Raise moves exactly what a Dig of the same brush moves.
		const FVoxelFalloff Default;
		constexpr int32 Rings = 128;
		const double R = FMath::Max(0.0f, Radius);
		double CubicCm = 0.0;
		for (int32 I = 0; I < Rings; ++I)
		{
			const double R0 = R * I / Rings, R1 = R * (I + 1) / Rings;
			const float Falloff = FVoxelFalloff::GetFalloff(Default.Type, float(0.5 * (R0 + R1)), float(R), Default.Amount);
			CubicCm += R / 10.0 * FMath::Clamp(Strength, 0.0f, 1.0f) * Falloff * PI * (R1 * R1 - R0 * R0);
		}
		return float(CubicCm / 1.0e6);
	}

	void HeightFlatten(AActor* SculptActor, const FVector2D& Center, float Radius, float Falloff, float TargetHeight)
	{
		AVoxelSculptHeight* A = AsHeight(SculptActor);
		if (!IsValid(A)) { return; }
		UVoxelHeightSculptBlueprintLibrary::Flatten(
			A, Center, Radius, Falloff, EVoxelLevelToolType::Both, TargetHeight);
	}

	void HeightSmooth(AActor* SculptActor, const FVector2D& Center, float Radius, float Strength)
	{
		AVoxelSculptHeight* A = AsHeight(SculptActor);
		if (!IsValid(A)) { return; }
		UVoxelHeightSculptBlueprintLibrary::Smooth(A, Center, Radius, Strength);
	}

	// --- volume sculpting -----------------------------------------------------
	void VolumeSphere(AActor* SculptActor, const FVector& Location, float Radius, bool bAdd, float Smoothness)
	{
		AVoxelSculptVolume* A = AsVolume(SculptActor);
		if (!IsValid(A)) { return; }
		UVoxelVolumeSculptBlueprintLibrary::SculptSphere(
			A, Location, Radius, bAdd ? EVoxelSculptMode::Add : EVoxelSculptMode::Remove, Smoothness);
	}

	void VolumeFlatten(AActor* SculptActor, const FVector& Location, const FVector& Normal, float Radius, float Height, float Falloff)
	{
		AVoxelSculptVolume* A = AsVolume(SculptActor);
		if (!IsValid(A)) { return; }
		UVoxelVolumeSculptBlueprintLibrary::Flatten(
			A, Location, Normal, Radius, Height, Falloff, EVoxelLevelToolType::Both);
	}

	void VolumeSmooth(AActor* SculptActor, const FVector& Location, float Radius, float Strength)
	{
		AVoxelSculptVolume* A = AsVolume(SculptActor);
		if (!IsValid(A)) { return; }
		UVoxelVolumeSculptBlueprintLibrary::Smooth(A, Location, Radius, Strength);
	}

	// --- persistence ----------------------------------------------------------
	// Voxel's get/load are async (FVoxelFuture); ExecuteSynchronously blocks the
	// calling thread until the operation completes — correct for save/load, which
	// happen at save points rather than per-frame.
	bool SaveHeightSculpt(AActor* SculptActor, TArray<uint8>& OutBytes)
	{
		AVoxelSculptHeight* A = AsHeight(SculptActor);
		if (!IsValid(A)) { return false; }

		FVoxelHeightSculptSave Save;
		Voxel::ExecuteSynchronously([&] { return UVoxelHeightSculptBlueprintLibrary::K2_GetSave(Save, A, /*bCompress*/ true); });
		if (!Save.IsValid()) { return false; }

		FMemoryWriter Writer(OutBytes);
		Save.Serialize(Writer);
		return true;
	}

	bool LoadHeightSculpt(AActor* SculptActor, const TArray<uint8>& InBytes)
	{
		AVoxelSculptHeight* A = AsHeight(SculptActor);
		if (!IsValid(A) || InBytes.Num() == 0) { return false; }

		FVoxelHeightSculptSave Save;
		FMemoryReader Reader(InBytes, /*bIsPersistent*/ true);
		Save.Serialize(Reader);
		if (!Save.IsValid()) { return false; }

		Voxel::ExecuteSynchronously([&] { return UVoxelHeightSculptBlueprintLibrary::LoadFromSave(A, Save); });
		return true;
	}

	bool SaveVolumeSculpt(AActor* SculptActor, TArray<uint8>& OutBytes)
	{
		AVoxelSculptVolume* A = AsVolume(SculptActor);
		if (!IsValid(A)) { return false; }

		FVoxelVolumeSculptSave Save;
		Voxel::ExecuteSynchronously([&] { return UVoxelVolumeSculptBlueprintLibrary::K2_GetSave(Save, A, /*bCompress*/ true); });
		if (!Save.IsValid()) { return false; }

		FMemoryWriter Writer(OutBytes);
		Save.Serialize(Writer);
		return true;
	}

	bool LoadVolumeSculpt(AActor* SculptActor, const TArray<uint8>& InBytes)
	{
		AVoxelSculptVolume* A = AsVolume(SculptActor);
		if (!IsValid(A) || InBytes.Num() == 0) { return false; }

		FVoxelVolumeSculptSave Save;
		FMemoryReader Reader(InBytes, /*bIsPersistent*/ true);
		Save.Serialize(Reader);
		if (!Save.IsValid()) { return false; }

		Voxel::ExecuteSynchronously([&] { return UVoxelVolumeSculptBlueprintLibrary::LoadFromSave(A, Save); });
		return true;
	}
}
