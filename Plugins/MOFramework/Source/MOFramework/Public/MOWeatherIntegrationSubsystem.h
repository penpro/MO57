#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOWeatherTypes.h"
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
class MOFRAMEWORK_API UMOWeatherIntegrationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
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
