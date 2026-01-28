#include "MOSkillDatabaseSettings.h"

#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

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

bool UMOSkillDatabaseSettings::IsConfigured()
{
	const UMOSkillDatabaseSettings* Settings = GetDefault<UMOSkillDatabaseSettings>();
	if (!Settings)
	{
		return false;
	}

	return !Settings->SkillDefinitionsDataTable.IsNull();
}
