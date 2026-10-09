/**
 * =============================================================================
 * MOCommunitySettings.h - project-wide links to the game's community (bug reports, ...)
 * =============================================================================
 *
 * WHY: the in-game menu's "Bug Report" button opens a web page. Where that page lives (a Discord invite, an issue tracker) is a decision about the
 * COMMUNITY, not about the code, so it is project config (Project Settings > MOFramework > Community, stored in Config/DefaultGame.ini), never a literal
 * in a widget. Change the link by editing one line of DefaultGame.ini; no rebuild of code is needed for a config change.
 *
 *   [/Script/MOFramework.MOCommunitySettings]
 *   BugReportUrl="https://discord.gg/<invite>"      <- KEEP THE QUOTES: an unquoted // starts a comment in a UE ini value (read back as just "https:")
 *
 * Only https:// links are ever opened (IsOpenableUrl): the value is handed to the operating system's URL handler, and a config typo or a hostile edit must
 * not be able to launch a file:// path or another scheme.
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MOCommunitySettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Community"))
class MOFRAMEWORK_API UMOCommunitySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("MOFramework"); }
	virtual FName GetSectionName() const override { return TEXT("Community"); }

	/** Where the in-game "Bug Report" button sends the player (https only). */
	UPROPERTY(EditAnywhere, Config, Category="Links")
	FString BugReportUrl = TEXT("https://discord.gg/dH3wAkHjt");

	/** The configured bug report link (may be empty or not openable: check with IsOpenableUrl). */
	UFUNCTION(BlueprintPure, Category="MO|Community")
	static FString GetBugReportUrl();

	/**
	 * Open the bug report link in the default browser; if the OS cannot launch one, copy the link to the clipboard instead. `OutMessage` is what to tell
	 * the player in every case (opened / copied / not configured). True only when a browser was launched. The ONE implementation behind the in-game menu's
	 * button and the bug report panel's Discord button.
	 */
	static bool OpenBugReportLink(FText& OutMessage);

	/**
	 * True for a link that is safe to hand to the OS URL handler: starts with "https://", has a host, and contains no whitespace or control characters.
	 * Pure (no engine state): unit-tested.
	 */
	static bool IsOpenableUrl(const FString& Url);
};
