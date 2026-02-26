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
};
