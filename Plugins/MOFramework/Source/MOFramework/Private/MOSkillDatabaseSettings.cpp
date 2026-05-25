#include "MOSkillDatabaseSettings.h"
#include "MOFramework.h"

#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

// Static cache members
bool UMOSkillDatabaseSettings::bCachesDirty = true;
TMap<EMOSkillCategory, TArray<FName>> UMOSkillDatabaseSettings::SkillsByCategory;
TWeakObjectPtr<UDataTable> UMOSkillDatabaseSettings::CachedDataTable;
TMap<FName, FMOSkillDefinitionRow> UMOSkillDatabaseSettings::ModSkillDefinitions;

UDataTable* UMOSkillDatabaseSettings::GetSkillDefinitionsDataTable() const
{
	// Return cached table if still valid
	if (CachedDataTable.IsValid())
	{
		return CachedDataTable.Get();
	}

	UDataTable* LoadedTable = nullptr;

	// Try loading from soft reference first
	if (!SkillDefinitionsDataTable.IsNull())
	{
		LoadedTable = SkillDefinitionsDataTable.LoadSynchronous();
		if (LoadedTable)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOSkillDatabaseSettings] Loaded DataTable from soft ref: %s (%d rows)"),
				*LoadedTable->GetName(), LoadedTable->GetRowNames().Num());
			CachedDataTable = LoadedTable;
			return LoadedTable;
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSkillDatabaseSettings] Failed to load from soft ref: %s, trying fallback..."),
				*SkillDefinitionsDataTable.ToString());
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSkillDatabaseSettings] Soft ref is NULL, trying fallback path..."));
	}

	// Try fallback path for packaged builds
	if (!FallbackSkillsDataTablePath.IsEmpty())
	{
		LoadedTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *FallbackSkillsDataTablePath));
		if (LoadedTable)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOSkillDatabaseSettings] Loaded DataTable from fallback path: %s (%d rows)"),
				*LoadedTable->GetName(), LoadedTable->GetRowNames().Num());
			CachedDataTable = LoadedTable;
			return LoadedTable;
		}
		else
		{
			UE_LOG(LogMOFramework, Error, TEXT("[MOSkillDatabaseSettings] Failed to load from fallback path: %s"),
				*FallbackSkillsDataTablePath);
		}
	}

	// Try hardcoded common paths as last resort
	static const TCHAR* CommonPaths[] = {
		TEXT("/MOFramework/Data/DT_Skills.DT_Skills"),          // Plugin content
		TEXT("/Game/Data/DT_Skills.DT_Skills"),                 // Game content
		TEXT("/Script/MOFramework.DT_Skills"),                  // Script path
	};

	for (const TCHAR* Path : CommonPaths)
	{
		LoadedTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, Path));
		if (LoadedTable)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOSkillDatabaseSettings] Loaded DataTable from hardcoded path: %s (%d rows)"),
				Path, LoadedTable->GetRowNames().Num());
			CachedDataTable = LoadedTable;
			return LoadedTable;
		}
	}

	UE_LOG(LogMOFramework, Error, TEXT("[MOSkillDatabaseSettings] Could not load Skills DataTable from any source!"));
	return nullptr;
}

const FMOSkillDefinitionRow* UMOSkillDatabaseSettings::GetSkillDefinition(FName SkillId)
{
	if (SkillId.IsNone())
	{
		return nullptr;
	}

	// Mod overlay wins on collision — same rule as items / recipes.
	if (const FMOSkillDefinitionRow* ModRow = ModSkillDefinitions.Find(SkillId))
	{
		return ModRow;
	}

	const UMOSkillDatabaseSettings* Settings = GetDefault<UMOSkillDatabaseSettings>();
	if (!Settings)
	{
		return nullptr;
	}

	UDataTable* DataTable = Settings->GetSkillDefinitionsDataTable();
	if (!IsValid(DataTable))
	{
		return nullptr;
	}

	return DataTable->FindRow<FMOSkillDefinitionRow>(SkillId, TEXT("GetSkillDefinition"), false);
}

bool UMOSkillDatabaseSettings::GetSkillDefinitionBP(FName SkillId, FMOSkillDefinitionRow& OutDefinition)
{
	OutDefinition = FMOSkillDefinitionRow();

	const FMOSkillDefinitionRow* FoundRow = GetSkillDefinition(SkillId);
	if (!FoundRow)
	{
		return false;
	}

	OutDefinition = *FoundRow;
	return true;
}

UTexture2D* UMOSkillDatabaseSettings::GetSkillIcon(FName SkillId)
{
	const FMOSkillDefinitionRow* Definition = GetSkillDefinition(SkillId);
	if (!Definition)
	{
		return nullptr;
	}

	if (Definition->Icon.IsNull())
	{
		return nullptr;
	}

	return Definition->Icon.LoadSynchronous();
}

FText UMOSkillDatabaseSettings::GetSkillDisplayName(FName SkillId)
{
	const FMOSkillDefinitionRow* Definition = GetSkillDefinition(SkillId);
	if (!Definition)
	{
		return FText::GetEmpty();
	}

	return Definition->DisplayName;
}

void UMOSkillDatabaseSettings::GetAllSkillIds(TArray<FName>& OutSkillIds)
{
	OutSkillIds.Empty();

	const UMOSkillDatabaseSettings* Settings = GetDefault<UMOSkillDatabaseSettings>();
	if (Settings)
	{
		if (UDataTable* DataTable = Settings->GetSkillDefinitionsDataTable())
		{
			OutSkillIds = DataTable->GetRowNames();
		}
	}

	for (const auto& Pair : ModSkillDefinitions)
	{
		OutSkillIds.AddUnique(Pair.Key);
	}
}

void UMOSkillDatabaseSettings::GetSkillsByCategory(EMOSkillCategory Category, TArray<FName>& OutSkillIds)
{
	EnsureCachesBuilt();

	if (const TArray<FName>* Found = SkillsByCategory.Find(Category))
	{
		OutSkillIds = *Found;
	}
	else
	{
		OutSkillIds.Empty();
	}
}

bool UMOSkillDatabaseSettings::IsConfigured()
{
	const UMOSkillDatabaseSettings* Settings = GetDefault<UMOSkillDatabaseSettings>();
	if (!Settings)
	{
		return false;
	}

	return !Settings->SkillDefinitionsDataTable.IsNull();
}

void UMOSkillDatabaseSettings::InvalidateCache()
{
	bCachesDirty = true;
	SkillsByCategory.Empty();
	CachedDataTable.Reset();

	UE_LOG(LogMOFramework, Log,
		TEXT("[MOSkillDatabaseSettings] Cache invalidated (mod overlay retained: %d skills)"),
		ModSkillDefinitions.Num());
}

void UMOSkillDatabaseSettings::EnsureCachesBuilt()
{
	if (!bCachesDirty)
	{
		return;
	}

	BuildCaches();
}

void UMOSkillDatabaseSettings::BuildCaches()
{
	SkillsByCategory.Empty();

	TSet<FName> VisitedIds;

	auto IndexRow = [&VisitedIds](FName SkillId, const FMOSkillDefinitionRow* Skill)
	{
		if (!Skill || SkillId.IsNone()) return;
		if (VisitedIds.Contains(SkillId)) return;  // mod already covered
		VisitedIds.Add(SkillId);

		TArray<FName>& CategorySkills = SkillsByCategory.FindOrAdd(Skill->Category);
		CategorySkills.Add(SkillId);
	};

	// Mod overlay first so mod-wins-over-base.
	for (const auto& Pair : ModSkillDefinitions)
	{
		IndexRow(Pair.Key, &Pair.Value);
	}

	const UMOSkillDatabaseSettings* Settings = GetDefault<UMOSkillDatabaseSettings>();
	UDataTable* DataTable = Settings ? Settings->GetSkillDefinitionsDataTable() : nullptr;

	int32 BaseSkillCount = 0;
	if (IsValid(DataTable))
	{
		TArray<FName> AllSkillIds = DataTable->GetRowNames();
		BaseSkillCount = AllSkillIds.Num();

		for (const FName& SkillId : AllSkillIds)
		{
			const FMOSkillDefinitionRow* Skill = DataTable->FindRow<FMOSkillDefinitionRow>(SkillId, TEXT("BuildCaches"), false);
			IndexRow(SkillId, Skill);
		}
	}

	bCachesDirty = false;

	if (ModSkillDefinitions.Num() > 0)
	{
		UE_LOG(LogMOFramework, Log,
			TEXT("[MOSkillDatabaseSettings] Cache built: %d base + %d mod = %d total skills, %d categories"),
			BaseSkillCount, ModSkillDefinitions.Num(), VisitedIds.Num(), SkillsByCategory.Num());
	}
	else
	{
		UE_LOG(LogMOFramework, Log,
			TEXT("[MOSkillDatabaseSettings] Cache built: %d skills, %d categories"),
			BaseSkillCount, SkillsByCategory.Num());
	}
}

// =============================================================================
// MOD OVERLAY API
// =============================================================================

void UMOSkillDatabaseSettings::RegisterModSkill(FName SkillId, const FMOSkillDefinitionRow& Row)
{
	if (SkillId.IsNone())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSkillDatabaseSettings] RegisterModSkill refused — empty SkillId"));
		return;
	}

	const bool bWasReplaced = ModSkillDefinitions.Contains(SkillId);
	ModSkillDefinitions.Add(SkillId, Row);
	bCachesDirty = true;

	UE_LOG(LogMOFramework, Log,
		TEXT("[MOSkillDatabaseSettings] %s mod skill '%s' (display='%s')"),
		bWasReplaced ? TEXT("Replaced") : TEXT("Registered"),
		*SkillId.ToString(),
		*Row.DisplayName.ToString());
}

int32 UMOSkillDatabaseSettings::MergeModSkillTable(UDataTable* SourceTable)
{
	if (!IsValid(SourceTable))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSkillDatabaseSettings] MergeModSkillTable: null SourceTable"));
		return 0;
	}

	if (SourceTable->GetRowStruct() != FMOSkillDefinitionRow::StaticStruct())
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOSkillDatabaseSettings] MergeModSkillTable: '%s' has row struct '%s', expected FMOSkillDefinitionRow"),
			*SourceTable->GetName(),
			SourceTable->GetRowStruct() ? *SourceTable->GetRowStruct()->GetName() : TEXT("<null>"));
		return 0;
	}

	const TMap<FName, uint8*>& RowMap = SourceTable->GetRowMap();
	int32 Merged = 0;
	int32 Skipped = 0;

	for (const auto& Pair : RowMap)
	{
		const FMOSkillDefinitionRow* Row = reinterpret_cast<const FMOSkillDefinitionRow*>(Pair.Value);
		if (!Row)
		{
			++Skipped;
			continue;
		}
		RegisterModSkill(Pair.Key, *Row);
		++Merged;
	}

	UE_LOG(LogMOFramework, Log,
		TEXT("[MOSkillDatabaseSettings] Merged mod table '%s': %d skills added, %d skipped"),
		*SourceTable->GetName(), Merged, Skipped);

	return Merged;
}

void UMOSkillDatabaseSettings::ClearModSkills()
{
	const int32 PreviousCount = ModSkillDefinitions.Num();
	ModSkillDefinitions.Empty();
	InvalidateCache();
	UE_LOG(LogMOFramework, Log,
		TEXT("[MOSkillDatabaseSettings] Cleared %d mod skills"),
		PreviousCount);
}

int32 UMOSkillDatabaseSettings::GetModSkillCount()
{
	return ModSkillDefinitions.Num();
}
