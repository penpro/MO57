#include "MODataImportCommandlet.h"
#include "MOFramework.h"
#include "MOItemDefinitionRow.h"
#include "MORecipeDefinitionRow.h"
#include "MOItemDatabaseSettings.h"
#include "MORecipeDatabaseSettings.h"
#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

// Forward declaration for CSV line parser
static void ParseCSVLine(const FString& Line, TArray<FString>& OutValues);

UMODataImportCommandlet::UMODataImportCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UMODataImportCommandlet::Main(const FString& Params)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MODataImport] Starting data import commandlet..."));

	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	int32 TotalImported = 0;

	// Check for items CSV
	if (const FString* ItemsPath = ParamVals.Find(TEXT("items")))
	{
		int32 Count = ImportItemsFromCSV(*ItemsPath, false);
		if (Count >= 0)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MODataImport] Imported %d items from %s"), Count, **ItemsPath);
			TotalImported += Count;
		}
	}

	// Check for recipes CSV
	if (const FString* RecipesPath = ParamVals.Find(TEXT("recipes")))
	{
		int32 Count = ImportRecipesFromCSV(*RecipesPath, false);
		if (Count >= 0)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MODataImport] Imported %d recipes from %s"), Count, **RecipesPath);
			TotalImported += Count;
		}
	}

	// Check for directory
	if (const FString* DirPath = ParamVals.Find(TEXT("dir")))
	{
		TotalImported += ImportAllFromDirectory(*DirPath, false);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MODataImport] Commandlet complete. Total rows imported: %d"), TotalImported);
	return 0;
}

int32 UMODataImportCommandlet::ImportItemsFromCSV(const FString& CSVFilePath, bool bClearExisting)
{
	// Get the DataTable
	UDataTable* ItemTable = UMOItemDatabaseSettings::GetItemDefinitionTable();
	if (!ItemTable)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MODataImport] Item DataTable not configured in project settings!"));
		return -1;
	}

	// Parse CSV
	TArray<TArray<FString>> Rows;
	TArray<FString> Headers;
	if (!ParseCSVFile(CSVFilePath, Rows, Headers))
	{
		return -1;
	}

	if (bClearExisting)
	{
		ItemTable->EmptyTable();
	}

	int32 ImportedCount = 0;

	// Process each row
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		const TArray<FString>& Values = Rows[i];
		if (Values.Num() == 0)
		{
			continue;
		}

		// First column is always RowName
		FName RowName = FName(*Values[0]);
		if (RowName.IsNone())
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MODataImport] Skipping row %d - empty RowName"), i + 2);
			continue;
		}

		FMOItemDefinitionRow NewRow;
		if (ParseItemRow(Headers, Values, RowName, NewRow))
		{
			// Add or update row
			ItemTable->AddRow(RowName, NewRow);
			ImportedCount++;
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MODataImport] Failed to parse item row: %s"), *RowName.ToString());
		}
	}

	// Mark package dirty for saving
	ItemTable->MarkPackageDirty();

	UE_LOG(LogMOFramework, Log, TEXT("[MODataImport] Imported %d items from %s"), ImportedCount, *CSVFilePath);
	return ImportedCount;
}

int32 UMODataImportCommandlet::ImportRecipesFromCSV(const FString& CSVFilePath, bool bClearExisting)
{
	// Get the DataTable
	UDataTable* RecipeTable = UMORecipeDatabaseSettings::GetRecipeDefinitionTable();
	if (!RecipeTable)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MODataImport] Recipe DataTable not configured in project settings!"));
		return -1;
	}

	// Parse CSV
	TArray<TArray<FString>> Rows;
	TArray<FString> Headers;
	if (!ParseCSVFile(CSVFilePath, Rows, Headers))
	{
		return -1;
	}

	if (bClearExisting)
	{
		RecipeTable->EmptyTable();
	}

	int32 ImportedCount = 0;

	// Process each row
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		const TArray<FString>& Values = Rows[i];
		if (Values.Num() == 0)
		{
			continue;
		}

		// First column is always RowName
		FName RowName = FName(*Values[0]);
		if (RowName.IsNone())
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MODataImport] Skipping row %d - empty RowName"), i + 2);
			continue;
		}

		FMORecipeDefinitionRow NewRow;
		if (ParseRecipeRow(Headers, Values, RowName, NewRow))
		{
			// Add or update row
			RecipeTable->AddRow(RowName, NewRow);
			ImportedCount++;
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MODataImport] Failed to parse recipe row: %s"), *RowName.ToString());
		}
	}

	// Mark package dirty for saving
	RecipeTable->MarkPackageDirty();

	UE_LOG(LogMOFramework, Log, TEXT("[MODataImport] Imported %d recipes from %s"), ImportedCount, *CSVFilePath);
	return ImportedCount;
}

int32 UMODataImportCommandlet::ImportAllFromDirectory(const FString& DirectoryPath, bool bClearExisting)
{
	FString FullPath = DirectoryPath;
	if (FPaths::IsRelative(FullPath))
	{
		FullPath = FPaths::ProjectContentDir() / FullPath;
	}

	int32 TotalImported = 0;

	// Look for Items.csv
	FString ItemsCSV = FullPath / TEXT("Items.csv");
	if (FPaths::FileExists(ItemsCSV))
	{
		int32 Count = ImportItemsFromCSV(ItemsCSV, bClearExisting);
		if (Count >= 0)
		{
			TotalImported += Count;
		}
	}

	// Look for Recipes.csv
	FString RecipesCSV = FullPath / TEXT("Recipes.csv");
	if (FPaths::FileExists(RecipesCSV))
	{
		int32 Count = ImportRecipesFromCSV(RecipesCSV, bClearExisting);
		if (Count >= 0)
		{
			TotalImported += Count;
		}
	}

	return TotalImported;
}

bool UMODataImportCommandlet::ExportItemsToCSV(const FString& CSVFilePath)
{
	UDataTable* ItemTable = UMOItemDatabaseSettings::GetItemDefinitionTable();
	if (!ItemTable)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MODataImport] Item DataTable not configured!"));
		return false;
	}

	TArray<FString> Lines;

	// Header row
	Lines.Add(TEXT("RowName,DisplayName,ItemType,Rarity,MaxStackSize,Weight,bConsumable,bIsTool,ToolType,ToolQuality,MaxDurability,Calories,Water,Protein,Carbs,Fat,Fiber,Tags"));

	// Data rows
	TArray<FName> RowNames = ItemTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		const FMOItemDefinitionRow* Row = ItemTable->FindRow<FMOItemDefinitionRow>(RowName, TEXT("Export"));
		if (!Row)
		{
			continue;
		}

		// Build tags string
		FString TagsStr;
		for (int32 i = 0; i < Row->Tags.Num(); ++i)
		{
			if (i > 0) TagsStr += TEXT("|");
			TagsStr += Row->Tags[i].ToString();
		}

		FString Line = FString::Printf(TEXT("%s,\"%s\",%s,%s,%d,%.2f,%s,%s,%s,%.2f,%d,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,\"%s\""),
			*RowName.ToString(),
			*Row->DisplayName.ToString(),
			*ItemTypeToString(Row->ItemType),
			*RarityToString(Row->Rarity),
			Row->MaxStackSize,
			Row->Weight,
			Row->bConsumable ? TEXT("true") : TEXT("false"),
			Row->bIsTool ? TEXT("true") : TEXT("false"),
			*Row->ToolType.ToString(),
			Row->ToolQuality,
			Row->MaxDurability,
			Row->Nutrition.Calories,
			Row->Nutrition.WaterContent,
			Row->Nutrition.Protein,
			Row->Nutrition.Carbohydrates,
			Row->Nutrition.Fat,
			Row->Nutrition.Fiber,
			*TagsStr
		);
		Lines.Add(Line);
	}

	FString Output = FString::Join(Lines, TEXT("\n"));
	if (!FFileHelper::SaveStringToFile(Output, *CSVFilePath))
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MODataImport] Failed to write to %s"), *CSVFilePath);
		return false;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MODataImport] Exported %d items to %s"), RowNames.Num(), *CSVFilePath);
	return true;
}

bool UMODataImportCommandlet::ExportRecipesToCSV(const FString& CSVFilePath)
{
	UDataTable* RecipeTable = UMORecipeDatabaseSettings::GetRecipeDefinitionTable();
	if (!RecipeTable)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MODataImport] Recipe DataTable not configured!"));
		return false;
	}

	TArray<FString> Lines;

	// Header row
	Lines.Add(TEXT("RowName,DisplayName,CraftTime,Station,SkillId,SkillLevel,SkillXP,Category,bRequiresDiscovery,Ingredients,Outputs,Tools"));

	// Data rows
	TArray<FName> RowNames = RecipeTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		const FMORecipeDefinitionRow* Row = RecipeTable->FindRow<FMORecipeDefinitionRow>(RowName, TEXT("Export"));
		if (!Row)
		{
			continue;
		}

		// Build ingredients string: "itemId:qty|itemId:qty"
		FString IngredientsStr;
		for (int32 i = 0; i < Row->Ingredients.Num(); ++i)
		{
			if (i > 0) IngredientsStr += TEXT("|");
			IngredientsStr += FString::Printf(TEXT("%s:%d"),
				*Row->Ingredients[i].ItemDefinitionId.ToString(),
				Row->Ingredients[i].Quantity);
		}

		// Build outputs string: "itemId:qty:chance|itemId:qty:chance"
		FString OutputsStr;
		for (int32 i = 0; i < Row->Outputs.Num(); ++i)
		{
			if (i > 0) OutputsStr += TEXT("|");
			if (FMath::IsNearlyEqual(Row->Outputs[i].Chance, 1.0f))
			{
				OutputsStr += FString::Printf(TEXT("%s:%d"),
					*Row->Outputs[i].ItemDefinitionId.ToString(),
					Row->Outputs[i].Quantity);
			}
			else
			{
				OutputsStr += FString::Printf(TEXT("%s:%d:%.2f"),
					*Row->Outputs[i].ItemDefinitionId.ToString(),
					Row->Outputs[i].Quantity,
					Row->Outputs[i].Chance);
			}
		}

		// Build tools string: "toolType:minQuality:durability|..."
		FString ToolsStr;
		for (int32 i = 0; i < Row->RequiredTools.Num(); ++i)
		{
			if (i > 0) ToolsStr += TEXT("|");
			ToolsStr += FString::Printf(TEXT("%s:%.1f:%d"),
				*Row->RequiredTools[i].ToolType.ToString(),
				Row->RequiredTools[i].MinQuality,
				Row->RequiredTools[i].DurabilityConsumed);
		}

		FString Line = FString::Printf(TEXT("%s,\"%s\",%.1f,%s,%s,%d,%.1f,%s,%s,\"%s\",\"%s\",\"%s\""),
			*RowName.ToString(),
			*Row->DisplayName.ToString(),
			Row->CraftTime,
			*StationToString(Row->RequiredStation),
			*Row->RequiredSkillId.ToString(),
			Row->RequiredSkillLevel,
			Row->SkillXPReward,
			*Row->Category.ToString(),
			Row->bRequiresDiscovery ? TEXT("true") : TEXT("false"),
			*IngredientsStr,
			*OutputsStr,
			*ToolsStr
		);
		Lines.Add(Line);
	}

	FString Output = FString::Join(Lines, TEXT("\n"));
	if (!FFileHelper::SaveStringToFile(Output, *CSVFilePath))
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MODataImport] Failed to write to %s"), *CSVFilePath);
		return false;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MODataImport] Exported %d recipes to %s"), RowNames.Num(), *CSVFilePath);
	return true;
}

bool UMODataImportCommandlet::ParseCSVFile(const FString& FilePath, TArray<TArray<FString>>& OutRows, TArray<FString>& OutHeaders)
{
	FString FullPath = FilePath;
	if (FPaths::IsRelative(FullPath))
	{
		FullPath = FPaths::ProjectContentDir() / FullPath;
	}

	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *FullPath))
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MODataImport] Failed to read file: %s"), *FullPath);
		return false;
	}

	TArray<FString> Lines;
	FileContent.ParseIntoArrayLines(Lines, true);

	if (Lines.Num() < 2)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MODataImport] CSV file must have header row and at least one data row"));
		return false;
	}

	// Parse header row
	ParseCSVLine(Lines[0], OutHeaders);

	// Parse data rows
	for (int32 i = 1; i < Lines.Num(); ++i)
	{
		FString& Line = Lines[i];
		Line.TrimStartAndEndInline();

		// Skip empty lines and comments
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")) || Line.StartsWith(TEXT("//")))
		{
			continue;
		}

		TArray<FString> Values;
		ParseCSVLine(Line, Values);
		OutRows.Add(Values);
	}

	return true;
}

// Helper to parse a CSV line respecting quoted fields
static void ParseCSVLine(const FString& Line, TArray<FString>& OutValues)
{
	OutValues.Empty();

	bool bInQuotes = false;
	FString CurrentValue;

	for (int32 i = 0; i < Line.Len(); ++i)
	{
		TCHAR c = Line[i];

		if (c == TEXT('"'))
		{
			// Check for escaped quote ""
			if (bInQuotes && i + 1 < Line.Len() && Line[i + 1] == TEXT('"'))
			{
				CurrentValue += TEXT('"');
				++i;
			}
			else
			{
				bInQuotes = !bInQuotes;
			}
		}
		else if (c == TEXT(',') && !bInQuotes)
		{
			CurrentValue.TrimStartAndEndInline();
			OutValues.Add(CurrentValue);
			CurrentValue.Empty();
		}
		else
		{
			CurrentValue += c;
		}
	}

	// Add last value
	CurrentValue.TrimStartAndEndInline();
	OutValues.Add(CurrentValue);
}

TArray<FString> UMODataImportCommandlet::ParsePipeDelimitedArray(const FString& Input)
{
	TArray<FString> Result;
	FString Trimmed = Input;
	Trimmed.TrimStartAndEndInline();

	if (Trimmed.IsEmpty())
	{
		return Result;
	}

	Trimmed.ParseIntoArray(Result, TEXT("|"), true);

	// Trim each element
	for (FString& Elem : Result)
	{
		Elem.TrimStartAndEndInline();
	}

	return Result;
}

bool UMODataImportCommandlet::ParseItemRow(const TArray<FString>& Headers, const TArray<FString>& Values, FName RowName, FMOItemDefinitionRow& OutRow)
{
	OutRow.ItemId = RowName;

	// Required fields
	int32 Idx = GetColumnIndex(Headers, TEXT("DisplayName"));
	if (Idx >= 0)
	{
		OutRow.DisplayName = FText::FromString(GetColumnValue(Values, Idx));
	}

	Idx = GetColumnIndex(Headers, TEXT("ItemType"));
	if (Idx >= 0)
	{
		OutRow.ItemType = ParseItemType(GetColumnValue(Values, Idx));
	}

	Idx = GetColumnIndex(Headers, TEXT("Rarity"));
	if (Idx >= 0)
	{
		OutRow.Rarity = ParseItemRarity(GetColumnValue(Values, Idx));
	}

	Idx = GetColumnIndex(Headers, TEXT("MaxStackSize"));
	if (Idx >= 0)
	{
		OutRow.MaxStackSize = FCString::Atoi(*GetColumnValue(Values, Idx));
		if (OutRow.MaxStackSize < 1) OutRow.MaxStackSize = 1;
	}

	Idx = GetColumnIndex(Headers, TEXT("Weight"));
	if (Idx >= 0)
	{
		OutRow.Weight = FCString::Atof(*GetColumnValue(Values, Idx));
	}

	Idx = GetColumnIndex(Headers, TEXT("bConsumable"));
	if (Idx >= 0)
	{
		FString Val = GetColumnValue(Values, Idx).ToLower();
		OutRow.bConsumable = (Val == TEXT("true") || Val == TEXT("1") || Val == TEXT("yes"));
	}

	// Tool properties
	Idx = GetColumnIndex(Headers, TEXT("bIsTool"));
	if (Idx >= 0)
	{
		FString Val = GetColumnValue(Values, Idx).ToLower();
		OutRow.bIsTool = (Val == TEXT("true") || Val == TEXT("1") || Val == TEXT("yes"));
	}

	Idx = GetColumnIndex(Headers, TEXT("ToolType"));
	if (Idx >= 0)
	{
		OutRow.ToolType = FName(*GetColumnValue(Values, Idx));
	}

	Idx = GetColumnIndex(Headers, TEXT("ToolQuality"));
	if (Idx >= 0)
	{
		OutRow.ToolQuality = FCString::Atof(*GetColumnValue(Values, Idx));
		if (OutRow.ToolQuality < 0.1f) OutRow.ToolQuality = 1.0f;
	}

	Idx = GetColumnIndex(Headers, TEXT("MaxDurability"));
	if (Idx >= 0)
	{
		OutRow.MaxDurability = FCString::Atoi(*GetColumnValue(Values, Idx));
	}

	// Nutrition
	Idx = GetColumnIndex(Headers, TEXT("Calories"));
	if (Idx >= 0)
	{
		OutRow.Nutrition.Calories = FCString::Atof(*GetColumnValue(Values, Idx));
	}

	Idx = GetColumnIndex(Headers, TEXT("Water"));
	if (Idx >= 0)
	{
		OutRow.Nutrition.WaterContent = FCString::Atof(*GetColumnValue(Values, Idx));
	}

	Idx = GetColumnIndex(Headers, TEXT("Protein"));
	if (Idx >= 0)
	{
		OutRow.Nutrition.Protein = FCString::Atof(*GetColumnValue(Values, Idx));
	}

	Idx = GetColumnIndex(Headers, TEXT("Carbs"));
	if (Idx >= 0)
	{
		OutRow.Nutrition.Carbohydrates = FCString::Atof(*GetColumnValue(Values, Idx));
	}

	Idx = GetColumnIndex(Headers, TEXT("Fat"));
	if (Idx >= 0)
	{
		OutRow.Nutrition.Fat = FCString::Atof(*GetColumnValue(Values, Idx));
	}

	Idx = GetColumnIndex(Headers, TEXT("Fiber"));
	if (Idx >= 0)
	{
		OutRow.Nutrition.Fiber = FCString::Atof(*GetColumnValue(Values, Idx));
	}

	// Tags (pipe-delimited)
	Idx = GetColumnIndex(Headers, TEXT("Tags"));
	if (Idx >= 0)
	{
		TArray<FString> TagStrings = ParsePipeDelimitedArray(GetColumnValue(Values, Idx));
		for (const FString& TagStr : TagStrings)
		{
			if (!TagStr.IsEmpty())
			{
				OutRow.Tags.Add(FName(*TagStr));
			}
		}
	}

	// Description (optional)
	Idx = GetColumnIndex(Headers, TEXT("Description"));
	if (Idx >= 0)
	{
		OutRow.Description = FText::FromString(GetColumnValue(Values, Idx));
	}

	return true;
}

bool UMODataImportCommandlet::ParseRecipeRow(const TArray<FString>& Headers, const TArray<FString>& Values, FName RowName, FMORecipeDefinitionRow& OutRow)
{
	OutRow.RecipeId = RowName;

	int32 Idx = GetColumnIndex(Headers, TEXT("DisplayName"));
	if (Idx >= 0)
	{
		OutRow.DisplayName = FText::FromString(GetColumnValue(Values, Idx));
	}

	Idx = GetColumnIndex(Headers, TEXT("CraftTime"));
	if (Idx >= 0)
	{
		OutRow.CraftTime = FCString::Atof(*GetColumnValue(Values, Idx));
		if (OutRow.CraftTime < 0.0f) OutRow.CraftTime = 1.0f;
	}

	Idx = GetColumnIndex(Headers, TEXT("Station"));
	if (Idx >= 0)
	{
		OutRow.RequiredStation = ParseCraftingStation(GetColumnValue(Values, Idx));
	}

	Idx = GetColumnIndex(Headers, TEXT("SkillId"));
	if (Idx >= 0)
	{
		OutRow.RequiredSkillId = FName(*GetColumnValue(Values, Idx));
	}

	Idx = GetColumnIndex(Headers, TEXT("SkillLevel"));
	if (Idx >= 0)
	{
		OutRow.RequiredSkillLevel = FCString::Atoi(*GetColumnValue(Values, Idx));
	}

	Idx = GetColumnIndex(Headers, TEXT("SkillXP"));
	if (Idx >= 0)
	{
		OutRow.SkillXPReward = FCString::Atof(*GetColumnValue(Values, Idx));
	}

	Idx = GetColumnIndex(Headers, TEXT("Category"));
	if (Idx >= 0)
	{
		OutRow.Category = FName(*GetColumnValue(Values, Idx));
	}

	Idx = GetColumnIndex(Headers, TEXT("bRequiresDiscovery"));
	if (Idx >= 0)
	{
		FString Val = GetColumnValue(Values, Idx).ToLower();
		OutRow.bRequiresDiscovery = (Val == TEXT("true") || Val == TEXT("1") || Val == TEXT("yes"));
	}

	// Parse ingredients: "itemId:qty|itemId:qty"
	Idx = GetColumnIndex(Headers, TEXT("Ingredients"));
	if (Idx >= 0)
	{
		TArray<FString> IngredientStrings = ParsePipeDelimitedArray(GetColumnValue(Values, Idx));
		for (const FString& IngStr : IngredientStrings)
		{
			TArray<FString> Parts;
			IngStr.ParseIntoArray(Parts, TEXT(":"), true);

			if (Parts.Num() >= 2)
			{
				FMORecipeIngredient Ingredient;
				Ingredient.ItemDefinitionId = FName(*Parts[0].TrimStartAndEnd());
				Ingredient.Quantity = FCString::Atoi(*Parts[1]);
				if (Ingredient.Quantity < 1) Ingredient.Quantity = 1;
				OutRow.Ingredients.Add(Ingredient);
			}
		}
	}

	// Parse outputs: "itemId:qty|itemId:qty:chance"
	Idx = GetColumnIndex(Headers, TEXT("Outputs"));
	if (Idx >= 0)
	{
		TArray<FString> OutputStrings = ParsePipeDelimitedArray(GetColumnValue(Values, Idx));
		for (const FString& OutStr : OutputStrings)
		{
			TArray<FString> Parts;
			OutStr.ParseIntoArray(Parts, TEXT(":"), true);

			if (Parts.Num() >= 2)
			{
				FMORecipeOutput Output;
				Output.ItemDefinitionId = FName(*Parts[0].TrimStartAndEnd());
				Output.Quantity = FCString::Atoi(*Parts[1]);
				if (Output.Quantity < 1) Output.Quantity = 1;

				if (Parts.Num() >= 3)
				{
					Output.Chance = FCString::Atof(*Parts[2]);
					Output.Chance = FMath::Clamp(Output.Chance, 0.0f, 1.0f);
				}
				else
				{
					Output.Chance = 1.0f;
				}

				OutRow.Outputs.Add(Output);
			}
		}
	}

	// Parse tools: "toolType:minQuality:durability"
	Idx = GetColumnIndex(Headers, TEXT("Tools"));
	if (Idx >= 0)
	{
		TArray<FString> ToolStrings = ParsePipeDelimitedArray(GetColumnValue(Values, Idx));
		for (const FString& ToolStr : ToolStrings)
		{
			TArray<FString> Parts;
			ToolStr.ParseIntoArray(Parts, TEXT(":"), true);

			if (Parts.Num() >= 1 && !Parts[0].IsEmpty())
			{
				FMOToolRequirement Tool;
				Tool.ToolType = FName(*Parts[0].TrimStartAndEnd());

				if (Parts.Num() >= 2)
				{
					Tool.MinQuality = FCString::Atof(*Parts[1]);
				}

				if (Parts.Num() >= 3)
				{
					Tool.DurabilityConsumed = FCString::Atoi(*Parts[2]);
				}

				OutRow.RequiredTools.Add(Tool);
			}
		}
	}

	// Description (optional)
	Idx = GetColumnIndex(Headers, TEXT("Description"));
	if (Idx >= 0)
	{
		OutRow.Description = FText::FromString(GetColumnValue(Values, Idx));
	}

	return true;
}

int32 UMODataImportCommandlet::GetColumnIndex(const TArray<FString>& Headers, const FString& ColumnName)
{
	for (int32 i = 0; i < Headers.Num(); ++i)
	{
		if (Headers[i].Equals(ColumnName, ESearchCase::IgnoreCase))
		{
			return i;
		}
	}
	return -1;
}

FString UMODataImportCommandlet::GetColumnValue(const TArray<FString>& Values, int32 Index)
{
	if (Index >= 0 && Index < Values.Num())
	{
		return Values[Index];
	}
	return FString();
}

EMOItemType UMODataImportCommandlet::ParseItemType(const FString& TypeString)
{
	FString Lower = TypeString.ToLower();
	if (Lower == TEXT("consumable")) return EMOItemType::Consumable;
	if (Lower == TEXT("material")) return EMOItemType::Material;
	if (Lower == TEXT("tool")) return EMOItemType::Tool;
	if (Lower == TEXT("weapon")) return EMOItemType::Weapon;
	if (Lower == TEXT("ammo")) return EMOItemType::Ammo;
	if (Lower == TEXT("armor")) return EMOItemType::Armor;
	if (Lower == TEXT("keyitem") || Lower == TEXT("key")) return EMOItemType::KeyItem;
	if (Lower == TEXT("quest")) return EMOItemType::Quest;
	if (Lower == TEXT("currency")) return EMOItemType::Currency;
	if (Lower == TEXT("misc")) return EMOItemType::Misc;
	return EMOItemType::None;
}

EMOItemRarity UMODataImportCommandlet::ParseItemRarity(const FString& RarityString)
{
	FString Lower = RarityString.ToLower();
	if (Lower == TEXT("uncommon")) return EMOItemRarity::Uncommon;
	if (Lower == TEXT("rare")) return EMOItemRarity::Rare;
	if (Lower == TEXT("epic")) return EMOItemRarity::Epic;
	if (Lower == TEXT("legendary")) return EMOItemRarity::Legendary;
	return EMOItemRarity::Common;
}

EMOCraftingStation UMODataImportCommandlet::ParseCraftingStation(const FString& StationString)
{
	FString Lower = StationString.ToLower();
	if (Lower == TEXT("campfire")) return EMOCraftingStation::Campfire;
	if (Lower == TEXT("workbench")) return EMOCraftingStation::Workbench;
	if (Lower == TEXT("forge")) return EMOCraftingStation::Forge;
	if (Lower == TEXT("alchemy")) return EMOCraftingStation::Alchemy;
	if (Lower == TEXT("kitchen")) return EMOCraftingStation::Kitchen;
	if (Lower == TEXT("loom")) return EMOCraftingStation::Loom;
	return EMOCraftingStation::None;
}

FString UMODataImportCommandlet::ItemTypeToString(EMOItemType Type)
{
	switch (Type)
	{
		case EMOItemType::Consumable: return TEXT("Consumable");
		case EMOItemType::Material: return TEXT("Material");
		case EMOItemType::Tool: return TEXT("Tool");
		case EMOItemType::Weapon: return TEXT("Weapon");
		case EMOItemType::Ammo: return TEXT("Ammo");
		case EMOItemType::Armor: return TEXT("Armor");
		case EMOItemType::KeyItem: return TEXT("KeyItem");
		case EMOItemType::Quest: return TEXT("Quest");
		case EMOItemType::Currency: return TEXT("Currency");
		case EMOItemType::Misc: return TEXT("Misc");
		default: return TEXT("None");
	}
}

FString UMODataImportCommandlet::RarityToString(EMOItemRarity Rarity)
{
	switch (Rarity)
	{
		case EMOItemRarity::Uncommon: return TEXT("Uncommon");
		case EMOItemRarity::Rare: return TEXT("Rare");
		case EMOItemRarity::Epic: return TEXT("Epic");
		case EMOItemRarity::Legendary: return TEXT("Legendary");
		default: return TEXT("Common");
	}
}

FString UMODataImportCommandlet::StationToString(EMOCraftingStation Station)
{
	switch (Station)
	{
		case EMOCraftingStation::Campfire: return TEXT("Campfire");
		case EMOCraftingStation::Workbench: return TEXT("Workbench");
		case EMOCraftingStation::Forge: return TEXT("Forge");
		case EMOCraftingStation::Alchemy: return TEXT("Alchemy");
		case EMOCraftingStation::Kitchen: return TEXT("Kitchen");
		case EMOCraftingStation::Loom: return TEXT("Loom");
		default: return TEXT("None");
	}
}
