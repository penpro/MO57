#include "MOSkillDatabaseSettings.h"
#include "MOFramework.h"

#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

// Static cache members
bool UMOSkillDatabaseSettings::bCachesDirty = true;
TMap<EMOSkillCategory, TArray<FName>> UMOSkillDatabaseSettings::SkillsByCategory;
TWeakObjectPtr<UDataTable> UMOSkillDatabaseSettings::CachedDataTable;

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
	if (!Settings)
	{
		return;
	}

	UDataTable* DataTable = Settings->GetSkillDefinitionsDataTable();
	if (!IsValid(DataTable))
	{
		return;
	}

	OutSkillIds = DataTable->GetRowNames();
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

	UE_LOG(LogMOFramework, Log, TEXT("[MOSkillDatabaseSettings] Cache invalidated"));
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

	const UMOSkillDatabaseSettings* Settings = GetDefault<UMOSkillDatabaseSettings>();
	if (!Settings)
	{
		bCachesDirty = false;
		return;
	}

	UDataTable* DataTable = Settings->GetSkillDefinitionsDataTable();
	if (!IsValid(DataTable))
	{
		bCachesDirty = false;
		return;
	}

	TArray<FName> AllSkillIds = DataTable->GetRowNames();

	for (const FName& SkillId : AllSkillIds)
	{
		const FMOSkillDefinitionRow* Skill = DataTable->FindRow<FMOSkillDefinitionRow>(SkillId, TEXT("BuildCaches"), false);
		if (!Skill)
		{
			continue;
		}

		TArray<FName>& CategorySkills = SkillsByCategory.FindOrAdd(Skill->Category);
		CategorySkills.Add(SkillId);
	}

	bCachesDirty = false;

	UE_LOG(LogMOFramework, Log, TEXT("[MOSkillDatabaseSettings] Cache built: %d skills, %d categories"),
		AllSkillIds.Num(), SkillsByCategory.Num());
}
