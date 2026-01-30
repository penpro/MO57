// Copyright Epic Games, Inc. All Rights Reserved.

#include "MOFramework.h"
#include "MOItemDatabaseSettings.h"
#include "MOPersistenceSettings.h"
#include "MOMedicalDatabaseSettings.h"

// Define the log category
DEFINE_LOG_CATEGORY(LogMOFramework);

#define LOCTEXT_NAMESPACE "FMOFrameworkModule"

void FMOFrameworkModule::StartupModule()
{
	// Validate configuration settings - logs warnings if required settings aren't configured.
	// Skip validation during commandlet runs (cooking, packaging, etc.) to avoid noise.
	if (!IsRunningCommandlet())
	{
		UMOItemDatabaseSettings::ValidateConfiguration();
		UMOPersistenceSettings::ValidateConfiguration();
		UMOMedicalDatabaseSettings::ValidateConfiguration();
	}
}

void FMOFrameworkModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMOFrameworkModule, MOFramework)