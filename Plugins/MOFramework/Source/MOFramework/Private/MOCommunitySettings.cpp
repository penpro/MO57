#include "MOCommunitySettings.h"

FString UMOCommunitySettings::GetBugReportUrl()
{
	const UMOCommunitySettings* Settings = GetDefault<UMOCommunitySettings>();
	return Settings ? Settings->BugReportUrl.TrimStartAndEnd() : FString();
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
