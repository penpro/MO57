#include "CoreMinimal.h"
#include "MOFramework.h"

#if WITH_EDITOR

#include "MORecipeDatabaseSettings.h"
#include "MORecipeDefinitionRow.h"
#include "Engine/DataTable.h"
#include "HAL/IConsoleManager.h"

/**
 * Editor-only console commands for setting up DataTable data that can't survive CSV round-trips.
 * Run these commands in the editor console after importing CSVs.
 */

static void SetupBuildingRecipes()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOEditorCommands] Setting up building recipes..."));

	// Get the recipe DataTable
	const UMORecipeDatabaseSettings* Settings = GetDefault<UMORecipeDatabaseSettings>();
	if (!Settings)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOEditorCommands] Failed to get MORecipeDatabaseSettings"));
		return;
	}

	UDataTable* DataTable = Settings->GetRecipeDefinitionsDataTable();
	if (!IsValid(DataTable))
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOEditorCommands] Failed to get Recipe DataTable"));
		return;
	}

	// Helper lambda to set up a building recipe
	auto SetupBuildRecipe = [DataTable](
		FName RecipeId,
		TArray<FMOBuildPart> BuildParts,
		TArray<FName> AcceptedFuelItems)
	{
		FMORecipeDefinitionRow* Row = DataTable->FindRow<FMORecipeDefinitionRow>(RecipeId, TEXT("SetupBuildingRecipes"), false);
		if (!Row)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOEditorCommands] Recipe not found: %s"), *RecipeId.ToString());
			return;
		}

		Row->BuildParts = BuildParts;
		Row->AcceptedFuelItems = AcceptedFuelItems;

		UE_LOG(LogMOFramework, Log, TEXT("[MOEditorCommands] Set up %s: %d BuildParts, %d FuelItems"),
			*RecipeId.ToString(), BuildParts.Num(), AcceptedFuelItems.Num());
	};

	// ============================================================================
	// BuildCampfire
	// ============================================================================
	{
		TArray<FMOBuildPart> Parts;

		FMOBuildPart Stone;
		Stone.ItemDefinitionId = FName("Stone01");
		Stone.Quantity = 8;
		Stone.Weight = 2;
		Parts.Add(Stone);

		FMOBuildPart Firewood;
		Firewood.ItemDefinitionId = FName("Firewood01");
		Firewood.Quantity = 5;
		Firewood.Weight = 1;
		Parts.Add(Firewood);

		FMOBuildPart Tinder;
		Tinder.ItemDefinitionId = FName("TinderBundle01");
		Tinder.Quantity = 1;
		Tinder.Weight = 1;
		Parts.Add(Tinder);

		TArray<FName> Fuel;
		Fuel.Add(FName("Firewood01"));
		Fuel.Add(FName("Charcoal01"));

		SetupBuildRecipe(FName("BuildCampfire"), Parts, Fuel);
	}

	// ============================================================================
	// BuildForge
	// ============================================================================
	{
		TArray<FMOBuildPart> Parts;

		FMOBuildPart Stone;
		Stone.ItemDefinitionId = FName("Stone01");
		Stone.Quantity = 20;
		Stone.Weight = 3;
		Parts.Add(Stone);

		FMOBuildPart Clay;
		Clay.ItemDefinitionId = FName("Clay01");
		Clay.Quantity = 15;
		Clay.Weight = 2;
		Parts.Add(Clay);

		FMOBuildPart Bellows;
		Bellows.ItemDefinitionId = FName("Bellows01");
		Bellows.Quantity = 1;
		Bellows.Weight = 1;
		Parts.Add(Bellows);

		TArray<FName> Fuel;
		Fuel.Add(FName("Charcoal01"));

		SetupBuildRecipe(FName("BuildForge"), Parts, Fuel);
	}

	// ============================================================================
	// BuildBloomery
	// ============================================================================
	{
		TArray<FMOBuildPart> Parts;

		FMOBuildPart Stone;
		Stone.ItemDefinitionId = FName("Stone01");
		Stone.Quantity = 40;
		Stone.Weight = 3;
		Parts.Add(Stone);

		FMOBuildPart Clay;
		Clay.ItemDefinitionId = FName("Clay01");
		Clay.Quantity = 30;
		Clay.Weight = 3;
		Parts.Add(Clay);

		FMOBuildPart Bellows;
		Bellows.ItemDefinitionId = FName("Bellows01");
		Bellows.Quantity = 2;
		Bellows.Weight = 1;
		Parts.Add(Bellows);

		TArray<FName> Fuel;
		Fuel.Add(FName("Charcoal01"));

		SetupBuildRecipe(FName("BuildBloomery"), Parts, Fuel);
	}

	// ============================================================================
	// BuildWorkbench
	// ============================================================================
	{
		TArray<FMOBuildPart> Parts;

		FMOBuildPart Hardwood;
		Hardwood.ItemDefinitionId = FName("Hardwood01");
		Hardwood.Quantity = 6;
		Hardwood.Weight = 2;
		Parts.Add(Hardwood);

		FMOBuildPart Plank;
		Plank.ItemDefinitionId = FName("Plank01");
		Plank.Quantity = 4;
		Plank.Weight = 2;
		Parts.Add(Plank);

		FMOBuildPart Cordage;
		Cordage.ItemDefinitionId = FName("Cordage01");
		Cordage.Quantity = 3;
		Cordage.Weight = 1;
		Parts.Add(Cordage);

		TArray<FName> Fuel; // No fuel needed

		SetupBuildRecipe(FName("BuildWorkbench"), Parts, Fuel);
	}

	// ============================================================================
	// BuildLoom
	// ============================================================================
	{
		TArray<FMOBuildPart> Parts;

		FMOBuildPart Hardwood;
		Hardwood.ItemDefinitionId = FName("Hardwood01");
		Hardwood.Quantity = 8;
		Hardwood.Weight = 2;
		Parts.Add(Hardwood);

		FMOBuildPart Cordage;
		Cordage.ItemDefinitionId = FName("Cordage01");
		Cordage.Quantity = 10;
		Cordage.Weight = 2;
		Parts.Add(Cordage);

		FMOBuildPart Stone;
		Stone.ItemDefinitionId = FName("Stone01");
		Stone.Quantity = 6;
		Stone.Weight = 1;
		Parts.Add(Stone);

		TArray<FName> Fuel; // No fuel needed

		SetupBuildRecipe(FName("BuildLoom"), Parts, Fuel);
	}

	// Mark the DataTable as modified so it can be saved
	DataTable->MarkPackageDirty();

	UE_LOG(LogMOFramework, Log, TEXT("[MOEditorCommands] Building recipes setup complete. Remember to save the DataTable!"));
}

// Register the console command
static FAutoConsoleCommand SetupBuildingRecipesCmd(
	TEXT("MO.SetupBuildingRecipes"),
	TEXT("Sets up BuildParts and AcceptedFuelItems for building recipes (run after CSV import)"),
	FConsoleCommandDelegate::CreateStatic(&SetupBuildingRecipes)
);

#endif // WITH_EDITOR
