/**
 * =============================================================================
 * MOAmbientEnvironmentProvider.h - ambient environment contract (C1 phase 3)
 * =============================================================================
 *
 * PURPOSE:
 * The medical simulation needs two environmental inputs — feels-like
 * temperature and weather exposure at a location — but must not know the
 * weather subsystem (it lives above the medical layer). This is the seam:
 * a plain C++ provider interface plus a tiny world-subsystem registry in
 * Core. The weather integration subsystem registers itself on world begin;
 * vitals asks the registry. Same shape as IMOSaveDomain: contract at the
 * bottom, registration from above.
 *
 * Plain C++ interface on purpose (not a UInterface): no UHT surface, no
 * reflection cost, and Core stays free of upper-module types.
 *
 * =============================================================================
 * RELATED FILES: MOWeatherTypes.h, MOWeatherIntegrationSubsystem.h (impl),
 *   MOVitalsComponent.h (consumer)
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOWeatherTypes.h"
#include "MOAmbientEnvironmentProvider.generated.h"

/** What the medical layer may ask of the environment. */
class IMOAmbientEnvironmentProvider
{
public:
	virtual ~IMOAmbientEnvironmentProvider() = default;

	/** Feels-like temperature at a world location (wind chill, wet-cold, heat index). */
	virtual float GetAmbientFeelsLikeCelsius(const FVector& Location) const = 0;

	/** Rain/snow/wind exposure at a world location. */
	virtual FMOWeatherExposure GetAmbientExposure(const FVector& Location) const = 0;

	/** Global rain intensity 0..1 (wetness accumulation input). */
	virtual float GetAmbientRainIntensity01() const = 0;
};

/**
 * A point source of warmth in the world — campfire, forge, hearth. Actors
 * above register while they burn; the medical layer feels the aggregate
 * through the registry without knowing any fire exists.
 */
class IMOLocalHeatSource
{
public:
	virtual ~IMOLocalHeatSource() = default;

	/** World position the heat radiates from. */
	virtual FVector GetHeatLocation() const = 0;

	/** Temperature boost at the source itself (°C above ambient). */
	virtual float GetHeatDeltaCelsius() const = 0;

	/** Distance at which the boost falls to zero (cm). */
	virtual float GetHeatRadius() const = 0;

	/** False while unlit or out of fuel — contributes nothing. */
	virtual bool IsHeatActive() const = 0;
};

/**
 * A point source of visible light near the eye -- a flame: a carried torch, a campfire. Registered while it burns (on every machine that
 * renders it); the eye's adaptation feels the aggregate (UMOEyeAdaptationComponent) without knowing what burns.
 */
class IMOLocalLightSource
{
public:
	virtual ~IMOLocalLightSource() = default;

	/** World position of the flame. */
	virtual FVector GetLightLocation() const = 0;

	/** Its luminous intensity now (cd): flicker and all. */
	virtual float GetLuminousIntensityCd() const = 0;
};

/**
 * Core-level locator the provider registers into. Lives in Core so both the
 * provider (above) and the consumers (medical layer) can reach it without
 * knowing each other. Also aggregates registered point heat sources —
 * mechanics only; what emits heat and how much is decided above.
 */
UCLASS()
class MOFRAMEWORKCORE_API UMOAmbientEnvironmentRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UMOAmbientEnvironmentRegistry* Get(const UObject* WorldContext)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		return World ? World->GetSubsystem<UMOAmbientEnvironmentRegistry>() : nullptr;
	}

	void RegisterProvider(IMOAmbientEnvironmentProvider* InProvider) { Provider = InProvider; }
	void UnregisterProvider(const IMOAmbientEnvironmentProvider* InProvider)
	{
		if (Provider == InProvider)
		{
			Provider = nullptr;
		}
	}

	/** Null when no provider is up yet — consumers keep their own fallback. */
	IMOAmbientEnvironmentProvider* GetProvider() const { return Provider; }

	/** All registered sources, lit or not — callers filter on IsHeatActive().
	 *  For AI that SEEKS warmth (colony shelter pass); thermal queries should
	 *  use GetLocalHeatDeltaAt instead. */
	const TArray<IMOLocalHeatSource*>& GetHeatSources() const { return HeatSources; }

	void RegisterHeatSource(IMOLocalHeatSource* InSource) { HeatSources.AddUnique(InSource); }
	void UnregisterHeatSource(const IMOLocalHeatSource* InSource)
	{
		HeatSources.RemoveAll([InSource](const IMOLocalHeatSource* S) { return S == InSource; });
	}

	/**
	 * Warmth contributed by point sources at a location (°C above ambient,
	 * >= 0). Linear falloff to the source radius; overlapping fires take the
	 * strongest, not the sum — standing between two campfires is not twice
	 * as warm.
	 */
	float GetLocalHeatDeltaAt(const FVector& Location) const
	{
		float Best = 0.0f;
		for (const IMOLocalHeatSource* Source : HeatSources)
		{
			if (!Source || !Source->IsHeatActive())
			{
				continue;
			}
			const float Radius = Source->GetHeatRadius();
			if (Radius <= 0.0f)
			{
				continue;
			}
			const float Dist = FVector::Dist(Location, Source->GetHeatLocation());
			if (Dist < Radius)
			{
				Best = FMath::Max(Best, Source->GetHeatDeltaCelsius() * (1.0f - Dist / Radius));
			}
		}
		return Best;
	}

private:
	/** Raw on purpose: the registering subsystem unregisters in Deinitialize,
	 *  and both live in the same world lifetime. */
	IMOAmbientEnvironmentProvider* Provider = nullptr;

	/** Raw on purpose: sources unregister in EndPlay (always runs before the
	 *  world tears the subsystem down). */
	TArray<IMOLocalHeatSource*> HeatSources;

	/** Raw on purpose, as HeatSources. */
	TArray<IMOLocalLightSource*> LightSources;

public:
	void RegisterLightSource(IMOLocalLightSource* InSource) { LightSources.AddUnique(InSource); }
	void UnregisterLightSource(const IMOLocalLightSource* InSource)
	{
		LightSources.RemoveAll([InSource](const IMOLocalLightSource* S) { return S == InSource; });
	}
	const TArray<IMOLocalLightSource*>& GetLightSources() const { return LightSources; }

	/**
	 * Illuminance at a location from the registered flames (lux): each source's intensity over the square of its distance in metres (the
	 * inverse-square law), summed -- light adds up, unlike warmth. Distances under MinDistanceCm count as MinDistanceCm (a flame is not a point).
	 * Occlusion is ignored: what the eye sees of a flame round a corner is small, and the caller only wants how light-adapted the eye is.
	 */
	float GetIlluminanceLuxAt(const FVector& Location, float MinDistanceCm = 50.0f) const
	{
		float Lux = 0.0f;
		for (const IMOLocalLightSource* Source : LightSources)
		{
			if (!Source)
			{
				continue;
			}
			const float DistM = FMath::Max(FVector::Dist(Location, Source->GetLightLocation()), MinDistanceCm) / 100.0f;
			Lux += FMath::Max(Source->GetLuminousIntensityCd(), 0.0f) / (DistM * DistM);
		}
		return Lux;
	}
};
