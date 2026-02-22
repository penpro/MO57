#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MOWeatherTypes.h"
#include "MOWeatherProviderInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UMOWeatherProviderInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for weather system providers.
 *
 * Implement this interface in a Blueprint or C++ class that bridges to your
 * weather system (e.g., Ultra Dynamic Weather, custom system, etc.).
 *
 * The MOFramework survival/medical systems query this interface for weather
 * data without being coupled to any specific weather implementation.
 *
 * Example Blueprint implementation for Ultra Dynamic Weather:
 * 1. Create a Blueprint Actor implementing this interface
 * 2. Add a reference to your UDW actor
 * 3. Implement each function by calling the corresponding UDW function
 * 4. Place one instance in your level
 * 5. Register it with the Weather Integration Subsystem
 */
class MOFRAMEWORK_API IMOWeatherProviderInterface
{
	GENERATED_BODY()

public:
	// ============================================================================
	// TEMPERATURE
	// ============================================================================

	/**
	 * Get the current temperature at a world location.
	 * @param Location World location to sample (use FVector::ZeroVector for global)
	 * @param Unit Desired temperature unit
	 * @return Temperature in the requested unit
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Weather")
	float GetTemperatureAtLocation(const FVector& Location, EMOTemperatureUnit Unit) const;

	/**
	 * Get the global/ambient temperature (not location-specific).
	 * @param Unit Desired temperature unit
	 * @return Global temperature in the requested unit
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Weather")
	float GetGlobalTemperature(EMOTemperatureUnit Unit) const;

	// ============================================================================
	// WEATHER EXPOSURE
	// ============================================================================

	/**
	 * Get weather exposure values at a specific location.
	 * @param Location World location to sample
	 * @return Exposure values for rain, snow, wind, dust
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Weather")
	FMOWeatherExposure GetWeatherExposureAtLocation(const FVector& Location) const;

	/**
	 * Check if a location is considered sheltered/indoors.
	 * @param Location World location to check
	 * @return True if sheltered from weather
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Weather")
	bool IsLocationSheltered(const FVector& Location) const;

	// ============================================================================
	// WEATHER STATE
	// ============================================================================

	/**
	 * Get the current global weather state.
	 * @return Current weather conditions
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Weather")
	FMOWeatherState GetCurrentWeatherState() const;

	/**
	 * Get human-readable weather description.
	 * @return Display name like "Light Rain", "Blizzard", etc.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Weather")
	FText GetWeatherDisplayName() const;

	// ============================================================================
	// TIME OF DAY
	// ============================================================================

	/**
	 * Get current date and time directly from UDS.
	 * @return FDateTime with full date/time information
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Weather")
	FDateTime GetDateTime() const;

	/**
	 * Check if it's currently daytime.
	 * @return True if sun is up
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Weather")
	bool IsDaytime() const;

	// ============================================================================
	// WIND
	// ============================================================================

	/**
	 * Get wind velocity at a location.
	 * @param Location World location to sample
	 * @return Wind velocity vector (direction and magnitude)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Weather")
	FVector GetWindVelocityAtLocation(const FVector& Location) const;

	// ============================================================================
	// STATE SETTERS (for save/load)
	// ============================================================================

	/**
	 * Set the date and time.
	 * @param DateTime Full date and time to set
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Weather")
	void SetDateTime(const FDateTime& DateTime);

	/**
	 * Set the weather preset by object reference.
	 * @param PresetObject Weather preset object (UDS_Weather_Settings from UDW)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Weather")
	void SetWeatherPreset(UObject* PresetObject);

	/**
	 * Get the current weather preset object (UDS_Weather_Settings from UDW).
	 * @return Weather preset object reference
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Weather")
	UObject* GetCurrentWeatherPreset() const;

	/**
	 * Build save data from current weather/time state.
	 * @return Save data structure
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Weather")
	FMOWeatherSaveData BuildWeatherSaveData() const;

	/**
	 * Apply save data to restore weather/time state.
	 * @param SaveData Previously saved weather state
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Weather")
	void ApplyWeatherSaveData(const FMOWeatherSaveData& SaveData);
};
