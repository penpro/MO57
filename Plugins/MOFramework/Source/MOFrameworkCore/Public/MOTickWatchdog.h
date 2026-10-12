// A zero-setup watchdog for periodic game-thread work: MO_TICK_WATCH(TEXT("Name")) at the top of a Tick / timer callback logs "[MOTICK] <Name> took N ms" when the scope runs longer than the threshold.
//
// Why it exists: the CSV profiler pinned a once-a-second 30 ms hitch on "Exclusive/GameThread/Tickables" (every tickable subsystem together), which does not say WHICH subsystem. Wrap each tickable's
// Tick with this and the packaged log names it. Compiled out of Shipping builds; the cost elsewhere is two clock reads per scope.

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"

#if !UE_BUILD_SHIPPING

struct FMOTickWatchdog
{
	explicit FMOTickWatchdog(const TCHAR* InName, double InThresholdMs = 5.0)
		: Name(InName), ThresholdMs(InThresholdMs), StartSeconds(FPlatformTime::Seconds())
	{
	}

	~FMOTickWatchdog()
	{
		const double Ms = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
		if (Ms > ThresholdMs)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MOTICK] %s took %.1f ms"), Name, Ms);
		}
	}

private:
	const TCHAR* Name;
	double ThresholdMs;
	double StartSeconds;
};

#define MO_TICK_WATCH(NameLiteral) FMOTickWatchdog MoTickWatchdogScope(NameLiteral)

#else

#define MO_TICK_WATCH(NameLiteral)

#endif
