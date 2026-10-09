/**
 * =============================================================================
 * MOWorldSyncSubsystem.h - makes co-op clients follow the host's clock and sky
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH CO-OP TIME OR WEATHER
 *
 * THE RULE: the game clock and the weather are per-MACHINE world subsystems, so left alone every player runs a private day and a
 * private sky. The HOST's values are authoritative and travel on AMOGameState (FMOWorldClockInfo / FMOWorldWeatherInfo).
 *
 *   HOST   Tick: once a second publish the clock (DateTime + TimeScale + server time); every few seconds look at the weather
 *          preset the sky is showing and publish it when it CHANGES. (A skip -- MO.Clock.SetTime, sleeping -- shows up within
 *          a second because DateTime itself is what is published.)
 *   CLIENT AMOGameState::OnRep_* -> HandleReplicated*: the clock keeps advancing locally between updates and is SNAPPED only
 *          if it has drifted past a threshold; the weather preset is applied through the same provider the host uses (the
 *          UDS bridge), so each machine runs its own smooth transition. A late joiner gets the current values on arrival.
 *
 * Standalone (single player) never publishes. Apply code only ever runs on a client (NM_Client).
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOWorldSyncTypes.h"
#include "MOWorldSyncSubsystem.generated.h"

UCLASS()
class MOFRAMEWORK_API UMOWorldSyncSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UMOWorldSyncSubsystem* Get(const UObject* WorldContextObject);

	/** Called by AMOGameState::OnRep_WorldClock on a client. */
	void HandleReplicatedClock(const FMOWorldClockInfo& Info);

	/** Called by AMOGameState::OnRep_WorldWeather on a client. */
	void HandleReplicatedWeather(const FMOWorldWeatherInfo& Info);

	/** How far (in game seconds) a client's clock may drift before it is snapped to the host's. */
	static double SnapThresholdGameSeconds(float TimeScale);

	//~ Begin UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End UTickableWorldSubsystem

private:
	/** Seconds between host clock publishes. */
	static constexpr float ClockPublishIntervalSeconds = 1.0f;
	/** Seconds between host weather checks (a publish happens only when the preset changed). */
	static constexpr float WeatherPollIntervalSeconds = 3.0f;

	float ClockTimer = 0.f;
	float WeatherTimer = 0.f;

	/** Host: the last preset we published, so an unchanged sky costs nothing on the wire. */
	FSoftObjectPath LastPublishedWeather;

	/** Client: the host's preset we still have to put on screen (the provider may not exist yet). */
	FSoftObjectPath DesiredWeather;
	bool bWeatherDirty = false;

	void PublishClock(class AMOGameState& State);
	void PublishWeather(class AMOGameState& State);
	void TryApplyDesiredWeather();
};
