/**
 * =============================================================================
 * MOBugReport.h - the in-game bug report: collects the game's state, sends it where crash reports already go
 * =============================================================================
 *
 * WHAT IT DOES: the Bug Report panel (UMOBugReportPanel) fills an FMOBugReportDraft and calls UMOBugReportSubsystem::Submit. The subsystem builds ONE
 * bundle in the crash reporter's own "CR1" format (see MOBugReportBundle.h) holding
 *   CrashContext.runtime-xml  the player's words + the game state, as the crash tooling reads it (CrashType=BugReport)
 *   BugReport.txt             the same thing for a human; ALSO exactly what the panel's preview shows (one builder: BuildReportText)
 *   game.log                  the last part of this session's log (optional, sanitised)
 *   Screenshot.jpg            the frame the report was opened on (optional)
 * and POSTs it to [CrashReportClient] DataRouterUrl (Config/DefaultEngine.ini) -- the crash endpoint -- with ReportKind=bugreport in the query. No website
 * change was needed: the site stores whatever arrives there.
 *
 * ADDING WHAT A REPORT CARRIES (extension without modification): a system that has state worth knowing calls
 *     UMOBugReportSubsystem::Get(this)->RegisterContributor("Fishing", [](const UWorld* World, FMOBugReportFields& Out) { Out.Emplace("Rod", ...); });
 * and its rows appear as "Fishing.Rod" in every report and in the preview. Nothing here changes when a system is added. The built-ins (Build, System,
 * Session, Clock, Weather, Player) are registered the same way in Initialize().
 *
 * PRIVACY: automatically collected values and the log are scrubbed of the OS user name, the computer name and C:\Users\<name> paths (MOBugReportBundle::
 * Sanitize); what the player typed is sent as typed (they see it in the preview); the contact line is optional and only sent if they fill it in. The
 * machine id in the query is the same one the crash reporter already sends. One successful send per cooldown (MO.BugReport.CooldownSeconds, 60 s).
 *
 * WHEN THE SEND FAILS (offline, endpoint down): the bundle is saved to Saved/BugReports/<id>.uecrash and the player is told where it is and that the
 * Discord link still works. Nothing is lost silently.
 *
 * TEST SEAMS (console, Cheat): MO.BugReport.Preview, MO.BugReport.SendTest [-shot], MO.BugReport.EndpointOverride <url> (loopback hosts only, http ok),
 * MO.BugReport.CooldownSeconds <n>. Tools/bugreport_receiver.py is the local endpoint the end-to-end test posts to.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MOBugReport.generated.h"

class UWorld;
class IHttpRequest;

/** (name, value) rows a contributor adds; the collector prefixes each name with the contributor's id ("Build.Commit"). */
using FMOBugReportFields = TArray<TPair<FString, FString>>;

/** Adds state to a report. `World` is the game instance's current world (may be null in a menu-only context). Runs on the game thread, at report time. */
using FMOBugReportContributor = TFunction<void(const UWorld* World, FMOBugReportFields& OutFields)>;

/** What the player fills in. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOBugReportDraft
{
	GENERATED_BODY()

	/** One line: what is wrong. Required. */
	UPROPERTY(BlueprintReadWrite, Category="MO|BugReport")
	FString Title;

	/** What happened and what was expected. */
	UPROPERTY(BlueprintReadWrite, Category="MO|BugReport")
	FString Description;

	/** How to make it happen again. */
	UPROPERTY(BlueprintReadWrite, Category="MO|BugReport")
	FString Steps;

	/** One of UMOBugReportSubsystem::GetCategories(). */
	UPROPERTY(BlueprintReadWrite, Category="MO|BugReport")
	FString Category;

	/** Optional: how to reach the player (a Discord name, an email). Sent only when filled in, never scrubbed. */
	UPROPERTY(BlueprintReadWrite, Category="MO|BugReport")
	FString Contact;

	UPROPERTY(BlueprintReadWrite, Category="MO|BugReport")
	bool bIncludeLog = true;

	UPROPERTY(BlueprintReadWrite, Category="MO|BugReport")
	bool bIncludeScreenshot = true;
};

/** What Submit reports back (also for a refusal, so the panel has exactly one path to show a message). */
struct FMOBugReportResult
{
	/** True when the endpoint accepted the report. */
	bool bSent = false;

	/** One or two sentences for the player. */
	FString Message;

	/** Set when the send failed and the bundle was kept on disk. */
	FString SavedPath;

	FGuid ReportId;
	int32 HttpStatus = 0;
};

DECLARE_DELEGATE_OneParam(FMOBugReportFinished, const FMOBugReportResult&);

UCLASS()
class MOFRAMEWORK_API UMOBugReportSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UMOBugReportSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ----- limits (also what the panel's counters show) -----
	static constexpr int32 MaxTitleChars = 120;
	static constexpr int32 MaxTextChars = 4000;       // description, steps
	static constexpr int32 MaxContactChars = 200;
	static constexpr int32 MaxFieldValueChars = 512;  // one collected value
	static constexpr int32 MaxLogChars = 256 * 1024;  // tail of the log that is attached
	static constexpr int32 MaxBundleBytes = 4 * 1024 * 1024;
	static constexpr int32 ScreenshotMaxWidth = 1280;

	/** The categories the panel offers (first one is the default). */
	static const TArray<FString>& GetCategories();

	// ----- what a report carries -----
	/** Add (or replace) a contributor. Its rows appear as "<Id>.<name>". */
	void RegisterContributor(FName Id, FMOBugReportContributor Contributor);
	void UnregisterContributor(FName Id);
	int32 GetNumContributors() const { return Contributors.Num(); }

	/** Run every contributor; names get their "<Id>." prefix, values are scrubbed (user/computer name, profile paths) and capped. */
	void CollectFields(FMOBugReportFields& OutFields) const;

	// ----- the screenshot -----
	/** Ask for a picture of the frame being drawn now (arrives at the end of the frame; stored for the next report). No-op without a drawing viewport. */
	void CaptureScreenshot();
	bool HasScreenshot() const { return CapturedScreenshot.Num() > 0; }
	int32 GetScreenshotBytes() const { return CapturedScreenshot.Num(); }
	void DiscardScreenshot() { CapturedScreenshot.Reset(); }

	/** Attach a ready-made JPEG instead of capturing one (tools, and the size-cap test). */
	void SetScreenshot(TArray<uint8> Jpeg, int32 Width, int32 Height)
	{
		CapturedScreenshot = MoveTemp(Jpeg);
		CapturedScreenshotWidth = Width;
		CapturedScreenshotHeight = Height;
	}

	/**
	 * Downscale (box filter) to at most MaxWidth wide and encode as JPEG. Pure: no engine state beyond the image wrapper module. False on an empty or
	 * inconsistent bitmap.
	 */
	static bool EncodeScreenshotJpeg(int32 Width, int32 Height, const TArray<FColor>& Bitmap, int32 MaxWidth, TArray<uint8>& OutJpeg, int32& OutWidth, int32& OutHeight);

	// ----- building -----
	/** Empty when the draft may be sent; otherwise one sentence saying what is missing. Pure. */
	static FString ValidateDraft(const FMOBugReportDraft& Draft);

	/** BugReport.txt / the panel preview: the player's words, every collected row and the attachment list. One builder, so the preview cannot lie. */
	FString BuildReportText(const FMOBugReportDraft& Draft, const FGuid& ReportId, bool bPreview) const;

	/** The whole upload: CR1 bundle, zlib compressed. `OutUncompressedSize` (optional) is the number MOBugReportBundle::Parse needs. */
	bool BuildBundle(const FMOBugReportDraft& Draft, const FGuid& ReportId, TArray<uint8>& OutBundle, FString& OutError, int32* OutUncompressedSize = nullptr) const;

	// ----- sending -----
	/** True while a report is on its way. */
	bool IsSubmitting() const { return bSubmitting; }

	/**
	 * Validate, build and POST. `OnDone` runs exactly once -- immediately for a refusal (nothing to send, cooldown, already sending), later for the
	 * network result. On failure the bundle is kept in Saved/BugReports.
	 */
	void Submit(const FMOBugReportDraft& Draft, FMOBugReportFinished OnDone);

	/** The upload URL: base + the same query parameters the crash reporter sends, plus ReportKind=bugreport. Pure. */
	static FString BuildUploadUrl(const FString& BaseUrl, const FString& EngineVersion, const FString& UserId);

	/** https only; the test override additionally accepts loopback hosts over http. Pure. */
	static bool IsAllowedEndpoint(const FString& Url, bool bIsTestOverride);

	/** The endpoint to POST to (DataRouterUrl, or the test override). Empty when none is configured or it is not allowed. */
	static FString ResolveEndpoint(bool* bOutIsTestOverride = nullptr);

	/** Where an unsent bundle is kept. */
	static FString GetSavedReportsDir();

private:
	struct FContributorEntry
	{
		FName Id;
		FMOBugReportContributor Fn;
	};

	void RegisterBuiltInContributors();
	void HandleScreenshotCaptured(int32 Width, int32 Height, const TArray<FColor>& Bitmap);
	void ClearScreenshotCaptureBinding();
	FString ReadLogTail() const;
	void Finish(const FMOBugReportResult& Result);
	FString SaveBundleToDisk(const TArray<uint8>& Bundle, const FGuid& ReportId) const;

	TArray<FContributorEntry> Contributors;

	/** JPEG of the frame the report was opened on. */
	TArray<uint8> CapturedScreenshot;
	int32 CapturedScreenshotWidth = 0;
	int32 CapturedScreenshotHeight = 0;
	FDelegateHandle ScreenshotCaptureHandle;

	bool bSubmitting = false;
	double LastSuccessfulSendSeconds = -1.0e9;
	FMOBugReportFinished PendingDone;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveRequest;
	FGuid ActiveReportId;
	TArray<uint8> ActiveBundle;
};
