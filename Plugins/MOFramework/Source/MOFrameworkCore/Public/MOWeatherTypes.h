/**
 * =============================================================================
 * MOWeatherTypes.h - Weather System Data Structures
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Data structures for the weather system. Defines weather exposure, weather
 * state, time of day, temperature units, and save data structures.
 *
 * KEY TYPES:
 * - FMOWeatherExposure: Rain/snow/wind/dust exposure values (0-1)
 * - FMOWeatherState: Global weather conditions (cloud, fog, rain, wind)
 * - FMOTimeOfDay: Time and season information
 * - EMOTemperatureUnit: Celsius vs Fahrenheit
 * - FMOExposureShelter: Multi-axis shelter test result (overhead/wind/enclosure)
 * - FMOWeatherSaveData: Persistence structure
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] UDS TIME FORMAT: TimeValue uses UDS format 0-2400 where last two
 *   digits are minutes (1230 = 12:30, 600 = 6:00). Use CalculateHourMinuteFromTimeValue().
 *
 * [2024-02] EXPOSURE VALUES: All exposure values are 0.0-1.0 normalized.
 *   Use HasSignificantExposure() with threshold for meaningful checks.
 *
 * [2024-02] SAVE DATA PRESET: WeatherPresetObject is Transient and won't
 *   serialize to disk. Blueprint must handle preset restoration separately.
 *
 * [2026-05] SAVE DATA PRESET FIX: Added WeatherPresetPath (FSoftObjectPath)
 *   which DOES serialize to disk. Bridge BP populates it from the runtime
 *   object via "Make Soft Object Path" on build, and resolves it via "Load
 *   Synchronous" on apply. WeatherPresetObject stays as a transient runtime
 *   cache for efficiency but disk roundtrips go through WeatherPresetPath.
 *   Verified working via MO.Weather.* console commands + save/load test.
 *
 * =============================================================================
 * RELATED FILES: MOWeatherProviderInterface.h, MOWeatherIntegrationSubsystem.h
 * LAST UPDATED: 2026-05-24
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOWeatherTypes.generated.h"

/**
 * Weather exposure values at a specific location.
 * Values range from 0.0 (no exposure) to 1.0 (full exposure).
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORKCORE_API FMOWeatherExposure
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
struct MOFRAMEWORKCORE_API FMOWeatherState
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
struct MOFRAMEWORKCORE_API FMOTimeOfDay
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

/**
 * Comprehensive shelter / exposure-protection result for a world location.
 *
 * Models three INDEPENDENT axes — a real shelter situation is the combination
 * of all three:
 *
 *   1. OverheadCover — blocks rain, snow, sky radiative heat loss.
 *      Bus-stop overhang: HIGH. Open field: ZERO.
 *
 *   2. WindShelter — blocks wind, dust, wind-driven precipitation.
 *      Computed RELATIVE to wind direction — a wall to the north only counts
 *      if wind is from the north. Bus-stop overhang: ZERO. Doorway recess
 *      facing away from wind: HIGH.
 *
 *   3. Enclosure — overall "indoors-ness" from close (~3m) geometry on all
 *      sides. Feeds thermal/insulation systems (the math for "how warm
 *      indoors" lives there, not here).
 *      Small cabin: HIGH. Standing under tree in open field: LOW.
 *
 * Produced by UMOWeatherBlueprintLibrary::TestExposureShelter (comprehensive)
 * or TestOverheadCover (cheap, only fills OverheadCover).
 *
 * NOTE: Tests `ECC_Visibility`. Foliage that should block weather must have
 * collision configured to block visibility. If a tree shows zero overhead
 * cover even though you're under it, that's a collision-channel issue, not
 * a math issue.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORKCORE_API FMOExposureShelter
{
	GENERATED_BODY()

	// --- Three axes (continuous 0-1) ---

	/** Fraction of upper hemisphere blocked by geometry within ~50m. Primary
	 *  driver for rain/snow protection and sky radiative heat loss. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Weather")
	float OverheadCover = 0.0f;

	/** Distance-weighted wind-blocking score for the UPWIND direction passed
	 *  to the test. 0 = wind blows freely. 1 = solid windbreak directly upwind.
	 *  Closer geometry contributes more (a wall at 1m is better than a wall
	 *  at 14m). If wind direction was zero/unknown, this stays at 0. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Weather")
	float WindShelter = 0.0f;

	/** How densely the point is surrounded by close (<3m) geometry. Drives
	 *  "indoors" determination for thermal/insulation systems. Does NOT
	 *  modulate the four weather exposure channels directly. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Weather")
	float Enclosure = 0.0f;

	// --- Convenience bools (computed from the axes) ---

	/** OverheadCover >= 0.5 — "there's a roof above me." */
	UPROPERTY(BlueprintReadOnly, Category="MO|Weather")
	bool bIsCovered = false;

	/** WindShelter >= 0.5 — "I have a windbreak between me and the wind." */
	UPROPERTY(BlueprintReadOnly, Category="MO|Weather")
	bool bIsWindSheltered = false;

	/** Enclosure >= 0.85 AND close overhead + walls both >= 0.7 — "I'm inside
	 *  a building or small enclosed space." */
	UPROPERTY(BlueprintReadOnly, Category="MO|Weather")
	bool bIsIndoors = false;

	// --- Raw counts (for callers who want custom thresholds) ---

	UPROPERTY(BlueprintReadOnly, Category="MO|Weather")
	int32 OverheadHits = 0;

	UPROPERTY(BlueprintReadOnly, Category="MO|Weather")
	int32 OverheadRayCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="MO|Weather")
	int32 WallHits = 0;

	UPROPERTY(BlueprintReadOnly, Category="MO|Weather")
	int32 WallRayCount = 0;
};

/**
 * Save data for weather and time of day state.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORKCORE_API FMOWeatherSaveData
{
	GENERATED_BODY()

	/** Full date and time from UDS (includes year, month, day, hour, minute, second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather|Save")
	FDateTime DateTime;

	/**
	 * Soft path to the active weather preset asset (e.g. an instance of
	 * UDS_Weather_Settings_C like Rain, Snow_Blizzard, etc.).
	 *
	 * SERIALIZES TO DISK as a path string and is resolved on load — unlike a
	 * raw UObject* / TObjectPtr<UObject> which would either need
	 * UPROPERTY(Transient) (lost across disk save/load) or full inline
	 * serialization (heavy + brittle to asset moves).
	 *
	 * On Build: convert the runtime preset object via "Make Soft Object Path
	 * from Object" (BP) or `FSoftObjectPath(PresetObject)` (C++).
	 * On Apply: resolve via "Load Synchronous" (BP) or `TryLoad()` (C++),
	 * cast to UDS_Weather_Settings_C, pass to UDW Change Weather.
	 *
	 * If the asset moves or is renamed between save and load, the path will
	 * resolve to null and weather falls back to whatever UDW currently shows.
	 * The cloud/fog fields below are direct values that always survive.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather|Save")
	FSoftObjectPath WeatherPresetPath;

	/**
	 * Runtime-only cache of the resolved preset object. Populated by Build /
	 * Apply for in-session efficiency. NOT serialized — disk persistence uses
	 * WeatherPresetPath above.
	 */
	UPROPERTY(Transient, BlueprintReadWrite, Category="MO|Weather|Save")
	TObjectPtr<UObject> WeatherPresetObject;

	/** Cloud coverage override (0-1, -1 means use weather preset default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather|Save")
	float CloudCoverage = -1.0f;

	/** Fog density override (0-1, -1 means use weather preset default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather|Save")
	float FogDensity = -1.0f;

	/** Whether this save data is valid/populated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather|Save")
	bool bIsValid = false;
};
