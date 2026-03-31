/**
 * MOUISettings.cpp - UI System Developer Settings Implementation
 */

#include "MOUISettings.h"
#include "MOPrimaryGameLayout.h"

TSubclassOf<UMOPrimaryGameLayout> UMOUISettings::GetPrimaryGameLayoutClass()
{
	const UMOUISettings* Settings = GetDefault<UMOUISettings>();
	if (!Settings)
	{
		return nullptr;
	}

	if (Settings->PrimaryGameLayoutClass.IsNull())
	{
		return nullptr;
	}

	// Load synchronously - this is typically called once at player join
	return Settings->PrimaryGameLayoutClass.LoadSynchronous();
}

bool UMOUISettings::IsConfigured()
{
	const UMOUISettings* Settings = GetDefault<UMOUISettings>();
	return Settings && !Settings->PrimaryGameLayoutClass.IsNull();
}
