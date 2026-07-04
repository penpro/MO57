#include "MOBiomeDatabaseSettings.h"
#include "MOFramework.h"
#include "Engine/DataTable.h"

TWeakObjectPtr<UDataTable> UMOBiomeDatabaseSettings::CachedDataTable;

UDataTable* UMOBiomeDatabaseSettings::GetBiomeDefinitionsDataTable() const
{
	if (CachedDataTable.IsValid())
	{
		return CachedDataTable.Get();
	}

	UDataTable* Table = BiomeDefinitionsDataTable.LoadSynchronous();
	if (Table)
	{
		CachedDataTable = Table;
	}
	else
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOBiomeDatabase] BiomeDefinitionsDataTable not configured or failed to load (%s)"),
			*BiomeDefinitionsDataTable.ToString());
	}
	return Table;
}

const FMOBiomeDefinitionRow* UMOBiomeDatabaseSettings::GetBiomeDefinition(FName BiomeId)
{
	const UMOBiomeDatabaseSettings* Settings = GetDefault<UMOBiomeDatabaseSettings>();
	UDataTable* Table = Settings ? Settings->GetBiomeDefinitionsDataTable() : nullptr;
	if (!Table || BiomeId.IsNone())
	{
		return nullptr;
	}
	return Table->FindRow<FMOBiomeDefinitionRow>(BiomeId, TEXT("GetBiomeDefinition"), /*bWarnIfRowMissing=*/false);
}

bool UMOBiomeDatabaseSettings::GetBiomeDefinitionBP(FName BiomeId, FMOBiomeDefinitionRow& OutDefinition)
{
	if (const FMOBiomeDefinitionRow* Row = GetBiomeDefinition(BiomeId))
	{
		OutDefinition = *Row;
		return true;
	}
	return false;
}

void UMOBiomeDatabaseSettings::GetAllBiomeIds(TArray<FName>& OutBiomeIds)
{
	OutBiomeIds.Reset();
	const UMOBiomeDatabaseSettings* Settings = GetDefault<UMOBiomeDatabaseSettings>();
	UDataTable* Table = Settings ? Settings->GetBiomeDefinitionsDataTable() : nullptr;
	if (Table)
	{
		OutBiomeIds = Table->GetRowNames();
	}
}

bool UMOBiomeDatabaseSettings::IsConfigured()
{
	const UMOBiomeDatabaseSettings* Settings = GetDefault<UMOBiomeDatabaseSettings>();
	return Settings && Settings->GetBiomeDefinitionsDataTable() != nullptr;
}
