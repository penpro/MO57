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
#include "MOWorldGenSettings.h"
#include "Engine/DataTable.h"
#include "MOWorldFeatureCook.h"
#if WITH_EDITOR
#include "GameDelegates.h"
#endif

// Define the log category
DEFINE_LOG_CATEGORY(LogMOFramework);

#if defined(MO_SHIPPING_DIAG) && NO_LOGGING
#include "HAL/PlatformFileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"

// The MO57ShipDiag target's log sink (see MOFramework.h): one line per UE_LOG, UE's own line format, flushed per line so a crash keeps it.
void MOShipDiagWrite(const TCHAR* Category, ELogVerbosity::Type Verbosity, const FString& Message)
{
	static FCriticalSection Lock;
	static TUniquePtr<IFileHandle> File;
	static bool bOpened = false;
	FScopeLock ScopeLock(&Lock);
	if (!bOpened)
	{
		bOpened = true;
		FString Path;
		if (!FParse::Value(FCommandLine::Get(), TEXT("ABSLOG="), Path, false))
		{
			Path = FPaths::ProjectLogDir() / TEXT("MO57ShipDiag.log");
		}
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		File.Reset(FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*Path, /*bAppend=*/false, /*bAllowRead=*/true));   // the harness tails it live
	}
	if (!File)
	{
		return;
	}
	const TCHAR* Level = Verbosity == ELogVerbosity::Fatal ? TEXT("Fatal: ") : Verbosity == ELogVerbosity::Error ? TEXT("Error: ")
		: Verbosity == ELogVerbosity::Warning ? TEXT("Warning: ") : Verbosity == ELogVerbosity::Display ? TEXT("Display: ") : TEXT("");
	const FString Line = FString::Printf(TEXT("[%s][%3d]%s: %s%s\r\n"), *FDateTime::UtcNow().ToString(TEXT("%Y.%m.%d-%H.%M.%S:%s")),
		static_cast<int32>(GFrameCounter % 1000), Category, Level, *Message);
	const FTCHARToUTF8 Utf8(*Line);
	File->Write(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	File->Flush();
}

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "MOCheatSubsystem.h"

// MO.Delay lives in MOPerfBench.cpp, which Shipping compiles out; the diag target gets the same command (same log line) so pkg_probe recipes carry over.
static FAutoConsoleCommand GMOShipDiagDelayCmd(
	TEXT("MO.Delay"),
	TEXT("Diag: run a console command after N seconds. Usage: MO.Delay <seconds> <command ...>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2 || !Args[0].IsNumeric())
		{
			return;
		}
		FString Command;
		for (int32 I = 1; I < Args.Num(); ++I)
		{
			Command += (I > 1 ? TEXT(" ") : TEXT("")) + Args[I];
		}
		const double At = FPlatformTime::Seconds() + FMath::Clamp(FCString::Atof(*Args[0]), 0.0f, 36000.0f);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([At, Command](float)
		{
			if (FPlatformTime::Seconds() < At)
			{
				return true;
			}
			UWorld* World = nullptr;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::Game && Context.World())
				{
					World = Context.World();
				}
			}
			UMOCheatSubsystem::ExecAsLocalPlayer(World, Command);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOPERF] MO.Delay ran: %s"), *Command);
			return false;
		}), 0.25f);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPERF] will run '%s' in %s s"), *Command, *Args[0]);
	}));
#endif

#define LOCTEXT_NAMESPACE "FMOFrameworkModule"

void FMOFrameworkModule::StartupModule()
{
	// Force-load all database tables to ensure they are included in cooked builds.
	// TSoftObjectPtr references in DeveloperSettings won't be cooked unless something loads them.
	PreloadDatabaseTables();

	// Publish the world-generator snapshot (tuning + DT_Biomes) so editor graph previews, PIE and the PCG spawner have data before any world exists.
	// Also loads DT_Biomes here, which is what gets it cooked. Re-published before every voxel runtime (UMOWorldSeedSubsystem::RegenerateVoxelWorld).
	UMOWorldGenSettings::RefreshPublishedParams();

#if WITH_EDITOR
	// The world features' graphs, PCG data assets and meshes are named in DT_WorldFeatures text the cooker cannot follow: the cook asks for them
	// (FMOWorldFeatureCook; the packaged build had no cliffs without it, Oct 25 2026).
	ModifyCookHandle = FGameDelegates::Get().GetModifyCookDelegate().AddStatic(&FMOWorldFeatureCook::ModifyCook);
#endif

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
#if WITH_EDITOR
	FGameDelegates::Get().GetModifyCookDelegate().Remove(ModifyCookHandle);
#endif
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMOFrameworkModule, MOFramework)