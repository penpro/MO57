/**
 * =============================================================================
 * MORecipeDatabaseSettings.h - Recipe Database Project Settings
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Project Settings for the recipe database system. Points to the central
 * recipe DataTable and provides O(1) cached lookups by station, category,
 * and building/craftable type. Accessible via Project Settings > Plugins >
 * MO Recipe Database.
 *
 * CACHING ARCHITECTURE:
 * - RecipesByStation: Pre-sorted by EMOCraftingStation enum
 * - BuildingRecipeIds: bIsBuilding=true recipes
 * - CraftableRecipeIds: bIsBuilding=false recipes
 * - RecipesByCategory: Grouped by FName category
 *
 * Caches are lazy-built on first access. Call InvalidateCache() after
 * runtime DataTable modifications.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] CACHE INVALIDATION: If you modify recipes at runtime (adding rows,
 *   changing categories), you MUST call InvalidateCache() or lookups will
 *   return stale data.
 *
 * [2024-02] STATIC CACHES: All cache data is static. Thread-safe lazy init
 *   via EnsureCachesBuilt() but not thread-safe for writes.
 *
 * [2024-02] SYNC LOADING: GetRecipeIcon() loads textures synchronously.
 *   Avoid calling in tight loops.
 *
 * =============================================================================
 * RELATED FILES: MORecipeDefinitionRow.h, MOCraftingSubsystem.h, Recipes.csv
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

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
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Recipe Database"))
class MOFRAMEWORK_API UMORecipeDatabaseSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// UDeveloperSettings overrides — unified under "MOFramework" section in Project Settings.
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("MOFramework"); }
	virtual FName GetSectionName() const override { return TEXT("Recipe Database"); }

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

	// =========================================================================
	// MOD OVERLAY API (mirrors UMOItemDatabaseSettings — see that header for
	// the full rationale; same shape, same semantics, same modder workflow.)
	// =========================================================================

	/** Register or replace a single recipe by ID. Survives InvalidateCache. */
	UFUNCTION(BlueprintCallable, Category="MO|Recipe Database|Mod")
	static void RegisterModRecipe(FName RecipeId, const FMORecipeDefinitionRow& Row);

	/** Merge every FMORecipeDefinitionRow row from SourceTable into the mod overlay. */
	UFUNCTION(BlueprintCallable, Category="MO|Recipe Database|Mod")
	static int32 MergeModRecipeTable(UDataTable* SourceTable);

	/** Drop all mod registrations and invalidate the cache. */
	UFUNCTION(BlueprintCallable, Category="MO|Recipe Database|Mod")
	static void ClearModRecipes();

	/** Diagnostic count for MO.Mod.Status. */
	UFUNCTION(BlueprintCallable, Category="MO|Recipe Database|Mod")
	static int32 GetModRecipeCount();

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

	/**
	 * Mod overlay — rows registered via RegisterModRecipe / MergeModRecipeTable.
	 * Merged on top of base recipes during cache builds and queried first by
	 * GetRecipeDefinition. Mod entries WIN on ID collision.
	 */
	static TMap<FName, FMORecipeDefinitionRow> ModRecipeDefinitions;
};
