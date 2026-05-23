/**
 * =============================================================================
 * MOGameClockSubsystem.h - Authoritative game time clock
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Single source of truth for "how fast does in-game time pass" across every
 * gameplay system that scales by real-time. Before this subsystem existed,
 * MOVitals, MOMetabolism, MOAnatomy, MOMentalState, MOAdrenaline each held
 * their own TimeScaleMultiplier UPROPERTY — and the headers literally said
 * "must match across all components" with no enforcement. Inevitable drift.
 *
 * This subsystem owns:
 *   - TimeScale (float multiplier — how many in-game seconds per real second)
 *   - Cumulative real play time (for "Playtime: 5h 32m" displays + save metadata)
 *   - Cumulative in-game seconds (integrated game time)
 *
 * Consumers ask: GameClock->GetScaledDeltaTime(RealDelta) and use that for
 * any rate calculation (hunger decay, wound progression, vitamin loss, etc).
 *
 * =============================================================================
 * RELATIONSHIP WITH PAUSE POLICY
 * =============================================================================
 * MO57 doesn't pause (Docs/PAUSE_POLICY.md). Real time is monotonic, which
 * makes the clock simple: no "paused vs unpaused" time tracking, no thaw
 * logic on resume, no two-mode complexity. RealPlayTime advances every tick.
 *
 * =============================================================================
 * SCOPE — WHAT'S HERE TODAY (Phases 1 + 2)
 * =============================================================================
 * - TimeScale + GetScaledDeltaTime (Phase 1)
 * - Cumulative real + game seconds (Phase 1)
 * - Save/load (Phase 1)
 * - In-game DateTime, authoritative (Phase 2)
 * - IsDaytime + OnDayNightChanged (Phase 2)
 *
 * MOWeatherIntegrationSubsystem.GetDateTime / IsDaytime / OnDayNightChanged
 * now forward to this subsystem. They remain in the weather API for
 * backward compatibility — callers using either path get the same answer.
 *
 * UDS visual sync (rendering the sun position from clock's DateTime) is
 * Blueprint-side cleanup not yet done. Clock advances DateTime
 * independently of UDS for now. If you set UDS time in the BP, it WON'T
 * automatically sync to the clock — wire that up when you're ready.
 *
 * =============================================================================
 * KNOWN PITFALLS
 * =============================================================================
 * [2026-05] TICK ORDER: This subsystem is a UTickableWorldSubsystem. Its
 *   tick runs alongside other world subsystems; if a consumer ticks BEFORE
 *   the clock on the same frame, its delta lookup will use the previous
 *   frame's accumulated time. For per-second rates this is invisible. For
 *   millisecond-precision systems (none yet), revisit tick group.
 *
 * [2026-05] CACHED POINTER: Consumers should cache the subsystem pointer
 *   in BeginPlay (TWeakObjectPtr) and query it each tick — not lookup every
 *   frame. WorldSubsystem::Get is fast but not free.
 *
 * =============================================================================
 * RELATED FILES:
 * - MOVitalsComponent.h/cpp (consumer)
 * - MOMetabolismComponent.h/cpp (consumer)
 * - MOAnatomyComponent.h/cpp (consumer)
 * - MOMentalStateComponent.h/cpp (consumer)
 * - MOAdrenalineComponent.h/cpp (consumer)
 * - MOWeatherIntegrationSubsystem.h (DateTime source for Phase 2)
 * - Docs/PAUSE_POLICY.md (why the clock can stay monotonic)
 *
 * LAST UPDATED: 2026-05-23
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOGameClockSubsystem.generated.h"

/**
 * Save data for the game clock. Persisted with the player's save slot so
 * playtime + game time elapsed + in-game date/time survive save/reload.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOGameClockSaveData
{
	GENERATED_BODY()

	/** Cumulative real-world seconds the player has spent in this save. */
	UPROPERTY()
	double RealSecondsAccumulated = 0.0;

	/** Cumulative in-game seconds (RealSeconds integrated against TimeScale). */
	UPROPERTY()
	double GameSecondsAccumulated = 0.0;

	/** TimeScale at time of save (so SetTimeScale changes survive across loads). */
	UPROPERTY()
	float TimeScale = 1.0f;

	/** In-game date and time at the moment of save. */
	UPROPERTY()
	FDateTime GameDateTime = FDateTime(2026, 6, 1, 6, 0, 0);

	/** True if this struct was populated from a real save (vs. default-init). */
	UPROPERTY()
	bool bIsValid = false;
};

/**
 * Fired when TimeScale changes via SetTimeScale. Allows consumers that cache
 * the value (e.g. for UI display) to refresh.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOGameClockTimeScaleChanged, float, OldScale, float, NewScale);

/**
 * Fired when in-game time crosses the daytime/nighttime boundary. bIsDaytime
 * = true if the new state is daytime. CurrentDateTime is the game DateTime
 * at the moment of transition.
 *
 * This delegate replaces UMOWeatherIntegrationSubsystem::OnDayNightChanged
 * as the authoritative source. The weather subsystem now listens here and
 * re-broadcasts its own delegate for backward compatibility.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOGameClockDayNightChanged, bool, bIsDaytime, const FDateTime&, CurrentDateTime);

/**
 * Authoritative game time clock. See file header for design + scope.
 */
UCLASS()
class MOFRAMEWORK_API UMOGameClockSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Convenience accessor. Returns nullptr only if the world has no subsystem (PIE shutdown, etc). */
	UFUNCTION(BlueprintPure, Category="MO|Clock", meta=(WorldContext="WorldContextObject"))
	static UMOGameClockSubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	//~ Begin UTickableWorldSubsystem Interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End UTickableWorldSubsystem Interface

	// =========================================================================
	// TIME SCALE
	// =========================================================================

	/**
	 * Current time-scale multiplier.
	 *
	 * 1.0 = real time (1 real second = 1 in-game second). Tweak via
	 * SetTimeScale or via project settings. Default 1.0 for now; will
	 * tune based on day-length / survival pacing once Phase 2 lands.
	 *
	 * Read this every tick rather than caching the value, so SetTimeScale
	 * (from debug console or settings menu) takes effect immediately.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Clock")
	float GetTimeScale() const { return TimeScale; }

	/**
	 * Set the time scale. Fires OnTimeScaleChanged so consumers can refresh
	 * cached values (UI displays, debug overlays). Clamped to [0.01, 1000.0].
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Clock")
	void SetTimeScale(float NewScale);

	/**
	 * Convenience: RealDeltaTime * TimeScale. Use this from Tick handlers
	 * instead of multiplying by a local constant. Centralizes the
	 * multiplication so SetTimeScale immediately affects every consumer.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Clock")
	float GetScaledDeltaTime(float RealDeltaTime) const;

	/** Fired whenever SetTimeScale is called with a different value. */
	UPROPERTY(BlueprintAssignable, Category="MO|Clock")
	FMOGameClockTimeScaleChanged OnTimeScaleChanged;

	// =========================================================================
	// ACCUMULATED TIME
	// =========================================================================

	/**
	 * Cumulative real-world seconds since this save was started. Survives
	 * level transitions, save/load. Use for "Playtime: 5h 32m" display.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Clock")
	double GetRealPlayTimeSeconds() const { return RealSecondsAccumulated; }

	/**
	 * Cumulative in-game seconds since this save was started — basically
	 * GetRealPlayTimeSeconds integrated against TimeScale. If TimeScale
	 * was 60.0 for 10 real seconds, GameTime advanced 600 game-seconds.
	 *
	 * Use this for any "wall clock" tracking in the simulated world
	 * (food spoilage, wound age, scheduled NPC routines, etc).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Clock")
	double GetGameTimeSeconds() const { return GameSecondsAccumulated; }

	// =========================================================================
	// IN-GAME DATE / TIME
	// =========================================================================
	//
	// GameDateTime is the in-game calendar — what day is it, what hour,
	// what minute. Authoritative; advances on every Tick by the scaled
	// delta. AI schedules, food spoilage timers, scheduled NPC routines
	// read from here.
	//
	// Distinct from GetGameTimeSeconds (a monotonic accumulator). DateTime
	// is the "calendar" view; GameTimeSeconds is the "stopwatch" view.

	/** Current in-game date and time. */
	UFUNCTION(BlueprintPure, Category="MO|Clock|DateTime")
	FDateTime GetGameDateTime() const { return GameDateTime; }

	/**
	 * Force the in-game DateTime to a specific value. Use for save load,
	 * debug commands, and any "time skip" gameplay (sleep until morning,
	 * fast-travel time advance, etc). Triggers daytime re-evaluation and
	 * fires OnDayNightChanged if state changed.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Clock|DateTime")
	void SetGameDateTime(const FDateTime& NewDateTime);

	/**
	 * True if the in-game time is within the configured daytime hour window
	 * [DaytimeStartHour, DaytimeEndHour). Default 6 AM – 6 PM.
	 *
	 * Computed from GameDateTime, not from any weather provider. UDS
	 * visual lighting may show a different state if not wired to the clock.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Clock|DateTime")
	bool IsDaytime() const { return bIsDaytime; }

	/**
	 * Fired exactly once whenever the daytime boolean flips. Subscribers
	 * include UMOWeatherIntegrationSubsystem (which re-broadcasts its own
	 * delegate for compatibility) and any creature AI that toggles
	 * behavior on day/night.
	 */
	UPROPERTY(BlueprintAssignable, Category="MO|Clock|DateTime")
	FMOGameClockDayNightChanged OnDayNightChanged;

	// =========================================================================
	// SAVE / LOAD
	// =========================================================================

	/** Snapshot current clock state for persistence. */
	UFUNCTION(BlueprintCallable, Category="MO|Clock|Persistence")
	FMOGameClockSaveData BuildSaveData() const;

	/**
	 * Restore previously-saved clock state. Resets internal accumulators
	 * to the saved values so play-time continues from where it left off.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Clock|Persistence")
	void ApplySaveData(const FMOGameClockSaveData& SaveData);

protected:
	/**
	 * Time-scale multiplier. EditDefaultsOnly — at runtime use SetTimeScale
	 * (which fires OnTimeScaleChanged). Default 1.0; tune in project
	 * settings or via debug console.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Clock", meta=(ClampMin="0.01", ClampMax="1000.0"))
	float TimeScale = 1.0f;

	/** Hour (0-23) at which daytime starts. Default 6 AM. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Clock|DateTime", meta=(ClampMin="0", ClampMax="23"))
	int32 DaytimeStartHour = 6;

	/** Hour (0-23, exclusive) at which daytime ends. Default 18 (6 PM). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Clock|DateTime", meta=(ClampMin="1", ClampMax="24"))
	int32 DaytimeEndHour = 18;

	/**
	 * Initial in-game DateTime when a fresh save starts (no clock save
	 * data to restore from). Default: June 1, 2026, 06:00:00.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Clock|DateTime")
	FDateTime DefaultStartDateTime = FDateTime(2026, 6, 1, 6, 0, 0);

	/** Recompute bIsDaytime from current GameDateTime; broadcast if state flipped. */
	void RefreshDaytimeState();

private:
	/** Cumulative real seconds since this clock started ticking (or since save load). */
	double RealSecondsAccumulated = 0.0;

	/** Cumulative in-game seconds (RealSecondsAccumulated * effective TimeScale, integrated). */
	double GameSecondsAccumulated = 0.0;

	/** Current in-game date and time. Advanced each tick by scaled delta. */
	FDateTime GameDateTime;

	/** Cached daytime state; updated only when GameDateTime crosses a boundary so the delegate doesn't fire every tick. */
	bool bIsDaytime = true;
};
