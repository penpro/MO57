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
