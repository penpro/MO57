#include "MOWaterSubsystem.h"
#include "MOFramework.h"
#include "MOWorldGen.h"
#include "MOWorldSeedSubsystem.h"
#include "MOUplandLakeSubsystem.h"
#include "Actors/OceanologyInfiniteOceanActor.h"
#include "Actors/OceanologyLakeActor.h"
#include "Components/Wave/OceanologyGerstnerWaveSolverComponent.h"
#include "ShaderUtils/OceanologyLegacyGerstnerWaveUtils.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PhysicsVolume.h"

bool UMOWaterSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UMOWaterSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	GetSwimVolume();
}

UMOWaterSubsystem* UMOWaterSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UMOWaterSubsystem>() : nullptr;
}

bool UMOWaterSubsystem::GetSurfaceZ(double X, double Y, float& OutSurfaceZ) const
{
	if (!GetStillSurfaceZ(X, Y, OutSurfaceZ))
	{
		return false;
	}
	OutSurfaceZ += GetWaveOffsetCm(X, Y, OutSurfaceZ);
	return true;
}

bool UMOWaterSubsystem::GetStillSurfaceZ(double X, double Y, float& OutSurfaceZ) const
{
	const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(GetWorld());
	if (!Seeds || !Seeds->HasActiveSeed() || !FMOWorldGenParamsProvider::HasPublished())
	{
		return false;
	}
	const FMOWorldGenParamsRef Params = FMOWorldGenParamsProvider::Get();
	return FMOWorldGen::GetWaterSurfaceCm(*Params, X, Y, Seeds->GetActiveTerrainSeed(), OutSurfaceZ);
}

float UMOWaterSubsystem::GetWaveOffsetCm(double X, double Y, float StillSurfaceZ) const
{
	if (!IsInGameThread())
	{
		return 0.0f;
	}
	const AOceanologyWaterParent* Body = FindWaterBody(X, Y, StillSurfaceZ);
	const UOceanologyGerstnerWaveSolverComponent* Waves = Body ? Cast<UOceanologyGerstnerWaveSolverComponent>(Body->GetWaveSolver()) : nullptr;
	if (!Waves)
	{
		return 0.0f;
	}
	// The clock the water is drawn with (UOceanologyWaveSolverComponent::SetGameTimeInSeconds): the server's, so every machine draws -- and swims on -- the same waves. Computed here rather
	// than read from the solver (it stores the time it last ticked, and its own height query reads an owner it only has once the level began play).
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const float Time = static_cast<float>(GameState && Waves->WaveTimeMode == EOceanologyWaveTimeMode::ServerTimeForReplication ? GameState->GetServerWorldTimeSeconds()
		: GetWorld()->GetTimeSeconds());
	const float Offset = WaveHeightOver(X, Y, [Waves, Time](const FVector& Still)
	{
		return UOceanologyLegacyGerstnerWaveUtils::ComputeGerstnerWaves(Still, Time, Waves->GlobalDisplacement, Waves->BaseOffset, Waves->Wave_1, Waves->Wave_2, Waves->Wave_3, Waves->Wave_4);
	});
	return FMath::IsFinite(Offset) ? Offset : 0.0f;   // a zero swell amplitude makes the sum NaN (it divides by it): such water is not drawn at all (Docs/Water.md section 5a)
}

float UMOWaterSubsystem::WaveHeightOver(double X, double Y, TFunctionRef<FVector(const FVector&)> Displacement)
{
	FVector From(X, Y, 0.0);
	for (int32 Step = 0; Step < WaveInversionSteps; ++Step)
	{
		const FVector Moved = Displacement(From);
		From = FVector(X - Moved.X, Y - Moved.Y, 0.0);
	}
	return static_cast<float>(Displacement(From).Z);
}

AOceanologyWaterParent* UMOWaterSubsystem::FindWaterBody(double X, double Y, float StillSurfaceZ) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	if (!FMath::IsNearlyEqual(StillSurfaceZ, FMOWorldGenParamsProvider::Get()->Tuning.SeaLevelCm, 0.01f))
	{
		// Not the sea plane: an upland lake (FMOWorldGen::ClassifyWaterBody tells them apart the same way) -- its own actor, when one is spawned round a player.
		const UMOUplandLakeSubsystem* Lakes = UMOUplandLakeSubsystem::Get(World);
		AOceanologyLake* Lake = Lakes ? Lakes->FindLakeBodyAt(X, Y) : nullptr;
		return Lake && FMath::Abs(Lake->GetActorLocation().Z - StillSurfaceZ) < 1.0 ? Lake : nullptr;
	}
	if (!Ocean.IsValid() && World->GetTimeSeconds() >= NextOceanSearchSeconds)
	{
		NextOceanSearchSeconds = World->GetTimeSeconds() + 1.0;
		for (TActorIterator<AOceanologyInfiniteOcean> It(World); It; ++It)
		{
			Ocean = *It;
			break;
		}
	}
	return Ocean.Get();
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
	if (!SwimVolume && GetWorld() && IsInGameThread())
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Name = TEXT("MOSwimVolume");
		Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;   // a name already taken gets a suffix; the default (Required_Fatal) crashes
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
