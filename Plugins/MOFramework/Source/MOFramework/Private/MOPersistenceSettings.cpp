#include "MOPersistenceSettings.h"
#include "MOFramework.h"

#include "GameFramework/Pawn.h"

TSubclassOf<APawn> UMOPersistenceSettings::GetDefaultPersistedPawnClass()
{
	const UMOPersistenceSettings* Settings = GetDefault<UMOPersistenceSettings>();
	if (!Settings)
	{
		return nullptr;
	}

	return Settings->DefaultPersistedPawnClass.LoadSynchronous();
}

bool UMOPersistenceSettings::IsConfigured()
{
	const UMOPersistenceSettings* Settings = GetDefault<UMOPersistenceSettings>();
	if (!Settings)
	{
		return false;
	}

	return !Settings->DefaultPersistedPawnClass.IsNull();
}

void UMOPersistenceSettings::ValidateConfiguration()
{
	if (!IsConfigured())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOFramework] Persistence fallback pawn not configured. Set 'DefaultPersistedPawnClass' in Project Settings > Plugins > MO Persistence to prevent data loss if Blueprint pawns are renamed/moved."));
	}
}
