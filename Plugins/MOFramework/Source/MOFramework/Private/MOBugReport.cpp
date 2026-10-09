#include "MOBugReport.h"
#include "MOBugReportBundle.h"
#include "MOFramework.h"
#include "MOBuildInfo.h"
#include "MOGameClockSubsystem.h"
#include "MOGameSettings.h"
#include "MOPersistenceSubsystem.h"
#include "MOWeatherIntegrationSubsystem.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "GenericPlatform/GenericPlatformOutputDevices.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HttpModule.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreMiscDefines.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UnrealClient.h"

namespace
{
	/** TEST ONLY: send to this URL instead of the configured crash endpoint. Loopback hosts only (IsAllowedEndpoint), http allowed. */
	TAutoConsoleVariable<FString> CVarEndpointOverride(
		TEXT("MO.BugReport.EndpointOverride"), TEXT(""),
		TEXT("TEST ONLY: POST bug reports to this URL instead of [CrashReportClient] DataRouterUrl. Only loopback hosts (127.0.0.1, localhost) are accepted."), ECVF_Cheat);

	/** Seconds that must pass after a successful send before the next one (a stuck key or a script cannot flood the endpoint). */
	TAutoConsoleVariable<float> CVarCooldownSeconds(
		TEXT("MO.BugReport.CooldownSeconds"), 60.0f,
		TEXT("Seconds between two successful bug report sends. Tests set 0."), ECVF_Cheat);

	/**
	 * The host of an http(s) URL, lower case, without port. Empty when the URL has no host or carries user-info ("http://localhost:x@evil.example/" would
	 * otherwise read as "localhost" in a naive split while the connection goes to evil.example).
	 */
	FString ToHostLower(const FString& Url)
	{
		const int32 SchemeEnd = Url.Find(TEXT("://"));
		if (SchemeEnd == INDEX_NONE)
		{
			return FString();
		}
		FString Authority = Url.Mid(SchemeEnd + 3);
		int32 End = INDEX_NONE;
		for (int32 Index = 0; Index < Authority.Len(); ++Index)
		{
			if (Authority[Index] == TEXT('/') || Authority[Index] == TEXT('?') || Authority[Index] == TEXT('#'))
			{
				End = Index;
				break;
			}
		}
		if (End != INDEX_NONE)
		{
			Authority.LeftInline(End);
		}
		if (Authority.Contains(TEXT("@")))
		{
			return FString();
		}
		if (Authority.StartsWith(TEXT("[")))
		{
			int32 Close = INDEX_NONE;
			return Authority.FindChar(TEXT(']'), Close) ? Authority.Left(Close + 1).ToLower() : FString(); // "[::1]:8080" -> "[::1]"
		}
		int32 Colon = INDEX_NONE;
		if (Authority.FindChar(TEXT(':'), Colon))
		{
			Authority.LeftInline(Colon);
		}
		return Authority.ToLower();
	}

	TArray<uint8> ToUtf8(const FString& Text)
	{
		FTCHARToUTF8 Converter(*Text);
		TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
		return Bytes;
	}

	const TCHAR* NetModeName(ENetMode Mode)
	{
		switch (Mode)
		{
		case NM_Standalone: return TEXT("Standalone");
		case NM_DedicatedServer: return TEXT("DedicatedServer");
		case NM_ListenServer: return TEXT("ListenServer");
		case NM_Client: return TEXT("Client");
		default: return TEXT("Unknown");
		}
	}

	/** Box-filter downscale: each output pixel is the average of the source block it covers. */
	void BoxDownscale(int32 Width, int32 Height, const TArray<FColor>& Source, int32 OutWidth, int32 OutHeight, TArray<FColor>& Out)
	{
		Out.SetNumUninitialized(OutWidth * OutHeight);
		for (int32 OutY = 0; OutY < OutHeight; ++OutY)
		{
			const int32 Y0 = static_cast<int32>((static_cast<int64>(OutY) * Height) / OutHeight);
			const int32 Y1 = FMath::Max(Y0 + 1, static_cast<int32>((static_cast<int64>(OutY + 1) * Height) / OutHeight));
			for (int32 OutX = 0; OutX < OutWidth; ++OutX)
			{
				const int32 X0 = static_cast<int32>((static_cast<int64>(OutX) * Width) / OutWidth);
				const int32 X1 = FMath::Max(X0 + 1, static_cast<int32>((static_cast<int64>(OutX + 1) * Width) / OutWidth));
				uint32 R = 0, G = 0, B = 0, Count = 0;
				for (int32 Y = Y0; Y < Y1 && Y < Height; ++Y)
				{
					for (int32 X = X0; X < X1 && X < Width; ++X)
					{
						const FColor& Pixel = Source[Y * Width + X];
						R += Pixel.R;
						G += Pixel.G;
						B += Pixel.B;
						++Count;
					}
				}
				Count = FMath::Max<uint32>(Count, 1);
				Out[OutY * OutWidth + OutX] = FColor(static_cast<uint8>(R / Count), static_cast<uint8>(G / Count), static_cast<uint8>(B / Count), 255);
			}
		}
	}
}

UMOBugReportSubsystem* UMOBugReportSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (!GameInstance && GEngine)
	{
		// A console command given at startup (-ExecCmds) has no world yet; the game instance is the same object for every world of the session.
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.OwningGameInstance && (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE))
			{
				GameInstance = Context.OwningGameInstance;
				break;
			}
		}
	}
	return GameInstance ? GameInstance->GetSubsystem<UMOBugReportSubsystem>() : nullptr;
}

void UMOBugReportSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RegisterBuiltInContributors();
}

void UMOBugReportSubsystem::Deinitialize()
{
	ClearScreenshotCaptureBinding();
	if (ActiveRequest.IsValid())
	{
		ActiveRequest->OnProcessRequestComplete().Unbind();
		ActiveRequest->CancelRequest();
		ActiveRequest.Reset();
	}
	PendingDone.Unbind();
	bSubmitting = false;
	Super::Deinitialize();
}

const TArray<FString>& UMOBugReportSubsystem::GetCategories()
{
	static const TArray<FString> Categories = {
		TEXT("Gameplay"), TEXT("Crash or freeze"), TEXT("Multiplayer"), TEXT("Interface"), TEXT("Performance"), TEXT("Graphics"), TEXT("Other") };
	return Categories;
}

// ============================================================================
// Contributors
// ============================================================================

void UMOBugReportSubsystem::RegisterContributor(FName Id, FMOBugReportContributor Contributor)
{
	if (Id.IsNone() || !Contributor)
	{
		return;
	}
	for (FContributorEntry& Entry : Contributors)
	{
		if (Entry.Id == Id)
		{
			Entry.Fn = MoveTemp(Contributor); // re-registering replaces: a system that restarts does not appear twice
			return;
		}
	}
	Contributors.Add({ Id, MoveTemp(Contributor) });
}

void UMOBugReportSubsystem::UnregisterContributor(FName Id)
{
	Contributors.RemoveAll([Id](const FContributorEntry& Entry) { return Entry.Id == Id; });
}

void UMOBugReportSubsystem::CollectFields(FMOBugReportFields& OutFields) const
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	const FString User = FPlatformProcess::UserName(false);
	const FString Computer = FPlatformProcess::ComputerName();

	for (const FContributorEntry& Entry : Contributors)
	{
		FMOBugReportFields Local;
		Entry.Fn(World, Local);
		for (const TPair<FString, FString>& Row : Local)
		{
			const FString Name = FString::Printf(TEXT("%s.%s"), *Entry.Id.ToString(), *Row.Key);
			OutFields.Emplace(Name, MOBugReportBundle::ClampText(MOBugReportBundle::Sanitize(Row.Value, User, Computer), MaxFieldValueChars));
		}
	}
}

void UMOBugReportSubsystem::RegisterBuiltInContributors()
{
	RegisterContributor(TEXT("Build"), [](const UWorld*, FMOBugReportFields& Out)
	{
		Out.Emplace(TEXT("Version"), UMOBuildInfo::GetProjectVersion());
		Out.Emplace(TEXT("Commit"), UMOBuildInfo::GetCommitHash());
		Out.Emplace(TEXT("Branch"), UMOBuildInfo::GetBranch());
		Out.Emplace(TEXT("Dirty"), UMOBuildInfo::IsDirtyBuild() ? TEXT("yes") : TEXT("no"));
		Out.Emplace(TEXT("BuiltAt"), UMOBuildInfo::GetBuildTimestamp());
		Out.Emplace(TEXT("Configuration"), LexToString(FApp::GetBuildConfiguration()));
		Out.Emplace(TEXT("Engine"), FEngineVersion::Current().ToString());
	});

	RegisterContributor(TEXT("System"), [](const UWorld*, FMOBugReportFields& Out)
	{
		FString OsLabel, OsSubLabel;
		FPlatformMisc::GetOSVersions(OsLabel, OsSubLabel);
		Out.Emplace(TEXT("OS"), (OsLabel + TEXT(" ") + OsSubLabel).TrimStartAndEnd());
		Out.Emplace(TEXT("CPU"), FPlatformMisc::GetCPUBrand().TrimStartAndEnd());
		Out.Emplace(TEXT("Cores"), FString::FromInt(FPlatformMisc::NumberOfCores()));
		Out.Emplace(TEXT("RamGB"), FString::FromInt(static_cast<int32>(FPlatformMemory::GetConstants().TotalPhysicalGB)));
		Out.Emplace(TEXT("GPU"), FPlatformMisc::GetPrimaryGPUBrand());
		Out.Emplace(TEXT("Locale"), FPlatformMisc::GetDefaultLocale());
		if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
		{
			const FIntPoint Size = GEngine->GameViewport->Viewport->GetSizeXY();
			Out.Emplace(TEXT("Viewport"), FString::Printf(TEXT("%dx%d"), Size.X, Size.Y));
		}
		Out.Emplace(TEXT("FrameMs"), FString::Printf(TEXT("%.1f"), FApp::GetDeltaTime() * 1000.0)); // the last frame only: a menu is open while a report is written
		Out.Emplace(TEXT("SessionUptimeMin"), FString::Printf(TEXT("%.1f"), (FPlatformTime::Seconds() - GStartTime) / 60.0));
	});

	RegisterContributor(TEXT("Session"), [](const UWorld* World, FMOBugReportFields& Out)
	{
		if (!World)
		{
			Out.Emplace(TEXT("World"), TEXT("(none)"));
			return;
		}
		Out.Emplace(TEXT("Map"), UWorld::RemovePIEPrefix(World->GetMapName()));
		Out.Emplace(TEXT("NetMode"), NetModeName(World->GetNetMode()));
		if (const AGameStateBase* State = World->GetGameState())
		{
			Out.Emplace(TEXT("Players"), FString::FromInt(State->PlayerArray.Num()));
		}
		Out.Emplace(TEXT("WorldSeed"), FString::FromInt(UMOGameSettings::GetWorldSeed()));
		if (const UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (const UMOPersistenceSubsystem* Persistence = GameInstance->GetSubsystem<UMOPersistenceSubsystem>())
			{
				Out.Emplace(TEXT("SaveSlot"), Persistence->GetCurrentSlotName());
			}
		}
	});

	RegisterContributor(TEXT("Clock"), [](const UWorld* World, FMOBugReportFields& Out)
	{
		if (const UMOGameClockSubsystem* Clock = World ? UMOGameClockSubsystem::Get(World) : nullptr)
		{
			Out.Emplace(TEXT("GameTime"), Clock->GetGameDateTime().ToIso8601());
			Out.Emplace(TEXT("TimeScale"), FString::Printf(TEXT("%.2f"), Clock->GetTimeScale()));
			Out.Emplace(TEXT("PlayMinutes"), FString::Printf(TEXT("%.1f"), Clock->GetRealPlayTimeSeconds() / 60.0));
		}
	});

	RegisterContributor(TEXT("Weather"), [](const UWorld* World, FMOBugReportFields& Out)
	{
		if (const UMOWeatherIntegrationSubsystem* Weather = World ? World->GetSubsystem<UMOWeatherIntegrationSubsystem>() : nullptr)
		{
			Out.Emplace(TEXT("SkyTime"), Weather->GetSkyDateTime().ToIso8601());
			Out.Emplace(TEXT("Preset"), Weather->GetCurrentWeatherPresetPath().ToString());
		}
	});

	RegisterContributor(TEXT("Player"), [](const UWorld* World, FMOBugReportFields& Out)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			Out.Emplace(TEXT("Pawn"), TEXT("(none)"));
			return;
		}
		Out.Emplace(TEXT("Pawn"), Pawn->GetClass()->GetName());
		const FVector Location = Pawn->GetActorLocation();
		Out.Emplace(TEXT("Location"), FString::Printf(TEXT("%.0f, %.0f, %.0f"), Location.X, Location.Y, Location.Z));
		Out.Emplace(TEXT("Speed"), FString::Printf(TEXT("%.0f"), Pawn->GetVelocity().Size()));
	});
}

// ============================================================================
// Screenshot
// ============================================================================

bool UMOBugReportSubsystem::EncodeScreenshotJpeg(int32 Width, int32 Height, const TArray<FColor>& Bitmap, int32 MaxWidth, TArray<uint8>& OutJpeg, int32& OutWidth, int32& OutHeight)
{
	OutJpeg.Reset();
	OutWidth = OutHeight = 0;
	if (Width <= 0 || Height <= 0 || MaxWidth <= 0 || Bitmap.Num() != Width * Height)
	{
		return false;
	}

	OutWidth = FMath::Min(Width, MaxWidth);
	OutHeight = FMath::Max(1, static_cast<int32>((static_cast<int64>(Height) * OutWidth) / Width));

	TArray<FColor> Scaled;
	BoxDownscale(Width, Height, Bitmap, OutWidth, OutHeight, Scaled);

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
	if (!ImageWrapper.IsValid()
		|| !ImageWrapper->SetRaw(Scaled.GetData(), Scaled.Num() * sizeof(FColor), OutWidth, OutHeight, ERGBFormat::BGRA, 8))
	{
		return false;
	}
	const TArray64<uint8> Compressed = ImageWrapper->GetCompressed(80);
	if (Compressed.Num() == 0)
	{
		return false;
	}
	OutJpeg.Append(Compressed.GetData(), Compressed.Num());
	return true;
}

void UMOBugReportSubsystem::CaptureScreenshot()
{
	CapturedScreenshot.Reset();
	if (!GEngine || !GEngine->GameViewport || IsRunningDedicatedServer() || IsRunningCommandlet() || !FApp::CanEverRender())
	{
		return;
	}
	if (!ScreenshotCaptureHandle.IsValid())
	{
		ScreenshotCaptureHandle = UGameViewportClient::OnScreenshotCaptured().AddUObject(this, &UMOBugReportSubsystem::HandleScreenshotCaptured);
	}
	FScreenshotRequest::RequestScreenshot(/*bInShowUI=*/false); // the game as drawn, without the menu that is about to cover it
}

void UMOBugReportSubsystem::HandleScreenshotCaptured(int32 Width, int32 Height, const TArray<FColor>& Bitmap)
{
	ClearScreenshotCaptureBinding(); // one shot
	TArray<uint8> Jpeg;
	int32 OutWidth = 0, OutHeight = 0;
	if (EncodeScreenshotJpeg(Width, Height, Bitmap, ScreenshotMaxWidth, Jpeg, OutWidth, OutHeight))
	{
		CapturedScreenshot = MoveTemp(Jpeg);
		CapturedScreenshotWidth = OutWidth;
		CapturedScreenshotHeight = OutHeight;
		UE_LOG(LogMOFramework, Log, TEXT("[MOBugReport] Screenshot captured: %dx%d -> %dx%d JPEG, %d bytes"), Width, Height, OutWidth, OutHeight, CapturedScreenshot.Num());
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBugReport] Screenshot: could not encode the %dx%d capture (%d pixels)"), Width, Height, Bitmap.Num());
	}
}

void UMOBugReportSubsystem::ClearScreenshotCaptureBinding()
{
	if (ScreenshotCaptureHandle.IsValid())
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotCaptureHandle);
		ScreenshotCaptureHandle.Reset();
	}
}

// ============================================================================
// Building
// ============================================================================

FString UMOBugReportSubsystem::ValidateDraft(const FMOBugReportDraft& Draft)
{
	if (Draft.Title.TrimStartAndEnd().Len() < 3)
	{
		return TEXT("Please give the report a title (at least a few words) so we know what it is about.");
	}
	return FString();
}

FString UMOBugReportSubsystem::ReadLogTail() const
{
	if (GLog)
	{
		GLog->Flush(); // what the game said a moment ago must be on disk before we read the file
	}
	const FString LogFile = FGenericPlatformOutputDevices::GetAbsoluteLogFilename();
	TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*LogFile, FILEREAD_AllowWrite)); // the engine still has it open for writing
	if (!Reader)
	{
		return FString();
	}
	const int64 Size = Reader->TotalSize();
	// UTF-8 text: MaxLogChars characters are at most 4x that many bytes, but logs are nearly all ASCII; read a little more than needed and trim by characters.
	const int64 ReadBytes = FMath::Min<int64>(Size, static_cast<int64>(MaxLogChars) * 2);
	TArray<uint8> Bytes;
	Bytes.SetNumUninitialized(static_cast<int32>(ReadBytes));
	Reader->Seek(Size - ReadBytes);
	Reader->Serialize(Bytes.GetData(), ReadBytes);
	Reader->Close();

	FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
	const FString Text(Converter.Length(), Converter.Get());
	const FString Scrubbed = MOBugReportBundle::Sanitize(Text, FPlatformProcess::UserName(false), FPlatformProcess::ComputerName());
	return MOBugReportBundle::TailText(Scrubbed, MaxLogChars);
}

FString UMOBugReportSubsystem::BuildReportText(const FMOBugReportDraft& Draft, const FGuid& ReportId, bool bPreview) const
{
	FMOBugReportFields Fields;
	CollectFields(Fields);

	auto OrNone = [](const FString& Text) { return Text.TrimStartAndEnd().IsEmpty() ? FString(TEXT("(none)")) : Text; };

	FString Out;
	Out += TEXT("MO57 BUG REPORT\n");
	Out += ReportId.IsValid() ? FString::Printf(TEXT("Report id: %s\n"), *ReportId.ToString(EGuidFormats::Digits)) : FString(TEXT("Report id: (assigned when sent)\n"));
	Out += FString::Printf(TEXT("Created:   %s UTC\n"), *FDateTime::UtcNow().ToIso8601());
	Out += FString::Printf(TEXT("Category:  %s\n"), *Draft.Category);
	Out += FString::Printf(TEXT("Title:     %s\n\n"), *MOBugReportBundle::ClampText(Draft.Title, MaxTitleChars));
	Out += FString::Printf(TEXT("What happened:\n%s\n\n"), *OrNone(MOBugReportBundle::ClampText(Draft.Description, MaxTextChars)));
	Out += FString::Printf(TEXT("How to make it happen again:\n%s\n\n"), *OrNone(MOBugReportBundle::ClampText(Draft.Steps, MaxTextChars)));
	Out += FString::Printf(TEXT("Contact (optional, entered by the player): %s\n\n"), *OrNone(MOBugReportBundle::ClampText(Draft.Contact, MaxContactChars)));

	Out += TEXT("--- Game state (collected automatically) ---\n");
	for (const TPair<FString, FString>& Row : Fields)
	{
		Out += FString::Printf(TEXT("%s: %s\n"), *Row.Key, *Row.Value);
	}

	Out += TEXT("\n--- Attachments ---\n");
	Out += Draft.bIncludeLog
		? FString::Printf(TEXT("game.log: the last %d KB of this session's log%s\n"), MaxLogChars / 1024, bPreview ? TEXT(" (not shown here)") : TEXT(""))
		: FString(TEXT("game.log: not included\n"));
	if (!Draft.bIncludeScreenshot)
	{
		Out += TEXT("Screenshot.jpg: not included\n");
	}
	else if (HasScreenshot())
	{
		Out += FString::Printf(TEXT("Screenshot.jpg: %dx%d, %d KB (the frame this report was opened on)\n"), CapturedScreenshotWidth, CapturedScreenshotHeight, (CapturedScreenshot.Num() + 1023) / 1024);
	}
	else
	{
		Out += TEXT("Screenshot.jpg: could not be captured\n");
	}
	return Out;
}

bool UMOBugReportSubsystem::BuildBundle(const FMOBugReportDraft& Draft, const FGuid& ReportId, TArray<uint8>& OutBundle, FString& OutError, int32* OutUncompressedSize) const
{
	const FString Directory = MOBugReportBundle::MakeDirectoryName(ReportId);

	FMOBugReportFields GameFields;
	CollectFields(GameFields);
	if (!Draft.Contact.TrimStartAndEnd().IsEmpty())
	{
		GameFields.Emplace(TEXT("Report.Contact"), MOBugReportBundle::ClampText(Draft.Contact, MaxContactChars)); // the player's own words: not scrubbed
	}
	GameFields.Emplace(TEXT("Report.Category"), Draft.Category);

	const FString LogTail = Draft.bIncludeLog ? ReadLogTail() : FString();

	// Variants, most complete first: a bundle over the cap loses the screenshot, then most of the log, then the log.
	struct FVariant { bool bScreenshot; int32 LogChars; };
	const FVariant Variants[] = {
		{ Draft.bIncludeScreenshot, MaxLogChars },
		{ false, MaxLogChars },
		{ false, 64 * 1024 },
		{ false, 0 },
	};

	for (const FVariant& Variant : Variants)
	{
		FMOBugReportDraft Effective = Draft;
		Effective.bIncludeScreenshot = Variant.bScreenshot;
		Effective.bIncludeLog = Draft.bIncludeLog && Variant.LogChars > 0;

		FMOBugReportFields Properties;
		Properties.Emplace(TEXT("CrashVersion"), TEXT("3"));
		Properties.Emplace(TEXT("CrashGUID"), Directory);
		Properties.Emplace(TEXT("ExecutionGuid"), ReportId.ToString(EGuidFormats::Digits));
		Properties.Emplace(TEXT("CrashType"), TEXT("BugReport"));
		Properties.Emplace(TEXT("ReportKind"), TEXT("bugreport"));
		Properties.Emplace(TEXT("IsEnsure"), TEXT("false"));
		Properties.Emplace(TEXT("IsAssert"), TEXT("false"));
		Properties.Emplace(TEXT("ErrorMessage"), MOBugReportBundle::ClampText(Draft.Title, MaxTitleChars));
		Properties.Emplace(TEXT("UserDescription"), MOBugReportBundle::ClampText(Draft.Description, MaxTextChars));
		Properties.Emplace(TEXT("GameName"), FApp::GetProjectName());
		Properties.Emplace(TEXT("ExecutableName"), FPlatformProcess::ExecutableName(false));
		Properties.Emplace(TEXT("BuildConfiguration"), LexToString(FApp::GetBuildConfiguration()));
		Properties.Emplace(TEXT("EngineVersion"), FEngineVersion::Current().ToString());
		Properties.Emplace(TEXT("BuildVersion"), FApp::GetBuildVersion());
		Properties.Emplace(TEXT("PlatformName"), FPlatformProperties::PlatformName());
		Properties.Emplace(TEXT("MachineId"), FPlatformMisc::GetLoginId());
		Properties.Emplace(TEXT("GameSessionID"), FApp::GetSessionId().ToString());

		TArray<MOBugReportBundle::FFile> Files;
		Files.Add({ TEXT("CrashContext.runtime-xml"), ToUtf8(MOBugReportBundle::BuildContextXml(Properties, GameFields)) });
		Files.Add({ TEXT("BugReport.txt"), ToUtf8(BuildReportText(Effective, ReportId, false)) });
		if (Effective.bIncludeLog && !LogTail.IsEmpty())
		{
			Files.Add({ TEXT("game.log"), ToUtf8(MOBugReportBundle::TailText(LogTail, Variant.LogChars)) });
		}
		if (Effective.bIncludeScreenshot && HasScreenshot())
		{
			Files.Add({ TEXT("Screenshot.jpg"), CapturedScreenshot });
		}

		OutBundle.Reset();
		if (!MOBugReportBundle::Build(Directory, Files, OutBundle, OutError, OutUncompressedSize))
		{
			return false;
		}
		if (OutBundle.Num() <= MaxBundleBytes)
		{
			return true;
		}
	}

	OutError = FString::Printf(TEXT("the report is larger than %d MB even without attachments"), MaxBundleBytes / (1024 * 1024));
	OutBundle.Reset();
	return false;
}

// ============================================================================
// Sending
// ============================================================================

bool UMOBugReportSubsystem::IsAllowedEndpoint(const FString& Url, bool bIsTestOverride)
{
	if (Url.IsEmpty())
	{
		return false;
	}
	for (const TCHAR Character : Url)
	{
		if (FChar::IsWhitespace(Character) || Character < 0x20)
		{
			return false;
		}
	}
	if (Url.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase) && !bIsTestOverride)
	{
		return !ToHostLower(Url).IsEmpty();
	}
	if (bIsTestOverride)
	{
		// The override exists so a test can post to a local receiver; it must never be able to point a shipped game at somebody else's server.
		const FString Host = ToHostLower(Url);
		const bool bLoopback = Host == TEXT("127.0.0.1") || Host == TEXT("localhost") || Host == TEXT("[::1]");
		return bLoopback && (Url.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase) || Url.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase));
	}
	return false;
}

FString UMOBugReportSubsystem::ResolveEndpoint(bool* bOutIsTestOverride)
{
	if (bOutIsTestOverride)
	{
		*bOutIsTestOverride = false;
	}
	const FString Override = CVarEndpointOverride.GetValueOnGameThread();
	if (!Override.IsEmpty())
	{
		if (bOutIsTestOverride)
		{
			*bOutIsTestOverride = true;
		}
		return IsAllowedEndpoint(Override, true) ? Override : FString();
	}

	FString Url;
	GConfig->GetString(TEXT("CrashReportClient"), TEXT("DataRouterUrl"), Url, GEngineIni);
	return IsAllowedEndpoint(Url, false) ? Url : FString();
}

FString UMOBugReportSubsystem::BuildUploadUrl(const FString& BaseUrl, const FString& EngineVersion, const FString& UserId)
{
	return FString::Printf(TEXT("%s%sAppID=CrashReporter&AppVersion=%s&AppEnvironment=Release&UploadType=crashreports&ReportKind=bugreport&UserID=%s"),
		*BaseUrl, BaseUrl.Contains(TEXT("?")) ? TEXT("&") : TEXT("?"),
		*FGenericPlatformHttp::UrlEncode(EngineVersion), *FGenericPlatformHttp::UrlEncode(UserId));
}

FString UMOBugReportSubsystem::GetSavedReportsDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BugReports")));
}

FString UMOBugReportSubsystem::SaveBundleToDisk(const TArray<uint8>& Bundle, const FGuid& ReportId) const
{
	const FString Path = FPaths::Combine(GetSavedReportsDir(), ReportId.ToString(EGuidFormats::Digits).Left(8) + TEXT(".uecrash"));
	return FFileHelper::SaveArrayToFile(Bundle, *Path) ? Path : FString();
}

void UMOBugReportSubsystem::Finish(const FMOBugReportResult& Result)
{
	bSubmitting = false;
	ActiveRequest.Reset();
	ActiveBundle.Reset();
	if (Result.bSent)
	{
		LastSuccessfulSendSeconds = FPlatformTime::Seconds();
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MOBugReport] %s id=%s http=%d: %s%s"), Result.bSent ? TEXT("SENT") : TEXT("NOT SENT"),
		*Result.ReportId.ToString(EGuidFormats::Digits).Left(8), Result.HttpStatus, *Result.Message,
		Result.SavedPath.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("  [saved: %s]"), *Result.SavedPath));

	FMOBugReportFinished Done = MoveTemp(PendingDone);
	PendingDone.Unbind();
	Done.ExecuteIfBound(Result);
}

void UMOBugReportSubsystem::Submit(const FMOBugReportDraft& Draft, FMOBugReportFinished OnDone)
{
	FMOBugReportResult Refusal;
	Refusal.ReportId = FGuid::NewGuid();
	auto Refuse = [&OnDone, &Refusal](const FString& Message)
	{
		Refusal.Message = Message;
		OnDone.ExecuteIfBound(Refusal);
	};

	if (bSubmitting)
	{
		Refuse(TEXT("A report is already being sent. Please wait a moment."));
		return;
	}
	const FString Problem = ValidateDraft(Draft);
	if (!Problem.IsEmpty())
	{
		Refuse(Problem);
		return;
	}
	const double Cooldown = FMath::Max(0.0f, CVarCooldownSeconds.GetValueOnGameThread());
	const double Since = FPlatformTime::Seconds() - LastSuccessfulSendSeconds;
	if (Since < Cooldown)
	{
		Refuse(FString::Printf(TEXT("Your last report was just sent. Please wait %d more seconds before sending another."), FMath::CeilToInt(Cooldown - Since)));
		return;
	}

	FMOBugReportDraft Normalised = Draft;
	if (!GetCategories().Contains(Normalised.Category))
	{
		Normalised.Category = GetCategories().Last();
	}

	const FGuid ReportId = FGuid::NewGuid();
	TArray<uint8> Bundle;
	FString Error;
	if (!BuildBundle(Normalised, ReportId, Bundle, Error))
	{
		Refusal.ReportId = ReportId;
		Refuse(FString::Printf(TEXT("Couldn't build the report (%s)."), *Error));
		return;
	}

	bool bIsTestOverride = false;
	const FString Endpoint = ResolveEndpoint(&bIsTestOverride);

	bSubmitting = true;
	PendingDone = MoveTemp(OnDone);
	ActiveReportId = ReportId;
	ActiveBundle = Bundle;

	if (Endpoint.IsEmpty())
	{
		FMOBugReportResult Result;
		Result.ReportId = ReportId;
		Result.SavedPath = SaveBundleToDisk(Bundle, ReportId);
		Result.Message = Result.SavedPath.IsEmpty()
			? FString(TEXT("Bug reporting isn't set up in this build."))
			: FString::Printf(TEXT("Bug reporting isn't set up in this build. Your report was saved to %s - you can attach that file on Discord."), *Result.SavedPath);
		Finish(Result);
		return;
	}

	const FString Url = BuildUploadUrl(Endpoint, FEngineVersion::Current().ToString(), FString::Printf(TEXT("%s||"), *FPlatformMisc::GetLoginId()));
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));
	Request->SetTimeout(30.0f);
	Request->SetContent(Bundle);
	ActiveRequest = Request;

	UE_LOG(LogMOFramework, Log, TEXT("[MOBugReport] Sending report %s (%d bytes) to %s"), *ReportId.ToString(EGuidFormats::Digits).Left(8), Bundle.Num(), *ToHostLower(Endpoint));

	TWeakObjectPtr<UMOBugReportSubsystem> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda([WeakThis, ReportId](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
	{
		UMOBugReportSubsystem* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		FMOBugReportResult Result;
		Result.ReportId = ReportId;
		Result.HttpStatus = Response.IsValid() ? Response->GetResponseCode() : 0;
		const FString ShortId = ReportId.ToString(EGuidFormats::Digits).Left(8);
		if (bConnected && Response.IsValid() && Result.HttpStatus >= 200 && Result.HttpStatus < 300)
		{
			Result.bSent = true;
			Result.Message = FString::Printf(TEXT("Thank you - your report was sent. Its id is %s."), *ShortId);
			Self->Finish(Result);
			return;
		}

		Result.SavedPath = Self->SaveBundleToDisk(Self->ActiveBundle, ReportId);
		const FString Why = (bConnected && Response.IsValid())
			? FString::Printf(TEXT("The server answered with error %d."), Result.HttpStatus)
			: FString(TEXT("Couldn't reach the bug report server."));
		Result.Message = Result.SavedPath.IsEmpty()
			? Why
			: FString::Printf(TEXT("%s Your report was saved to %s - you can attach that file on Discord."), *Why, *Result.SavedPath);
		Self->Finish(Result);
	});

	if (!Request->ProcessRequest())
	{
		FMOBugReportResult Result;
		Result.ReportId = ReportId;
		Result.SavedPath = SaveBundleToDisk(Bundle, ReportId);
		Result.Message = FString::Printf(TEXT("Couldn't start the upload.%s"),
			Result.SavedPath.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" Your report was saved to %s - you can attach that file on Discord."), *Result.SavedPath));
		Request->OnProcessRequestComplete().Unbind();
		Finish(Result);
	}
}

// ============================================================================
// Console verbs (tests and diagnostics)
// ============================================================================

static FAutoConsoleCommandWithWorldAndArgs GBugReportPreviewCommand(
	TEXT("MO.BugReport.Preview"),
	TEXT("Log the bug report text exactly as the form's preview would show it (no screenshot request, nothing sent)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		UMOBugReportSubsystem* Reports = UMOBugReportSubsystem::Get(World);
		if (!Reports)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOBugReport] Preview: no bug report subsystem in this world"));
			return;
		}
		FMOBugReportDraft Draft;
		Draft.Title = TEXT("Preview");
		Draft.Category = UMOBugReportSubsystem::GetCategories()[0];
		TArray<FString> Lines;
		Reports->BuildReportText(Draft, FGuid::NewGuid(), true).ParseIntoArrayLines(Lines, /*bCullEmpty=*/false);
		for (const FString& Line : Lines)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOBugReport] preview | %s"), *Line);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GBugReportSendTestCommand(
	TEXT("MO.BugReport.SendTest"),
	TEXT("Send a test bug report through the real path. Usage: MO.BugReport.SendTest [-shot] [-delay=SECONDS] [title words]. -shot attaches a screenshot; -delay waits first (for -ExecCmds at startup). Point it at a local receiver with MO.BugReport.EndpointOverride."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UMOBugReportSubsystem* Reports = UMOBugReportSubsystem::Get(World);
		if (!Reports)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOBugReport] SendTest: no bug report subsystem in this world"));
			return;
		}

		bool bShot = false;
		float DelaySeconds = 0.0f;
		FString Title;
		for (const FString& Arg : Args)
		{
			if (Arg.Equals(TEXT("-shot"), ESearchCase::IgnoreCase))
			{
				bShot = true;
			}
			else if (Arg.StartsWith(TEXT("-delay="), ESearchCase::IgnoreCase))
			{
				DelaySeconds = FMath::Clamp(FCString::Atof(*Arg.Mid(7)), 0.0f, 600.0f);
			}
			else
			{
				Title += (Title.IsEmpty() ? TEXT("") : TEXT(" ")) + Arg;
			}
		}

		FMOBugReportDraft Draft;
		Draft.Title = Title.IsEmpty() ? TEXT("MO.BugReport.SendTest") : Title;
		Draft.Description = TEXT("Sent by the MO.BugReport.SendTest console command (automated end-to-end test).");
		Draft.Steps = TEXT("1. Run MO.BugReport.SendTest.");
		Draft.Category = UMOBugReportSubsystem::GetCategories().Last();
		Draft.bIncludeLog = true;
		Draft.bIncludeScreenshot = bShot;

		auto SendNow = [Draft](TWeakObjectPtr<UMOBugReportSubsystem> Weak)
		{
			if (UMOBugReportSubsystem* Subsystem = Weak.Get())
			{
				Subsystem->Submit(Draft, FMOBugReportFinished::CreateLambda([](const FMOBugReportResult& Result)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOBugReport] SendTest result: sent=%d http=%d message=%s"), Result.bSent ? 1 : 0, Result.HttpStatus, *Result.Message);
				}));
			}
		};

		// Capture (if asked) a moment before sending: the picture arrives at the end of the frame, and in the form the player's typing gives it time.
		// -delay=N postpones all of it, so a command given at startup (-ExecCmds) can fire once the host's world is up.
		TWeakObjectPtr<UMOBugReportSubsystem> Weak(Reports);
		auto CaptureThenSend = [SendNow, Weak, bShot]()
		{
			if (!bShot)
			{
				SendNow(Weak);
				return;
			}
			if (UMOBugReportSubsystem* Subsystem = Weak.Get())
			{
				Subsystem->CaptureScreenshot();
			}
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([SendNow, Weak](float)
			{
				SendNow(Weak);
				return false;
			}), 1.0f);
		};
		if (DelaySeconds > 0.0f)
		{
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([CaptureThenSend](float)
			{
				CaptureThenSend();
				return false;
			}), DelaySeconds);
		}
		else
		{
			CaptureThenSend();
		}
	}));
