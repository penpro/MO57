#include "MOItemDatabaseSettings.h"
#include "MOFramework.h"

#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

// Static member initialization
TMap<FName, FMOItemDefinitionRow> UMOItemDatabaseSettings::CachedItemDefinitions;
bool UMOItemDatabaseSettings::bCacheBuilt = false;

UDataTable* UMOItemDatabaseSettings::GetItemDefinitionsDataTable() const
{
	if (ItemDefinitionsDataTable.IsNull())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOItemDatabase] DataTable path is NULL - check DefaultGame.ini config"));
		return nullptr;
	}

	UDataTable* Table = ItemDefinitionsDataTable.LoadSynchronous();
	if (!Table)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOItemDatabase] Failed to load DataTable from path: %s"),
			*ItemDefinitionsDataTable.GetLongPackageName());
	}

	return Table;
}

bool UMOItemDatabaseSettings::GetItemDefinition(FName ItemDefinitionId, FMOItemDefinitionRow& OutDefinition)
{
	OutDefinition = FMOItemDefinitionRow();

	if (ItemDefinitionId.IsNone())
	{
		return false;
	}

	// Build cache if needed
	BuildCacheIfNeeded();

	// Look up in cache
	const FMOItemDefinitionRow* CachedRow = CachedItemDefinitions.Find(ItemDefinitionId);
	if (CachedRow)
	{
		OutDefinition = *CachedRow;
		return true;
	}

	return false;
}

void UMOItemDatabaseSettings::BuildCacheIfNeeded()
{
	if (bCacheBuilt)
	{
		return;
	}

	const UMOItemDatabaseSettings* Settings = GetDefault<UMOItemDatabaseSettings>();
	if (!Settings)
	{
		bCacheBuilt = true; // Mark as built to prevent repeated attempts
		return;
	}

	UDataTable* DataTable = Settings->GetItemDefinitionsDataTable();
	if (!IsValid(DataTable))
	{
		bCacheBuilt = true;
		return;
	}

	// Build the cache from all rows
	const TMap<FName, uint8*>& RowMap = DataTable->GetRowMap();
	CachedItemDefinitions.Reserve(RowMap.Num());

	for (const auto& Pair : RowMap)
	{
		const FMOItemDefinitionRow* Row = reinterpret_cast<const FMOItemDefinitionRow*>(Pair.Value);
		if (Row)
		{
			CachedItemDefinitions.Add(Pair.Key, *Row);
		}
	}

	bCacheBuilt = true;
	UE_LOG(LogMOFramework, Log, TEXT("[MOItemDatabase] Built cache with %d items"), CachedItemDefinitions.Num());
}

void UMOItemDatabaseSettings::InvalidateCache()
{
	CachedItemDefinitions.Empty();
	bCacheBuilt = false;
	UE_LOG(LogMOFramework, Log, TEXT("[MOItemDatabase] Cache invalidated"));
}

UTexture2D* UMOItemDatabaseSettings::GetItemIconSmall(FName ItemDefinitionId)
{
	FMOItemDefinitionRow Definition;
	if (!GetItemDefinition(ItemDefinitionId, Definition))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOItemDatabase] GetItemIconSmall: Item '%s' not found in database"),
			*ItemDefinitionId.ToString());
		return nullptr;
	}

	if (Definition.UI.IconSmall.IsNull())
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOItemDatabase] GetItemIconSmall: Item '%s' has no IconSmall set"),
			*ItemDefinitionId.ToString());
		return nullptr;
	}

	UTexture2D* Icon = Definition.UI.IconSmall.LoadSynchronous();
	if (!Icon)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOItemDatabase] GetItemIconSmall: Failed to load icon '%s' for item '%s'"),
			*Definition.UI.IconSmall.GetLongPackageName(), *ItemDefinitionId.ToString());
	}

	return Icon;
}

UTexture2D* UMOItemDatabaseSettings::GetItemIconLarge(FName ItemDefinitionId)
{
	FMOItemDefinitionRow Definition;
	if (!GetItemDefinition(ItemDefinitionId, Definition))
	{
		return nullptr;
	}

	if (Definition.UI.IconLarge.IsNull())
	{
		return nullptr;
	}

	return Definition.UI.IconLarge.LoadSynchronous();
}

FText UMOItemDatabaseSettings::GetItemDisplayName(FName ItemDefinitionId)
{
	FMOItemDefinitionRow Definition;
	if (!GetItemDefinition(ItemDefinitionId, Definition))
	{
		return FText::GetEmpty();
	}

	return Definition.DisplayName;
}

bool UMOItemDatabaseSettings::IsConfigured()
{
	const UMOItemDatabaseSettings* Settings = GetDefault<UMOItemDatabaseSettings>();
	if (!Settings)
	{
		return false;
	}

	return !Settings->ItemDefinitionsDataTable.IsNull();
}

void UMOItemDatabaseSettings::ValidateConfiguration()
{
	if (!IsConfigured())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOFramework] Item Database not configured. Set 'ItemDefinitionsDataTable' in Project Settings > Plugins > MO Item Database for inventory/item features to work."));
	}
}
