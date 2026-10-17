#include "MOLightSourceSettings.h"

UMOLightSourceSettings::UMOLightSourceSettings()
{
	// Calibration (Docs/Light_Sources.md): Medina-Alcaide et al. 2021 (PLOS ONE 16(6) e0250497) burned replica Paleolithic lights in Isuntza 1 cave. Their best
	// juniper torch: 14 cd, a 2.5 m working radius, 61 minutes (others 21-41); a stone lamp of bone marrow with three juniper wicks: 0.6 cd, 1.6 m, over an hour.
	FMOLightSourceProfile& Torch = Profiles.Add(TEXT("Torch01"));
	Torch.LuminousIntensityCd = 10.0f;
	Torch.AttenuationRadiusCm = 1500.0f;
	Torch.ColorTemperatureK = 1900.0f;
	Torch.SourceRadiusCm = 5.0f;
	Torch.FlickerAmount = 0.18f;
	Torch.FlickerHz = 6.0f;
	Torch.StillIntensityScale = 0.6f;
	Torch.LightSeconds = 6.0f;
	Torch.BurnSecondsPerDurability = 30.0f;
	Torch.FlameEffect = FSoftObjectPath(TEXT("/Game/M5VFXVOL2/Niagara/Fire_for_BP/NTorchfire_BP.NTorchfire_BP"));

	// A lamp burns steadily whether it is moved or not, and dazzles more than it shows: a small low flame, light thrown sideways.
	FMOLightSourceProfile& Lamp = Profiles.Add(TEXT("OilLamp01"));
	Lamp.LuminousIntensityCd = 0.8f;
	Lamp.AttenuationRadiusCm = 700.0f;
	Lamp.ColorTemperatureK = 1850.0f;
	Lamp.SourceRadiusCm = 1.5f;
	Lamp.FlickerAmount = 0.06f;
	Lamp.FlickerHz = 3.0f;
	Lamp.StillIntensityScale = 1.0f;
	Lamp.LightSeconds = 4.0f;
	Lamp.BurnSecondsPerDurability = 90.0f;
	Lamp.FlamePositionAlongMesh = 0.75f;
}

const FMOLightSourceProfile* UMOLightSourceSettings::Find(FName ItemId)
{
	const UMOLightSourceSettings* Settings = GetDefault<UMOLightSourceSettings>();
	return Settings && !ItemId.IsNone() ? Settings->Profiles.Find(ItemId) : nullptr;
}

float UMOLightSourceSettings::AirflowScale(const FMOLightSourceProfile& Profile, float SpeedCmPerSec)
{
	const float Still = FMath::Clamp(Profile.StillIntensityScale, 0.05f, 1.0f);
	const float Full = FMath::Max(Profile.FullAirflowSpeedCmPerSec, 1.0f);
	return FMath::Lerp(Still, 1.0f, FMath::Clamp(SpeedCmPerSec / Full, 0.0f, 1.0f));
}
