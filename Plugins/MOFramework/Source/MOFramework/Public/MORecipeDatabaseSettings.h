#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MORecipeDefinitionRow.h"

#include "MORecipeDatabaseSettings.generated.h"

class UDataTable;

/**
 * Project Settings entry to point the plugin at a recipe definition DataTable.
 *
 * CACHING ARCHITECTURE:
 * This class maintains several cached indexes for O(1) access patterns:
 * - RecipesByStation: Pre-sorted recipes by crafting station type
 * - BuildingRecipeIds: Recipes marked as buildings (bIsBuilding=true)
 * - CraftableRecipeIds: Recipes NOT marked as buildings
 * - RecipesByCategory: Recipes grouped by FName category
 *
 * Caches are lazily initialized on first access and invalidated via InvalidateCache().
 * Call InvalidateCache() if you modify the DataTable at runtime.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="MO Recipe Database"))
class MOFRAMEWORK_API UMORecipeDatabaseSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// UDeveloperSettings overrides
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("MO Recipe Database"); }

	/** The central DataTable containing FMORecipeDefinitionRow rows. */
	UPROPERTY(EditAnywhere, Config, Category="Database")
	TSoftObjectPtr<UDataTable> RecipeDefinitionsDataTable;

	UDataTable* GetRecipeDefinitionsDataTable() const;

	/** Look up a recipe definition by ID. Returns pointer or nullptr if not found. */
	static const FMORecipeDefinitionRow* GetRecipeDefinition(FName RecipeId);

	/** Look up a recipe definition by ID. Returns true if found. */
	UFUNCTION(BlueprintCallable, Category="MO|Recipe Database", meta=(DisplayName="Get Recipe Definition"))
	static bool GetRecipeDefinitionBP(FName RecipeId, FMORecipeDefinitionRow& OutDefinition);

	/** Get the icon for a recipe (loads synchronously). Returns nullptr if not found. */
	UFUNCTION(BlueprintCallable, Category="MO|Recipe Database")
	static UTexture2D* GetRecipeIcon(FName RecipeId);

	/** Get display name for a recipe. Returns empty text if not found. */
	UFUNCTION(BlueprintCallable, Category="MO|Recipe Database")
	static FText GetRecipeDisplayName(FName RecipeId);

	/** Get all recipe IDs from the database. */
	UFUNCTION(BlueprintCallable, Category="MO|Recipe Database")
	static void GetAllRecipeIds(TArray<FName>& OutRecipeIds);

	/** Get all recipes that can be crafted at a specific station. O(1) cached lookup. */
	UFUNCTION(BlueprintCallable, Category="MO|Recipe Database")
	static void GetRecipesForStation(EMOCraftingStation Station, TArray<FName>& OutRecipeIds);

	/** Get all building recipes (bIsBuilding=true). O(1) cached lookup. */
	UFUNCTION(BlueprintCallable, Category="MO|Recipe Database")
	static void GetBuildingRecipes(TArray<FName>& OutRecipeIds);

	/** Get all craftable recipes (bIsBuilding=false). O(1) cached lookup. */
	UFUNCTION(BlueprintCallable, Category="MO|Recipe Database")
	static void GetCraftableRecipes(TArray<FName>& OutRecipeIds);

	/** Get all recipes in a category. O(1) cached lookup. */
	UFUNCTION(BlueprintCallable, Category="MO|Recipe Database")
	static void GetRecipesByCategory(FName Category, TArray<FName>& OutRecipeIds);

	/** Check if the Recipe Database is properly configured. */
	UFUNCTION(BlueprintCallable, Category="MO|Recipe Database")
	static bool IsConfigured();

	/** Invalidate all cached indexes. Call after modifying the DataTable at runtime. */
	UFUNCTION(BlueprintCallable, Category="MO|Recipe Database")
	static void InvalidateCache();

private:
	/** Ensure caches are built. Thread-safe lazy initialization. */
	static void EnsureCachesBuilt();

	/** Build all cached indexes from DataTable. */
	static void BuildCaches();

	/** Flag indicating caches need rebuild. */
	static bool bCachesDirty;

	/** Recipes indexed by station type. Key=station enum, Value=array of recipe IDs. */
	static TMap<EMOCraftingStation, TArray<FName>> RecipesByStation;

	/** All recipes marked as buildings. */
	static TArray<FName> BuildingRecipeIds;

	/** All recipes NOT marked as buildings (craftable items). */
	static TArray<FName> CraftableRecipeIds;

	/** Recipes indexed by category name. */
	static TMap<FName, TArray<FName>> RecipesByCategory;
};
