#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MOWeatherTypes.h"
#include "MOWeatherBlueprintLibrary.generated.h"

/**
 * Blueprint function library for weather-related helpers.
 * Use these to easily construct weather structs from UDS/UDW values.
 */
UCLASS()
class MOFRAMEWORK_API UMOWeatherBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Create FMOTimeOfDay from UDS values. Hour/Minute are calculated automatically.
	 * @param TimeValue Time of Day from UDS (0-2400)
	 * @param Season Season from UDS (0-4 float)
	 * @param bIsDaytime Is It Daytime from UDS
	 * @param DayOfYear Day of year (optional, defaults to 1)
	 * @return Fully populated FMOTimeOfDay struct
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather", meta=(DisplayName="Make Time Of Day (from UDS)"))
	static FMOTimeOfDay MakeTimeOfDayFromUDS(float TimeValue, float Season, bool bIsDaytime, int32 DayOfYear = 1);

	/**
	 * Create FMOWeatherState from UDW values.
	 * @param DisplayName Weather display name from UDW
	 * @param CloudCoverage Cloud coverage 0-1
	 * @param Fog Fog density 0-1
	 * @param Rain Rain intensity 0-1
	 * @param Snow Snow intensity 0-1
	 * @param WindIntensity Wind intensity 0-1
	 * @param WindDirection Wind direction rotator
	 * @param Thunder Thunder/lightning intensity 0-1
	 * @return Fully populated FMOWeatherState struct
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather", meta=(DisplayName="Make Weather State (from UDW)"))
	static FMOWeatherState MakeWeatherStateFromUDW(
		FText DisplayName,
		float CloudCoverage,
		float Fog,
		float Rain,
		float Snow,
		float WindIntensity,
		FRotator WindDirection,
		float Thunder);

	/**
	 * Create FMOWeatherExposure from UDW Test Actor for Weather Exposure result.
	 * @param Rain Rain exposure 0-1
	 * @param Snow Snow exposure 0-1
	 * @param Wind Wind exposure 0-1
	 * @param Dust Dust exposure 0-1
	 * @param bIsSheltered Whether location is sheltered
	 * @return Fully populated FMOWeatherExposure struct
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather", meta=(DisplayName="Make Weather Exposure (from UDW)"))
	static FMOWeatherExposure MakeWeatherExposureFromUDW(
		float Rain,
		float Snow,
		float Wind,
		float Dust,
		bool bIsSheltered);
};
