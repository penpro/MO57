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
#include "VoxelWorld.h"                                    // AVoxelWorld::LayerStack (HeightLayerSurfaceCm)
#include "VoxelStackLayer.h"
#include "VoxelLayer.h"
#include "VoxelLayerStack.h"
#include "VoxelQueryBlueprintLibrary.h"
#include "EngineUtils.h"

#include "MOFramework.h"
#include "Engine/World.h"
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

	AActor* EnsureVolumeSculptActor(UWorld* World)
	{
		if (AActor* Found = FindVolumeSculptActor(World))
		{
			return Found;
		}
		if (!World)
		{
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.Name = FName(VolumeSculptName);
		Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.bDeferConstruction = true;
		AVoxelSculptVolume* Volume = World->SpawnActor<AVoxelSculptVolume>(AVoxelSculptVolume::StaticClass(), FTransform::Identity, Params);
		if (!Volume)
		{
			return nullptr;
		}
		// Each machine spawns its own (the actor does not replicate), under the same name in the persistent level: net-addressable by that name, so
		// the plugin's channel can carry its sculpt data host -> joiners (it refuses a spawned sculpt whose name is not stable for networking).
		Volume->SetReplicates(false);
		Volume->SetNetAddressable();
		Volume->bReplicateSculptData = World->GetNetMode() != NM_Standalone;
		{
			const FVoxelVolumeSculptStampRef Stamp = Volume->GetStamp();
			Stamp->Scale = 25.0f;        // its samples every 25 cm (default 100): a dig's scoop takes the 50 cm voxel world's shape, not a metre grid's
			Stamp->bIsInfinite = true;   // "for runtime stamps": an empty sculpt has no bounds, so its first edit would not reach the chunks
			// Last in its layer ("higher priority stamps will be applied last"): the player's digs cut through every stamp of the world -- a stamped
			// cliff or island at priority 1 re-added each dug sphere over a sculpt at the default 0 (Nov 4: five digs, the face 0 cm).
			Stamp->Priority = SculptPriority;
		}
		Volume->FinishSpawning(FTransform::Identity);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOVoxel] spawned the volume sculpt %s (the level has none)%s"), *Volume->GetName(),
			Volume->bReplicateSculptData ? TEXT(", its edits replicate") : TEXT(""));
		if (Volume->GetName() != VolumeSculptName)
		{
			UE_LOG(LogMOFramework, Error, TEXT("[MOVoxel] the volume sculpt got the name %s, not %s: another machine's would not match it"), *Volume->GetName(), VolumeSculptName);
		}
		return Volume;
	}

	int32 EnableSculptReplication(UWorld* World)
	{
		int32 Enabled = 0;
		TArray<AActor*> Actors;
		GetHeightSculptActors(World, Actors);
		for (AActor* Actor : Actors)
		{
			AVoxelSculptHeight* Height = AsHeight(Actor);
			if (IsValid(Height) && !Height->HasActorBegunPlay() && Height->IsNameStableForNetworking())
			{
				Height->bReplicateSculptData = true;
				++Enabled;
			}
		}
		GetVolumeSculptActors(World, Actors);
		for (AActor* Actor : Actors)
		{
			AVoxelSculptVolume* Volume = AsVolume(Actor);
			if (IsValid(Volume) && !Volume->HasActorBegunPlay() && Volume->IsNameStableForNetworking())
			{
				Volume->bReplicateSculptData = true;
				++Enabled;
			}
		}
		return Enabled;
	}

	bool IsHeightSculptActor(const AActor* Actor) { return Actor && Actor->IsA<AVoxelSculptHeight>(); }
	bool IsVolumeSculptActor(const AActor* Actor) { return Actor && Actor->IsA<AVoxelSculptVolume>(); }

	bool HeightLayerSurfaceCm(UWorld* World, double X, double Y, float& OutHeightCm)
	{
		AVoxelSculptHeight* Sculpt = AsHeight(FindHeightSculptActor(World));
		if (!World || !IsValid(Sculpt))
		{
			return false;
		}
		UVoxelHeightLayer* Layer = Sculpt->GetStamp()->Layer;
		UVoxelLayerStack* Stack = nullptr;
		for (TActorIterator<AVoxelWorld> It(World); It && !Stack; ++It)
		{
			Stack = It->LayerStack;
		}
		if (!Stack)
		{
			Stack = LoadObject<UVoxelLayerStack>(nullptr, TEXT("/Voxel/Default/DefaultStack.DefaultStack"));
		}
		if (!Layer || !Stack)
		{
			return false;
		}
		bool bSuccess = false;
		FVoxelQueryResult Result;
		Voxel::ExecuteSynchronously([&]
		{
			return UVoxelQueryBlueprintLibrary::K2_QueryVoxelLayer(bSuccess, Result, World, FVoxelStackHeightLayer(Stack, Layer), FVector(X, Y, 0.0), {});
		});
		OutHeightCm = Result.Value;
		return bSuccess;
	}

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
