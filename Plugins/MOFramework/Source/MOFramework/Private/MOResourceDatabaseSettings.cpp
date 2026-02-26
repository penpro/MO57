#include "MOResourceDatabaseSettings.h"
#include "MOFramework.h"
#include "Engine/DataTable.h"

UDataTable* UMOResourceDatabaseSettings::GetResourceDefinitionsDataTable() const
{
	return ResourceDefinitionsDataTable.LoadSynchronous();
}

const FMOResourceNodeDefinitionRow* UMOResourceDatabaseSettings::GetResourceDefinition(FName ResourceNodeId)
{
	if (ResourceNodeId.IsNone())
	{
		return nullptr;
	}

	const UMOResourceDatabaseSettings* Settings = GetDefault<UMOResourceDatabaseSettings>();
	if (!Settings)
	{
		return nullptr;
	}

	UDataTable* DataTable = Settings->GetResourceDefinitionsDataTable();
	if (!DataTable)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOResourceDB] Resource DataTable not configured in Project Settings"));
		return nullptr;
	}

	static const FString ContextString(TEXT("UMOResourceDatabaseSettings::GetResourceDefinition"));
	return DataTable->FindRow<FMOResourceNodeDefinitionRow>(ResourceNodeId, ContextString);
}

bool UMOResourceDatabaseSettings::IsConfigured()
{
	const UMOResourceDatabaseSettings* Settings = GetDefault<UMOResourceDatabaseSettings>();
	return Settings && !Settings->ResourceDefinitionsDataTable.IsNull();
}
