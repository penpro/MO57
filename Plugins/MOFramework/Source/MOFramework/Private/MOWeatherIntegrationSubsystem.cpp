#include "MOWeatherIntegrationSubsystem.h"
#include "MOTickWatchdog.h"
#include "MOPersistenceSubsystem.h"
#include "MOworldSaveGame.h"
#include "MOWeatherProviderInterface.h"
#include "MOGameClockSubsystem.h"
#include "MOGameUIManagerSubsystem.h"
#include "MOActivatableWidget.h"
#include "MOFramework.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "MOGameState.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"

void UMOWeatherIntegrationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// NOTE: the clock binding moved to OnWorldBeginPlay (codex review,
	// 2026-07-07) — same rationale as the UI hook below: never fetch a
	// sibling subsystem from Initialize (UE5.8 cook/commandlet trap), even
	// one that currently exists in every world. One rule, no exceptions.

	// NOTE: the UI hook (OnActivatableWidgetRegistered) is bound in
	// OnWorldBeginPlay(), NOT here. UMOGameUIManagerSubsystem only creates for
	// real game worlds, so it is absent during cook/commandlet (and editor
	// preview). Fetching an absent sibling subsystem from within Initialize()
	// trips a UE5.8 ensure (SubsystemCollection.cpp:109) that is *fatal in the
	// cook commandlet* — it was the cause of packaging failing. Deferring the
	// bind to OnWorldBeginPlay (gameplay-only, after every subsystem exists)
	// keeps the cook path from ever touching the UI subsystem.

	// Safety-net poll: fires every UdsSyncPollIntervalSeconds real-time.
	// Most ticks throttle blocks (less than MinHoursBetweenUdsSyncs game-
	// hours since last sync). When the window has elapsed AND no event has
	// fired, this catches up.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			UdsSyncPollHandle,
			FTimerDelegate::CreateUObject(this, &UMOWeatherIntegrationSubsystem::HandleUdsSyncPollTick),
			FMath::Max(0.1f, UdsSyncPollIntervalSeconds),
			/*bLoop*/ true);
	}

	UE_LOG(LogMOFramework, Log,
		TEXT("[MOWeatherIntegration] Initialized — UDS sync throttle=%.2fh, poll=%.1fs, bound to clock + UI events"),
		MinHoursBetweenUdsSyncs, UdsSyncPollIntervalSeconds);
}

void UMOWeatherIntegrationSubsystem::Deinitialize()
{
	if (UMOAmbientEnvironmentRegistry* Ambient = UMOAmbientEnvironmentRegistry::Get(this))
	{
		Ambient->UnregisterProvider(this);
	}
	if (UWorld* W = GetWorld())
	{
		if (UGameInstance* GI = W->GetGameInstance())
		{
			if (UMOPersistenceSubsystem* Persist = GI->GetSubsystem<UMOPersistenceSubsystem>())
			{
				Persist->UnregisterSaveDomain(this);
			}
		}
	}
	// Unhook from the clock so the dynamic delegate doesn't leak across
	// subsystem teardown ordering surprises.
	if (UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this))
	{
		Clock->OnDayNightChanged.RemoveDynamic(this, &UMOWeatherIntegrationSubsystem::HandleClockDayNightChanged);
		Clock->OnHourChanged.RemoveDynamic(this, &UMOWeatherIntegrationSubsystem::HandleClockHourChanged);
	}

	if (UMOGameUIManagerSubsystem* UISub = UMOGameUIManagerSubsystem::Get(this))
	{
		UISub->OnActivatableWidgetRegistered.Remove(ActivatableWidgetRegisteredHandle);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UdsSyncPollHandle);
	}

	WeatherProvider = nullptr;
	Super::Deinitialize();
}

void UMOWeatherIntegrationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Clock forwarding (moved from Initialize — cook-trap rule, no exceptions).
	if (UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this))
	{
		Clock->OnDayNightChanged.RemoveDynamic(this, &UMOWeatherIntegrationSubsystem::HandleClockDayNightChanged);
		Clock->OnDayNightChanged.AddDynamic(this, &UMOWeatherIntegrationSubsystem::HandleClockDayNightChanged);
		Clock->OnHourChanged.RemoveDynamic(this, &UMOWeatherIntegrationSubsystem::HandleClockHourChanged);
		Clock->OnHourChanged.AddDynamic(this, &UMOWeatherIntegrationSubsystem::HandleClockHourChanged);
		bCachedIsDaytime = Clock->IsDaytime();
	}

	// Ambient-environment provider for the medical layer (C1 registry in Core).
	if (UMOAmbientEnvironmentRegistry* Ambient = UMOAmbientEnvironmentRegistry::Get(&InWorld))
	{
		Ambient->RegisterProvider(this);
		UE_LOG(LogMOFramework, Log, TEXT("[MOWeather] Ambient provider registered for %s"), *InWorld.GetName());
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWeather] AMBIENT SEAM: registry MISSING for %s"), *InWorld.GetName());
	}

	if (UGameInstance* GI = InWorld.GetGameInstance())
	{
		if (UMOPersistenceSubsystem* Persist = GI->GetSubsystem<UMOPersistenceSubsystem>())
		{
			Persist->RegisterSaveDomain(this);
		}
	}

	// Bind the UI hook here, not in Initialize(): this runs only when a world
	// begins play (PIE / packaged game), by which point UMOGameUIManagerSubsystem
	// exists and can be fetched without the during-Initialize ensure. It never
	// runs during cook, so packaging stays clean.
	if (UMOGameUIManagerSubsystem* UISub = UMOGameUIManagerSubsystem::Get(this))
	{
		UISub->OnActivatableWidgetRegistered.RemoveAll(this);
		ActivatableWidgetRegisteredHandle = UISub->OnActivatableWidgetRegistered.AddUObject(
			this, &UMOWeatherIntegrationSubsystem::HandleActivatableWidgetRegistered);
	}
}

void UMOWeatherIntegrationSubsystem::HandleClockDayNightChanged(bool bIsDaytime, const FDateTime& CurrentDateTime)
{
	bCachedIsDaytime = bIsDaytime;
	// Re-broadcast on our own delegate for back-compat consumers (BTs,
	// existing BP listeners, etc). Eventually those should point directly
	// at UMOGameClockSubsystem::OnDayNightChanged and this can be removed.
	OnDayNightChanged.Broadcast(bIsDaytime, CurrentDateTime);
	UE_LOG(LogMOFramework, Log, TEXT("[MOWeatherIntegration] Day/Night forwarded from clock -> %s"),
		bIsDaytime ? TEXT("DAY") : TEXT("NIGHT"));
}

void UMOWeatherIntegrationSubsystem::HandleClockHourChanged(int32 NewHour, const FDateTime& CurrentDateTime)
{
	// Hour roll is just one of several triggers for UDS sync. Funnel
	// through the throttled API so we get consistent gating.
	RequestUdsSync();
}

void UMOWeatherIntegrationSubsystem::HandleActivatableWidgetRegistered(UMOActivatableWidget* /*Widget*/)
{
	// Menu opens are great masking moments for the sync glitch — player is
	// looking at UI, not the sky. If enough game time has passed, do it now.
	RequestUdsSync();
}

void UMOWeatherIntegrationSubsystem::HandleUdsSyncPollTick()
{
	// Safety net: catches the case where neither hour rolls nor menus open
	// for an extended period (unlikely in normal play, defensive otherwise).
	RequestUdsSync();
}

void UMOWeatherIntegrationSubsystem::RequestUdsSync()
{
	if (!HasWeatherProvider() || bSkyPreviewActive)
	{
		return;
	}

	const UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this);
	if (!Clock)
	{
		return;
	}

	const FDateTime Now = Clock->GetGameDateTime();
	const FTimespan Elapsed = Now - LastUdsSyncDateTime;
	const float ElapsedHours = static_cast<float>(Elapsed.GetTotalHours());

	// Throttle: bail if the window hasn't elapsed. Multiple requests within
	// the window are silently dropped — that's the whole point. The throttle
	// is what protects UDS from being spammed.
	if (ElapsedHours < MinHoursBetweenUdsSyncs)
	{
		return;
	}

	DoUdsSyncInternal(Now);
}

float UMOWeatherIntegrationSubsystem::GetGameHoursSinceLastUdsSync() const
{
	const UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this);
	if (!Clock) return 0.0f;
	const FTimespan Elapsed = Clock->GetGameDateTime() - LastUdsSyncDateTime;
	return static_cast<float>(Elapsed.GetTotalHours());
}

void UMOWeatherIntegrationSubsystem::DoUdsSyncInternal(const FDateTime& Now)
{
	IMOWeatherProviderInterface::Execute_SetDateTime(WeatherProvider.GetObject(), Now);
	LastUdsSyncDateTime = Now;
	UE_LOG(LogMOFramework, Log,
		TEXT("[MOWeatherIntegration] UDS sync -> %s (next eligible in %.1fh)"),
		*Now.ToString(), MinHoursBetweenUdsSyncs);
}

void UMOWeatherIntegrationSubsystem::PreviewSkyAt(const FDateTime& DateTime)
{
	if (!HasWeatherProvider())
	{
		return;
	}
	bSkyPreviewActive = true;
	IMOWeatherProviderInterface::Execute_SetDateTime(WeatherProvider.GetObject(), DateTime);   // LastUdsSyncDateTime untouched: the clock never moved
}

void UMOWeatherIntegrationSubsystem::EndSkyPreview()
{
	if (!bSkyPreviewActive)
	{
		return;
	}
	bSkyPreviewActive = false;
	const UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this);
	if (Clock && HasWeatherProvider())
	{
		DoUdsSyncInternal(Clock->GetGameDateTime());   // now, not after the throttle window: the previewed sky must not linger
	}
}

namespace
{
	/** A number variable on a Blueprint object by its exact name (UDS names have spaces), whatever its numeric type. */
	bool ReadSkyNumber(const UObject* Object, const TCHAR* Name, double& Out)
	{
		const FNumericProperty* Property = CastField<FNumericProperty>(Object->GetClass()->FindPropertyByName(FName(Name)));
		if (!Property)
		{
			return false;
		}
		const void* Value = Property->ContainerPtrToValuePtr<void>(Object);
		Out = Property->IsFloatingPoint() ? Property->GetFloatingPointPropertyValue(Value) : static_cast<double>(Property->GetSignedIntPropertyValue(Value));
		return true;
	}

	/** UDS keeps times of day as hundredths of an hour (650 = 06:30). */
	float SkyTimeToHours(double Value)
	{
		return static_cast<float>(Value > 24.0 ? Value / 100.0 : Value);
	}
}

bool UMOWeatherIntegrationSubsystem::GetSunriseSunsetHours(float& OutSunriseHour, float& OutSunsetHour) const
{
	OutSunriseHour = 6.0f;
	OutSunsetHour = 18.0f;
	AActor* Sky = CachedSkyActor.Get();
	if (!Sky && GetWorld())
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (It->GetClass()->GetName().Contains(TEXT("Ultra_Dynamic_Sky")))
			{
				Sky = *It;
				CachedSkyActor = Sky;
				break;
			}
		}
	}
	if (!Sky)
	{
		return false;
	}
	const FBoolProperty* RealSun = CastField<FBoolProperty>(Sky->GetClass()->FindPropertyByName(FName(TEXT("Simulate Real Sun"))));
	const bool bRealSun = RealSun && RealSun->GetPropertyValue_InContainer(Sky);
	double Rise = 0.0, Set = 0.0;
	const bool bRead = bRealSun
		? (ReadSkyNumber(Sky, TEXT("Simulated Sunrise Time"), Rise) && ReadSkyNumber(Sky, TEXT("Simulated Sunset Time"), Set))
		: (ReadSkyNumber(Sky, TEXT("Dawn Time"), Rise) && ReadSkyNumber(Sky, TEXT("Dusk Time"), Set));
	const float RiseHours = SkyTimeToHours(Rise), SetHours = SkyTimeToHours(Set);
	if (!bRead || RiseHours <= 0.0f || SetHours <= RiseHours || SetHours >= 24.0f)
	{
		return false;
	}
	OutSunriseHour = RiseHours;
	OutSunsetHour = SetHours;
	return true;
}

int32 UMOWeatherIntegrationSubsystem::GetGoldenHourMinutesOfDay(bool bEvening) const
{
	float Sunrise = 6.0f, Sunset = 18.0f;
	GetSunriseSunsetHours(Sunrise, Sunset);
	const int32 Minutes = bEvening ? FMath::RoundToInt(Sunset * 60.0f) - GoldenHourOffsetMinutes : FMath::RoundToInt(Sunrise * 60.0f) + GoldenHourOffsetMinutes;
	return FMath::Clamp(FMath::RoundToInt(Minutes / 5.0f) * 5, 0, 24 * 60 - 1);
}

void UMOWeatherIntegrationSubsystem::Tick(float DeltaTime)
{
	MO_TICK_WATCH(TEXT("UMOWeatherIntegrationSubsystem"));
	Super::Tick(DeltaTime);

	TimeSinceLastCheck += DeltaTime;
	if (TimeSinceLastCheck >= WeatherCheckInterval)
	{
		TimeSinceLastCheck = 0.0f;
		CheckForWeatherChanges();
		UpdateGroundWeather();
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
	UObject* ProviderObject = Provider.GetObject();
	if (!ProviderObject)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWeatherIntegration] Attempted to register null weather provider (object is null)"));
		return;
	}

	// Use ImplementsInterface() for Blueprint-implemented interfaces
	// TScriptInterface::GetInterface() only works for native C++ implementations
	if (!ProviderObject->GetClass()->ImplementsInterface(UMOWeatherProviderInterface::StaticClass()))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWeatherIntegration] Provider object '%s' does not implement IMOWeatherProviderInterface."),
			*GetNameSafe(ProviderObject));
		return;
	}

	WeatherProvider = Provider;

	UE_LOG(LogMOFramework, Warning, TEXT("[MOWeatherIntegration] Registered weather provider: %s (pending save data: %s)"),
		*GetNameSafe(Provider.GetObject()),
		bHasPendingSaveData ? TEXT("YES — will apply now") : TEXT("no"));

	// Apply any pending save data that was waiting for a provider
	if (bHasPendingSaveData && PendingSaveData.bIsValid)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOWeatherIntegration] Applying queued save data on provider registration: DateTime=%s Cloud=%.3f Fog=%.3f Preset=%s"),
			*PendingSaveData.DateTime.ToString(),
			PendingSaveData.CloudCoverage,
			PendingSaveData.FogDensity,
			*GetNameSafe(PendingSaveData.WeatherPresetObject.Get()));

		IMOWeatherProviderInterface::Execute_ApplyWeatherSaveData(WeatherProvider.GetObject(), PendingSaveData);

		bHasPendingSaveData = false;
		PendingSaveData = FMOWeatherSaveData(); // Clear
	}

	// A new game asked for clear skies before this provider existed.
	TryApplyPendingClearSkies();

	// Initialize cached state
	CachedWeatherState = GetCurrentWeatherState();
	bCachedIsDaytime = IsDaytime();
	bWasRaining = CachedWeatherState.IsRaining();
	bWasSnowing = CachedWeatherState.IsSnowing();

	const float CurrentTemp = GetGlobalTemperature(EMOTemperatureUnit::Celsius);
	bWasAboveHeatThreshold = CurrentTemp > HeatThresholdCelsius;
	bWasBelowColdThreshold = CurrentTemp < ColdThresholdCelsius;
}

void UMOWeatherIntegrationSubsystem::UnregisterWeatherProvider()
{
	if (WeatherProvider.GetObject())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOWeatherIntegration] Unregistered weather provider"));
	}
	WeatherProvider = nullptr;
}

bool UMOWeatherIntegrationSubsystem::HasWeatherProvider() const
{
	// Check object exists - validation was done during registration
	return WeatherProvider.GetObject() != nullptr;
}

// ============================================================================
// TEMPERATURE QUERIES
// ============================================================================

float UMOWeatherIntegrationSubsystem::GetTemperatureAtLocation(const FVector& Location, EMOTemperatureUnit Unit) const
{
	if (!HasWeatherProvider())
	{
		return ConvertTemperature(GetFallbackTemperatureCelsius(), EMOTemperatureUnit::Celsius, Unit);
	}

	return IMOWeatherProviderInterface::Execute_GetTemperatureAtLocation(
		WeatherProvider.GetObject(), Location, Unit);
}

int32 UMOWeatherIntegrationSubsystem::SeasonIndexFromMonth(int32 Month)
{
	// FMOTimeOfDay convention: 0=Spring 1=Summer 2=Autumn 3=Winter.
	switch (Month)
	{
	case 3: case 4: case 5:   return 0;
	case 6: case 7: case 8:   return 1;
	case 9: case 10: case 11: return 2;
	default:                  return 3;   // Dec/Jan/Feb (and bad input)
	}
}

int32 UMOWeatherIntegrationSubsystem::GetCurrentSeasonIndex() const
{
	const UWorld* World = GetWorld();
	const UMOGameClockSubsystem* Clock = World ? World->GetSubsystem<UMOGameClockSubsystem>() : nullptr;
	return Clock ? SeasonIndexFromMonth(Clock->GetGameDateTime().GetMonth()) : 1;
}

float UMOWeatherIntegrationSubsystem::ComputeSeasonalBaselineCelsius(int32 DayOfYear, float Hour,
	float AnnualMeanC, float AnnualAmplitudeC, float DiurnalAmplitudeC)
{
	// Annual wave peaks ~Jul 21 (day 202); diurnal wave peaks 15:00 (thermal
	// lag past solar noon). Both plain cosines — no randomness, save-stable.
	const float AnnualPhase = 2.0f * PI * (static_cast<float>(DayOfYear) - 202.0f) / 365.0f;
	const float DiurnalPhase = 2.0f * PI * (Hour - 15.0f) / 24.0f;
	return AnnualMeanC
		+ AnnualAmplitudeC * FMath::Cos(AnnualPhase)
		+ DiurnalAmplitudeC * FMath::Cos(DiurnalPhase);
}

float UMOWeatherIntegrationSubsystem::GetFallbackTemperatureCelsius() const
{
	const UWorld* World = GetWorld();
	const UMOGameClockSubsystem* Clock = World ? World->GetSubsystem<UMOGameClockSubsystem>() : nullptr;
	if (!bSeasonalFallback || !Clock)
	{
		return DefaultTemperatureCelsius;
	}
	const FDateTime Now = Clock->GetGameDateTime();
	const float Hour = static_cast<float>(Now.GetHour()) + static_cast<float>(Now.GetMinute()) / 60.0f;
	return ComputeSeasonalBaselineCelsius(Now.GetDayOfYear(), Hour,
		AnnualMeanCelsius, AnnualAmplitudeCelsius, DiurnalAmplitudeCelsius);
}

float UMOWeatherIntegrationSubsystem::GetGlobalTemperature(EMOTemperatureUnit Unit) const
{
	if (!HasWeatherProvider())
	{
		return ConvertTemperature(GetFallbackTemperatureCelsius(), EMOTemperatureUnit::Celsius, Unit);
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

	// The bridge feeds MakeWeatherExposureFromUDWWithShelter UDW's 0-10 numbers too (measured Oct 19 2026 under "Rain" in the open: rain 7.00, wind 2.79 --
	// a feels-like of -23.9 C at 11.1 C, hidden until N-175 stopped the shelter traces from reading every pawn as indoors). FMOWeatherExposure is 0-1.
	FMOWeatherExposure Exposure = IMOWeatherProviderInterface::Execute_GetWeatherExposureAtLocation(WeatherProvider.GetObject(), Location);
	const auto Scale = [this](float V) { return FMath::Clamp(V * ProviderIntensityScale, 0.0f, 1.0f); };
	Exposure.Rain = Scale(Exposure.Rain);
	Exposure.Snow = Scale(Exposure.Snow);
	Exposure.Wind = Scale(Exposure.Wind);
	Exposure.Dust = Scale(Exposure.Dust);
	return Exposure;
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

	// The bridge hands Ultra Dynamic Weather's 0-10 numbers; FMOWeatherState means 0-1 (ProviderIntensityScale).
	FMOWeatherState State = IMOWeatherProviderInterface::Execute_GetCurrentWeatherState(WeatherProvider.GetObject());
	const auto Scale = [this](float V) { return FMath::Clamp(V * ProviderIntensityScale, 0.0f, 1.0f); };
	State.CloudCoverage = Scale(State.CloudCoverage);
	State.Fog = Scale(State.Fog);
	State.RainIntensity = Scale(State.RainIntensity);
	State.SnowIntensity = Scale(State.SnowIntensity);
	State.WindIntensity = Scale(State.WindIntensity);
	State.ThunderIntensity = Scale(State.ThunderIntensity);
	return State;
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

FDateTime UMOWeatherIntegrationSubsystem::GetDateTime() const
{
	// Phase 2 of clock centralization: in-game DateTime lives on the clock.
	// This subsystem became a forwarder so existing callers (BTs, BP, etc)
	// keep working. New code should call UMOGameClockSubsystem::GetGameDateTime
	// directly. If the clock is unavailable for any reason (subsystem init
	// race, etc), fall back to the weather provider, then to FDateTime::Now.
	if (const UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this))
	{
		return Clock->GetGameDateTime();
	}
	if (HasWeatherProvider())
	{
		return IMOWeatherProviderInterface::Execute_GetDateTime(WeatherProvider.GetObject());
	}
	return FDateTime::Now();
}

FDateTime UMOWeatherIntegrationSubsystem::GetSkyDateTime() const
{
	return HasWeatherProvider() ? IMOWeatherProviderInterface::Execute_GetDateTime(WeatherProvider.GetObject()) : FDateTime();
}

bool UMOWeatherIntegrationSubsystem::IsDaytime() const
{
	// Phase 2: clock is authoritative for day/night. Forwarder for compat.
	if (const UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this))
	{
		return Clock->IsDaytime();
	}
	if (HasWeatherProvider())
	{
		return IMOWeatherProviderInterface::Execute_IsDaytime(WeatherProvider.GetObject());
	}
	return true;
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
	return ConvertTemperature(GetFeelsLikeBreakdown(Location).FeelsLikeC, EMOTemperatureUnit::Celsius, Unit);
}

FMOFeelsLike UMOWeatherIntegrationSubsystem::GetFeelsLikeBreakdown(const FVector& Location) const
{
	// Base on GLOBAL temperature, not the provider's location query. UDS's
	// location temp blends toward interior warmth under occlusion — tree
	// canopy reads as "indoors" and a -10C winter night comes back as +17C,
	// so nobody outdoors ever feels winter (V2.4 probe, July 2026). Local
	// microclimate is OUR domain: wind/wet exposure below, and roof shelter
	// via the vitals insulation bonus (CachedOverheadCover).
	FMOFeelsLike Out;
	Out.AirC = GetGlobalTemperature(EMOTemperatureUnit::Celsius);
	Out.Exposure = GetWeatherExposureAtLocation(Location);
	float Temperature = Out.AirC;

	// Wind chill calculation (simplified)
	// Real wind chill only applies below 10°C with wind
	if (Temperature < 10.0f && Out.Exposure.Wind > 0.1f)
	{
		// Approximate wind chill: each 10% wind exposure drops feels-like by ~2°C
		Out.WindChillC = -Out.Exposure.Wind * 20.0f;
		Temperature += Out.WindChillC;
	}

	// Wet/rain makes cold feel colder
	if (Temperature < 20.0f && Out.Exposure.Rain > 0.1f)
	{
		// Being wet in cold weather drops feels-like by up to 5°C
		Out.WetColdC = -Out.Exposure.Rain * 5.0f;
		Temperature += Out.WetColdC;
	}

	// Heat index (simplified) - humidity makes hot feel hotter
	// We don't have humidity directly, but rain indicates high humidity
	if (Temperature > 25.0f)
	{
		// Cloud coverage and rain indicate humidity
		const FMOWeatherState Weather = GetCurrentWeatherState();
		const float Humidity = FMath::Max(Weather.CloudCoverage, Weather.RainIntensity);
		// Each 10% humidity above 25°C adds ~1°C feels-like
		Out.HeatIndexC = Humidity * 10.0f * ((Temperature - 25.0f) / 15.0f);
		Temperature += Out.HeatIndexC;
	}

	Out.FeelsLikeC = Temperature;
	return Out;
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
	const float CurrentTemp = GetGlobalTemperature(EMOTemperatureUnit::Celsius);
	// NOTE: bIsDaytime detection moved to UMOGameClockSubsystem. We listen
	// to its OnDayNightChanged delegate in HandleClockDayNightChanged and
	// re-broadcast — no per-tick polling needed here anymore.

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

	// Day/night transitions are owned by UMOGameClockSubsystem now;
	// HandleClockDayNightChanged forwards them to OnDayNightChanged.

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

// ============================================================================
// GROUND WEATHER (BR18-16)
// ============================================================================

void UMOWeatherIntegrationSubsystem::SetGroundWeather(const FMOGroundWeatherState& NewState)
{
	GroundWeather.Wetness = FMath::Clamp(NewState.Wetness, 0.0f, 1.0f);
	GroundWeather.SnowCover = FMath::Clamp(NewState.SnowCover, 0.0f, 1.0f);
	UWorld* World = GetWorld();
	if (AMOGameState* GameState = World ? World->GetGameState<AMOGameState>() : nullptr)
	{
		if (GameState->HasAuthority())
		{
			GameState->SetGroundWeather(GroundWeather);
		}
	}
}

void UMOWeatherIntegrationSubsystem::UpdateGroundWeather()
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	AMOGameState* GameState = World->GetGameState<AMOGameState>();
	if (World->GetNetMode() == NM_Client)
	{
		// The host's ground (a client's own weather would drift: its sky follows the host's preset, not its history).
		if (GameState)
		{
			GroundWeather = GameState->GetGroundWeather();
		}
	}
	else
	{
		const UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this);
		if (Clock && HasWeatherProvider())
		{
			const FDateTime Now = Clock->GetGameDateTime();
			if (LastGroundAdvance.GetTicks() > 0 && Now > LastGroundAdvance)
			{
				// Game time, not frame time: a time skip or the offline catch-up dries / wets the ground over the skipped hours (the weather now held).
				const FMOWeatherState Weather = GetCurrentWeatherState();
				FMOGroundWeatherInputs In;
				In.Rain01 = Weather.RainIntensity;
				In.Snow01 = Weather.SnowIntensity;
				In.Cloud01 = Weather.CloudCoverage;
				In.TemperatureC = GetGlobalTemperature(EMOTemperatureUnit::Celsius);
				In.bSunUp = IsDaytime();
				// Two game days at most: a longer gap is a calendar jump (a mismatched saved sky date), not weather the ground lived through.
				const double Seconds = FMath::Min((Now - LastGroundAdvance).GetTotalSeconds(), 48.0 * 3600.0);
				FMOGroundWeather::Advance(GroundWeather, In, Seconds, GroundRates);
			}
			LastGroundAdvance = Now;
		}
		if (GameState)
		{
			GameState->SetGroundWeather(GroundWeather);
		}
	}

	// Every machine draws its own terrain: the collection is local.
	if (!bGroundWrittenToMaterial || GroundWeather != GroundWrittenToMaterial)
	{
		if (!LoadedGroundCollection && !bGroundCollectionMissing)
		{
			LoadedGroundCollection = GroundWeatherCollection.LoadSynchronous();
			bGroundCollectionMissing = LoadedGroundCollection == nullptr;
			if (bGroundCollectionMissing)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOWeatherIntegration] no ground weather collection at %s: the terrain shows no rain or snow"),
					*GroundWeatherCollection.ToString());
			}
		}
		if (LoadedGroundCollection)
		{
			UKismetMaterialLibrary::SetScalarParameterValue(World, LoadedGroundCollection, TEXT("GroundWetness"), GroundWeather.Wetness);
			UKismetMaterialLibrary::SetScalarParameterValue(World, LoadedGroundCollection, TEXT("GroundSnow"), GroundWeather.SnowCover);
		}
		GroundWrittenToMaterial = GroundWeather;
		bGroundWrittenToMaterial = true;
	}
}

// ============================================================================
// PERSISTENCE
// ============================================================================

FMOWeatherSaveData UMOWeatherIntegrationSubsystem::BuildWeatherSaveData() const
{
	FMOWeatherSaveData SaveData;

	if (!HasWeatherProvider())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWeatherIntegration] BuildWeatherSaveData: No provider registered — returning empty (bIsValid=false)"));
		SaveData.bIsValid = false;
		return SaveData;
	}

	// Delegate to provider for full save data capture
	SaveData = IMOWeatherProviderInterface::Execute_BuildWeatherSaveData(WeatherProvider.GetObject());
	SaveData.bIsValid = true;
	SaveData.Ground = GroundWeather;   // ours, not the bridge's (BR18-16)
	SaveData.bHasGround = true;

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOWeatherIntegration] BuildWeatherSaveData OUT: bIsValid=%s DateTime=%s Cloud=%.3f Fog=%.3f PresetPath=%s PresetObject=%s"),
		SaveData.bIsValid ? TEXT("true") : TEXT("false"),
		*SaveData.DateTime.ToString(),
		SaveData.CloudCoverage,
		SaveData.FogDensity,
		SaveData.WeatherPresetPath.IsValid() ? *SaveData.WeatherPresetPath.ToString() : TEXT("(empty — bridge BP didn't populate path)"),
		*GetNameSafe(SaveData.WeatherPresetObject.Get()));

	return SaveData;
}

bool UMOWeatherIntegrationSubsystem::ApplyWeatherSaveData(const FMOWeatherSaveData& SaveData)
{
	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOWeatherIntegration] ApplyWeatherSaveData IN: bIsValid=%s DateTime=%s Cloud=%.3f Fog=%.3f PresetPath=%s PresetObject=%s"),
		SaveData.bIsValid ? TEXT("true") : TEXT("false"),
		*SaveData.DateTime.ToString(),
		SaveData.CloudCoverage,
		SaveData.FogDensity,
		SaveData.WeatherPresetPath.IsValid() ? *SaveData.WeatherPresetPath.ToString() : TEXT("(empty)"),
		*GetNameSafe(SaveData.WeatherPresetObject.Get()));

	if (!SaveData.bIsValid)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWeatherIntegration] ApplyWeatherSaveData: bIsValid=false — rejecting"));
		return false;
	}

	// The ground as saved (older saves: dry and bare), advanced from the saved game time: the clock's offline catch-up then dries / wets it like any other time.
	SetGroundWeather(SaveData.bHasGround ? SaveData.Ground : FMOGroundWeatherState());
	LastGroundAdvance = SaveData.DateTime.GetTicks() > 0 ? SaveData.DateTime : FDateTime(0);
	UE_LOG(LogMOFramework, Log, TEXT("[MOWeatherIntegration] ground restored: wet %.3f snow %.3f (from %s)"),
		GroundWeather.Wetness, GroundWeather.SnowCover, SaveData.bHasGround ? TEXT("the save") : TEXT("an older save: dry"));

	if (!HasWeatherProvider())
	{
		// No provider yet - store as pending and apply when provider registers
		PendingSaveData = SaveData;
		bHasPendingSaveData = true;
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOWeatherIntegration] ApplyWeatherSaveData: No provider yet — QUEUED as pending. Will fire when bridge registers."));
		return true; // Return true because we've queued it
	}

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOWeatherIntegration] ApplyWeatherSaveData: Dispatching to provider %s"),
		*GetNameSafe(WeatherProvider.GetObject()));

	// Delegate to provider for full state restoration
	IMOWeatherProviderInterface::Execute_ApplyWeatherSaveData(WeatherProvider.GetObject(), SaveData);

	// Update cached state
	CachedWeatherState = GetCurrentWeatherState();
	bCachedIsDaytime = IsDaytime();
	bWasRaining = CachedWeatherState.IsRaining();
	bWasSnowing = CachedWeatherState.IsSnowing();

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOWeatherIntegration] ApplyWeatherSaveData DONE — post-apply state: Cloud=%.3f Fog=%.3f Rain=%.3f Daytime=%s"),
		CachedWeatherState.CloudCoverage,
		CachedWeatherState.Fog,
		CachedWeatherState.RainIntensity,
		bCachedIsDaytime ? TEXT("yes") : TEXT("no"));

	return true;
}

void UMOWeatherIntegrationSubsystem::SetDateTime(const FDateTime& DateTime)
{
	// Authoritative store is on the clock now. We also forward to the UDS
	// provider (if registered) so the visual sky updates. Future BP-level
	// cleanup: wire the provider to listen to clock directly and remove
	// this dual write.
	if (UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this))
	{
		Clock->SetGameDateTime(DateTime);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWeatherIntegration] SetDateTime: clock subsystem unavailable, falling back to provider only"));
	}

	if (HasWeatherProvider())
	{
		IMOWeatherProviderInterface::Execute_SetDateTime(WeatherProvider.GetObject(), DateTime);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOWeatherIntegration] Set date/time to %s"), *DateTime.ToString());
}

UObject* UMOWeatherIntegrationSubsystem::LoadUdsWeatherPreset(const FString& PresetName)
{
	// UDS weather presets are INSTANCES of UDS_Weather_Settings_C saved as data assets, NOT subclasses. Load path is
	//   /Game/UltraDynamicSky/Blueprints/Weather_Effects/Weather_Presets/<Name>.<Name>
	// (object name = asset name, no _C suffix -- that would be the class).
	const FString InstancePath = FString::Printf(
		TEXT("/Game/UltraDynamicSky/Blueprints/Weather_Effects/Weather_Presets/%s.%s"), *PresetName, *PresetName);

	UObject* PresetInstance = LoadObject<UObject>(nullptr, *InstancePath);

	// Fallback: maybe this version of UDS uses subclasses (older versions?)
	if (!PresetInstance)
	{
		const FString ClassPath = InstancePath + TEXT("_C");
		if (UClass* PresetClass = LoadClass<UObject>(nullptr, *ClassPath))
		{
			PresetInstance = PresetClass->GetDefaultObject();
		}
	}

	if (!PresetInstance)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWeatherIntegration] UDS weather preset '%s' not found at %s"), *PresetName, *InstancePath);
	}
	return PresetInstance;
}

const TArray<FMOWeatherPresetChoice>& UMOWeatherIntegrationSubsystem::GetPresetChoices()
{
	static const TArray<FMOWeatherPresetChoice> Choices = []
	{
		TArray<FMOWeatherPresetChoice> Out;
		auto Add = [&Out](const TCHAR* Id, const FText& Label, bool bBasic)
		{
			FMOWeatherPresetChoice& C = Out.AddDefaulted_GetRef();
			C.Id = FName(Id);
			C.Label = Label;
			C.bBasic = bBasic;
		};
		Add(TEXT("Clear_Skies"), NSLOCTEXT("MOWeather", "ClearSkies", "Clear"), true);
		Add(TEXT("Partly_Cloudy"), NSLOCTEXT("MOWeather", "PartlyCloudy", "Partly Cloudy"), true);
		Add(TEXT("Cloudy"), NSLOCTEXT("MOWeather", "Cloudy", "Cloudy"), false);
		Add(TEXT("Overcast"), NSLOCTEXT("MOWeather", "Overcast", "Overcast"), true);
		Add(TEXT("Foggy"), NSLOCTEXT("MOWeather", "Foggy", "Fog"), true);
		Add(TEXT("Rain_Light"), NSLOCTEXT("MOWeather", "RainLight", "Light Rain"), true);
		Add(TEXT("Rain"), NSLOCTEXT("MOWeather", "Rain", "Rain"), true);
		Add(TEXT("Rain_Thunderstorm"), NSLOCTEXT("MOWeather", "Thunderstorm", "Thunderstorm"), true);
		Add(TEXT("Snow_Light"), NSLOCTEXT("MOWeather", "SnowLight", "Light Snow"), true);
		Add(TEXT("Snow"), NSLOCTEXT("MOWeather", "Snow", "Snow"), true);
		Add(TEXT("Snow_Blizzard"), NSLOCTEXT("MOWeather", "Blizzard", "Blizzard"), true);
		Add(TEXT("Sand_Dust_Calm"), NSLOCTEXT("MOWeather", "DustCalm", "Dust"), false);
		Add(TEXT("Sand_Dust_Storm"), NSLOCTEXT("MOWeather", "DustStorm", "Dust Storm"), false);
		return Out;
	}();
	return Choices;
}

bool UMOWeatherIntegrationSubsystem::SetWeatherPresetByName(const FString& PresetName)
{
	if (!HasWeatherProvider())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWeatherIntegration] SetWeatherPresetByName('%s'): no weather provider"), *PresetName);
		return false;
	}
	UObject* Preset = LoadUdsWeatherPreset(PresetName);
	if (!Preset)
	{
		return false;
	}
	SetWeatherPreset(Preset);
	return true;
}

FSoftObjectPath UMOWeatherIntegrationSubsystem::GetCurrentWeatherPresetPath() const
{
	if (!HasWeatherProvider())
	{
		return FSoftObjectPath();
	}
	const FMOWeatherSaveData Data = IMOWeatherProviderInterface::Execute_BuildWeatherSaveData(WeatherProvider.GetObject());
	if (Data.WeatherPresetPath.IsValid())
	{
		return Data.WeatherPresetPath;
	}
	return Data.WeatherPresetObject ? FSoftObjectPath(Data.WeatherPresetObject) : FSoftObjectPath();
}

void UMOWeatherIntegrationSubsystem::ApplyNewGameStartConditions()
{
	bPendingClearSkies = true;
	TryApplyPendingClearSkies();
	if (bPendingClearSkies)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOWeatherIntegration] New game: weather provider not registered yet; clear skies queued"));
	}
}

void UMOWeatherIntegrationSubsystem::TryApplyPendingClearSkies()
{
	if (!bPendingClearSkies || !HasWeatherProvider())
	{
		return;
	}
	bPendingClearSkies = false;

	if (UObject* ClearSkies = LoadUdsWeatherPreset(TEXT("Clear_Skies")))
	{
		SetWeatherPreset(ClearSkies);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWeatherIntegration] New game: applied weather preset 'Clear_Skies'"));
	}
}

void UMOWeatherIntegrationSubsystem::SetWeatherPreset(UObject* PresetObject)
{
	if (!HasWeatherProvider())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWeatherIntegration] SetWeatherPreset: No provider registered (PresetObject=%s)"),
			*GetNameSafe(PresetObject));
		return;
	}

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOWeatherIntegration] SetWeatherPreset: PresetObject=%s (class=%s) → dispatching to provider %s"),
		*GetNameSafe(PresetObject),
		PresetObject ? *GetNameSafe(PresetObject->GetClass()) : TEXT("null"),
		*GetNameSafe(WeatherProvider.GetObject()));

	IMOWeatherProviderInterface::Execute_SetWeatherPreset(WeatherProvider.GetObject(), PresetObject);
}

// ---- IMOSaveDomain (Weather) — see MOSaveDomainInterface.h ----

void UMOWeatherIntegrationSubsystem::CaptureSaveDomain(UMOWorldSaveGame& Save)
{
	if (!HasWeatherProvider())
	{
		return;   // nothing meaningful to capture without a provider
	}
	Save.WeatherData = BuildWeatherSaveData();
}

void UMOWeatherIntegrationSubsystem::ApplySaveDomain(const UMOWorldSaveGame& Save)
{
	if (!Save.WeatherData.bIsValid)
	{
		return;   // old save or no provider when saved
	}
	// Stores as pending internally if the provider is not up yet.
	ApplyWeatherSaveData(Save.WeatherData);
}
