// Copyright Epic Games, Inc. All Rights Reserved.

#include "MOFramework.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MORecipeDatabaseSettings.h"
#include "MORecipeDefinitionRow.h"
#include "MOSkillDatabaseSettings.h"
#include "MOSkillDefinitionRow.h"
#include "MOPersistenceSettings.h"
#include "MOMedicalDatabaseSettings.h"
#include "Engine/DataTable.h"

// Define the log category
DEFINE_LOG_CATEGORY(LogMOFramework);

#define LOCTEXT_NAMESPACE "FMOFrameworkModule"

void FMOFrameworkModule::StartupModule()
{
	// Force-load all database tables to ensure they are included in cooked builds.
	// TSoftObjectPtr references in DeveloperSettings won't be cooked unless something loads them.
	PreloadDatabaseTables();

	// Validate configuration settings - logs warnings if required settings aren't configured.
	// Skip validation during commandlet runs (cooking, packaging, etc.) to avoid noise.
	if (!IsRunningCommandlet())
	{
		UMOItemDatabaseSettings::ValidateConfiguration();
		UMOPersistenceSettings::ValidateConfiguration();
		UMOMedicalDatabaseSettings::ValidateConfiguration();
	}
}

void FMOFrameworkModule::PreloadDatabaseTables()
{
	// Load Item Database and all soft-referenced assets inside
	if (const UMOItemDatabaseSettings* ItemSettings = GetDefault<UMOItemDatabaseSettings>())
	{
		if (!ItemSettings->ItemDefinitionsDataTable.IsNull())
		{
			UDataTable* ItemTable = ItemSettings->ItemDefinitionsDataTable.LoadSynchronous();
			if (ItemTable)
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOFramework] Preloaded Item Database: %s"), *ItemTable->GetPathName());

				// Force-load all soft-referenced assets in item definitions (icons, meshes, etc.)
				// This ensures the cooker includes them in packaged builds.
				static const FString ContextString(TEXT("MOFramework_PreloadItems"));
				TArray<FMOItemDefinitionRow*> AllItems;
				ItemTable->GetAllRows<FMOItemDefinitionRow>(ContextString, AllItems);

				int32 LoadedAssets = 0;
				for (const FMOItemDefinitionRow* Item : AllItems)
				{
					if (!Item) continue;

					// Load UI assets
					if (!Item->UI.IconSmall.IsNull())
					{
						Item->UI.IconSmall.LoadSynchronous();
						LoadedAssets++;
					}
					if (!Item->UI.IconLarge.IsNull())
					{
						Item->UI.IconLarge.LoadSynchronous();
						LoadedAssets++;
					}

					// Load world assets
					if (!Item->WorldVisual.StaticMesh.IsNull())
					{
						Item->WorldVisual.StaticMesh.LoadSynchronous();
						LoadedAssets++;
					}
					if (!Item->WorldVisual.MaterialOverride.IsNull())
					{
						Item->WorldVisual.MaterialOverride.LoadSynchronous();
						LoadedAssets++;
					}
					if (!Item->WorldVisual.WorldActorClass.IsNull())
					{
						Item->WorldVisual.WorldActorClass.LoadSynchronous();
						LoadedAssets++;
					}
				}

				UE_LOG(LogMOFramework, Log, TEXT("[MOFramework] Preloaded %d item definitions, %d soft-referenced assets"),
					AllItems.Num(), LoadedAssets);
			}
		}
	}

	// Load Recipe Database and assets
	if (const UMORecipeDatabaseSettings* RecipeSettings = GetDefault<UMORecipeDatabaseSettings>())
	{
		if (!RecipeSettings->RecipeDefinitionsDataTable.IsNull())
		{
			UDataTable* RecipeTable = RecipeSettings->RecipeDefinitionsDataTable.LoadSynchronous();
			if (RecipeTable)
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOFramework] Preloaded Recipe Database: %s"), *RecipeTable->GetPathName());

				// Force-load recipe soft-referenced assets
				static const FString ContextString(TEXT("MOFramework_PreloadRecipes"));
				TArray<FMORecipeDefinitionRow*> AllRecipes;
				RecipeTable->GetAllRows<FMORecipeDefinitionRow>(ContextString, AllRecipes);

				int32 LoadedAssets = 0;
				for (const FMORecipeDefinitionRow* Recipe : AllRecipes)
				{
					if (!Recipe) continue;

					if (!Recipe->Icon.IsNull())
					{
						Recipe->Icon.LoadSynchronous();
						LoadedAssets++;
					}
					if (!Recipe->PlacementData.PreviewMesh.IsNull())
					{
						Recipe->PlacementData.PreviewMesh.LoadSynchronous();
						LoadedAssets++;
					}
				}

				UE_LOG(LogMOFramework, Log, TEXT("[MOFramework] Preloaded %d recipes, %d soft-referenced assets"),
					AllRecipes.Num(), LoadedAssets);
			}
		}
	}

	// Load Skill Database and assets
	if (const UMOSkillDatabaseSettings* SkillSettings = GetDefault<UMOSkillDatabaseSettings>())
	{
		if (!SkillSettings->SkillDefinitionsDataTable.IsNull())
		{
			UDataTable* SkillTable = SkillSettings->SkillDefinitionsDataTable.LoadSynchronous();
			if (SkillTable)
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOFramework] Preloaded Skill Database: %s"), *SkillTable->GetPathName());

				// Force-load skill soft-referenced assets
				static const FString ContextString(TEXT("MOFramework_PreloadSkills"));
				TArray<FMOSkillDefinitionRow*> AllSkills;
				SkillTable->GetAllRows<FMOSkillDefinitionRow>(ContextString, AllSkills);

				int32 LoadedAssets = 0;
				for (const FMOSkillDefinitionRow* Skill : AllSkills)
				{
					if (!Skill) continue;

					if (!Skill->Icon.IsNull())
					{
						Skill->Icon.LoadSynchronous();
						LoadedAssets++;
					}
				}

				UE_LOG(LogMOFramework, Log, TEXT("[MOFramework] Preloaded %d skills, %d soft-referenced assets"),
					AllSkills.Num(), LoadedAssets);
			}
		}
	}

	// Load Medical Databases
	if (const UMOMedicalDatabaseSettings* MedSettings = GetDefault<UMOMedicalDatabaseSettings>())
	{
		if (!MedSettings->BodyPartDefinitionsTable.IsNull())
		{
			UDataTable* Table = MedSettings->BodyPartDefinitionsTable.LoadSynchronous();
			if (Table)
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOFramework] Preloaded Body Part Database: %s"), *Table->GetPathName());
			}
		}
		if (!MedSettings->WoundTypeDefinitionsTable.IsNull())
		{
			UDataTable* Table = MedSettings->WoundTypeDefinitionsTable.LoadSynchronous();
			if (Table)
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOFramework] Preloaded Wound Type Database: %s"), *Table->GetPathName());
			}
		}
		if (!MedSettings->ConditionDefinitionsTable.IsNull())
		{
			UDataTable* Table = MedSettings->ConditionDefinitionsTable.LoadSynchronous();
			if (Table)
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOFramework] Preloaded Condition Database: %s"), *Table->GetPathName());
			}
		}
		if (!MedSettings->MedicalTreatmentsTable.IsNull())
		{
			UDataTable* Table = MedSettings->MedicalTreatmentsTable.LoadSynchronous();
			if (Table)
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOFramework] Preloaded Treatment Database: %s"), *Table->GetPathName());
			}
		}
	}
}

void FMOFrameworkModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMOFrameworkModule, MOFramework)