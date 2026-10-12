/**
 * MOOceanologyIntegration.cpp - see the header
 */

#include "MOOceanologyIntegration.h"
#include "MOTickWatchdog.h"
#include "MOFramework.h"
#include "Actors/OceanologyWaterParentActor.h"
#include "Components/BoxComponent.h"
#include "Components/OceanologyUnderwaterComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Structs/OceanologyUnderwater.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarFixUnderwaterVolume(
		TEXT("MO.Water.FixUnderwaterVolume"),
		1,
		TEXT("Dev: 1 (default) = give Oceanology's underwater volume a query-only body so its post process can find the camera; 0 = leave the plugin's own no-collision volume (reproduces the missing underwater look). Set it before the world starts."),
		ECVF_Default);

	/** The plugin ships 0.05 for both; Wes asked for a quarter of the look it had (2026-10-11). */
	TAutoConsoleVariable<float> CVarUnderwaterDistortion(
		TEXT("MO.Water.UnderwaterDistortion"),
		0.0125f,
		TEXT("Cap on the underwater screen wobble (Oceanology's DistortionIntensity and VignetteDistortion; the plugin's own look is 0.05, 0 = none). Applied to every water actor within half a second."),
		ECVF_Default);
}

namespace MOOceanology
{
	bool IsUnderwaterVolumeQueryable(const AOceanologyWaterParent& Water)
	{
		const UBoxComponent* Box = Water.UnderwaterBoxVolumeComponent;
		return Box && Box->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
	}

	float GetUnderwaterDistortionCap()
	{
		return FMath::Max(0.0f, CVarUnderwaterDistortion.GetValueOnGameThread());
	}

	bool ApplyUnderwaterLook(AOceanologyWaterParent& Water)
	{
		UOceanologyUnderwaterComponent* Underwater = Water.UnderwaterComponent;
		if (!Underwater || !Water.HasActorBegunPlay() || Underwater->UnderwaterMode != EOceanologyUnderwaterMode::Underwater)
		{
			return false;
		}
		const float Cap = GetUnderwaterDistortionCap();
		FOceanologyUnderwater& Look = Underwater->Underwater;
		if (Look.DistortionIntensity <= Cap && Look.VignetteDistortion <= Cap)
		{
			return false;
		}
		Look.DistortionIntensity = FMath::Min(Look.DistortionIntensity, Cap);
		Look.VignetteDistortion = FMath::Min(Look.VignetteDistortion, Cap);
		if (UMaterialInstanceDynamic* Material = Underwater->GetUnderwaterMID())
		{
			UOceanologyUnderwaterHelper::SetMaterialParameters(Material, Look);
		}
		return true;
	}

	bool EnsureUnderwaterVolumeQueryable(AOceanologyWaterParent& Water)
	{
		UBoxComponent* Box = Water.UnderwaterBoxVolumeComponent;
		if (CVarFixUnderwaterVolume.GetValueOnGameThread() == 0 || !Box || Box->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			return false;   // nothing to fix: no volume, or it already has a body (the lakes' box overlaps pawns by design)
		}
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionResponseToAllChannels(ECR_Ignore);   // a body, but one nothing can hit or overlap: the ocean-wide box must be invisible to gameplay
		Box->SetGenerateOverlapEvents(false);
		UE_LOG(LogMOFramework, Log, TEXT("[MOWater] %s: underwater volume made queryable (query-only, ignores every channel) so its post process can find the camera"), *Water.GetName());
		return true;
	}
}

TStatId UMOOceanologyIntegrationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMOOceanologyIntegrationSubsystem, STATGROUP_Tickables);
}

void UMOOceanologyIntegrationSubsystem::Tick(float DeltaTime)
{
	MO_TICK_WATCH(TEXT("UMOOceanologyIntegrationSubsystem"));
	Super::Tick(DeltaTime);
	TimeSinceTune += DeltaTime;
	if (TimeSinceTune < TuneIntervalSeconds)
	{
		return;
	}
	TimeSinceTune = 0.0f;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AOceanologyWaterParent> It(World); It; ++It)
		{
			MOOceanology::ApplyUnderwaterLook(**It);
		}
	}
}

bool UMOOceanologyIntegrationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UMOOceanologyIntegrationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UWorld* World = GetWorld())
	{
		// Water actors spawned after this point (nothing does today, but a lake or a modder's ocean may).
		ActorSpawnedHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UMOOceanologyIntegrationSubsystem::HandleActorSpawned));
	}
}

void UMOOceanologyIntegrationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// The level's own oceans exist already.
	for (TActorIterator<AOceanologyWaterParent> It(&InWorld); It; ++It)
	{
		MOOceanology::EnsureUnderwaterVolumeQueryable(**It);
	}
}

void UMOOceanologyIntegrationSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}
	ActorSpawnedHandle.Reset();
	Super::Deinitialize();
}

void UMOOceanologyIntegrationSubsystem::HandleActorSpawned(AActor* Actor)
{
	if (AOceanologyWaterParent* Water = Cast<AOceanologyWaterParent>(Actor))
	{
		MOOceanology::EnsureUnderwaterVolumeQueryable(*Water);
	}
}
