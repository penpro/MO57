/**
 * MOHarvestDebugSubsystem.h - File-based diagnostic logger for the harvest /
 * resource-depletion pipeline.
 *
 * Modeled on UMOUIDebugSubsystem. Each game session opens a fresh log file in
 * Saved/Logs/HarvestDebug_<timestamp>.log and writes timestamped entries from
 * MOHarvestSubsystem and MOResourceDepletionSubsystem.
 *
 * Usage:
 *   MOHARVEST_LOG(this, "Category", "Message format: %s", *Value);
 *
 * Console commands:
 *   MO.Harvest.Enable [0|1] — turn the file log on / off (DEFAULT OFF, like the UI debug log: audit H55)
 *   MO.Harvest.DumpState — write a snapshot of the current depletion map
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HAL/PlatformFileManager.h"
#include "MOHarvestDebugSubsystem.generated.h"

UCLASS()
class MOFRAMEWORK_API UMOHarvestDebugSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UMOHarvestDebugSubsystem* Get(const UObject* WorldContextObject);
	static void LogStatic(const UObject* WorldContextObject, const FString& Category, const FString& Message);

	void Log(const FString& Category, const FString& Message);

	/** Snapshot the current depletion map. */
	void DumpDepletionState(const FString& Reason);

	const FString& GetLogFilePath() const { return LogFilePath; }

	bool IsEnabled() const { return bEnabled; }
	/** Opens the log file (and writes the banner) the first time it is switched on. */
	void SetEnabled(bool bInEnabled);

	/** Process-wide mirror of bEnabled for the MOHARVEST_LOG macro's pre-format gate (no subsystem lookup, no Printf when off). */
	static bool IsLoggingEnabled() { return bGloballyEnabled; }

private:
	void OpenLogFile();
	void CloseLogFile();
	void RegisterConsoleCommands();
	void UnregisterConsoleCommands();

	FString LogFilePath;
	IFileHandle* LogFileHandle = nullptr;
	FCriticalSection LogMutex;
	// Default OFF (the H55 fix, applied to its twin): always-on it opened a file every session and paid a critical section + UTF-8 convert +
	// synchronous Write + Flush on the game thread for EVERY inventory add, harvest and PCG resource cell -- 8-11 ms of a survivor's spawn was its
	// starting items going through here (Insights, Oct 13 2026). Opt in with MO.Harvest.Enable 1.
	bool bEnabled = false;
	static bool bGloballyEnabled;

	TArray<IConsoleCommand*> ConsoleCommands;
};

/** Null-safe convenience macro. Compiled out in Shipping; otherwise gated on the process-static enable BEFORE formatting (off by default: one bool read). */
#if UE_BUILD_SHIPPING
#define MOHARVEST_LOG(WorldCtx, Category, Format, ...) do {} while (0)
#else
#define MOHARVEST_LOG(WorldCtx, Category, Format, ...) \
	do { \
		if (UMOHarvestDebugSubsystem::IsLoggingEnabled()) \
		{ \
			UMOHarvestDebugSubsystem::LogStatic((WorldCtx), TEXT(Category), FString::Printf(TEXT(Format), ##__VA_ARGS__)); \
		} \
	} while (0)
#endif
