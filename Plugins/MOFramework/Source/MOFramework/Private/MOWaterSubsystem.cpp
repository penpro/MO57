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

EMOWaterBodyKind UMOWaterSubsystem::ClassifyWaterAt(double X, double Y) const
{
	const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(GetWorld());
	if (!Seeds || !Seeds->HasActiveSeed() || !FMOWorldGenParamsProvider::HasPublished())
	{
		return EMOWaterBodyKind::None;
	}
	return FMOWorldGen::ClassifyWaterBody(*FMOWorldGenParamsProvider::Get(), X, Y, Seeds->GetActiveTerrainSeed());
}

bool UMOWaterSubsystem::FindWaterAlongRay(const FVector& Start, const FVector& Dir, float MaxCm, float BlockedAtCm, FVector& OutPoint, EMOWaterBodyKind& OutKind) const
{
	const FVector D = Dir.GetSafeNormal();
	const float Limit = FMath::Min(MaxCm, BlockedAtCm);
	constexpr float StepCm = 20.0f;
	for (float T = 0.0f; T <= Limit; T += StepCm)
	{
		const FVector P = Start + D * T;
		float Surface = 0.0f;
		if (!GetSurfaceZ(P.X, P.Y, Surface))
		{
			return false;   // no generator: no generated water
		}
		if (P.Z > Surface)
		{
			continue;
		}
		// The ray crossed a water surface plane between the last step and this one; it is water only where the ground there is under it (the sea plane runs under
		// the land everywhere).
		const EMOWaterBodyKind Kind = ClassifyWaterAt(P.X, P.Y);
		if (Kind != EMOWaterBodyKind::None)
		{
			OutPoint = FVector(P.X, P.Y, Surface);
			OutKind = Kind;
			return true;
		}
		return false;   // under the plane but over land: the land was in the way
	}
	return false;
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
