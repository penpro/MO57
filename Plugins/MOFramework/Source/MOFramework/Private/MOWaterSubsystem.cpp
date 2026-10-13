#include "MOWaterSubsystem.h"
#include "MOFramework.h"
#include "MOWorldGen.h"
#include "MOWorldSeedSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PhysicsVolume.h"

bool UMOWaterSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UMOWaterSubsystem* UMOWaterSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UMOWaterSubsystem>() : nullptr;
}

bool UMOWaterSubsystem::GetSurfaceZ(double X, double Y, float& OutSurfaceZ) const
{
	const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(GetWorld());
	if (!Seeds || !Seeds->HasActiveSeed() || !FMOWorldGenParamsProvider::HasPublished())
	{
		return false;
	}
	const FMOWorldGenParamsRef Params = FMOWorldGenParamsProvider::Get();
	return FMOWorldGen::GetWaterSurfaceCm(*Params, X, Y, Seeds->GetActiveTerrainSeed(), OutSurfaceZ);
}

bool UMOWaterSubsystem::GetGroundZ(double X, double Y, float& OutGroundZ) const
{
	const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(GetWorld());
	if (!Seeds || !Seeds->HasActiveSeed() || !FMOWorldGenParamsProvider::HasPublished())
	{
		return false;
	}
	const FMOWorldGenParamsRef Params = FMOWorldGenParamsProvider::Get();
	OutGroundZ = FMOWorldGen::SampleHeightCm(*Params, X, Y, Seeds->GetActiveTerrainSeed(), Params->Tuning.VoxelSizeCm);
	return true;
}

APhysicsVolume* UMOWaterSubsystem::GetSwimVolume()
{
	if (!SwimVolume && GetWorld())
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Name = TEXT("MOSwimVolume");
		SwimVolume = GetWorld()->SpawnActor<APhysicsVolume>(APhysicsVolume::StaticClass(), FTransform::Identity, Params);
		if (SwimVolume)
		{
			SwimVolume->bWaterVolume = true;
			SwimVolume->FluidFriction = 1.6f;      // PhysSwimming damps by 0.5 x FluidFriction x depth: ~0.8 /s at full immersion, so a stroke glides about a second
			SwimVolume->TerminalVelocity = 400.0f; // a sinking body does not accelerate like a falling one
			SwimVolume->Priority = 0;
			UE_LOG(LogMOFramework, Log, TEXT("[MOWater] swim volume spawned"));
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOWater] could not spawn the swim volume: characters will not swim"));
		}
	}
	return SwimVolume;
}
