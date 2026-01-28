#include "MORecipeDatabaseSettings.h"

#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

UDataTable* UMORecipeDatabaseSettings::GetRecipeDefinitionsDataTable() const
{
	return RecipeDefinitionsDataTable.LoadSynchronous();
}

const FMORecipeDefinitionRow* UMORecipeDatabaseSettings::GetRecipeDefinition(FName RecipeId)
{
	if (RecipeId.IsNone())
	{
		return nullptr;
	}

	const UMORecipeDatabaseSettings* Settings = GetDefault<UMORecipeDatabaseSettings>();
	if (!Settings)
	{
		return nullptr;
	}

	UDataTable* DataTable = Settings->GetRecipeDefinitionsDataTable();
	if (!IsValid(DataTable))
	{
		return nullptr;
	}

	return DataTable->FindRow<FMORecipeDefinitionRow>(RecipeId, TEXT("GetRecipeDefinition"), false);
}

bool UMORecipeDatabaseSettings::GetRecipeDefinitionBP(FName RecipeId, FMORecipeDefinitionRow& OutDefinition)
{
	OutDefinition = FMORecipeDefinitionRow();

	const FMORecipeDefinitionRow* FoundRow = GetRecipeDefinition(RecipeId);
	if (!FoundRow)
	{
		return false;
	}

	OutDefinition = *FoundRow;
	return true;
}

UTexture2D* UMORecipeDatabaseSettings::GetRecipeIcon(FName RecipeId)
{
	const FMORecipeDefinitionRow* Definition = GetRecipeDefinition(RecipeId);
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

FText UMORecipeDatabaseSettings::GetRecipeDisplayName(FName RecipeId)
{
	const FMORecipeDefinitionRow* Definition = GetRecipeDefinition(RecipeId);
	if (!Definition)
	{
		return FText::GetEmpty();
	}

	return Definition->DisplayName;
}

void UMORecipeDatabaseSettings::GetAllRecipeIds(TArray<FName>& OutRecipeIds)
{
	OutRecipeIds.Empty();

	const UMORecipeDatabaseSettings* Settings = GetDefault<UMORecipeDatabaseSettings>();
	if (!Settings)
	{
		return;
	}

	UDataTable* DataTable = Settings->GetRecipeDefinitionsDataTable();
	if (!IsValid(DataTable))
	{
		return;
	}

	OutRecipeIds = DataTable->GetRowNames();
}

void UMORecipeDatabaseSettings::GetRecipesForStation(EMOCraftingStation Station, TArray<FName>& OutRecipeIds)
{
	OutRecipeIds.Empty();

	const UMORecipeDatabaseSettings* Settings = GetDefault<UMORecipeDatabaseSettings>();
	if (!Settings)
	{
		return;
	}

	UDataTable* DataTable = Settings->GetRecipeDefinitionsDataTable();
	if (!IsValid(DataTable))
	{
		return;
	}

	TArray<FName> AllRecipeIds = DataTable->GetRowNames();
	for (const FName& RecipeId : AllRecipeIds)
	{
		const FMORecipeDefinitionRow* Recipe = DataTable->FindRow<FMORecipeDefinitionRow>(RecipeId, TEXT("GetRecipesForStation"), false);
		if (Recipe && Recipe->RequiredStation == Station)
		{
			OutRecipeIds.Add(RecipeId);
		}
	}
}

bool UMORecipeDatabaseSettings::IsConfigured()
{
	const UMORecipeDatabaseSettings* Settings = GetDefault<UMORecipeDatabaseSettings>();
	if (!Settings)
	{
		return false;
	}

	return !Settings->RecipeDefinitionsDataTable.IsNull();
}
