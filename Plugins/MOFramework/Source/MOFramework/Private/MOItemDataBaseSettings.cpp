#include "MOItemDatabaseSettings.h"
#include "MOFramework.h"

#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

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

	const FMOItemDefinitionRow* FoundRow = DataTable->FindRow<FMOItemDefinitionRow>(ItemDefinitionId, TEXT("GetItemDefinition"), false);
	if (!FoundRow)
	{
		return false;
	}

	OutDefinition = *FoundRow;
	return true;
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
