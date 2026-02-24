#include "MOItemDatabaseSettings.h"
#include "MOFramework.h"

#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

// Static member initialization
TMap<FName, FMOItemDefinitionRow> UMOItemDatabaseSettings::CachedItemDefinitions;
bool UMOItemDatabaseSettings::bCacheBuilt = false;
TWeakObjectPtr<UDataTable> UMOItemDatabaseSettings::CachedDataTable;
bool UMOItemDatabaseSettings::bDataTableLoggedOnce = false;

UDataTable* UMOItemDatabaseSettings::GetItemDefinitionsDataTable() const
{
	// Return cached table if still valid
	if (CachedDataTable.IsValid())
	{
		return CachedDataTable.Get();
	}

	// Try loading from soft reference first
	if (!ItemDefinitionsDataTable.IsNull())
	{
		UDataTable* Table = ItemDefinitionsDataTable.LoadSynchronous();
		if (Table)
		{
			CachedDataTable = Table;
			// Only log the first time we load
			if (!bDataTableLoggedOnce)
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOItemDatabase] Loaded DataTable from soft ref: %s (%d rows)"),
					*Table->GetName(), Table->GetRowNames().Num());
				bDataTableLoggedOnce = true;
			}
			return Table;
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOItemDatabase] Failed to load from soft ref: %s, trying fallback..."),
				*ItemDefinitionsDataTable.GetLongPackageName());
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOItemDatabase] Soft ref is NULL, trying fallback path..."));
	}

	// Try fallback path for packaged builds
	if (!FallbackItemsDataTablePath.IsEmpty())
	{
		UDataTable* FallbackTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *FallbackItemsDataTablePath));
		if (FallbackTable)
		{
			CachedDataTable = FallbackTable;
			UE_LOG(LogMOFramework, Log, TEXT("[MOItemDatabase] Loaded DataTable from fallback path: %s (%d rows)"),
				*FallbackTable->GetName(), FallbackTable->GetRowNames().Num());
			bDataTableLoggedOnce = true;
			return FallbackTable;
		}
		else
		{
			UE_LOG(LogMOFramework, Error, TEXT("[MOItemDatabase] Failed to load from fallback path: %s"),
				*FallbackItemsDataTablePath);
		}
	}

	// Try hardcoded common paths as last resort
	static const TCHAR* CommonPaths[] = {
		TEXT("/MOFramework/Data/DT_Items.DT_Items"),          // Plugin content
		TEXT("/Game/Data/DT_Items.DT_Items"),                 // Game content
	};

	for (const TCHAR* Path : CommonPaths)
	{
		UDataTable* HardcodedTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, Path));
		if (HardcodedTable)
		{
			CachedDataTable = HardcodedTable;
			UE_LOG(LogMOFramework, Log, TEXT("[MOItemDatabase] Loaded DataTable from hardcoded path: %s (%d rows)"),
				Path, HardcodedTable->GetRowNames().Num());
			bDataTableLoggedOnce = true;
			return HardcodedTable;
		}
	}

	UE_LOG(LogMOFramework, Error, TEXT("[MOItemDatabase] Could not load Items DataTable from any source!"));
	return nullptr;
}

bool UMOItemDatabaseSettings::GetItemDefinition(FName ItemDefinitionId, FMOItemDefinitionRow& OutDefinition)
{
	OutDefinition = FMOItemDefinitionRow();

	if (ItemDefinitionId.IsNone())
	{
		return false;
	}

#if WITH_EDITOR
	// In editor builds, read directly from DataTable to always pick up changes
	const UMOItemDatabaseSettings* Settings = GetDefault<UMOItemDatabaseSettings>();
	if (!Settings)
	{
		return false;
	}

	UDataTable* DataTable = Settings->GetItemDefinitionsDataTable();
	if (!IsValid(DataTable))
	{
		return false;
	}

	const FMOItemDefinitionRow* Row = DataTable->FindRow<FMOItemDefinitionRow>(ItemDefinitionId, TEXT("GetItemDefinition"));
	if (Row)
	{
		OutDefinition = *Row;
		return true;
	}
	return false;
#else
	// In packaged builds, use cache for performance
	BuildCacheIfNeeded();

	// Look up in cache
	const FMOItemDefinitionRow* CachedRow = CachedItemDefinitions.Find(ItemDefinitionId);
	if (CachedRow)
	{
		OutDefinition = *CachedRow;
		return true;
	}

	return false;
#endif
}

void UMOItemDatabaseSettings::BuildCacheIfNeeded()
{
#if WITH_EDITOR
	// In editor builds, skip caching entirely to always pick up DataTable changes
	// Performance impact is negligible for development
	return;
#endif

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
