#include "MOItemDatabaseSettings.h"
#include "MOFramework.h"

#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

// Static member initialization
TMap<FName, FMOItemDefinitionRow> UMOItemDatabaseSettings::CachedItemDefinitions;
TMap<FName, FMOItemDefinitionRow> UMOItemDatabaseSettings::ModItemDefinitions;
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

	// Mod overlay wins on ID collision in BOTH editor and packaged paths so
	// modders can override base items the same way in either build. Check
	// before falling through to the regular lookup.
	if (const FMOItemDefinitionRow* ModRow = ModItemDefinitions.Find(ItemDefinitionId))
	{
		OutDefinition = *ModRow;
		return true;
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
	// In editor builds, skip caching entirely to always pick up DataTable changes.
	// Performance impact is negligible for development. The cache-building code
	// below is #else'd out so it isn't compiled (and flagged unreachable) in
	// editor builds — UE5.8/V7 build settings treat unreachable code as an error.
	return;
#else
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
	CachedItemDefinitions.Reserve(RowMap.Num() + ModItemDefinitions.Num());

	for (const auto& Pair : RowMap)
	{
		const FMOItemDefinitionRow* Row = reinterpret_cast<const FMOItemDefinitionRow*>(Pair.Value);
		if (Row)
		{
			CachedItemDefinitions.Add(Pair.Key, *Row);
		}
	}

	// Merge mod overlay on top — mod entries win on ID collision so modders
	// can override base items. Survives every cache rebuild because ModItems
	// is a separate static.
	int32 OverriddenBase = 0;
	for (const auto& Pair : ModItemDefinitions)
	{
		if (CachedItemDefinitions.Contains(Pair.Key))
		{
			++OverriddenBase;
		}
		CachedItemDefinitions.Add(Pair.Key, Pair.Value);
	}

	bCacheBuilt = true;
	if (ModItemDefinitions.Num() > 0)
	{
		UE_LOG(LogMOFramework, Log,
			TEXT("[MOItemDatabase] Built cache: %d base + %d mod (of which %d overrode base) = %d total"),
			RowMap.Num(), ModItemDefinitions.Num(), OverriddenBase, CachedItemDefinitions.Num());
	}
	else
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOItemDatabase] Built cache with %d items"), CachedItemDefinitions.Num());
	}
#endif // !WITH_EDITOR
}

void UMOItemDatabaseSettings::InvalidateCache()
{
	CachedItemDefinitions.Empty();
	bCacheBuilt = false;
	UE_LOG(LogMOFramework, Log,
		TEXT("[MOItemDatabase] Cache invalidated (mod overlay retained: %d items)"),
		ModItemDefinitions.Num());
}

// =============================================================================
// MOD OVERLAY API
// =============================================================================

void UMOItemDatabaseSettings::RegisterModItem(FName ItemId, const FMOItemDefinitionRow& Row)
{
	if (ItemId.IsNone())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOItemDatabase] RegisterModItem refused — empty ItemId"));
		return;
	}

	const bool bWasReplaced = ModItemDefinitions.Contains(ItemId);
	ModItemDefinitions.Add(ItemId, Row);

	// If the live cache is already built, write through so callers see the
	// new row immediately. Without this, the row would only appear after
	// the next InvalidateCache + lookup.
	if (bCacheBuilt)
	{
		CachedItemDefinitions.Add(ItemId, Row);
	}

	UE_LOG(LogMOFramework, Log,
		TEXT("[MOItemDatabase] %s mod item '%s' (display='%s')"),
		bWasReplaced ? TEXT("Replaced") : TEXT("Registered"),
		*ItemId.ToString(),
		*Row.DisplayName.ToString());
}

int32 UMOItemDatabaseSettings::MergeModItemTable(UDataTable* SourceTable)
{
	if (!IsValid(SourceTable))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOItemDatabase] MergeModItemTable: null SourceTable"));
		return 0;
	}

	// Guard the row struct so modders get a clear error rather than a
	// silent garbage-data read if they pass the wrong DataTable type.
	if (SourceTable->GetRowStruct() != FMOItemDefinitionRow::StaticStruct())
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOItemDatabase] MergeModItemTable: '%s' has row struct '%s', expected FMOItemDefinitionRow"),
			*SourceTable->GetName(),
			SourceTable->GetRowStruct() ? *SourceTable->GetRowStruct()->GetName() : TEXT("<null>"));
		return 0;
	}

	const TMap<FName, uint8*>& RowMap = SourceTable->GetRowMap();
	int32 Merged = 0;
	int32 Skipped = 0;

	for (const auto& Pair : RowMap)
	{
		const FMOItemDefinitionRow* Row = reinterpret_cast<const FMOItemDefinitionRow*>(Pair.Value);
		if (!Row)
		{
			++Skipped;
			continue;
		}
		RegisterModItem(Pair.Key, *Row);
		++Merged;
	}

	UE_LOG(LogMOFramework, Log,
		TEXT("[MOItemDatabase] Merged mod table '%s': %d items added, %d skipped (null rows)"),
		*SourceTable->GetName(), Merged, Skipped);

	return Merged;
}

void UMOItemDatabaseSettings::ClearModItems()
{
	const int32 PreviousCount = ModItemDefinitions.Num();
	ModItemDefinitions.Empty();
	InvalidateCache();
	UE_LOG(LogMOFramework, Log,
		TEXT("[MOItemDatabase] Cleared %d mod items + invalidated cache (will reload base on next lookup)"),
		PreviousCount);
}

int32 UMOItemDatabaseSettings::GetModItemCount()
{
	return ModItemDefinitions.Num();
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

bool UMOItemDatabaseSettings::IsItemInForageSeason(FName ItemDefinitionId, int32 SeasonIndex)
{
	FMOItemDefinitionRow ItemDef;
	if (!GetItemDefinition(ItemDefinitionId, ItemDef))
	{
		return true;
	}
	return ItemDef.IsInForageSeason(SeasonIndex);
}
