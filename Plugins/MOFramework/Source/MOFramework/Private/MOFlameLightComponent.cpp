#include "MOFlameLightComponent.h"
#include "Components/LocalLightComponent.h"
#include "Engine/Scene.h"

UMOFlameLightComponent::UMOFlameLightComponent()
{
	PrimaryComponentTick.bCanEverTick = false;   // the eye asks; nothing to do per frame
	SetIsReplicatedByDefault(false);            // every machine registers its own copy of the actor's lights
}

void UMOFlameLightComponent::BeginPlay()
{
	Super::BeginPlay();
	Lights.Reset();
	if (const AActor* Owner = GetOwner())
	{
		TArray<ULocalLightComponent*> Found;
		Owner->GetComponents(Found);
		for (ULocalLightComponent* Light : Found)
		{
			Lights.Add(Light);
		}
	}
	if (GetNetMode() != NM_DedicatedServer && Lights.Num() > 0)
	{
		if (UMOAmbientEnvironmentRegistry* Registry = UMOAmbientEnvironmentRegistry::Get(this))
		{
			Registry->RegisterLightSource(this);
		}
	}
}

void UMOFlameLightComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UMOAmbientEnvironmentRegistry* Registry = UMOAmbientEnvironmentRegistry::Get(this))
	{
		Registry->UnregisterLightSource(this);
	}
	Super::EndPlay(EndPlayReason);
}

float UMOFlameLightComponent::CandelasFromUnits(float Intensity, uint8 Units)
{
	switch (static_cast<ELightUnits>(Units))
	{
	case ELightUnits::Unitless:
		return Intensity * 16.0f / 10000.0f;   // UPointLightComponent: unitless brightness x16, a candela x(100 x 100)
	case ELightUnits::Candelas:
		return Intensity;
	case ELightUnits::Lumens:
		return Intensity / (4.0f * UE_PI);
	default:
		return 0.0f;                           // nits / EV depend on the light's size: not used for flames here
	}
}

float UMOFlameLightComponent::CandelasOf(const ULocalLightComponent* Light)
{
	if (!Light || !Light->IsVisible() || !Light->bAffectsWorld)
	{
		return 0.0f;
	}
	return FMath::Max(0.0f, CandelasFromUnits(Light->Intensity, static_cast<uint8>(Light->IntensityUnits)));
}

FVector UMOFlameLightComponent::GetLightLocation() const
{
	float Best = -1.0f;
	FVector At = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	for (const TWeakObjectPtr<ULocalLightComponent>& Weak : Lights)
	{
		const ULocalLightComponent* Light = Weak.Get();
		const float Cd = CandelasOf(Light);
		if (Light && Cd > Best)
		{
			Best = Cd;
			At = Light->GetComponentLocation();
		}
	}
	return At;
}

float UMOFlameLightComponent::GetLuminousIntensityCd() const
{
	float Sum = 0.0f;
	for (const TWeakObjectPtr<ULocalLightComponent>& Weak : Lights)
	{
		Sum += CandelasOf(Weak.Get());
	}
	return Sum;
}
