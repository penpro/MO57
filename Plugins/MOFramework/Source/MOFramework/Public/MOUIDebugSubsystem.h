#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HAL/PlatformFileManager.h"
#include "MOUIDebugSubsystem.generated.h"

/**
 * File-based UI debug logger. Auto-creates a log file in Saved/Logs/UIDebug_<timestamp>.log
 * on init and writes timestamped entries with categories.
 *
 * Usage from anywhere:
 *   MOUI_LOG(this, TEXT("Cursor"), TEXT("Set bShowMouseCursor = %s"), bShow ? TEXT("true") : TEXT("false"));
 *
 * Or dump full state:
 *   MOUI_DUMP_STATE(this);
 *
 * Console commands:
 *   MO.UI.DumpState  - snapshot current UI/input/focus state
 *   MO.UI.OpenLog    - reveal log file location in console
 *   MO.UI.Enable/Disable - toggle logging
 */
UCLASS()
class MOFRAMEWORK_API UMOUIDebugSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Get from any world context. Returns nullptr if not available. */
	static UMOUIDebugSubsystem* Get(const UObject* WorldContextObject);

	/** Convenience: log via static helper that handles null subsystem gracefully. */
	static void LogStatic(const UObject* WorldContextObject, const FString& Category, const FString& Message);

	/** Append a timestamped, categorized line to the log file. */
	void Log(const FString& Category, const FString& Message);

	/** Snapshot current UI/input/cursor state and write to log. */
	void DumpFullState(const FString& Reason);

	/** Returns the absolute path of the current log file. */
	const FString& GetLogFilePath() const { return LogFilePath; }

	bool IsEnabled() const { return bEnabled; }
	void SetEnabled(bool bInEnabled);

private:
	void OpenLogFile();
	void CloseLogFile();
	void RegisterConsoleCommands();
	void UnregisterConsoleCommands();

	void HookFocusEvents();
	void UnhookFocusEvents();
	void HandleFocusChanging(const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldFocusedWidgetPath,
		const TSharedPtr<SWidget>& OldFocusedWidget, const FWidgetPath& NewFocusedWidgetPath,
		const TSharedPtr<SWidget>& NewFocusedWidget);

	FString LogFilePath;
	IFileHandle* LogFileHandle = nullptr;
	FCriticalSection LogMutex;
	bool bEnabled = true;

	TArray<IConsoleCommand*> ConsoleCommands;
	FDelegateHandle FocusChangingHandle;
};

// Convenience macros — null-safe, never crash even if subsystem is gone.
#define MOUI_LOG(WorldCtx, Category, Format, ...) \
	UMOUIDebugSubsystem::LogStatic((WorldCtx), TEXT(Category), FString::Printf(TEXT(Format), ##__VA_ARGS__))

#define MOUI_DUMP_STATE_LITERAL(WorldCtx, ReasonLiteral) \
	do { \
		if (UMOUIDebugSubsystem* DbgSys = UMOUIDebugSubsystem::Get(WorldCtx)) \
		{ \
			DbgSys->DumpFullState(TEXT(ReasonLiteral)); \
		} \
	} while (0)

#define MOUI_DUMP_STATE(WorldCtx, ReasonStr) \
	do { \
		if (UMOUIDebugSubsystem* DbgSys = UMOUIDebugSubsystem::Get(WorldCtx)) \
		{ \
			DbgSys->DumpFullState(ReasonStr); \
		} \
	} while (0)
