#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MOItemDefinitionRow.h"
#include "MORecipeDefinitionRow.h"
#include "MODataImportCommandlet.generated.h"

/**
 * Commandlet for importing item and recipe data from CSV files.
 *
 * Usage:
 *   UE5Editor.exe ProjectName -run=MODataImport -items=Path/To/Items.csv -recipes=Path/To/Recipes.csv
 *
 * Or call ImportFromCSV() directly from Editor Utility Blueprints.
 *
 * CSV Format for Items:
 *   RowName,DisplayName,ItemType,Rarity,MaxStackSize,Weight,bConsumable,bIsTool,ToolType,ToolQuality,MaxDurability,Calories,Water,Protein,Carbs,Fat,Fiber,Tags
 *
 * CSV Format for Recipes:
 *   RowName,DisplayName,CraftTime,Station,SkillId,SkillLevel,SkillXP,Category,bRequiresDiscovery,Ingredients,Outputs,Tools
 *
 * Array fields use pipe (|) delimiter: "stone:2|stick:1"
 * Key-value pairs use colon (:) delimiter: "itemId:quantity" or "itemId:quantity:chance"
 */
UCLASS()
class MOFRAMEWORK_API UMODataImportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UMODataImportCommandlet();

	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface

	/**
	 * Import items from a CSV file into the item DataTable.
	 * @param CSVFilePath Absolute or project-relative path to CSV file
	 * @param bClearExisting If true, clears existing rows before import
	 * @return Number of rows imported, -1 on error
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Data Import")
	static int32 ImportItemsFromCSV(const FString& CSVFilePath, bool bClearExisting = false);

	/**
	 * Import recipes from a CSV file into the recipe DataTable.
	 * @param CSVFilePath Absolute or project-relative path to CSV file
	 * @param bClearExisting If true, clears existing rows before import
	 * @return Number of rows imported, -1 on error
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Data Import")
	static int32 ImportRecipesFromCSV(const FString& CSVFilePath, bool bClearExisting = false);

	/**
	 * Import all CSVs from a directory (looks for Items.csv and Recipes.csv).
	 * @param DirectoryPath Path to directory containing CSV files
	 * @param bClearExisting If true, clears existing rows before import
	 * @return Total number of rows imported
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Data Import")
	static int32 ImportAllFromDirectory(const FString& DirectoryPath, bool bClearExisting = false);

	/**
	 * Export current item DataTable to CSV.
	 * @param CSVFilePath Output file path
	 * @return True if export succeeded
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Data Import")
	static bool ExportItemsToCSV(const FString& CSVFilePath);

	/**
	 * Export current recipe DataTable to CSV.
	 * @param CSVFilePath Output file path
	 * @return True if export succeeded
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Data Import")
	static bool ExportRecipesToCSV(const FString& CSVFilePath);

private:
	/** Parse a CSV file into rows. Returns false on error. */
	static bool ParseCSVFile(const FString& FilePath, TArray<TArray<FString>>& OutRows, TArray<FString>& OutHeaders);

	/** Parse a pipe-delimited array string. */
	static TArray<FString> ParsePipeDelimitedArray(const FString& Input);

	/** Parse an item row from CSV columns. */
	static bool ParseItemRow(const TArray<FString>& Headers, const TArray<FString>& Values, FName RowName, FMOItemDefinitionRow& OutRow);

	/** Parse a recipe row from CSV columns. */
	static bool ParseRecipeRow(const TArray<FString>& Headers, const TArray<FString>& Values, FName RowName, FMORecipeDefinitionRow& OutRow);

	/** Get column index by header name (case-insensitive). Returns -1 if not found. */
	static int32 GetColumnIndex(const TArray<FString>& Headers, const FString& ColumnName);

	/** Safely get a string value from columns. */
	static FString GetColumnValue(const TArray<FString>& Values, int32 Index);

	/** Parse EMOItemType from string. */
	static EMOItemType ParseItemType(const FString& TypeString);

	/** Parse EMOItemRarity from string. */
	static EMOItemRarity ParseItemRarity(const FString& RarityString);

	/** Parse EMOCraftingStation from string. */
	static EMOCraftingStation ParseCraftingStation(const FString& StationString);

	/** Convert item type to string for export. */
	static FString ItemTypeToString(EMOItemType Type);

	/** Convert rarity to string for export. */
	static FString RarityToString(EMOItemRarity Rarity);

	/** Convert station to string for export. */
	static FString StationToString(EMOCraftingStation Station);
};
