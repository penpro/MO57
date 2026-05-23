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
	UE_LOG(LogMOFramework, Log, TEXT("[MOGameClock] Initialized — TimeScale=%.2f"), TimeScale);
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
	// no "is paused?" gate needed here. Every tick advances both
	// accumulators.
	RealSecondsAccumulated += DeltaTime;
	GameSecondsAccumulated += static_cast<double>(DeltaTime) * static_cast<double>(TimeScale);
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

FMOGameClockSaveData UMOGameClockSubsystem::BuildSaveData() const
{
	FMOGameClockSaveData Data;
	Data.RealSecondsAccumulated = RealSecondsAccumulated;
	Data.GameSecondsAccumulated = GameSecondsAccumulated;
	Data.TimeScale = TimeScale;
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

	UE_LOG(LogMOFramework, Log,
		TEXT("[MOGameClock] ApplySaveData — RealPlayTime=%.1fs, GameTime=%.1fs, TimeScale=%.2f"),
		RealSecondsAccumulated, GameSecondsAccumulated, TimeScale);
}
