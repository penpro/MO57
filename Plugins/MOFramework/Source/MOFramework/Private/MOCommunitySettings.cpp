#include "MOCommunitySettings.h"
#include "MOFramework.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"

namespace
{
	/** TEST ONLY: pretend the OS could not launch a browser, so the clipboard fallback can be exercised (a real launch failure cannot be forced). */
	TAutoConsoleVariable<bool> CVarSimulateBrowserFailure(
		TEXT("MO.BugReport.SimulateBrowserFailure"), false,
		TEXT("TEST ONLY: opening the bug report link behaves as if no browser could be launched (link goes to the clipboard)."), ECVF_Cheat);
}

FString UMOCommunitySettings::GetBugReportUrl()
{
	const UMOCommunitySettings* Settings = GetDefault<UMOCommunitySettings>();
	return Settings ? Settings->BugReportUrl.TrimStartAndEnd() : FString();
}

bool UMOCommunitySettings::OpenBugReportLink(FText& OutMessage)
{
	const FString Url = GetBugReportUrl();
	if (!IsOpenableUrl(Url))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCommunity] Bug report: the configured link '%s' is empty or not an https:// link (Project Settings > MOFramework > Community)"), *Url);
		OutMessage = NSLOCTEXT("MOCommunity", "BugReportNotConfigured", "The bug report link isn't set up yet.");
		return false;
	}

	// LaunchURL reports a failure through Error (empty on success): then fall back to the clipboard, so the player can still paste it into a browser.
	FString Error;
	if (CVarSimulateBrowserFailure.GetValueOnGameThread())
	{
		Error = TEXT("simulated browser failure (MO.BugReport.SimulateBrowserFailure)");
	}
	else
	{
		FPlatformProcess::LaunchURL(*Url, nullptr, &Error);
	}
	if (Error.IsEmpty())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCommunity] Bug report: opened %s in the default browser"), *Url);
		OutMessage = NSLOCTEXT("MOCommunity", "BugReportOpened", "Opening the bug report page in your browser...");
		return true;
	}

	FPlatformApplicationMisc::ClipboardCopy(*Url);
	UE_LOG(LogMOFramework, Warning, TEXT("[MOCommunity] Bug report: could not open a browser (%s); copied %s to the clipboard"), *Error, *Url);
	OutMessage = FText::Format(NSLOCTEXT("MOCommunity", "BugReportCopied", "Couldn't open your browser. The bug report link was copied to your clipboard: {0}"), FText::FromString(Url));
	return false;
}

bool UMOCommunitySettings::IsOpenableUrl(const FString& Url)
{
	static const FString Scheme(TEXT("https://"));
	if (!Url.StartsWith(Scheme, ESearchCase::IgnoreCase) || Url.Len() <= Scheme.Len())
	{
		return false;
	}
	for (const TCHAR Character : Url)
	{
		if (Character <= TEXT(' ') || Character == 0x7F) // whitespace and control characters never belong in a link
		{
			return false;
		}
	}
	// A host must follow the scheme ("https://" + "/path" or "https://?x" is not a link).
	const TCHAR First = Url[Scheme.Len()];
	return First != TEXT('/') && First != TEXT('?') && First != TEXT('#');
}
