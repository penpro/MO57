#include "MOSurvivorJobDatabaseSettings.h"
#include "MOFramework.h"

#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

// Static cache members
bool UMOSurvivorJobDatabaseSettings::bCachesDirty = true;
TMap<FName, TArray<FMOSurvivorJobDefinitionRow>> UMOSurvivorJobDatabaseSettings::JobsByCategory;
TMap<EMOSurvivorJobType, FMOSurvivorJobDefinitionRow> UMOSurvivorJobDatabaseSettings::JobsByType;
TWeakObjectPtr<UDataTable> UMOSurvivorJobDatabaseSettings::CachedDataTable;

UDataTable* UMOSurvivorJobDatabaseSettings::GetJobDefinitionsDataTable() const
{
	// Return cached table if still valid
	if (CachedDataTable.IsValid())
	{
		return CachedDataTable.Get();
	}

	UDataTable* LoadedTable = nullptr;

	// Try loading from soft reference first
	if (!JobDefinitionsDataTable.IsNull())
	{
		LoadedTable = JobDefinitionsDataTable.LoadSynchronous();
		if (LoadedTable)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorJobDatabaseSettings] Loaded DataTable from soft ref: %s (%d rows)"),
				*LoadedTable->GetName(), LoadedTable->GetRowNames().Num());
			CachedDataTable = LoadedTable;
			return LoadedTable;
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorJobDatabaseSettings] Failed to load from soft ref: %s, trying fallback..."),
				*JobDefinitionsDataTable.ToString());
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOSurvivorJobDatabaseSettings] Soft ref is NULL, trying fallback path..."));
	}

	// Try fallback path for packaged builds
	if (!FallbackJobsDataTablePath.IsEmpty())
	{
		LoadedTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *FallbackJobsDataTablePath));
		if (LoadedTable)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorJobDatabaseSettings] Loaded DataTable from fallback path: %s (%d rows)"),
				*LoadedTable->GetName(), LoadedTable->GetRowNames().Num());
			CachedDataTable = LoadedTable;
			return LoadedTable;
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorJobDatabaseSettings] Failed to load from fallback path: %s"),
				*FallbackJobsDataTablePath);
		}
	}

	// Try hardcoded common paths as last resort
	static const TCHAR* CommonPaths[] = {
		TEXT("/MOFramework/Data/DT_SurvivorJobs.DT_SurvivorJobs"),  // Plugin content
		TEXT("/Game/Data/DT_SurvivorJobs.DT_SurvivorJobs"),         // Game content
	};

	for (const TCHAR* Path : CommonPaths)
	{
		LoadedTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, Path));
		if (LoadedTable)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorJobDatabaseSettings] Loaded DataTable from hardcoded path: %s (%d rows)"),
				Path, LoadedTable->GetRowNames().Num());
			CachedDataTable = LoadedTable;
			return LoadedTable;
		}
	}

	UE_LOG(LogMOFramework, Error, TEXT("[MOSurvivorJobDatabaseSettings] Could not load SurvivorJobs DataTable from any source!"));
	return nullptr;
}

const FMOSurvivorJobDefinitionRow* UMOSurvivorJobDatabaseSettings::GetJobDefinition(EMOSurvivorJobType JobType)
{
	if (JobType == EMOSurvivorJobType::None)
	{
		return nullptr;
	}

	EnsureCachesBuilt();

	if (const FMOSurvivorJobDefinitionRow* Found = JobsByType.Find(JobType))
	{
		return Found;
	}

	return nullptr;
}

bool UMOSurvivorJobDatabaseSettings::GetJobDefinitionBP(EMOSurvivorJobType JobType, FMOSurvivorJobDefinitionRow& OutDefinition)
{
	OutDefinition = FMOSurvivorJobDefinitionRow();

	const FMOSurvivorJobDefinitionRow* FoundRow = GetJobDefinition(JobType);
	if (!FoundRow)
	{
		return false;
	}

	OutDefinition = *FoundRow;
	return true;
}

UTexture2D* UMOSurvivorJobDatabaseSettings::GetJobIcon(EMOSurvivorJobType JobType)
{
	const FMOSurvivorJobDefinitionRow* Definition = GetJobDefinition(JobType);
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

FText UMOSurvivorJobDatabaseSettings::GetJobDisplayName(EMOSurvivorJobType JobType)
{
	const FMOSurvivorJobDefinitionRow* Definition = GetJobDefinition(JobType);
	if (!Definition)
	{
		return FText::GetEmpty();
	}

	return Definition->DisplayName;
}

void UMOSurvivorJobDatabaseSettings::GetAllJobDefinitions(TArray<FMOSurvivorJobDefinitionRow>& OutDefinitions)
{
	OutDefinitions.Empty();
	EnsureCachesBuilt();

	for (const auto& Pair : JobsByType)
	{
		OutDefinitions.Add(Pair.Value);
	}

	// Sort by UI priority (higher first)
	OutDefinitions.Sort([](const FMOSurvivorJobDefinitionRow& A, const FMOSurvivorJobDefinitionRow& B)
	{
		return A.UIPriority > B.UIPriority;
	});
}

void UMOSurvivorJobDatabaseSettings::GetJobsByCategory(FName Category, TArray<FMOSurvivorJobDefinitionRow>& OutDefinitions)
{
	OutDefinitions.Empty();
	EnsureCachesBuilt();

	if (const TArray<FMOSurvivorJobDefinitionRow>* Found = JobsByCategory.Find(Category))
	{
		OutDefinitions = *Found;
	}
}

void UMOSurvivorJobDatabaseSettings::GetAllWorkTasks(TArray<FMOSurvivorJobDefinitionRow>& OutDefinitions)
{
	OutDefinitions.Empty();
	EnsureCachesBuilt();

	for (const auto& Pair : JobsByType)
	{
		if (!Pair.Value.bIsCommand)
		{
			OutDefinitions.Add(Pair.Value);
		}
	}

	// Sort by UI priority (higher first)
	OutDefinitions.Sort([](const FMOSurvivorJobDefinitionRow& A, const FMOSurvivorJobDefinitionRow& B)
	{
		return A.UIPriority > B.UIPriority;
	});
}

bool UMOSurvivorJobDatabaseSettings::IsConfigured()
{
	const UMOSurvivorJobDatabaseSettings* Settings = GetDefault<UMOSurvivorJobDatabaseSettings>();
	if (!Settings)
	{
		return false;
	}

	// Check if we can load the table from any source
	UDataTable* Table = Settings->GetJobDefinitionsDataTable();
	return IsValid(Table) && Table->GetRowNames().Num() > 0;
}

void UMOSurvivorJobDatabaseSettings::InvalidateCache()
{
	bCachesDirty = true;
	JobsByCategory.Empty();
	JobsByType.Empty();
	CachedDataTable.Reset();

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorJobDatabaseSettings] Cache invalidated"));
}

void UMOSurvivorJobDatabaseSettings::EnsureCachesBuilt()
{
	if (!bCachesDirty)
	{
		return;
	}

	BuildCaches();
}

void UMOSurvivorJobDatabaseSettings::BuildCaches()
{
	JobsByCategory.Empty();
	JobsByType.Empty();

	const UMOSurvivorJobDatabaseSettings* Settings = GetDefault<UMOSurvivorJobDatabaseSettings>();
	if (!Settings)
	{
		bCachesDirty = false;
		return;
	}

	UDataTable* DataTable = Settings->GetJobDefinitionsDataTable();
	if (!IsValid(DataTable))
	{
		bCachesDirty = false;
		return;
	}

	TArray<FMOSurvivorJobDefinitionRow*> AllRows;
	DataTable->GetAllRows<FMOSurvivorJobDefinitionRow>(TEXT("BuildCaches"), AllRows);

	for (const FMOSurvivorJobDefinitionRow* Row : AllRows)
	{
		if (!Row || Row->JobType == EMOSurvivorJobType::None)
		{
			continue;
		}

		// Index by job type
		JobsByType.Add(Row->JobType, *Row);

		// Index by category
		TArray<FMOSurvivorJobDefinitionRow>& CategoryJobs = JobsByCategory.FindOrAdd(Row->Category);
		CategoryJobs.Add(*Row);
	}

	bCachesDirty = false;

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorJobDatabaseSettings] Cache built: %d jobs, %d categories"),
		JobsByType.Num(), JobsByCategory.Num());
}
