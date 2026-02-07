#include "MOSkillDatabaseSettings.h"

#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

// Static cache members
bool UMOSkillDatabaseSettings::bCachesDirty = true;
TMap<EMOSkillCategory, TArray<FName>> UMOSkillDatabaseSettings::SkillsByCategory;

UDataTable* UMOSkillDatabaseSettings::GetSkillDefinitionsDataTable() const
{
	return SkillDefinitionsDataTable.LoadSynchronous();
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

	UE_LOG(LogTemp, Log, TEXT("[MOSkillDatabaseSettings] Cache invalidated"));
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

	UE_LOG(LogTemp, Log, TEXT("[MOSkillDatabaseSettings] Cache built: %d skills, %d categories"),
		AllSkillIds.Num(), SkillsByCategory.Num());
}
