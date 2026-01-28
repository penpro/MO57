#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MORecipeDefinitionRow.h"

#include "MORecipeDatabaseSettings.generated.h"

class UDataTable;

/**
 * Project Settings entry to point the plugin at a recipe definition DataTable.
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

	/** Get all recipes that can be crafted at a specific station. */
	UFUNCTION(BlueprintCallable, Category="MO|Recipe Database")
	static void GetRecipesForStation(EMOCraftingStation Station, TArray<FName>& OutRecipeIds);

	/** Check if the Recipe Database is properly configured. */
	UFUNCTION(BlueprintCallable, Category="MO|Recipe Database")
	static bool IsConfigured();
};
