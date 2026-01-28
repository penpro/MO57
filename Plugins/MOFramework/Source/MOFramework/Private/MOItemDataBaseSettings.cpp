#include "MOItemDatabaseSettings.h"
#include "MOFramework.h"

#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

UDataTable* UMOItemDatabaseSettings::GetItemDefinitionsDataTable() const
{
	return ItemDefinitionsDataTable.LoadSynchronous();
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
		return nullptr;
	}

	if (Definition.UI.IconSmall.IsNull())
	{
		return nullptr;
	}

	return Definition.UI.IconSmall.LoadSynchronous();
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
