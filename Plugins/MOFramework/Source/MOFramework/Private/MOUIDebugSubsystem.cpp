#include "MOUIDebugSubsystem.h"
#include "MOFramework.h"
#include "MOGameUIManagerSubsystem.h"
#include "MOPrimaryGameLayout.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Misc/ScopeLock.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Input/Events.h"
#include "Layout/WidgetPath.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "GameplayTagContainer.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "CommonActivatableWidget.h"
#include "Input/CommonUIActionRouterBase.h"

namespace
{
	FString GetTimestamp()
	{
		const FDateTime Now = FDateTime::Now();
		return FString::Printf(TEXT("%02d:%02d:%02d.%03d"),
			Now.GetHour(), Now.GetMinute(), Now.GetSecond(), Now.GetMillisecond());
	}

	FString GetInputModeDesc(APlayerController* PC)
	{
		if (!PC) return TEXT("<no PC>");
		// We can't query input mode directly; report what we know.
		return FString::Printf(TEXT("ShowCursor=%s IgnoreMove=%s IgnoreLook=%s"),
			PC->bShowMouseCursor ? TEXT("YES") : TEXT("no"),
			PC->IsMoveInputIgnored() ? TEXT("YES") : TEXT("no"),
			PC->IsLookInputIgnored() ? TEXT("YES") : TEXT("no"));
	}

	FString GetViewportCaptureDesc()
	{
		// Viewport-level capture info is not directly queryable from PlayerController in 5.7.
		// Check via Slate application instead.
		if (FSlateApplication::IsInitialized())
		{
			TSharedPtr<FSlateUser> User = FSlateApplication::Get().GetUser(0);
			if (User.IsValid())
			{
				return FString::Printf(TEXT("CursorCaptor=%s HasCursorCaptured=%s"),
					User->HasAnyCapture() ? TEXT("YES") : TEXT("no"),
					User->HasCursorCapture() ? TEXT("YES") : TEXT("no"));
			}
		}
		return TEXT("<slate unavailable>");
	}
}

void UMOUIDebugSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	OpenLogFile();
	RegisterConsoleCommands();
	HookFocusEvents();

	Log(TEXT("System"), TEXT("=========================================="));
	Log(TEXT("System"), FString::Printf(TEXT("UI Debug log started — %s"), *FDateTime::Now().ToString()));
	Log(TEXT("System"), FString::Printf(TEXT("Log path: %s"), *LogFilePath));
	Log(TEXT("System"), TEXT("=========================================="));
}

void UMOUIDebugSubsystem::Deinitialize()
{
	Log(TEXT("System"), TEXT("UI Debug log closing"));
	UnhookFocusEvents();
	UnregisterConsoleCommands();
	CloseLogFile();
	Super::Deinitialize();
}

void UMOUIDebugSubsystem::HookFocusEvents()
{
	if (FSlateApplication::IsInitialized())
	{
		FocusChangingHandle = FSlateApplication::Get().OnFocusChanging().AddUObject(
			this, &UMOUIDebugSubsystem::HandleFocusChanging);
	}
}

void UMOUIDebugSubsystem::UnhookFocusEvents()
{
	if (FocusChangingHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnFocusChanging().Remove(FocusChangingHandle);
		FocusChangingHandle.Reset();
	}
}

void UMOUIDebugSubsystem::HandleFocusChanging(const FFocusEvent& FocusEvent,
	const FWeakWidgetPath& OldFocusedWidgetPath, const TSharedPtr<SWidget>& OldFocusedWidget,
	const FWidgetPath& NewFocusedWidgetPath, const TSharedPtr<SWidget>& NewFocusedWidget)
{
	if (!bEnabled) return;

	const FString OldName = OldFocusedWidget.IsValid() ? OldFocusedWidget->GetTypeAsString() : TEXT("<none>");
	const FString NewName = NewFocusedWidget.IsValid() ? NewFocusedWidget->GetTypeAsString() : TEXT("<none>");

	// Stringify focus cause
	const TCHAR* CauseStr = TEXT("?");
	switch (FocusEvent.GetCause())
	{
	case EFocusCause::Mouse:        CauseStr = TEXT("Mouse"); break;
	case EFocusCause::Navigation:   CauseStr = TEXT("Navigation"); break;
	case EFocusCause::SetDirectly:  CauseStr = TEXT("SetDirectly"); break;
	case EFocusCause::Cleared:      CauseStr = TEXT("Cleared"); break;
	case EFocusCause::OtherWidgetLostFocus: CauseStr = TEXT("OtherLost"); break;
	case EFocusCause::WindowActivate: CauseStr = TEXT("WindowActivate"); break;
	}

	Log(TEXT("FocusChange"), FString::Printf(TEXT("%s -> %s  (cause=%s user=%d)"),
		*OldName, *NewName, CauseStr, FocusEvent.GetUser()));
}

UMOUIDebugSubsystem* UMOUIDebugSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject) return nullptr;
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World) return nullptr;
	UGameInstance* GI = World->GetGameInstance();
	return GI ? GI->GetSubsystem<UMOUIDebugSubsystem>() : nullptr;
}

void UMOUIDebugSubsystem::LogStatic(const UObject* WorldContextObject, const FString& Category, const FString& Message)
{
	if (UMOUIDebugSubsystem* Sys = Get(WorldContextObject))
	{
		Sys->Log(Category, Message);
	}
}

void UMOUIDebugSubsystem::Log(const FString& Category, const FString& Message)
{
	if (!bEnabled || !LogFileHandle) return;

	const FString Line = FString::Printf(TEXT("[%s] [frame %llu] [%s] %s\r\n"),
		*GetTimestamp(), GFrameCounter, *Category, *Message);

	FScopeLock Lock(&LogMutex);
	const FTCHARToUTF8 Converter(*Line);
	LogFileHandle->Write(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
	LogFileHandle->Flush();
}

void UMOUIDebugSubsystem::DumpFullState(const FString& Reason)
{
	if (!bEnabled) return;

	Log(TEXT("DUMP"), FString::Printf(TEXT(">>> FULL STATE DUMP — %s <<<"), *Reason));

	// PlayerController state
	APlayerController* PC = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (ULocalPlayer* LP = GI->GetFirstGamePlayer())
		{
			PC = LP->GetPlayerController(GetWorld());
		}
	}

	if (PC)
	{
		Log(TEXT("DUMP"), FString::Printf(TEXT("PlayerController: %s"), *GetInputModeDesc(PC)));
		Log(TEXT("DUMP"), FString::Printf(TEXT("Capture: %s"), *GetViewportCaptureDesc()));

		// Query the CommonUI action router for what IT thinks the active config is.
		// If our widgets aren't in its activation tree, this returns the default (All/NoCapture).
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UCommonUIActionRouterBase* Router = LP->GetSubsystem<UCommonUIActionRouterBase>())
			{
				const ECommonInputMode RouterMode = Router->GetActiveInputMode();
				const EMouseCaptureMode RouterCapture = Router->GetActiveMouseCaptureMode();
				Log(TEXT("DUMP"), FString::Printf(TEXT("ActionRouter: InputMode=%d MouseCapture=%d (LocalPlayer=%s)"),
					(int32)RouterMode, (int32)RouterCapture, *LP->GetName()));
			}
			else
			{
				Log(TEXT("DUMP"), TEXT("ActionRouter: <NOT FOUND on LocalPlayer — CommonUI not active>"));
			}
		}
	}
	else
	{
		Log(TEXT("DUMP"), TEXT("PlayerController: <unavailable>"));
	}

	// CommonUI layer stack contents
	UMOGameUIManagerSubsystem* UISub = UMOGameUIManagerSubsystem::Get(GetWorld());
	if (UISub)
	{
		Log(TEXT("DUMP"), FString::Printf(TEXT("UISubsystem: GetActiveMenuCount = %d"), UISub->GetActiveMenuCount()));

		if (UMOPrimaryGameLayout* Layout = UISub->GetRootLayout())
		{
			static const FName LayerNames[] = {
				FName("MO.UI.Layer.HUD"),
				FName("MO.UI.Layer.Game"),
				FName("MO.UI.Layer.GameOverlay"),
				FName("MO.UI.Layer.Menu"),
				FName("MO.UI.Layer.Modal"),
			};

			for (const FName& LayerName : LayerNames)
			{
				FGameplayTag Tag = FGameplayTag::RequestGameplayTag(LayerName, false);
				if (!Tag.IsValid()) continue;

				UCommonActivatableWidgetContainerBase* Stack = Layout->GetLayerStack(Tag);
				if (!Stack)
				{
					Log(TEXT("DUMP"), FString::Printf(TEXT("  Layer %s: <stack not registered>"), *LayerName.ToString()));
					continue;
				}

				const TArray<UCommonActivatableWidget*>& Widgets = Stack->GetWidgetList();
				UWidget* Active = Stack->GetActiveWidget();
				Log(TEXT("DUMP"), FString::Printf(TEXT("  Layer %s: %d widgets, Active = %s"),
					*LayerName.ToString(),
					Widgets.Num(),
					Active ? *Active->GetName() : TEXT("<none>")));

				for (int32 i = 0; i < Widgets.Num(); ++i)
				{
					if (UCommonActivatableWidget* W = Widgets[i])
					{
						ULocalPlayer* WidgetLP = W->GetOwningLocalPlayer();
						Log(TEXT("DUMP"), FString::Printf(TEXT("    [%d] %s (activated=%s, focusable=%s, inViewport=%s, OwningLP=%s)"),
							i, *W->GetName(),
							W->IsActivated() ? TEXT("YES") : TEXT("no"),
							W->IsFocusable() ? TEXT("YES") : TEXT("no"),
							W->IsInViewport() ? TEXT("YES") : TEXT("no"),
							WidgetLP ? *WidgetLP->GetName() : TEXT("<null>")));
					}
				}
			}
		}
		else
		{
			Log(TEXT("DUMP"), TEXT("  Root layout: <not configured>"));
		}
	}

	// Slate focus
	if (FSlateApplication::IsInitialized())
	{
		TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetUserFocusedWidget(0);
		Log(TEXT("DUMP"), FString::Printf(TEXT("Slate focus: %s"),
			FocusedWidget.IsValid() ? *FocusedWidget->GetTypeAsString() : TEXT("<none>")));
	}

	Log(TEXT("DUMP"), TEXT("<<< END DUMP >>>"));
}

void UMOUIDebugSubsystem::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;
	Log(TEXT("System"), FString::Printf(TEXT("Logging %s"), bEnabled ? TEXT("ENABLED") : TEXT("DISABLED")));
}

void UMOUIDebugSubsystem::OpenLogFile()
{
	const FString LogsDir = FPaths::ProjectSavedDir() / TEXT("Logs");
	IFileManager::Get().MakeDirectory(*LogsDir, true);

	const FDateTime Now = FDateTime::Now();
	const FString FileName = FString::Printf(TEXT("UIDebug_%04d%02d%02d_%02d%02d%02d.log"),
		Now.GetYear(), Now.GetMonth(), Now.GetDay(),
		Now.GetHour(), Now.GetMinute(), Now.GetSecond());

	LogFilePath = LogsDir / FileName;

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	LogFileHandle = PlatformFile.OpenWrite(*LogFilePath, /*bAppend*/ false, /*bAllowRead*/ true);

	if (!LogFileHandle)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOUIDebugSubsystem] Failed to open log file: %s"), *LogFilePath);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUIDebugSubsystem] UI debug log opened: %s"), *LogFilePath);
	}
}

void UMOUIDebugSubsystem::CloseLogFile()
{
	FScopeLock Lock(&LogMutex);
	if (LogFileHandle)
	{
		LogFileHandle->Flush();
		delete LogFileHandle;
		LogFileHandle = nullptr;
	}
}

void UMOUIDebugSubsystem::RegisterConsoleCommands()
{
	IConsoleManager& CM = IConsoleManager::Get();

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.UI.DumpState"),
		TEXT("Dump current UI/input/focus state to the UIDebug log file."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UMOUIDebugSubsystem* Sys = UMOUIDebugSubsystem::Get(World))
			{
				Sys->DumpFullState(TEXT("Manual via console"));
				UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Dumped state to %s"), *Sys->GetLogFilePath());
			}
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.UI.OpenLog"),
		TEXT("Print the UI debug log file path to the console."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UMOUIDebugSubsystem* Sys = UMOUIDebugSubsystem::Get(World))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Log file: %s"), *Sys->GetLogFilePath());
			}
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.UI.Enable"),
		TEXT("Enable UI debug logging."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UMOUIDebugSubsystem* Sys = UMOUIDebugSubsystem::Get(World)) Sys->SetEnabled(true);
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.UI.Disable"),
		TEXT("Disable UI debug logging."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UMOUIDebugSubsystem* Sys = UMOUIDebugSubsystem::Get(World)) Sys->SetEnabled(false);
		}),
		ECVF_Default));
}

void UMOUIDebugSubsystem::UnregisterConsoleCommands()
{
	IConsoleManager& CM = IConsoleManager::Get();
	for (IConsoleCommand* Cmd : ConsoleCommands)
	{
		CM.UnregisterConsoleObject(Cmd);
	}
	ConsoleCommands.Empty();
}
