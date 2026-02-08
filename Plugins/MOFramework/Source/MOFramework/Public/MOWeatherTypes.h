#pragma once

#include "CoreMinimal.h"
#include "MOWeatherTypes.generated.h"

/**
 * Weather exposure values at a specific location.
 * Values range from 0.0 (no exposure) to 1.0 (full exposure).
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWeatherExposure
{
	GENERATED_BODY()

	/** Exposure to rain (0-1). Affects wetness, visibility, fire extinguishing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather")
	float Rain = 0.0f;

	/** Exposure to snow (0-1). Affects cold stress, movement speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather")
	float Snow = 0.0f;

	/** Exposure to wind (0-1). Affects wind chill, projectile accuracy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather")
	float Wind = 0.0f;

	/** Exposure to dust/sand (0-1). Affects visibility, breathing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather")
	float Dust = 0.0f;

	/** Whether the location is considered sheltered/indoors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather")
	bool bIsSheltered = false;

	/** Get combined precipitation exposure (rain + snow). */
	float GetPrecipitationExposure() const { return FMath::Max(Rain, Snow); }

	/** Check if experiencing any significant weather exposure. */
	bool HasSignificantExposure(float Threshold = 0.1f) const
	{
		return Rain > Threshold || Snow > Threshold || Wind > Threshold || Dust > Threshold;
	}
};

/**
 * Current global weather state.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWeatherState
{
	GENERATED_BODY()

	/** Human-readable weather description (e.g., "Light Rain", "Blizzard"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather")
	FText DisplayName;

	/** Cloud coverage (0-1). Affects solar radiation, visibility. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather")
	float CloudCoverage = 0.0f;

	/** Fog density (0-1). Affects visibility. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather")
	float Fog = 0.0f;

	/** Global rain intensity (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather")
	float RainIntensity = 0.0f;

	/** Global snow intensity (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather")
	float SnowIntensity = 0.0f;

	/** Wind intensity (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather")
	float WindIntensity = 0.0f;

	/** Wind direction as a rotator. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather")
	FRotator WindDirection = FRotator::ZeroRotator;

	/** Thunder/lightning intensity (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather")
	float ThunderIntensity = 0.0f;

	/** Is it currently raining (above threshold)? */
	bool IsRaining(float Threshold = 0.1f) const { return RainIntensity > Threshold; }

	/** Is it currently snowing (above threshold)? */
	bool IsSnowing(float Threshold = 0.1f) const { return SnowIntensity > Threshold; }

	/** Is there a storm (thunder/lightning)? */
	bool IsStorming(float Threshold = 0.3f) const { return ThunderIntensity > Threshold; }
};

/**
 * Time and season information.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOTimeOfDay
{
	GENERATED_BODY()

	/** Time of day as float (0-2400, where 0=midnight, 600=6am, 1200=noon, 1800=6pm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather")
	float TimeValue = 1200.0f;

	/** Hour component (0-23). Calculated automatically from TimeValue if not set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather")
	int32 Hour = 12;

	/** Minute component (0-59). Calculated automatically from TimeValue if not set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather")
	int32 Minute = 0;

	/** Whether the sun is up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather")
	bool bIsDaytime = true;

	/** Current season (0=Spring, 1=Summer, 2=Autumn, 3=Winter). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather")
	float Season = 1.0f;

	/** Day of the year (1-365). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather")
	int32 DayOfYear = 1;

	/** Get season as integer (0-3). */
	int32 GetSeasonIndex() const { return FMath::Clamp(FMath::FloorToInt(Season), 0, 3); }

	/** Get season name. */
	FText GetSeasonName() const
	{
		static const FText SeasonNames[] = {
			NSLOCTEXT("MO", "Spring", "Spring"),
			NSLOCTEXT("MO", "Summer", "Summer"),
			NSLOCTEXT("MO", "Autumn", "Autumn"),
			NSLOCTEXT("MO", "Winter", "Winter")
		};
		return SeasonNames[GetSeasonIndex()];
	}

	/**
	 * Calculate Hour and Minute from TimeValue.
	 * Call this after setting TimeValue to auto-populate Hour/Minute.
	 * UDS format: 0-2400 where last two digits are minutes (0-59).
	 */
	void CalculateHourMinuteFromTimeValue()
	{
		// UDS format: 1230 = 12:30, 600 = 6:00, 1800 = 18:00
		Hour = FMath::Clamp(FMath::FloorToInt(TimeValue / 100.0f), 0, 23);
		Minute = FMath::Clamp(FMath::FloorToInt(FMath::Fmod(TimeValue, 100.0f)), 0, 59);
	}

	/**
	 * Create from just the essential UDS/UDW values. Hour/Minute calculated automatically.
	 * @param InTimeValue Time from UDS (0-2400)
	 * @param InSeason Season from UDS (0-4)
	 * @param bInIsDaytime Is It Daytime from UDS
	 * @param InDayOfYear Optional day of year (defaults to 1)
	 */
	static FMOTimeOfDay FromUDSValues(float InTimeValue, float InSeason, bool bInIsDaytime, int32 InDayOfYear = 1)
	{
		FMOTimeOfDay Result;
		Result.TimeValue = InTimeValue;
		Result.Season = InSeason;
		Result.bIsDaytime = bInIsDaytime;
		Result.DayOfYear = InDayOfYear;
		Result.CalculateHourMinuteFromTimeValue();
		return Result;
	}
};

/**
 * Temperature unit preference.
 */
UENUM(BlueprintType)
enum class EMOTemperatureUnit : uint8
{
	Celsius,
	Fahrenheit
};

/**
 * Weather change event type.
 */
UENUM(BlueprintType)
enum class EMOWeatherEventType : uint8
{
	None,
	StartedRaining,
	StoppedRaining,
	StartedSnowing,
	StoppedSnowing,
	WeatherChanged,
	TemperatureThresholdCrossed
};
