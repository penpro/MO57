#include "MOWeatherBlueprintLibrary.h"

FMOTimeOfDay UMOWeatherBlueprintLibrary::MakeTimeOfDayFromUDS(float TimeValue, float Season, bool bIsDaytime, int32 DayOfYear)
{
	return FMOTimeOfDay::FromUDSValues(TimeValue, Season, bIsDaytime, DayOfYear);
}

FMOWeatherState UMOWeatherBlueprintLibrary::MakeWeatherStateFromUDW(
	FText DisplayName,
	float CloudCoverage,
	float Fog,
	float Rain,
	float Snow,
	float WindIntensity,
	FRotator WindDirection,
	float Thunder)
{
	FMOWeatherState State;
	State.DisplayName = DisplayName;
	State.CloudCoverage = CloudCoverage;
	State.Fog = Fog;
	State.RainIntensity = Rain;
	State.SnowIntensity = Snow;
	State.WindIntensity = WindIntensity;
	State.WindDirection = WindDirection;
	State.ThunderIntensity = Thunder;
	return State;
}

FMOWeatherExposure UMOWeatherBlueprintLibrary::MakeWeatherExposureFromUDW(
	float Rain,
	float Snow,
	float Wind,
	float Dust,
	bool bIsSheltered)
{
	FMOWeatherExposure Exposure;
	Exposure.Rain = Rain;
	Exposure.Snow = Snow;
	Exposure.Wind = Wind;
	Exposure.Dust = Dust;
	Exposure.bIsSheltered = bIsSheltered;
	return Exposure;
}
