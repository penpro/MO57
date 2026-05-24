/**
 * MOGameClockSubsystem.cpp - Authoritative game time clock
 * See header for design.
 */

#include "MOGameClockSubsystem.h"
#include "MOFramework.h"
#include "Engine/World.h"

UMOGameClockSubsystem* UMOGameClockSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}
	const UWorld* World = WorldContextObject->GetWorld();
	return World ? World->GetSubsystem<UMOGameClockSubsystem>() : nullptr;
}

void UMOGameClockSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Seed the in-game DateTime from the configured default. ApplySaveData
	// overrides this when a save is loaded.
	GameDateTime = DefaultStartDateTime;

	// Prime LastBroadcastHour to the starting hour so the very first tick
	// doesn't fire a spurious OnHourChanged for "hour 6 just happened" when
	// in fact we started at hour 6.
	LastBroadcastHour = GameDateTime.GetHour();

	RefreshDaytimeState();

	UE_LOG(LogMOFramework, Log,
		TEXT("[MOGameClock] Initialized — TimeScale=%.2f, GameDateTime=%s (%s)"),
		TimeScale, *GameDateTime.ToString(), bIsDaytime ? TEXT("DAY") : TEXT("NIGHT"));
}

void UMOGameClockSubsystem::Deinitialize()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOGameClock] Deinitialize — RealPlayTime=%.1fs, GameTime=%.1fs"),
		RealSecondsAccumulated, GameSecondsAccumulated);
	Super::Deinitialize();
}

void UMOGameClockSubsystem::Tick(float DeltaTime)
{
	// MO57 doesn't pause (Docs/PAUSE_POLICY.md). Real time is monotonic;
	// no "is paused?" gate needed here. Every tick advances all
	// accumulators + the in-game DateTime.
	RealSecondsAccumulated += DeltaTime;

	const double ScaledDelta = static_cast<double>(DeltaTime) * static_cast<double>(TimeScale);
	GameSecondsAccumulated += ScaledDelta;

	// Advance the in-game DateTime by the scaled delta. FTimespan::FromSeconds
	// accepts fractional values, so we don't lose sub-second precision when
	// TimeScale is small.
	GameDateTime += FTimespan::FromSeconds(ScaledDelta);

	// Check if we crossed a day/night boundary this tick. Cheap (no
	// transition? do nothing); broadcasts only on actual flip.
	RefreshDaytimeState();

	// Check if the hour rolled over (e.g. 06:59:58 → 07:00:01). Cheap;
	// broadcasts at most once per in-game hour. Drives the UDS sync in
	// UMOWeatherIntegrationSubsystem and any future scheduled events.
	RefreshHourState();
}

TStatId UMOGameClockSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMOGameClockSubsystem, STATGROUP_Tickables);
}

void UMOGameClockSubsystem::SetTimeScale(float NewScale)
{
	const float Clamped = FMath::Clamp(NewScale, 0.01f, 1000.0f);
	if (FMath::IsNearlyEqual(Clamped, TimeScale))
	{
		return;
	}
	const float Old = TimeScale;
	TimeScale = Clamped;
	UE_LOG(LogMOFramework, Log, TEXT("[MOGameClock] TimeScale %.3f -> %.3f"), Old, TimeScale);
	OnTimeScaleChanged.Broadcast(Old, TimeScale);
}

float UMOGameClockSubsystem::GetScaledDeltaTime(float RealDeltaTime) const
{
	return RealDeltaTime * TimeScale;
}

// =============================================================================
// IN-GAME DATE / TIME
// =============================================================================

void UMOGameClockSubsystem::SetGameDateTime(const FDateTime& NewDateTime)
{
	if (NewDateTime == GameDateTime)
	{
		return;
	}

	GameDateTime = NewDateTime;
	UE_LOG(LogMOFramework, Log, TEXT("[MOGameClock] SetGameDateTime -> %s"), *GameDateTime.ToString());

	// Re-evaluate daytime + hour state after the jump. If we skipped from
	// 5 PM to 7 AM (a "sleep until morning" jump), both delegates fire —
	// once each, for the destination state.
	RefreshDaytimeState();
	RefreshHourState();
}

void UMOGameClockSubsystem::RefreshDaytimeState()
{
	const int32 Hour = GameDateTime.GetHour();
	const bool bNowDaytime = (Hour >= DaytimeStartHour && Hour < DaytimeEndHour);

	if (bNowDaytime == bIsDaytime)
	{
		return;
	}

	bIsDaytime = bNowDaytime;
	UE_LOG(LogMOFramework, Log, TEXT("[MOGameClock] Day/Night flipped -> %s at %s"),
		bIsDaytime ? TEXT("DAY") : TEXT("NIGHT"), *GameDateTime.ToString());
	OnDayNightChanged.Broadcast(bIsDaytime, GameDateTime);
}

void UMOGameClockSubsystem::RefreshHourState()
{
	const int32 CurrentHour = GameDateTime.GetHour();
	if (CurrentHour == LastBroadcastHour)
	{
		return;
	}

	LastBroadcastHour = CurrentHour;
	UE_LOG(LogMOFramework, Log, TEXT("[MOGameClock] Hour rolled -> %02d:00 at %s"),
		CurrentHour, *GameDateTime.ToString());
	OnHourChanged.Broadcast(CurrentHour, GameDateTime);
}

// =============================================================================
// SAVE / LOAD
// =============================================================================

FMOGameClockSaveData UMOGameClockSubsystem::BuildSaveData() const
{
	FMOGameClockSaveData Data;
	Data.RealSecondsAccumulated = RealSecondsAccumulated;
	Data.GameSecondsAccumulated = GameSecondsAccumulated;
	Data.TimeScale = TimeScale;
	Data.GameDateTime = GameDateTime;
	Data.bIsValid = true;
	return Data;
}

void UMOGameClockSubsystem::ApplySaveData(const FMOGameClockSaveData& SaveData)
{
	if (!SaveData.bIsValid)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameClock] ApplySaveData skipped — save data marked invalid (fresh save?)"));
		return;
	}

	RealSecondsAccumulated = SaveData.RealSecondsAccumulated;
	GameSecondsAccumulated = SaveData.GameSecondsAccumulated;

	// Restore time scale through SetTimeScale so OnTimeScaleChanged fires —
	// any UI / debug listeners need to refresh when a saved scale differs
	// from the default.
	SetTimeScale(SaveData.TimeScale);

	// Restore DateTime through SetGameDateTime so OnDayNightChanged fires
	// if the saved time crosses a day/night boundary from the default.
	SetGameDateTime(SaveData.GameDateTime);

	UE_LOG(LogMOFramework, Log,
		TEXT("[MOGameClock] ApplySaveData — RealPlayTime=%.1fs, GameTime=%.1fs, TimeScale=%.2f, GameDateTime=%s"),
		RealSecondsAccumulated, GameSecondsAccumulated, TimeScale, *GameDateTime.ToString());
}

// Console commands (MO.Clock.*) moved to UMOCheatSubsystem.
// UWorldSubsystem lifetime is incompatible with IConsoleManager — see the
// "DO NOT MAKE THIS A UWorldSubsystem" note in MOCheatSubsystem.h.
