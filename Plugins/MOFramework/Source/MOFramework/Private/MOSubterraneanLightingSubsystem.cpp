#include "MOSubterraneanLightingSubsystem.h"
#include "MOFramework.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"

namespace
{
	TAutoConsoleVariable<int32> CVarBlockFromBelow(
		TEXT("MO.Light.BlockFromBelow"), 1,
		TEXT("1 = directional lights that shine UP (sun/moon below the horizon) do not light the world. 0 = off (A/B the lighting)."),
		ECVF_Default);

	constexpr float CheckIntervalSeconds = 0.1f;
}

bool UMOSubterraneanLightingSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	// Played worlds only: not editor preview, not cook, not a dedicated server (nothing renders there).
	return World && World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer && Super::ShouldCreateSubsystem(Outer);
}

TStatId UMOSubterraneanLightingSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMOSubterraneanLightingSubsystem, STATGROUP_Tickables);
}

float UMOSubterraneanLightingSubsystem::AllowedFraction(const FVector& LightForward)
{
	// sin(5 degrees): the light's forward Z when it points 5 degrees up (i.e. the light is 5 degrees below the horizon).
	constexpr double FullyBlockedForwardZ = 0.0871557;
	return (float)FMath::Clamp(1.0 - LightForward.Z / FullyBlockedForwardZ, 0.0, 1.0);
}

void UMOSubterraneanLightingSubsystem::Deinitialize()
{
	// Give the lights back exactly as the sky left them.
	for (const TPair<TWeakObjectPtr<UDirectionalLightComponent>, FLightState>& Pair : Tracked)
	{
		if (UDirectionalLightComponent* Light = Pair.Key.Get())
		{
			if (FMath::IsNearlyEqual(Light->Intensity, Pair.Value.LastWritten, 1e-4f))
			{
				Light->SetIntensity(Pair.Value.Intended);
			}
		}
	}
	Tracked.Empty();
	Super::Deinitialize();
}

void UMOSubterraneanLightingSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TimeSinceCheck += DeltaTime;
	if (TimeSinceCheck >= CheckIntervalSeconds)
	{
		TimeSinceCheck = 0.0f;
		Update();
	}
}

void UMOSubterraneanLightingSubsystem::Update()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const bool bEnabled = CVarBlockFromBelow.GetValueOnGameThread() != 0;

	for (TObjectIterator<UDirectionalLightComponent> It; It; ++It)
	{
		UDirectionalLightComponent* Light = *It;
		if (!Light || Light->GetWorld() != World || !Light->IsRegistered())
		{
			continue;
		}

		FLightState* State = Tracked.Find(Light);
		if (!State)
		{
			State = &Tracked.Add(Light);
			State->Intended = State->LastWritten = Light->Intensity;
		}
		else if (!FMath::IsNearlyEqual(Light->Intensity, State->LastWritten, 1e-4f))
		{
			State->Intended = Light->Intensity;   // the sky (or MO.Light.Test) wrote a new value: that is what is wanted now
		}

		const float Target = State->Intended * (bEnabled ? AllowedFraction(Light->GetForwardVector()) : 1.0f);
		if (!FMath::IsNearlyEqual(Light->Intensity, Target, 1e-4f))
		{
			Light->SetIntensity(Target);
		}
		State->LastWritten = Target;
	}

	// Forget lights that were destroyed.
	for (auto It = Tracked.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
