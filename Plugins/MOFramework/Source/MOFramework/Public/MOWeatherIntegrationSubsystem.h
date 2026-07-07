/**
 * =============================================================================
 * MOWeatherIntegrationSubsystem.h - Weather Integration World Subsystem
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * World subsystem that integrates weather data with MOFramework systems.
 * Holds reference to active weather provider, provides convenient access
 * to weather data, monitors weather changes and fires delegates.
 *
 * USAGE:
 * 1. Create Blueprint implementing IMOWeatherProviderInterface
 * 2. Call RegisterWeatherProvider() with your provider instance
 * 3. Query weather data via this subsystem's functions
 * 4. Bind to delegates for weather change events
 *
 * MEDICAL INTEGRATION:
 * - GetFeelsLikeTemperature(): Accounts for wind chill and heat index
 * - GetColdStress()/GetHeatStress(): Normalized stress values (0-1)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] SINGLE PROVIDER: Only one weather provider can be active at a time.
 *   RegisterWeatherProvider() replaces any existing provider.
 *
 * [2024-02] DEFAULT VALUES: When no provider is registered, returns
 *   DefaultTemperatureCelsius (20.0) for temperature queries.
 *
 * [2024-02] PENDING SAVE DATA: If ApplyWeatherSaveData() is called before
 *   provider registration, data is queued in PendingSaveData.
 *
 * [2026-06] COOK CRASH — NEVER fetch a game-world-only subsystem (e.g.
 *   UMOGameUIManagerSubsystem) from within Initialize(). That subsystem is
 *   absent during cook/commandlet, and a during-Initialize fetch trips a
 *   UE5.8 ensure (SubsystemCollection.cpp:109) that is FATAL in the cook
 *   commandlet → packaging fails. The UI hook is bound in OnWorldBeginPlay()
 *   instead. (The clock subsystem IS safe to fetch in Initialize — it creates
 *   in every world.)
 *
 * =============================================================================
 * RELATED FILES: MOWeatherProviderInterface.h, MOWeatherTypes.h
 * LAST UPDATED: 2026-06-30
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOWeatherTypes.h"
#include "MOSaveDomainInterface.h"
#include "MOAmbientEnvironmentProvider.h"
#include "MOWeatherIntegrationSubsystem.generated.h"

class IMOWeatherProviderInterface;

// ============================================================================
// DELEGATES
// ============================================================================

/** Fired when weather state changes significantly. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOWeatherChangedSignature, const FMOWeatherState&, NewWeatherState);

/** Fired when it starts or stops raining. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMORainChangedSignature, bool, bIsRaining);

/** Fired when it starts or stops snowing. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOSnowChangedSignature, bool, bIsSnowing);

/** Fired when temperature crosses a threshold. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOTemperatureThresholdSignature, float, CurrentTemperature, float, Threshold, bool, bAboveThreshold);

/** Fired when time of day changes significantly (dawn, dusk, etc.). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOTimeOfDayChangedSignature, bool, bIsDaytime, const FDateTime&, DateTime);

/**
 * World subsystem that integrates weather data with MOFramework systems.
 *
 * This subsystem:
 * - Holds reference to the active weather provider (UDW bridge, etc.)
 * - Provides convenient access to weather data for all systems
 * - Monitors weather changes and fires delegates
 * - Integrates with medical system for temperature effects
 *
 * Usage:
 * 1. Create a Blueprint implementing IMOWeatherProviderInterface
 * 2. Call RegisterWeatherProvider() with your provider instance
 * 3. Query weather data via this subsystem's functions
 * 4. Bind to delegates for weather change events
 */
UCLASS()
class MOFRAMEWORK_API UMOWeatherIntegrationSubsystem : public UTickableWorldSubsystem, public IMOSaveDomain, public IMOAmbientEnvironmentProvider
{
	GENERATED_BODY()

public:
	//~ Begin IMOSaveDomain (Weather save data lives here, not in the persistence subsystem)
	virtual FName GetSaveDomainName() const override { return TEXT("Weather"); }
	virtual int32 GetSaveDomainApplyPriority() const override { return 30; }
	virtual void CaptureSaveDomain(UMOWorldSaveGame& Save) override;
	virtual void ApplySaveDomain(const UMOWorldSaveGame& Save) override;
	//~ End IMOSaveDomain

	//~ Begin IMOAmbientEnvironmentProvider (the medical layer's window on weather)
	virtual float GetAmbientFeelsLikeCelsius(const FVector& Location) const override
	{
		return GetFeelsLikeTemperature(Location, EMOTemperatureUnit::Celsius);
	}
	virtual FMOWeatherExposure GetAmbientExposure(const FVector& Location) const override
	{
		return GetWeatherExposureAtLocation(Location);
	}
	virtual float GetAmbientRainIntensity01() const override
	{
		return GetCurrentWeatherState().RainIntensity;
	}
	//~ End IMOAmbientEnvironmentProvider

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// ============================================================================
	// PROVIDER REGISTRATION
	// ============================================================================

	/**
	 * Register a weather provider (e.g., UDW bridge Blueprint).
	 * Only one provider can be active at a time.
	 * @param Provider Object implementing IMOWeatherProviderInterface
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Weather")
	void RegisterWeatherProvider(TScriptInterface<IMOWeatherProviderInterface> Provider);

	/**
	 * Unregister the current weather provider.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Weather")
	void UnregisterWeatherProvider();

	/**
	 * Check if a weather provider is registered.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather")
	bool HasWeatherProvider() const;

	// ============================================================================
	// TEMPERATURE QUERIES
	// ============================================================================

	/**
	 * Get temperature at a world location.
	 * @param Location World location to sample
	 * @param Unit Temperature unit preference
	 * @return Temperature value, or default if no provider
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather")
	float GetTemperatureAtLocation(const FVector& Location, EMOTemperatureUnit Unit = EMOTemperatureUnit::Celsius) const;

	/**
	 * Get the global ambient temperature.
	 * @param Unit Temperature unit preference
	 * @return Temperature value, or default if no provider
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather")
	float GetGlobalTemperature(EMOTemperatureUnit Unit = EMOTemperatureUnit::Celsius) const;

	// =========================================================================
	// SEASONS (V2.4) — the game clock is the season authority
	// =========================================================================

	/** Season index per FMOTimeOfDay convention: 0=Spring 1=Summer 2=Autumn 3=Winter. */
	UFUNCTION(BlueprintPure, Category="MO|Weather|Season")
	static int32 SeasonIndexFromMonth(int32 Month);

	/** Current season derived from the game clock's date (save-stable). */
	UFUNCTION(BlueprintPure, Category="MO|Weather|Season")
	int32 GetCurrentSeasonIndex() const;

	/**
	 * THE climate baseline — pure math for headless tests. No-provider ambient
	 * temperature: annual sinusoid (peak ~Jul 21) + diurnal sinusoid (peak
	 * 15:00). Replaces the old flat 20C default so winter EXISTS even before
	 * a UDS provider is wired: cold nights drive the vitals->metabolism->food
	 * cascade with no further code.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather|Season")
	static float ComputeSeasonalBaselineCelsius(int32 DayOfYear, float Hour,
		float AnnualMeanC = 11.0f, float AnnualAmplitudeC = 13.0f, float DiurnalAmplitudeC = 5.0f);

	/**
	 * Convert temperature between units.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather")
	static float ConvertTemperature(float Temperature, EMOTemperatureUnit FromUnit, EMOTemperatureUnit ToUnit);

	// ============================================================================
	// WEATHER EXPOSURE QUERIES
	// ============================================================================

	/**
	 * Get weather exposure at a location.
	 * @param Location World location to sample
	 * @return Exposure values for rain, snow, wind, dust
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather")
	FMOWeatherExposure GetWeatherExposureAtLocation(const FVector& Location) const;

	/**
	 * Check if a location is sheltered from weather.
	 * @param Location World location to check
	 * @return True if sheltered
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather")
	bool IsLocationSheltered(const FVector& Location) const;

	// ============================================================================
	// WEATHER STATE QUERIES
	// ============================================================================

	/**
	 * Get current global weather state.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather")
	FMOWeatherState GetCurrentWeatherState() const;

	/**
	 * Get weather display name.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather")
	FText GetWeatherDisplayName() const;

	/**
	 * Check if it's currently raining.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather")
	bool IsRaining() const;

	/**
	 * Check if it's currently snowing.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather")
	bool IsSnowing() const;

	// ============================================================================
	// TIME OF DAY QUERIES
	// ============================================================================

	/**
	 * Get current date and time from UDS.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather")
	FDateTime GetDateTime() const;

	/**
	 * Check if it's daytime.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather")
	bool IsDaytime() const;

	// ============================================================================
	// WIND QUERIES
	// ============================================================================

	/**
	 * Get wind velocity at a location.
	 * @param Location World location to sample
	 * @return Wind velocity vector
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather")
	FVector GetWindVelocityAtLocation(const FVector& Location) const;

	// ============================================================================
	// MEDICAL INTEGRATION HELPERS
	// ============================================================================

	/**
	 * Calculate effective "feels like" temperature accounting for wind chill and heat index.
	 * @param Location World location to sample
	 * @param Unit Temperature unit preference
	 * @return Adjusted temperature value
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather")
	float GetFeelsLikeTemperature(const FVector& Location, EMOTemperatureUnit Unit = EMOTemperatureUnit::Celsius) const;

	/**
	 * Get a normalized cold stress value (0 = comfortable, 1 = severe cold).
	 * Accounts for temperature, wind, wetness.
	 * @param Location World location to sample
	 * @return Cold stress value 0-1
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather")
	float GetColdStress(const FVector& Location) const;

	/**
	 * Get a normalized heat stress value (0 = comfortable, 1 = severe heat).
	 * Accounts for temperature, humidity.
	 * @param Location World location to sample
	 * @return Heat stress value 0-1
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather")
	float GetHeatStress(const FVector& Location) const;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** How often to check for weather changes (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather|Config")
	float WeatherCheckInterval = 1.0f;

	/** Temperature threshold for cold warnings (Celsius). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather|Config")
	float ColdThresholdCelsius = 5.0f;

	/** Temperature threshold for heat warnings (Celsius). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather|Config")
	float HeatThresholdCelsius = 35.0f;

	/** Comfortable temperature range center (Celsius). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather|Config")
	float ComfortableTemperatureCelsius = 20.0f;

	/** Default temperature when no provider is registered (Celsius). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather|Config")
	float DefaultTemperatureCelsius = 20.0f;

	/** Use the seasonal baseline (clock-driven) instead of the flat default
	 *  when no weather provider is registered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather|Season")
	bool bSeasonalFallback = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather|Season")
	float AnnualMeanCelsius = 11.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather|Season")
	float AnnualAmplitudeCelsius = 13.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Weather|Season")
	float DiurnalAmplitudeCelsius = 5.0f;

	// ============================================================================
	// PERSISTENCE
	// ============================================================================

	/**
	 * Build save data from current weather and time state.
	 * @return Weather save data (check bIsValid before using)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Weather")
	FMOWeatherSaveData BuildWeatherSaveData() const;

	/**
	 * Apply save data to restore weather and time state.
	 * @param SaveData Previously saved weather state
	 * @return True if successfully applied
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Weather")
	bool ApplyWeatherSaveData(const FMOWeatherSaveData& SaveData);

	/**
	 * Set date and time directly.
	 * @param DateTime Full date and time to set
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Weather")
	void SetDateTime(const FDateTime& DateTime);

	// ============================================================================
	// UDS SYNC (throttled push from clock → visual sky)
	// ============================================================================
	//
	// Goal: keep UDS visuals in tight lockstep with the gameplay clock so
	// the sun moves smoothly at every TimeScale. Original design assumed
	// UDS would glitch on frequent pushes (hence hourly sync); testing
	// proved UDS handles 4+ Hz pushes cleanly, so we now push aggressively.
	//
	//   1. Throttle by game-time (default ~1 game-minute = 0.25° sun arc).
	//      Constant degrees-per-push regardless of TimeScale → smooth at
	//      any speed.
	//   2. Drive syncs from multiple triggers, all gated by the throttle:
	//      - Hour roll      (clock OnHourChanged)
	//      - Menu activate  (UI sub OnActivatableWidgetRegistered)
	//      - Real-time poll (every UdsSyncPollIntervalSeconds, default 0.25s)
	//
	// The poll is the primary driver at high TimeScale (>=60); the other
	// triggers are noise that the throttle absorbs. At normal TimeScale=1,
	// the poll fires every 0.25 real-sec but the throttle blocks until
	// ~60 real-sec have passed (one game-minute) — minimal CPU cost.

	/**
	 * Request a UDS visual sync. If less than MinHoursBetweenUdsSyncs game-
	 * hours have elapsed since the last sync, this is a no-op. Otherwise,
	 * pushes GameClock's current DateTime to the registered provider.
	 *
	 * Idempotent / cheap to call repeatedly. Anything that wants to "give
	 * UDS a chance to sync" can call this — the throttle handles the rest.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Weather|UDSSync")
	void RequestUdsSync();

	/**
	 * Game hours elapsed since the last successful UDS push. Useful for
	 * debug UI / overlays. Returns a large number if no sync has happened
	 * yet this session.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Weather|UDSSync")
	float GetGameHoursSinceLastUdsSync() const;

	/**
	 * Minimum game-hours between UDS syncs. With the default of ~1 game-minute,
	 * sun motion is smooth at every TimeScale (0.25° arc per push at normal
	 * play, larger but still <1° at accelerated speeds).
	 *
	 * Earlier default was 1.0 (full game-hour between pushes), but that gave
	 * visibly jumpy 15° hour-by-hour sun jerks at high TimeScale. UDS handles
	 * sub-second pushes fine in practice — no documented per-push limit and
	 * community examples push from Tick (~60Hz) without issues — so we can
	 * afford to keep UDS in tight lockstep with the clock.
	 *
	 * Raise this if UDS visibly glitches; lower to 0.0028 (10 game-sec) for
	 * even smoother motion at very high TimeScale.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Weather|UDSSync", meta=(ClampMin="0.001"))
	float MinHoursBetweenUdsSyncs = 0.0167f;  // = 1 game-minute (1/60 of an hour)

	/**
	 * Real-time interval (seconds) for the safety-net poll. Sets the upper
	 * bound on sync responsiveness at high TimeScale: at TimeScale=600, this
	 * poll fires 4×/sec and each call advances UDS by ~2.5° of sun arc.
	 *
	 * 0.25s = 4Hz = essentially free CPU; visually smooth. Raise to 1.0+ if
	 * profiling ever shows UDS pushes are expensive (shouldn't happen).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Weather|UDSSync", meta=(ClampMin="0.01"))
	float UdsSyncPollIntervalSeconds = 0.25f;

	/**
	 * Set weather preset by object reference.
	 * @param PresetObject Weather preset object (UDS_Weather_Settings from UDW)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Weather")
	void SetWeatherPreset(UObject* PresetObject);

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Fired when weather state changes. */
	UPROPERTY(BlueprintAssignable, Category="MO|Weather|Events")
	FMOWeatherChangedSignature OnWeatherChanged;

	/** Fired when rain starts or stops. */
	UPROPERTY(BlueprintAssignable, Category="MO|Weather|Events")
	FMORainChangedSignature OnRainChanged;

	/** Fired when snow starts or stops. */
	UPROPERTY(BlueprintAssignable, Category="MO|Weather|Events")
	FMOSnowChangedSignature OnSnowChanged;

	/** Fired when temperature crosses hot or cold threshold. */
	UPROPERTY(BlueprintAssignable, Category="MO|Weather|Events")
	FMOTemperatureThresholdSignature OnTemperatureThresholdCrossed;

	/** Fired when transitioning between day and night. */
	UPROPERTY(BlueprintAssignable, Category="MO|Weather|Events")
	FMOTimeOfDayChangedSignature OnDayNightChanged;

private:
	/**
	 * Forwards day/night transitions from UMOGameClockSubsystem to this
	 * subsystem's own OnDayNightChanged delegate. Allows existing
	 * consumers (BT services, BP listeners) to keep using
	 * WeatherIntegrationSubsystem's API while the clock is the
	 * authoritative source. Phase 2 of clock centralization.
	 */
	UFUNCTION()
	void HandleClockDayNightChanged(bool bIsDaytime, const FDateTime& CurrentDateTime);

	/**
	 * Forwards clock OnHourChanged to RequestUdsSync. Hour rolls are one of
	 * several triggers that all funnel through the same throttled API; see
	 * the UDS Sync section comment above.
	 */
	UFUNCTION()
	void HandleClockHourChanged(int32 NewHour, const FDateTime& CurrentDateTime);

	/**
	 * Forwards UI subsystem OnActivatableWidgetRegistered to RequestUdsSync.
	 * Menu opens are masked moments — if the throttle's window has elapsed,
	 * sync UDS while the player can't see the sky.
	 */
	void HandleActivatableWidgetRegistered(class UMOActivatableWidget* Widget);

	/** Safety-net real-time poll calling RequestUdsSync. Fires every UdsSyncPollIntervalSeconds. */
	void HandleUdsSyncPollTick();

	/** Internal: actually push DateTime to provider + update LastUdsSyncDateTime. No throttle check. */
	void DoUdsSyncInternal(const FDateTime& Now);

	/** Most recent GameDateTime we successfully pushed to UDS. */
	FDateTime LastUdsSyncDateTime;

	/** Periodic safety-net poll timer (real-time). */
	FTimerHandle UdsSyncPollHandle;

	/** Delegate handle for the UI sub's OnActivatableWidgetRegistered. */
	FDelegateHandle ActivatableWidgetRegisteredHandle;

	/** No-provider ambient: seasonal baseline from the clock, or the flat default. */
	float GetFallbackTemperatureCelsius() const;

	/** The registered weather provider. */
	UPROPERTY()
	TScriptInterface<IMOWeatherProviderInterface> WeatherProvider;

	/** Cached weather state for change detection. */
	FMOWeatherState CachedWeatherState;

	/** Cached time of day for change detection. */
	bool bCachedIsDaytime = true;

	/** Was it raining last check? */
	bool bWasRaining = false;

	/** Was it snowing last check? */
	bool bWasSnowing = false;

	/** Was temperature above heat threshold? */
	bool bWasAboveHeatThreshold = false;

	/** Was temperature below cold threshold? */
	bool bWasBelowColdThreshold = false;

	/** Time since last weather check. */
	float TimeSinceLastCheck = 0.0f;

	/** Pending weather save data to apply when provider registers. */
	FMOWeatherSaveData PendingSaveData;

	/** Whether we have pending save data waiting for a provider. */
	bool bHasPendingSaveData = false;

	/** Check for weather changes and fire delegates. */
	void CheckForWeatherChanges();
};
