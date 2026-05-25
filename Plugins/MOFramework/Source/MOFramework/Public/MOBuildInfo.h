/**
 * =============================================================================
 * MOBuildInfo.h - Runtime accessor for build-time git/project metadata
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 *
 * PURPOSE:
 * Static helper that exposes build-time info (commit hash, branch, project
 * version) to C++ and Blueprint. The main menu shows GetDisplayLabel() so
 * QA / crash reports can tie a player's build to a specific commit.
 *
 * GENERATION:
 * The git fields come from MOBuildInfoGenerated.h, which Tools/Update-BuildInfo.ps1
 * overwrites on every build (hooked via MOFramework.Build.cs PreBuildSteps).
 * The project version comes from FApp::GetProjectVersion() (DefaultGame.ini).
 *
 * BEHAVIOR ON MISSING GIT:
 * If git is unavailable at build time the generator leaves "unknown" sentinel
 * values in the header. GetDisplayLabel() detects that and returns just the
 * project version, so the UI label degrades gracefully.
 *
 * =============================================================================
 * RELATED FILES:
 *   - MOBuildInfoGenerated.h  (auto-generated, do not edit)
 *   - Tools/Update-BuildInfo.ps1  (generator)
 *   - MOFramework.Build.cs        (PreBuildSteps hook)
 *   - MOMainMenuWidget            (consumer)
 * LAST UPDATED: 2026-05-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MOBuildInfo.generated.h"

/**
 * BlueprintFunctionLibrary so WBPs can call these directly without needing
 * a stateful object. All accessors are pure — values are baked at compile
 * time from MOBuildInfoGenerated.h (which the pre-build script overwrites)
 * plus FApp::GetProjectVersion() (which reads DefaultGame.ini).
 */
UCLASS()
class MOFRAMEWORK_API UMOBuildInfo : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Short git commit hash at build time (e.g. "f1aef40"). "unknown" if generator didn't run. */
	UFUNCTION(BlueprintPure, Category="MO|Build")
	static FString GetCommitHash();

	/** Full 40-char git SHA. "unknown" if generator didn't run. */
	UFUNCTION(BlueprintPure, Category="MO|Build")
	static FString GetCommitHashLong();

	/** Git branch name at build time (e.g. "master"). */
	UFUNCTION(BlueprintPure, Category="MO|Build")
	static FString GetBranch();

	/** ISO 8601 timestamp of the HEAD commit (e.g. "2026-05-25 05:55:33 -0700"). */
	UFUNCTION(BlueprintPure, Category="MO|Build")
	static FString GetCommitDate();

	/** True if the working tree had uncommitted changes when this build was compiled. */
	UFUNCTION(BlueprintPure, Category="MO|Build")
	static bool IsDirtyBuild();

	/** ISO 8601 timestamp the BuildInfo header was generated. Build time, not commit time. */
	UFUNCTION(BlueprintPure, Category="MO|Build")
	static FString GetBuildTimestamp();

	/** ProjectVersion from DefaultGame.ini (e.g. "0.0.0.1"). */
	UFUNCTION(BlueprintPure, Category="MO|Build")
	static FString GetProjectVersion();

	/**
	 * Pre-formatted single-line label suitable for a footer strip along the
	 * bottom of the main menu.
	 * Example:
	 *   "v0.0.0.1  ·  master @ f1aef40 (dirty)  ·  2026-05-25"
	 *
	 * If commit info isn't available (generator didn't run), returns just
	 * "v0.0.0.1" so the footer doesn't show "unknown" sentinels.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Build")
	static FText GetDisplayLabel();

	/** True if the generated header has real git data (not "unknown" sentinels). */
	UFUNCTION(BlueprintPure, Category="MO|Build")
	static bool HasCommitInfo();
};
