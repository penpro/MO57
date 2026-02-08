#include "MOWeatherIntegrationSubsystem.h"
#include "MOWeatherProviderInterface.h"
#include "MOFramework.h"

void UMOWeatherIntegrationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogMOFramework, Log, TEXT("[MOWeatherIntegration] Initialized"));
}

void UMOWeatherIntegrationSubsystem::Deinitialize()
{
	WeatherProvider = nullptr;
	Super::Deinitialize();
}

void UMOWeatherIntegrationSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TimeSinceLastCheck += DeltaTime;
	if (TimeSinceLastCheck >= WeatherCheckInterval)
	{
		TimeSinceLastCheck = 0.0f;
		CheckForWeatherChanges();
	}
}

TStatId UMOWeatherIntegrationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMOWeatherIntegrationSubsystem, STATGROUP_Tickables);
}

// ============================================================================
// PROVIDER REGISTRATION
// ============================================================================

void UMOWeatherIntegrationSubsystem::RegisterWeatherProvider(TScriptInterface<IMOWeatherProviderInterface> Provider)
{
	if (!Provider.GetInterface())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWeatherIntegration] Attempted to register null weather provider"));
		return;
	}

	WeatherProvider = Provider;

	// Initialize cached state
	CachedWeatherState = GetCurrentWeatherState();
	bCachedIsDaytime = IsDaytime();
	bWasRaining = CachedWeatherState.IsRaining();
	bWasSnowing = CachedWeatherState.IsSnowing();

	const float CurrentTemp = GetGlobalTemperature(EMOTemperatureUnit::Celsius);
	bWasAboveHeatThreshold = CurrentTemp > HeatThresholdCelsius;
	bWasBelowColdThreshold = CurrentTemp < ColdThresholdCelsius;

	UE_LOG(LogMOFramework, Log, TEXT("[MOWeatherIntegration] Registered weather provider: %s"),
		*GetNameSafe(Provider.GetObject()));
}

void UMOWeatherIntegrationSubsystem::UnregisterWeatherProvider()
{
	if (WeatherProvider.GetInterface())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOWeatherIntegration] Unregistered weather provider"));
	}
	WeatherProvider = nullptr;
}

bool UMOWeatherIntegrationSubsystem::HasWeatherProvider() const
{
	return WeatherProvider.GetInterface() != nullptr;
}

// ============================================================================
// TEMPERATURE QUERIES
// ============================================================================

float UMOWeatherIntegrationSubsystem::GetTemperatureAtLocation(const FVector& Location, EMOTemperatureUnit Unit) const
{
	if (!HasWeatherProvider())
	{
		return ConvertTemperature(DefaultTemperatureCelsius, EMOTemperatureUnit::Celsius, Unit);
	}

	return IMOWeatherProviderInterface::Execute_GetTemperatureAtLocation(
		WeatherProvider.GetObject(), Location, Unit);
}

float UMOWeatherIntegrationSubsystem::GetGlobalTemperature(EMOTemperatureUnit Unit) const
{
	if (!HasWeatherProvider())
	{
		return ConvertTemperature(DefaultTemperatureCelsius, EMOTemperatureUnit::Celsius, Unit);
	}

	return IMOWeatherProviderInterface::Execute_GetGlobalTemperature(
		WeatherProvider.GetObject(), Unit);
}

float UMOWeatherIntegrationSubsystem::ConvertTemperature(float Temperature, EMOTemperatureUnit FromUnit, EMOTemperatureUnit ToUnit)
{
	if (FromUnit == ToUnit)
	{
		return Temperature;
	}

	if (FromUnit == EMOTemperatureUnit::Celsius && ToUnit == EMOTemperatureUnit::Fahrenheit)
	{
		return (Temperature * 9.0f / 5.0f) + 32.0f;
	}
	else // Fahrenheit to Celsius
	{
		return (Temperature - 32.0f) * 5.0f / 9.0f;
	}
}

// ============================================================================
// WEATHER EXPOSURE QUERIES
// ============================================================================

FMOWeatherExposure UMOWeatherIntegrationSubsystem::GetWeatherExposureAtLocation(const FVector& Location) const
{
	if (!HasWeatherProvider())
	{
		return FMOWeatherExposure();
	}

	return IMOWeatherProviderInterface::Execute_GetWeatherExposureAtLocation(
		WeatherProvider.GetObject(), Location);
}

bool UMOWeatherIntegrationSubsystem::IsLocationSheltered(const FVector& Location) const
{
	if (!HasWeatherProvider())
	{
		return false;
	}

	return IMOWeatherProviderInterface::Execute_IsLocationSheltered(
		WeatherProvider.GetObject(), Location);
}

// ============================================================================
// WEATHER STATE QUERIES
// ============================================================================

FMOWeatherState UMOWeatherIntegrationSubsystem::GetCurrentWeatherState() const
{
	if (!HasWeatherProvider())
	{
		return FMOWeatherState();
	}

	return IMOWeatherProviderInterface::Execute_GetCurrentWeatherState(
		WeatherProvider.GetObject());
}

FText UMOWeatherIntegrationSubsystem::GetWeatherDisplayName() const
{
	if (!HasWeatherProvider())
	{
		return NSLOCTEXT("MO", "UnknownWeather", "Unknown");
	}

	return IMOWeatherProviderInterface::Execute_GetWeatherDisplayName(
		WeatherProvider.GetObject());
}

bool UMOWeatherIntegrationSubsystem::IsRaining() const
{
	return GetCurrentWeatherState().IsRaining();
}

bool UMOWeatherIntegrationSubsystem::IsSnowing() const
{
	return GetCurrentWeatherState().IsSnowing();
}

// ============================================================================
// TIME OF DAY QUERIES
// ============================================================================

FMOTimeOfDay UMOWeatherIntegrationSubsystem::GetTimeOfDay() const
{
	if (!HasWeatherProvider())
	{
		return FMOTimeOfDay();
	}

	return IMOWeatherProviderInterface::Execute_GetTimeOfDay(
		WeatherProvider.GetObject());
}

bool UMOWeatherIntegrationSubsystem::IsDaytime() const
{
	if (!HasWeatherProvider())
	{
		return true;
	}

	return IMOWeatherProviderInterface::Execute_IsDaytime(
		WeatherProvider.GetObject());
}

// ============================================================================
// WIND QUERIES
// ============================================================================

FVector UMOWeatherIntegrationSubsystem::GetWindVelocityAtLocation(const FVector& Location) const
{
	if (!HasWeatherProvider())
	{
		return FVector::ZeroVector;
	}

	return IMOWeatherProviderInterface::Execute_GetWindVelocityAtLocation(
		WeatherProvider.GetObject(), Location);
}

// ============================================================================
// MEDICAL INTEGRATION HELPERS
// ============================================================================

float UMOWeatherIntegrationSubsystem::GetFeelsLikeTemperature(const FVector& Location, EMOTemperatureUnit Unit) const
{
	float Temperature = GetTemperatureAtLocation(Location, EMOTemperatureUnit::Celsius);
	const FMOWeatherExposure Exposure = GetWeatherExposureAtLocation(Location);

	// Wind chill calculation (simplified)
	// Real wind chill only applies below 10°C with wind
	if (Temperature < 10.0f && Exposure.Wind > 0.1f)
	{
		// Approximate wind chill: each 10% wind exposure drops feels-like by ~2°C
		const float WindChillReduction = Exposure.Wind * 20.0f;
		Temperature -= WindChillReduction;
	}

	// Wet/rain makes cold feel colder
	if (Temperature < 20.0f && Exposure.Rain > 0.1f)
	{
		// Being wet in cold weather drops feels-like by up to 5°C
		const float WetColdReduction = Exposure.Rain * 5.0f;
		Temperature -= WetColdReduction;
	}

	// Heat index (simplified) - humidity makes hot feel hotter
	// We don't have humidity directly, but rain indicates high humidity
	if (Temperature > 25.0f)
	{
		// Cloud coverage and rain indicate humidity
		const FMOWeatherState Weather = GetCurrentWeatherState();
		const float Humidity = FMath::Max(Weather.CloudCoverage, Weather.RainIntensity);
		// Each 10% humidity above 25°C adds ~1°C feels-like
		const float HeatIndexAddition = Humidity * 10.0f * ((Temperature - 25.0f) / 15.0f);
		Temperature += HeatIndexAddition;
	}

	return ConvertTemperature(Temperature, EMOTemperatureUnit::Celsius, Unit);
}

float UMOWeatherIntegrationSubsystem::GetColdStress(const FVector& Location) const
{
	const float FeelsLike = GetFeelsLikeTemperature(Location, EMOTemperatureUnit::Celsius);

	// Cold stress scale:
	// 20°C+ = 0 (comfortable)
	// 0°C = 0.5 (moderate cold)
	// -20°C = 1.0 (severe cold)
	if (FeelsLike >= ComfortableTemperatureCelsius)
	{
		return 0.0f;
	}

	// Linear interpolation from comfortable to severe cold
	const float ColdRange = ComfortableTemperatureCelsius - (-20.0f); // 40 degrees range
	const float ColdAmount = ComfortableTemperatureCelsius - FeelsLike;

	return FMath::Clamp(ColdAmount / ColdRange, 0.0f, 1.0f);
}

float UMOWeatherIntegrationSubsystem::GetHeatStress(const FVector& Location) const
{
	const float FeelsLike = GetFeelsLikeTemperature(Location, EMOTemperatureUnit::Celsius);

	// Heat stress scale:
	// 25°C = 0 (comfortable)
	// 35°C = 0.5 (moderate heat)
	// 45°C+ = 1.0 (severe heat)
	const float HeatComfort = 25.0f;
	if (FeelsLike <= HeatComfort)
	{
		return 0.0f;
	}

	// Linear interpolation from comfortable to severe heat
	const float HeatRange = 45.0f - HeatComfort; // 20 degrees range
	const float HeatAmount = FeelsLike - HeatComfort;

	return FMath::Clamp(HeatAmount / HeatRange, 0.0f, 1.0f);
}

// ============================================================================
// WEATHER CHANGE DETECTION
// ============================================================================

void UMOWeatherIntegrationSubsystem::CheckForWeatherChanges()
{
	if (!HasWeatherProvider())
	{
		return;
	}

	const FMOWeatherState NewWeatherState = GetCurrentWeatherState();
	const bool bIsDaytime = IsDaytime();
	const float CurrentTemp = GetGlobalTemperature(EMOTemperatureUnit::Celsius);

	// Check rain state change
	const bool bIsRaining = NewWeatherState.IsRaining();
	if (bIsRaining != bWasRaining)
	{
		bWasRaining = bIsRaining;
		OnRainChanged.Broadcast(bIsRaining);
		UE_LOG(LogMOFramework, Log, TEXT("[MOWeatherIntegration] Rain %s"),
			bIsRaining ? TEXT("started") : TEXT("stopped"));
	}

	// Check snow state change
	const bool bIsSnowing = NewWeatherState.IsSnowing();
	if (bIsSnowing != bWasSnowing)
	{
		bWasSnowing = bIsSnowing;
		OnSnowChanged.Broadcast(bIsSnowing);
		UE_LOG(LogMOFramework, Log, TEXT("[MOWeatherIntegration] Snow %s"),
			bIsSnowing ? TEXT("started") : TEXT("stopped"));
	}

	// Check day/night change
	if (bIsDaytime != bCachedIsDaytime)
	{
		bCachedIsDaytime = bIsDaytime;
		OnDayNightChanged.Broadcast(bIsDaytime, GetTimeOfDay());
		UE_LOG(LogMOFramework, Log, TEXT("[MOWeatherIntegration] Transitioned to %s"),
			bIsDaytime ? TEXT("day") : TEXT("night"));
	}

	// Check temperature threshold crossings
	const bool bAboveHeat = CurrentTemp > HeatThresholdCelsius;
	const bool bBelowCold = CurrentTemp < ColdThresholdCelsius;

	if (bAboveHeat != bWasAboveHeatThreshold)
	{
		bWasAboveHeatThreshold = bAboveHeat;
		OnTemperatureThresholdCrossed.Broadcast(CurrentTemp, HeatThresholdCelsius, bAboveHeat);
		UE_LOG(LogMOFramework, Log, TEXT("[MOWeatherIntegration] Temperature %s heat threshold (%.1f°C)"),
			bAboveHeat ? TEXT("exceeded") : TEXT("dropped below"), HeatThresholdCelsius);
	}

	if (bBelowCold != bWasBelowColdThreshold)
	{
		bWasBelowColdThreshold = bBelowCold;
		OnTemperatureThresholdCrossed.Broadcast(CurrentTemp, ColdThresholdCelsius, !bBelowCold);
		UE_LOG(LogMOFramework, Log, TEXT("[MOWeatherIntegration] Temperature %s cold threshold (%.1f°C)"),
			bBelowCold ? TEXT("dropped below") : TEXT("rose above"), ColdThresholdCelsius);
	}

	// Check for general weather change (display name changed)
	if (!NewWeatherState.DisplayName.EqualTo(CachedWeatherState.DisplayName))
	{
		CachedWeatherState = NewWeatherState;
		OnWeatherChanged.Broadcast(NewWeatherState);
		UE_LOG(LogMOFramework, Log, TEXT("[MOWeatherIntegration] Weather changed to: %s"),
			*NewWeatherState.DisplayName.ToString());
	}
}
