/**
 * =============================================================================
 * MODataImportCommandlet.h - CSV Data Import/Export for DataTables
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Commandlet for bulk importing/exporting items, recipes, and skills between
 * CSV files and Unreal DataTables. Supports command-line and Blueprint usage.
 *
 * USAGE (Command Line):
 *   UE5Editor.exe Project -run=MODataImport -items=Items.csv -recipes=Recipes.csv
 *
 * USAGE (Blueprint/Editor Utility):
 *   UMODataImportCommandlet::ImportItemsFromCSV(FilePath, bClearExisting);
 *
 * CSV FORMAT:
 *   Items: RowName,DisplayName,ItemType,Rarity,MaxStackSize,...
 *   Recipes: RowName,DisplayName,CraftTime,Station,SkillId,...
 *   Array fields use pipe delimiter: "stone:2|stick:1"
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] CSV ENCODING: Files must be UTF-8 (with or without BOM). Other
 *   encodings cause parsing errors on non-ASCII characters.
 *
 * [2024-02] CLEAR EXISTING: bClearExisting=true deletes ALL existing rows
 *   before import. Use false to add/update without deleting.
 *
 * [2024-02] PIPE DELIMITER: Use | for arrays (not comma). Commas are CSV
 *   field separators and will break parsing.
 *
 * [2024-02] COLUMN ORDER: Header row must match expected column names.
 *   Column order doesn't matter - names are matched case-insensitively.
 *
 * =============================================================================
 * RELATED FILES: MOItemDefinitionRow.h, MORecipeDefinitionRow.h,
 *                MOSkillDefinitionRow.h, MOItemDatabaseSettings.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MOItemDefinitionRow.h"
#include "MORecipeDefinitionRow.h"
#include "MOSkillDefinitionRow.h"
#include "MODataImportCommandlet.generated.h"

/**
 * Commandlet for importing item and recipe data from CSV files.
 * See file header for usage and CSV format.
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
	 * Import skills from a CSV file into the skill DataTable.
	 * @param CSVFilePath Absolute or project-relative path to CSV file
	 * @param bClearExisting If true, clears existing rows before import
	 * @return Number of rows imported, -1 on error
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Data Import")
	static int32 ImportSkillsFromCSV(const FString& CSVFilePath, bool bClearExisting = false);

	/**
	 * Import all CSVs from a directory (looks for Items.csv, Recipes.csv, Skills.csv).
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

	/**
	 * Export current skill DataTable to CSV.
	 * @param CSVFilePath Output file path
	 * @return True if export succeeded
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Data Import")
	static bool ExportSkillsToCSV(const FString& CSVFilePath);

private:
	/** Parse a CSV file into rows. Returns false on error. */
	static bool ParseCSVFile(const FString& FilePath, TArray<TArray<FString>>& OutRows, TArray<FString>& OutHeaders);

	/** Parse a pipe-delimited array string. */
	static TArray<FString> ParsePipeDelimitedArray(const FString& Input);

	/** Parse an item row from CSV columns. */
	static bool ParseItemRow(const TArray<FString>& Headers, const TArray<FString>& Values, FName RowName, FMOItemDefinitionRow& OutRow);

	/** Parse a recipe row from CSV columns. */
	static bool ParseRecipeRow(const TArray<FString>& Headers, const TArray<FString>& Values, FName RowName, FMORecipeDefinitionRow& OutRow);

	/** Parse a skill row from CSV columns. */
	static bool ParseSkillRow(const TArray<FString>& Headers, const TArray<FString>& Values, FName RowName, FMOSkillDefinitionRow& OutRow);

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

	/** Parse EMOSkillCategory from string. */
	static EMOSkillCategory ParseSkillCategory(const FString& CategoryString);

	/** Convert item type to string for export. */
	static FString ItemTypeToString(EMOItemType Type);

	/** Convert rarity to string for export. */
	static FString RarityToString(EMOItemRarity Rarity);

	/** Convert station to string for export. */
	static FString StationToString(EMOCraftingStation Station);

	/** Convert skill category to string for export. */
	static FString SkillCategoryToString(EMOSkillCategory Category);
};
