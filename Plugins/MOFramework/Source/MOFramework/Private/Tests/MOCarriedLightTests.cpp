// A carried flame (UMOCarriedLightComponent + UMOLightSourceSettings): calibrated to measured Paleolithic lights, brighter when moved, placed at the far end of
// what is held, and burning down a durability point per BurnSecondsPerDurability with nothing lost between ticks.

#include "Misc/AutomationTest.h"
#include "MOCarriedLightComponent.h"
#include "MOLightSourceSettings.h"
#include "MOFireActions.h"
#include "MOEyeAdaptationComponent.h"
#include "MOAmbientEnvironmentProvider.h"
#include "MOFlameLightComponent.h"
#include "Engine/Scene.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCarriedLight_Calibration,
	"MOFramework.Light.Calibration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOCarriedLight_Calibration::RunTest(const FString& Parameters)
{
	const FMOLightSourceProfile* Torch = UMOLightSourceSettings::Find(TEXT("Torch01"));
	const FMOLightSourceProfile* Lamp = UMOLightSourceSettings::Find(TEXT("OilLamp01"));
	if (!TestNotNull(TEXT("a torch is a light source"), Torch) || !TestNotNull(TEXT("an oil lamp is a light source"), Lamp))
	{
		return false;
	}
	TestNull(TEXT("a stone is not"), UMOLightSourceSettings::Find(TEXT("Stone01")));
	TestNull(TEXT("nothing is not"), UMOLightSourceSettings::Find(NAME_None));
	// Medina-Alcaide et al. 2021: juniper torches 3.5-14 cd (by their illuminance and their stated intensity), stone fat lamps ~0.6 cd ("about a candle").
	TestTrue(TEXT("a torch burns within the measured 3-15 cd"), Torch->LuminousIntensityCd >= 3.0f && Torch->LuminousIntensityCd <= 15.0f);
	TestTrue(TEXT("a lamp is about a candle (0.3-1.5 cd)"), Lamp->LuminousIntensityCd >= 0.3f && Lamp->LuminousIntensityCd <= 1.5f);
	TestTrue(TEXT("a torch outshines a lamp several times over"), Torch->LuminousIntensityCd > 4.0f * Lamp->LuminousIntensityCd);
	// Their torches lasted 21-61 min; the Torch01 row has 90 durability points.
	const float TorchMinutes = 90.0f * Torch->BurnSecondsPerDurability / 60.0f;
	TestTrue(TEXT("one torch lasts within the measured 20-60 min"), TorchMinutes >= 20.0f && TorchMinutes <= 61.0f);
	TestTrue(TEXT("flames are warm (1700-2100 K)"), Torch->ColorTemperatureK >= 1700.0f && Torch->ColorTemperatureK <= 2100.0f);
	// A torch needs air: held still it burns low (5 -> 28 lux swung, at 20 cm). A lamp does not care.
	TestTrue(TEXT("a torch held still burns lower"), Torch->StillIntensityScale < 0.9f);
	TestEqual(TEXT("a lamp burns the same still"), Lamp->StillIntensityScale, 1.0f, 1e-5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCarriedLight_Airflow,
	"MOFramework.Light.Airflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOCarriedLight_Airflow::RunTest(const FString& Parameters)
{
	FMOLightSourceProfile P;
	P.StillIntensityScale = 0.6f;
	P.FullAirflowSpeedCmPerSec = 120.0f;
	TestEqual(TEXT("still: the still scale"), UMOLightSourceSettings::AirflowScale(P, 0.0f), 0.6f, 1e-5f);
	TestEqual(TEXT("half a walk: half way"), UMOLightSourceSettings::AirflowScale(P, 60.0f), 0.8f, 1e-5f);
	TestEqual(TEXT("a walk: a free burn"), UMOLightSourceSettings::AirflowScale(P, 120.0f), 1.0f, 1e-5f);
	TestEqual(TEXT("running: no brighter than free"), UMOLightSourceSettings::AirflowScale(P, 600.0f), 1.0f, 1e-5f);
	TestEqual(TEXT("a nonsense speed reads as still"), UMOLightSourceSettings::AirflowScale(P, -50.0f), 0.6f, 1e-5f);
	P.StillIntensityScale = 1.0f;
	TestEqual(TEXT("a lamp profile: constant"), UMOLightSourceSettings::AirflowScale(P, 0.0f), 1.0f, 1e-5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCarriedLight_FlamePlacement,
	"MOFramework.Light.FlamePlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOCarriedLight_FlamePlacement::RunTest(const FString& Parameters)
{
	// A 60 cm torch along +Z from a grip at the origin: the flame near the top.
	const FBox Up(FVector(-3, -3, -5), FVector(3, 3, 55));
	FVector F = UMOCarriedLightComponent::FlameLocalPosition(Up, 1.0f);
	TestEqual(TEXT("tip of a +Z torch"), F.Z, 55.0, 1e-3);
	TestEqual(TEXT("on its axis"), F.X, 0.0, 1e-3);
	F = UMOCarriedLightComponent::FlameLocalPosition(Up, 0.0f);
	TestEqual(TEXT("position 0 is the grip end"), F.Z, -5.0, 1e-3);
	// Modelled along -X (the far end is the larger magnitude).
	const FBox Back(FVector(-70, -4, -4), FVector(10, 4, 4));
	F = UMOCarriedLightComponent::FlameLocalPosition(Back, 1.0f);
	TestEqual(TEXT("the far end of a -X torch"), F.X, -70.0, 1e-3);
	F = UMOCarriedLightComponent::FlameLocalPosition(Back, 0.5f);
	TestEqual(TEXT("half way along"), F.X, -30.0, 1e-3);
	TestEqual(TEXT("an empty box: at the hand"), UMOCarriedLightComponent::FlameLocalPosition(FBox(ForceInit), 1.0f), FVector::ZeroVector);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCarriedLight_Burn,
	"MOFramework.Light.Burn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOCarriedLight_Burn::RunTest(const FString& Parameters)
{
	float Acc = 0.0f;
	int32 Total = 0;
	// 45 minutes of 60 fps frames at 30 s a point: exactly 90 points, none lost to rounding.
	for (int32 Frame = 0; Frame < 45 * 60 * 60; ++Frame)
	{
		Total += UMOCarriedLightComponent::ConsumeBurn(Acc, 1.0f / 60.0f, 30.0f);
	}
	TestTrue(TEXT("45 min of frames burn 90 points (+-1 for float sums)"), Total >= 89 && Total <= 90);
	Acc = 0.0f;
	TestEqual(TEXT("a long hitch burns all it covers"), UMOCarriedLightComponent::ConsumeBurn(Acc, 95.0f, 30.0f), 3);
	TestEqual(TEXT("the remainder is kept"), Acc, 5.0f, 1e-4f);
	TestEqual(TEXT("time running backwards burns nothing"), UMOCarriedLightComponent::ConsumeBurn(Acc, -10.0f, 30.0f), 0);
	TestEqual(TEXT("and keeps the remainder"), Acc, 5.0f, 1e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCarriedLight_CarriedFire,
	"MOFramework.Light.CarriedFire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOCarriedLight_CarriedFire::RunTest(const FString& Parameters)
{
	// Fire carried from the last camp: a flame to dry kindling nearly always takes; that is why people carried fire rather than drill a new one each time.
	const float Dry = FMOFireActions::LightFromFlameChance(false);
	TestTrue(TEXT("a flame to dry kindling nearly always takes"), Dry >= 0.9f && Dry < 1.0f);
	TestTrue(TEXT("rain makes it harder"), FMOFireActions::LightFromFlameChance(true) < Dry);
	TestTrue(TEXT("carried fire beats a novice's hand drill by far"), Dry > 2.0f * FMOFireActions::LightChance(FMOFireActions::ImprovisedDrillEffectiveness, 0, false, 0.0f));
	TestTrue(TEXT("and is quicker than a bow drill attempt (90 s)"), FMOFireActions::LightFromFlameSeconds < 90.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCarriedLight_EyeAdaptation,
	"MOFramework.Light.EyeAdaptation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOCarriedLight_EyeAdaptation::RunTest(const FString& Parameters)
{
	// Flames add up by the inverse-square law.
	struct FFlame : IMOLocalLightSource
	{
		FVector At;
		float Cd;
		virtual FVector GetLightLocation() const override { return At; }
		virtual float GetLuminousIntensityCd() const override { return Cd; }
	};
	UMOAmbientEnvironmentRegistry* Registry = NewObject<UMOAmbientEnvironmentRegistry>();
	FFlame Torch;
	Torch.At = FVector(0, 0, 0);
	Torch.Cd = 10.0f;
	Registry->RegisterLightSource(&Torch);
	TestEqual(TEXT("10 cd at 2 m: 2.5 lux"), Registry->GetIlluminanceLuxAt(FVector(200, 0, 0)), 2.5f, 1e-3f);
	TestEqual(TEXT("at 4 m: a quarter"), Registry->GetIlluminanceLuxAt(FVector(0, 400, 0)), 0.625f, 1e-3f);
	TestEqual(TEXT("in the flame: held at the 50 cm minimum"), Registry->GetIlluminanceLuxAt(FVector(1, 0, 0)), 40.0f, 1e-2f);
	FFlame Fire;
	Fire.At = FVector(400, 0, 0);
	Fire.Cd = 8.0f;
	Registry->RegisterLightSource(&Fire);
	TestEqual(TEXT("two flames add"), Registry->GetIlluminanceLuxAt(FVector(200, 0, 0)), 2.5f + 2.0f, 1e-3f);
	Registry->UnregisterLightSource(&Fire);
	Registry->UnregisterLightSource(&Torch);
	TestEqual(TEXT("none left: dark"), Registry->GetIlluminanceLuxAt(FVector(200, 0, 0)), 0.0f, 1e-6f);

	// The floor: the luminance of a 0.3 surround lit by 2.5 lux is 0.239 cd/m2, EV100 = log2(0.239 x 8) = 0.93.
	TestEqual(TEXT("2.5 lux on a 0.3 surround: EV100 0.93"), UMOEyeAdaptationComponent::AdaptationFloorEV100(2.5f, 0.3f, 0.0f), 0.935f, 0.01f);
	TestEqual(TEXT("four times the light: two stops more"), UMOEyeAdaptationComponent::AdaptationFloorEV100(10.0f, 0.3f, 0.0f)
		- UMOEyeAdaptationComponent::AdaptationFloorEV100(2.5f, 0.3f, 0.0f), 2.0f, 1e-3f);
	TestEqual(TEXT("the bias adds"), UMOEyeAdaptationComponent::AdaptationFloorEV100(2.5f, 0.3f, 1.5f), 0.935f + 1.5f, 0.01f);
	TestTrue(TEXT("a torch-lit cave sits far above the sky's -10 floor"), UMOEyeAdaptationComponent::AdaptationFloorEV100(0.5f, 0.3f, 2.0f) > -3.0f);
	TestTrue(TEXT("and far below daylight (EV100 ~14)"), UMOEyeAdaptationComponent::AdaptationFloorEV100(0.5f, 0.3f, 2.0f) < 6.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCarriedLight_FlameUnits,
	"MOFramework.Light.FlameUnits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOCarriedLight_FlameUnits::RunTest(const FString& Parameters)
{
	// A station's fire light counts for the eye in candelas whatever units it was authored in (UMOFlameLightComponent).
	TestEqual(TEXT("the campfire's unitless 5000 is 8 cd"), UMOFlameLightComponent::CandelasFromUnits(5000.0f, (uint8)ELightUnits::Unitless), 8.0f, 1e-4f);
	TestEqual(TEXT("candelas are candelas"), UMOFlameLightComponent::CandelasFromUnits(10.0f, (uint8)ELightUnits::Candelas), 10.0f, 1e-5f);
	TestEqual(TEXT("lumens over 4 pi"), UMOFlameLightComponent::CandelasFromUnits(4.0f * UE_PI * 3.0f, (uint8)ELightUnits::Lumens), 3.0f, 1e-4f);
	TestEqual(TEXT("size-dependent units count as nothing"), UMOFlameLightComponent::CandelasFromUnits(10.0f, (uint8)ELightUnits::Nits), 0.0f, 1e-6f);
	TestEqual(TEXT("no light, no candelas"), UMOFlameLightComponent::CandelasOf(nullptr), 0.0f, 1e-6f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
