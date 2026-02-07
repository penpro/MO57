#include "MORecipeDatabaseSettings.h"

#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

// Static cache members
bool UMORecipeDatabaseSettings::bCachesDirty = true;
TMap<EMOCraftingStation, TArray<FName>> UMORecipeDatabaseSettings::RecipesByStation;
TArray<FName> UMORecipeDatabaseSettings::BuildingRecipeIds;
TArray<FName> UMORecipeDatabaseSettings::CraftableRecipeIds;
TMap<FName, TArray<FName>> UMORecipeDatabaseSettings::RecipesByCategory;

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
	EnsureCachesBuilt();

	if (const TArray<FName>* Found = RecipesByStation.Find(Station))
	{
		OutRecipeIds = *Found;
	}
	else
	{
		OutRecipeIds.Empty();
	}
}

void UMORecipeDatabaseSettings::GetBuildingRecipes(TArray<FName>& OutRecipeIds)
{
	EnsureCachesBuilt();
	OutRecipeIds = BuildingRecipeIds;
}

void UMORecipeDatabaseSettings::GetCraftableRecipes(TArray<FName>& OutRecipeIds)
{
	EnsureCachesBuilt();
	OutRecipeIds = CraftableRecipeIds;
}

void UMORecipeDatabaseSettings::GetRecipesByCategory(FName Category, TArray<FName>& OutRecipeIds)
{
	EnsureCachesBuilt();

	if (const TArray<FName>* Found = RecipesByCategory.Find(Category))
	{
		OutRecipeIds = *Found;
	}
	else
	{
		OutRecipeIds.Empty();
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

void UMORecipeDatabaseSettings::InvalidateCache()
{
	bCachesDirty = true;
	RecipesByStation.Empty();
	BuildingRecipeIds.Empty();
	CraftableRecipeIds.Empty();
	RecipesByCategory.Empty();

	UE_LOG(LogTemp, Log, TEXT("[MORecipeDatabaseSettings] Cache invalidated"));
}

void UMORecipeDatabaseSettings::EnsureCachesBuilt()
{
	if (!bCachesDirty)
	{
		return;
	}

	BuildCaches();
}

void UMORecipeDatabaseSettings::BuildCaches()
{
	// Clear existing caches
	RecipesByStation.Empty();
	BuildingRecipeIds.Empty();
	CraftableRecipeIds.Empty();
	RecipesByCategory.Empty();

	const UMORecipeDatabaseSettings* Settings = GetDefault<UMORecipeDatabaseSettings>();
	if (!Settings)
	{
		bCachesDirty = false;
		return;
	}

	UDataTable* DataTable = Settings->GetRecipeDefinitionsDataTable();
	if (!IsValid(DataTable))
	{
		bCachesDirty = false;
		return;
	}

	// Single pass through all recipes to build all indexes
	TArray<FName> AllRecipeIds = DataTable->GetRowNames();

	// Pre-reserve capacity to avoid reallocations
	BuildingRecipeIds.Reserve(AllRecipeIds.Num() / 4);  // Estimate ~25% are buildings
	CraftableRecipeIds.Reserve(AllRecipeIds.Num());

	for (const FName& RecipeId : AllRecipeIds)
	{
		const FMORecipeDefinitionRow* Recipe = DataTable->FindRow<FMORecipeDefinitionRow>(RecipeId, TEXT("BuildCaches"), false);
		if (!Recipe)
		{
			continue;
		}

		// Index by station type
		TArray<FName>& StationRecipes = RecipesByStation.FindOrAdd(Recipe->RequiredStation);
		StationRecipes.Add(RecipeId);

		// Index building vs craftable
		if (Recipe->bIsBuilding)
		{
			BuildingRecipeIds.Add(RecipeId);
		}
		else
		{
			CraftableRecipeIds.Add(RecipeId);
		}

		// Index by category
		if (!Recipe->Category.IsNone())
		{
			TArray<FName>& CategoryRecipes = RecipesByCategory.FindOrAdd(Recipe->Category);
			CategoryRecipes.Add(RecipeId);
		}
	}

	bCachesDirty = false;

	UE_LOG(LogTemp, Log, TEXT("[MORecipeDatabaseSettings] Cache built: %d recipes, %d buildings, %d craftable, %d stations, %d categories"),
		AllRecipeIds.Num(),
		BuildingRecipeIds.Num(),
		CraftableRecipeIds.Num(),
		RecipesByStation.Num(),
		RecipesByCategory.Num());
}
