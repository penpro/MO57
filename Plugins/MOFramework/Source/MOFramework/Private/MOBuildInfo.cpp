/**
 * MOBuildInfo.cpp — Static accessors for build-time git/project metadata.
 */

#include "MOBuildInfo.h"
#include "MOBuildInfoGenerated.h"
#include "GeneralProjectSettings.h"

FString UMOBuildInfo::GetCommitHash()
{
	return FString(MO_BUILD_COMMIT_HASH);
}

FString UMOBuildInfo::GetCommitHashLong()
{
	return FString(MO_BUILD_COMMIT_HASH_LONG);
}

FString UMOBuildInfo::GetBranch()
{
	return FString(MO_BUILD_BRANCH);
}

FString UMOBuildInfo::GetCommitDate()
{
	return FString(MO_BUILD_COMMIT_DATE);
}

bool UMOBuildInfo::IsDirtyBuild()
{
	return MO_BUILD_DIRTY != 0;
}

FString UMOBuildInfo::GetBuildTimestamp()
{
	return FString(MO_BUILD_TIMESTAMP);
}

FString UMOBuildInfo::GetProjectVersion()
{
	// UGeneralProjectSettings is the canonical UE5.7 home for the
	// ProjectVersion config field — populated from DefaultGame.ini's
	// [/Script/EngineSettings.GeneralProjectSettings] section.
	if (const UGeneralProjectSettings* Settings = GetDefault<UGeneralProjectSettings>())
	{
		return Settings->ProjectVersion;
	}
	return TEXT("0.0.0.0");
}

bool UMOBuildInfo::HasCommitInfo()
{
	// Generator script writes "unknown" sentinel when git is unavailable.
	// Check the short hash since it's the most reliable signal.
	return GetCommitHash() != TEXT("unknown");
}

FText UMOBuildInfo::GetDisplayLabel()
{
	const FString Version = GetProjectVersion();

	if (!HasCommitInfo())
	{
		// No git data — show just the project version. Better than displaying
		// "unknown" sentinels along the menu bottom.
		return FText::FromString(FString::Printf(TEXT("v%s"), *Version));
	}

	// Pull commit date apart — full timestamp is too long for a footer.
	// Format from git: "2026-05-25 05:55:33 -0700". Cut at the first space
	// to keep just the date.
	const FString FullDate = GetCommitDate();
	FString DatePart;
	if (!FullDate.Split(TEXT(" "), &DatePart, nullptr))
	{
		DatePart = FullDate;
	}

	const TCHAR* DirtyMarker = IsDirtyBuild() ? TEXT(" (dirty)") : TEXT("");

	// Single-line footer suitable for a strip along the bottom of the main
	// menu. Middle-dot separators are the conventional compact build-info
	// delimiter — visually lighter than pipes, more legible than hyphens.
	const FString Combined = FString::Printf(
		TEXT("v%s  ·  %s @ %s%s  ·  %s"),
		*Version,
		*GetBranch(),
		*GetCommitHash(),
		DirtyMarker,
		*DatePart);

	return FText::FromString(Combined);
}
