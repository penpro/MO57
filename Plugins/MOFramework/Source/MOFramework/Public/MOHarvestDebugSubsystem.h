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
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

private:
	void OpenLogFile();
	void CloseLogFile();
	void RegisterConsoleCommands();
	void UnregisterConsoleCommands();

	FString LogFilePath;
	IFileHandle* LogFileHandle = nullptr;
	FCriticalSection LogMutex;
	bool bEnabled = true;

	TArray<IConsoleCommand*> ConsoleCommands;
};

/** Null-safe convenience macro. Never crashes if the subsystem is gone. */
#define MOHARVEST_LOG(WorldCtx, Category, Format, ...) \
	UMOHarvestDebugSubsystem::LogStatic((WorldCtx), TEXT(Category), FString::Printf(TEXT(Format), ##__VA_ARGS__))
