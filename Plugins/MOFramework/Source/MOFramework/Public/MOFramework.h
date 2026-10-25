/**
 * =============================================================================
 * MOFramework.h - Plugin Module Definition
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * MOFramework plugin module definition. Declares the log category and
 * module startup/shutdown. Handles preloading of DataTable assets.
 *
 * LOG CATEGORY:
 * Use UE_LOG(LogMOFramework, Log/Warning/Error, ...) for MO-specific logging.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] DATATABLE PRELOAD: PreloadDatabaseTables() forces loading of
 *   soft-referenced DataTables from DeveloperSettings at module startup.
 *   Required for cooker to include these assets in packaged builds.
 *
 * =============================================================================
 * RELATED FILES: MOFramework.cpp
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

// Log category for the MOFramework plugin
MOFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(LogMOFramework, Log, All);

#if defined(MO_SHIPPING_DIAG) && NO_LOGGING
// The MO57ShipDiag target only (Source/MO57ShipDiag.Target.cs, defined by MOFramework.Build.cs for that target): Shipping compiles UE_LOG out
// AND the engine's log file device (NO_LOGGING), so there this module's UE_LOG / UE_CLOG (Log and above) go to MOShipDiagWrite, a plain file
// (-abslog=<path>, else Saved/Logs/MO57ShipDiag.log) -- a Shipping executable that logs MO's own lines, for bugs only Shipping shows.
// Never defined for the MO57 / MO57Editor targets.
MOFRAMEWORK_API void MOShipDiagWrite(const TCHAR* Category, ELogVerbosity::Type Verbosity, const FString& Message);
#undef UE_LOG
#define UE_LOG(CategoryName, Verbosity, Format, ...) \
	do { if (ELogVerbosity::Verbosity <= ELogVerbosity::Log) { MOShipDiagWrite(TEXT(#CategoryName), ELogVerbosity::Verbosity, FString::Printf(Format, ##__VA_ARGS__)); } } while (0)
#undef UE_CLOG
#define UE_CLOG(Condition, CategoryName, Verbosity, Format, ...) \
	do { if (Condition) { UE_LOG(CategoryName, Verbosity, Format, ##__VA_ARGS__); } } while (0)
#endif

class FMOFrameworkModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/**
	 * Force-load all database DataTables at module startup.
	 * This ensures TSoftObjectPtr references in DeveloperSettings are cooked.
	 * Without this, the cooker won't include soft-referenced assets from config files.
	 */
	void PreloadDatabaseTables();

#if WITH_EDITOR
	/** FMOWorldFeatureCook::ModifyCook on the cook's ModifyCookDelegate. */
	FDelegateHandle ModifyCookHandle;
#endif
};
