#include "MOEyeAdaptationComponent.h"
#include "MOAmbientEnvironmentProvider.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"

UMOEyeAdaptationComponent::UMOEyeAdaptationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;   // the eye is slow; ten looks a second is plenty
	SetIsReplicatedByDefault(false);            // cosmetic, per viewer
}

float UMOEyeAdaptationComponent::AdaptationFloorEV100(float IlluminanceLux, float Albedo, float BiasEV)
{
	const float Luminance = FMath::Max(IlluminanceLux, 1.0e-6f) * FMath::Clamp(Albedo, 0.01f, 1.0f) / UE_PI;
	return FMath::Log2(Luminance * 100.0f / 12.5f) + BiasEV;
}

UCameraComponent* UMOEyeAdaptationComponent::FindCamera()
{
	if (!Camera.IsValid() && GetOwner())
	{
		Camera = GetOwner()->FindComponentByClass<UCameraComponent>();
	}
	return Camera.Get();
}

void UMOEyeAdaptationComponent::ClearFloor(UCameraComponent* Cam)
{
	if (bFloorActive && Cam)
	{
		Cam->PostProcessSettings.bOverride_AutoExposureMinBrightness = false;
	}
	bFloorActive = false;
}

void UMOEyeAdaptationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const APawn* Pawn = Cast<APawn>(GetOwner());
	UCameraComponent* Cam = FindCamera();
	if (!Pawn || !Cam)
	{
		return;
	}
	if (!Pawn->IsLocallyControlled() || !Pawn->IsPlayerControlled())
	{
		ClearFloor(Cam);   // nobody here looks through this pawn's eyes
		return;
	}
	const UMOAmbientEnvironmentRegistry* Registry = UMOAmbientEnvironmentRegistry::Get(this);
	LastIlluminanceLux = Registry ? Registry->GetIlluminanceLuxAt(Cam->GetComponentLocation()) : 0.0f;
	if (LastIlluminanceLux < MinIlluminanceLux)
	{
		ClearFloor(Cam);
		return;
	}
	const float Target = FMath::Clamp(AdaptationFloorEV100(LastIlluminanceLux, SurroundAlbedo, FloorBiasEV), -10.0f, 8.0f);
	CurrentFloorEV100 = bFloorActive ? FMath::FInterpTo(CurrentFloorEV100, Target, DeltaTime, FloorFollowSpeed) : Target;
	bFloorActive = true;
	Cam->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
	Cam->PostProcessSettings.AutoExposureMinBrightness = CurrentFloorEV100;
}
