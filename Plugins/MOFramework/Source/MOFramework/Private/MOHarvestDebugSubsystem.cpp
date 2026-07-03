#include "MOHarvestDebugSubsystem.h"
#include "MOFramework.h"
#include "MOResourceDepletionSubsystem.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Misc/ScopeLock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
	FString GetHarvestTimestamp()
	{
		const FDateTime Now = FDateTime::Now();
		return FString::Printf(TEXT("%02d:%02d:%02d.%03d"),
			Now.GetHour(), Now.GetMinute(), Now.GetSecond(), Now.GetMillisecond());
	}
}

namespace
{
	// Console-name ownership guard for multi-GameInstance PIE — see the
	// matching comment in MOCheatSubsystem.cpp (same crash class, 2026-07-03).
	UMOHarvestDebugSubsystem* GHarvestDebugConsoleOwner = nullptr;
}

void UMOHarvestDebugSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	OpenLogFile();
	if (GHarvestDebugConsoleOwner == nullptr)
	{
		RegisterConsoleCommands();
		GHarvestDebugConsoleOwner = this;
	}

	Log(TEXT("System"), TEXT("=========================================="));
	Log(TEXT("System"), FString::Printf(TEXT("Harvest Debug log started — %s"), *FDateTime::Now().ToString()));
	Log(TEXT("System"), FString::Printf(TEXT("Log path: %s"), *LogFilePath));
	Log(TEXT("System"), TEXT("=========================================="));
}

void UMOHarvestDebugSubsystem::Deinitialize()
{
	Log(TEXT("System"), TEXT("Harvest Debug log closing"));
	if (GHarvestDebugConsoleOwner == this)
	{
		UnregisterConsoleCommands();
		GHarvestDebugConsoleOwner = nullptr;
	}
	CloseLogFile();
	Super::Deinitialize();
}

UMOHarvestDebugSubsystem* UMOHarvestDebugSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine) return nullptr;
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!World) return nullptr;
	UGameInstance* GI = World->GetGameInstance();
	return GI ? GI->GetSubsystem<UMOHarvestDebugSubsystem>() : nullptr;
}

void UMOHarvestDebugSubsystem::LogStatic(const UObject* WorldContextObject, const FString& Category, const FString& Message)
{
	if (UMOHarvestDebugSubsystem* Sys = Get(WorldContextObject))
	{
		Sys->Log(Category, Message);
	}
}

void UMOHarvestDebugSubsystem::Log(const FString& Category, const FString& Message)
{
	if (!bEnabled || !LogFileHandle) return;

	const FString Line = FString::Printf(TEXT("[%s] [frame %llu] [%s] %s\r\n"),
		*GetHarvestTimestamp(), GFrameCounter, *Category, *Message);

	FScopeLock Lock(&LogMutex);
	const FTCHARToUTF8 Converter(*Line);
	LogFileHandle->Write(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
	LogFileHandle->Flush();
}

void UMOHarvestDebugSubsystem::DumpDepletionState(const FString& Reason)
{
	if (!bEnabled) return;

	Log(TEXT("DUMP"), FString::Printf(TEXT(">>> DEPLETION MAP — %s <<<"), *Reason));

	UMOResourceDepletionSubsystem* Depletion = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UWorld* World = GI->GetWorld())
		{
			Depletion = World->GetSubsystem<UMOResourceDepletionSubsystem>();
		}
	}

	if (!Depletion)
	{
		Log(TEXT("DUMP"), TEXT("UMOResourceDepletionSubsystem: <not found in current world>"));
		return;
	}

	// Log the configured item ranges so we can see what's tracked.
	Log(TEXT("DUMP"), FString::Printf(TEXT("Configured yield ranges: %d entries (RespawnHours=%.1f)"),
		Depletion->InitialCountByItem.Num(), Depletion->RespawnHoursReal));
	for (const TPair<FName, FMOResourceYieldRange>& Pair : Depletion->InitialCountByItem)
	{
		Log(TEXT("DUMP"), FString::Printf(TEXT("  %s: %d-%d"),
			*Pair.Key.ToString(), Pair.Value.MinCount, Pair.Value.MaxCount));
	}

	Log(TEXT("DUMP"), TEXT("<<< END DEPLETION MAP >>>"));
}

void UMOHarvestDebugSubsystem::OpenLogFile()
{
	const FString LogsDir = FPaths::ProjectSavedDir() / TEXT("Logs");
	IFileManager::Get().MakeDirectory(*LogsDir, true);

	const FDateTime Now = FDateTime::Now();
	const FString FileName = FString::Printf(TEXT("HarvestDebug_%04d%02d%02d_%02d%02d%02d.log"),
		Now.GetYear(), Now.GetMonth(), Now.GetDay(),
		Now.GetHour(), Now.GetMinute(), Now.GetSecond());

	LogFilePath = LogsDir / FileName;

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	LogFileHandle = PlatformFile.OpenWrite(*LogFilePath, /*bAppend*/ false, /*bAllowRead*/ true);

	if (!LogFileHandle)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOHarvestDebugSubsystem] Failed to open log file: %s"), *LogFilePath);
	}
}

void UMOHarvestDebugSubsystem::CloseLogFile()
{
	FScopeLock Lock(&LogMutex);
	if (LogFileHandle)
	{
		LogFileHandle->Flush();
		delete LogFileHandle;
		LogFileHandle = nullptr;
	}
}

void UMOHarvestDebugSubsystem::RegisterConsoleCommands()
{
	IConsoleManager& CM = IConsoleManager::Get();

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Harvest.DumpState"),
		TEXT("Dump current resource depletion map to the HarvestDebug log file."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UMOHarvestDebugSubsystem* Sys = UMOHarvestDebugSubsystem::Get(World))
			{
				Sys->DumpDepletionState(TEXT("console command"));
			}
		})));
}

void UMOHarvestDebugSubsystem::UnregisterConsoleCommands()
{
	IConsoleManager& CM = IConsoleManager::Get();
	for (IConsoleCommand* Cmd : ConsoleCommands)
	{
		if (Cmd) CM.UnregisterConsoleObject(Cmd);
	}
	ConsoleCommands.Empty();
}
