/**
 * =============================================================================
 * MOWeatherBlueprintLibrary.h - Weather Blueprint Function Library
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Blueprint function library for constructing weather structs from external
 * weather plugins (Ultra Dynamic Sky, Ultra Dynamic Weather). Provides helper
 * functions to convert UDS/UDW values to MOFramework weather types.
 *
 * FUNCTIONS:
 * - MakeTimeOfDayFromUDS: Create FMOTimeOfDay from UDS values
 * - MakeWeatherStateFromUDW: Create FMOWeatherState from UDW values
 * - MakeWeatherExposureFromUDW: Create FMOWeatherExposure from UDW exposure test
 * - MakeWeatherExposureFromUDWWithShelter: Apply multi-axis shelter modulation
 * - TestExposureShelter: Full 3-axis shelter test (overhead + wind + enclosure)
 * - TestOverheadCover: Cheap overhead-only test (for placed objects)
 *
 * INTEGRATION:
 * These functions are designed to be called from Blueprint event graphs in
 * weather integration actors that bridge UDS/UDW with MOFramework.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] UDS TIME FORMAT: UDS TimeValue is 0-2400 (not 0-24). Function
 *   converts internally to hour/minute.
 *
 * [2024-02] SEASON FLOAT: UDS Season is 0-4 float (0=Winter, 1=Spring, etc).
 *   Fractional values indicate transition between seasons.
 *
 * [2026-05] SHELTER TRACES USE ECC_VISIBILITY. Foliage / canopy meshes that
 *   should count as cover must have collision configured to BLOCK visibility
 *   traces. If a tree silhouette looks solid but TestShelter reports exposed,
 *   check the mesh's CollisionResponseToChannel(Visibility) — likely Ignore.
 *   We can migrate to a dedicated ECC_GameTraceChannelN "WeatherBlock" later
 *   if visibility-channel conflict becomes an issue.
 *
 * [2026-05] UDS MANUAL TIME REQUIRES bAnimateTimeOfDay=false ON THE UDS ACTOR.
 *   Otherwise UDS overwrites the Time of Day variable every tick from its
 *   internal Day Length / Night Length advancement. To drive UDS from our
 *   clock you MUST:
 *     1. Select the Ultra_Dynamic_Sky actor in the level.
 *     2. Uncheck "Animate Time of Day".
 *   Without this, MO.Clock.SetTime / hourly sync / etc. all appear to do
 *   nothing visually because UDS clobbers the write on the next frame.
 *
 * =============================================================================
 * RELATED FILES: MOWeatherTypes.h, MOWeatherIntegrationSubsystem.h
 * LAST UPDATED: 2026-05-24
 * =============================================================================
 */

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
class MOFRAMEWORKCORE_API UMOWeatherBlueprintLibrary : public UBlueprintFunctionLibrary
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
	 * Convert an FDateTime (from MOGameClockSubsystem) into UDS's "Time of Day"
	 * format. UDS uses 0-2400 where the hundreds digit = hour and the lower
	 * two digits = minute scaled to /60. Examples:
	 *   06:00 → 600.0
	 *   13:30 → 1350.0    (30 / 60 * 100 = 50)
	 *   18:45 → 1875.0
	 *   23:59 → 2398.33...
	 *
	 * Drop this between MOGameClockSubsystem::GetGameDateTime and the UDS
	 * actor's "Time of Day" variable Set node in BP_WeatherBridge::SetDateTime.
	 *
	 * Reminder: the UDS actor must have "Animate Time of Day" UNCHECKED
	 * or it will overwrite the value next tick.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather", meta=(DisplayName="Date Time -> UDS Time Of Day"))
	static float DateTimeToUDSTimeValue(const FDateTime& DateTime);

	/**
	 * Extract just the day-of-year (1-366) from a DateTime, for feeding into
	 * UDS's "Date" variable / sun-path calculation.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather", meta=(DisplayName="Date Time -> Day Of Year"))
	static int32 DateTimeToDayOfYear(const FDateTime& DateTime);

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

	/**
	 * Convert a UObject pointer to its FSoftObjectPath (for saving asset
	 * references that survive disk save/load). BP-equivalent built-ins are
	 * inconsistent across UE versions; this helper is the canonical path.
	 *
	 * Returns empty FSoftObjectPath if Object is null.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather",
		meta=(DisplayName="Make Soft Path From Object"))
	static FSoftObjectPath MakeSoftPathFromObject(const UObject* Object);

	/**
	 * Load a UObject from a FSoftObjectPath synchronously. Returns null if
	 * the path is empty/invalid or the asset can't be resolved.
	 *
	 * Sync load — fine for small assets (weather presets are tiny). Don't
	 * use for large meshes/textures during gameplay.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Weather",
		meta=(DisplayName="Load Object From Soft Path (Sync)"))
	static UObject* LoadObjectFromSoftPath(const FSoftObjectPath& Path);

	/**
	 * Build an FMOWeatherExposure from raw UDW values and a multi-axis shelter
	 * test result. Each weather channel is modulated by the AXIS that physically
	 * affects it:
	 *
	 *   Rain  = raw * (1 - OverheadCover)   -- vertical, blocked by roof
	 *   Snow  = raw * (1 - OverheadCover)   -- same as rain
	 *   Wind  = raw * (1 - WindShelter)     -- horizontal, blocked by upwind geom
	 *   Dust  = raw * (1 - WindShelter)     -- wind-borne, same as wind
	 *
	 * bIsSheltered on the output is the "fully sheltered" composite — true
	 * when indoors, or when BOTH overhead AND wind axes are above threshold.
	 *
	 * Enclosure is NOT applied to the four channels here — it's a separate
	 * signal for thermal/insulation systems. Read Shelter.bIsIndoors / .Enclosure
	 * directly in those systems.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather",
		meta=(DisplayName="Make Weather Exposure (from UDW + Shelter)"))
	static FMOWeatherExposure MakeWeatherExposureFromUDWWithShelter(
		float Rain,
		float Snow,
		float Wind,
		float Dust,
		const FMOExposureShelter& Shelter);

	/**
	 * Comprehensive shelter test. Runs three independent trace patterns in one
	 * call and fills all three axes of FMOExposureShelter:
	 *
	 *   1. Overhead — 9 long rays (50m), upper hemisphere at 35° from vertical
	 *   2. Wind cone — 5 medium rays (15m), cone toward upwind direction,
	 *      ±15° spread, distance-weighted (closer = better windbreak)
	 *   3. Walls — 4 short rays (3m), horizontal cardinal directions
	 *
	 * Enclosure is computed from the WALL hits + the count of overhead rays
	 * that hit *within 3m* (treats a close roof as part of enclosure).
	 *
	 * Use for characters (player, AI survivors) — anything that needs full
	 * resolution of "what kind of shelter is this?"
	 *
	 * Cost: 18 line traces per call. Cache and call at 1-2Hz per actor.
	 *
	 * @param WorldContextObject Any object that can resolve the world (self in BP)
	 * @param Location World-space test point (typically actor location or head)
	 * @param WindDirection Unit vector — the direction the wind is BLOWING. The
	 *   function tests the OPPOSITE direction (upwind) for shelter. Pass zero
	 *   vector if wind direction is unknown; WindShelter will stay 0.
	 * @param IgnoreActor Actor to exclude from all traces (pass the tested actor).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Weather|Shelter",
		meta=(WorldContext="WorldContextObject", DisplayName="Test Exposure Shelter (Full)"))
	static FMOExposureShelter TestExposureShelter(
		const UObject* WorldContextObject,
		FVector Location,
		FVector WindDirection,
		AActor* IgnoreActor = nullptr);

	/**
	 * Cheap overhead-only test. Fires the 9 overhead rays only — leaves
	 * WindShelter and Enclosure at zero.
	 *
	 * Use for:
	 *   - Fires (rain extinguish risk only depends on overhead)
	 *   - Plants / crops (rainfall capture)
	 *   - Dropped item rot (whether it's getting rained on)
	 *   - Anything that doesn't care about wind direction or indoors-ness
	 *
	 * Cost: 9 line traces per call (half of TestExposureShelter). Tick at 1Hz
	 *   per object or less.
	 *
	 * @param WorldContextObject Any object that can resolve the world (self in BP)
	 * @param Location World-space test point
	 * @param IgnoreActor Actor to exclude from traces (pass the placed object)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Weather|Shelter",
		meta=(WorldContext="WorldContextObject", DisplayName="Test Overhead Cover (Cheap)"))
	static FMOExposureShelter TestOverheadCover(
		const UObject* WorldContextObject,
		FVector Location,
		AActor* IgnoreActor = nullptr);
};
