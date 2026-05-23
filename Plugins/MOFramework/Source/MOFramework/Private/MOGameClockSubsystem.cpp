/**
 * MOGameClockSubsystem.cpp - Authoritative game time clock
 * See header for design.
 */

#include "MOGameClockSubsystem.h"
#include "MOFramework.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

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
	RefreshDaytimeState();

	RegisterConsoleCommands();

	UE_LOG(LogMOFramework, Log,
		TEXT("[MOGameClock] Initialized — TimeScale=%.2f, GameDateTime=%s (%s)"),
		TimeScale, *GameDateTime.ToString(), bIsDaytime ? TEXT("DAY") : TEXT("NIGHT"));
}

void UMOGameClockSubsystem::Deinitialize()
{
	UnregisterConsoleCommands();

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

	// Re-evaluate daytime state after the jump. If we skipped from 5 PM to
	// 7 AM (a "sleep until morning" jump), the day/night delegate fires.
	RefreshDaytimeState();
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

// =============================================================================
// CONSOLE COMMANDS
// =============================================================================
//
// MO.Clock.* commands. Follows the existing MO.X.Y namespace established by
// MOUIDebugSubsystem and MOHarvestDebugSubsystem. Lambdas resolve the world
// subsystem at call time so commands work after PIE restarts (a captured
// pointer would dangle).

void UMOGameClockSubsystem::RegisterConsoleCommands()
{
	IConsoleManager& CM = IConsoleManager::Get();

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Clock.Info"),
		TEXT("Print current clock state (TimeScale, RealPlayTime, GameTime, GameDateTime, IsDaytime)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			const UMOGameClockSubsystem* Sys = UMOGameClockSubsystem::Get(World);
			if (!Sys) { UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] No subsystem")); return; }
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MOClock] TimeScale=%.2f RealPlayTime=%.1fs GameTime=%.1fs GameDateTime=%s (%s)"),
				Sys->GetTimeScale(),
				Sys->GetRealPlayTimeSeconds(),
				Sys->GetGameTimeSeconds(),
				*Sys->GetGameDateTime().ToString(),
				Sys->IsDaytime() ? TEXT("DAY") : TEXT("NIGHT"));
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Clock.SetTimeScale"),
		TEXT("Set TimeScale (in-game seconds per real second). Usage: MO.Clock.SetTimeScale 60"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UMOGameClockSubsystem* Sys = UMOGameClockSubsystem::Get(World);
			if (!Sys) { UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] No subsystem")); return; }
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] Usage: MO.Clock.SetTimeScale <float>"));
				return;
			}
			const float NewScale = FCString::Atof(*Args[0]);
			Sys->SetTimeScale(NewScale);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] TimeScale -> %.2f"), Sys->GetTimeScale());
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Clock.SetTime"),
		TEXT("Jump to a specific hour:minute today. Usage: MO.Clock.SetTime 6 30  (= 06:30)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UMOGameClockSubsystem* Sys = UMOGameClockSubsystem::Get(World);
			if (!Sys) { UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] No subsystem")); return; }
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] Usage: MO.Clock.SetTime <hour 0-23> [minute 0-59]"));
				return;
			}
			const int32 Hour = FMath::Clamp(FCString::Atoi(*Args[0]), 0, 23);
			const int32 Minute = (Args.Num() > 1) ? FMath::Clamp(FCString::Atoi(*Args[1]), 0, 59) : 0;
			const FDateTime Current = Sys->GetGameDateTime();
			const FDateTime NewDT(Current.GetYear(), Current.GetMonth(), Current.GetDay(), Hour, Minute, 0);
			Sys->SetGameDateTime(NewDT);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] GameDateTime -> %s"), *NewDT.ToString());
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Clock.AdvanceHours"),
		TEXT("Fast-forward the in-game DateTime by N hours. Usage: MO.Clock.AdvanceHours 8"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UMOGameClockSubsystem* Sys = UMOGameClockSubsystem::Get(World);
			if (!Sys) { UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] No subsystem")); return; }
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] Usage: MO.Clock.AdvanceHours <float>"));
				return;
			}
			const float Hours = FCString::Atof(*Args[0]);
			const FDateTime NewDT = Sys->GetGameDateTime() + FTimespan::FromHours(Hours);
			Sys->SetGameDateTime(NewDT);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] Advanced %.2fh -> %s"), Hours, *NewDT.ToString());
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Clock.SkipToDay"),
		TEXT("Skip in-game time to the next 06:00 (start of daytime)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			UMOGameClockSubsystem* Sys = UMOGameClockSubsystem::Get(World);
			if (!Sys) { UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] No subsystem")); return; }
			const FDateTime Current = Sys->GetGameDateTime();
			// 06:00 today, or 06:00 tomorrow if we're already past 06:00.
			FDateTime Target(Current.GetYear(), Current.GetMonth(), Current.GetDay(), 6, 0, 0);
			if (Target <= Current) Target += FTimespan::FromDays(1);
			Sys->SetGameDateTime(Target);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] Skipped to %s"), *Target.ToString());
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Clock.SkipToNight"),
		TEXT("Skip in-game time to the next 18:00 (start of nighttime)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			UMOGameClockSubsystem* Sys = UMOGameClockSubsystem::Get(World);
			if (!Sys) { UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] No subsystem")); return; }
			const FDateTime Current = Sys->GetGameDateTime();
			FDateTime Target(Current.GetYear(), Current.GetMonth(), Current.GetDay(), 18, 0, 0);
			if (Target <= Current) Target += FTimespan::FromDays(1);
			Sys->SetGameDateTime(Target);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] Skipped to %s"), *Target.ToString());
		}),
		ECVF_Default));

	UE_LOG(LogMOFramework, Log, TEXT("[MOClock] Registered %d console commands"), ConsoleCommands.Num());
}

void UMOGameClockSubsystem::UnregisterConsoleCommands()
{
	IConsoleManager& CM = IConsoleManager::Get();
	for (IConsoleCommand* Cmd : ConsoleCommands)
	{
		if (Cmd) CM.UnregisterConsoleObject(Cmd);
	}
	ConsoleCommands.Reset();
}
