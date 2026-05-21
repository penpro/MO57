/**
 * =============================================================================
 * MOSurvivorJobDatabaseSettings.h - Survivor Job Database Project Settings
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Project Settings for the survivor job database. Points to the job
 * definitions DataTable and provides cached lookups by category and type.
 * Accessible via Project Settings > Plugins > MO Survivor Job Database.
 *
 * CACHING:
 * - JobsByCategory: Map of FName -> array of job definitions
 * - JobsByType: Map of EMOSurvivorJobType -> job definition
 * - CachedDataTable: Weak pointer to loaded DataTable
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] CACHE INVALIDATION: Call InvalidateCache() after runtime
 *   DataTable modifications.
 *
 * [2024-02] FALLBACK PATH: FallbackJobsDataTablePath used if soft reference
 *   fails. Format: /MOFramework/Data/DT_SurvivorJobs.DT_SurvivorJobs
 *
 * [2024-02] WORK TASKS: GetAllWorkTasks() excludes bIsCommand=true jobs.
 *
 * =============================================================================
 * RELATED FILES: MOSurvivorJobTypes.h, MOSurvivorJobQueueComponent.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MOSurvivorJobTypes.h"

#include "MOSurvivorJobDatabaseSettings.generated.h"

class UDataTable;

/**
 * Project Settings entry to point the plugin at a survivor job definition DataTable.
 *
 * CACHING ARCHITECTURE:
 * Maintains cached index of jobs by category for O(1) filtering.
 * Cache is lazily initialized on first access.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Survivor Job Database"))
class MOFRAMEWORK_API UMOSurvivorJobDatabaseSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// UDeveloperSettings overrides — unified under "MOFramework" section in Project Settings.
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("MOFramework"); }
	virtual FName GetSectionName() const override { return TEXT("Survivor Job Database"); }

	/** The central DataTable containing FMOSurvivorJobDefinitionRow rows. */
	UPROPERTY(EditAnywhere, Config, Category="Database")
	TSoftObjectPtr<UDataTable> JobDefinitionsDataTable;

	/**
	 * Fallback path for Jobs DataTable if soft reference fails in packaged builds.
	 * Format: /MOFramework/Data/DT_SurvivorJobs.DT_SurvivorJobs
	 */
	UPROPERTY(EditAnywhere, Config, Category="Database")
	FString FallbackJobsDataTablePath;

	/** Get the loaded DataTable (with caching and fallback logic). */
	UDataTable* GetJobDefinitionsDataTable() const;

	/** Look up a job definition by type. Returns pointer or nullptr if not found. */
	static const FMOSurvivorJobDefinitionRow* GetJobDefinition(EMOSurvivorJobType JobType);

	/** Look up a job definition by type. Returns true if found. */
	UFUNCTION(BlueprintCallable, Category="MO|Survivor Job Database", meta=(DisplayName="Get Job Definition"))
	static bool GetJobDefinitionBP(EMOSurvivorJobType JobType, FMOSurvivorJobDefinitionRow& OutDefinition);

	/** Get the icon for a job (loads synchronously). Returns nullptr if not found. */
	UFUNCTION(BlueprintCallable, Category="MO|Survivor Job Database")
	static UTexture2D* GetJobIcon(EMOSurvivorJobType JobType);

	/** Get display name for a job. Returns empty text if not found. */
	UFUNCTION(BlueprintCallable, Category="MO|Survivor Job Database")
	static FText GetJobDisplayName(EMOSurvivorJobType JobType);

	/** Get all job definitions. */
	UFUNCTION(BlueprintCallable, Category="MO|Survivor Job Database")
	static void GetAllJobDefinitions(TArray<FMOSurvivorJobDefinitionRow>& OutDefinitions);

	/** Get job definitions by category. O(1) cached lookup. */
	UFUNCTION(BlueprintCallable, Category="MO|Survivor Job Database")
	static void GetJobsByCategory(FName Category, TArray<FMOSurvivorJobDefinitionRow>& OutDefinitions);

	/** Get all work tasks (non-command jobs). */
	UFUNCTION(BlueprintCallable, Category="MO|Survivor Job Database")
	static void GetAllWorkTasks(TArray<FMOSurvivorJobDefinitionRow>& OutDefinitions);

	/** Check if the Job Database is properly configured. */
	UFUNCTION(BlueprintCallable, Category="MO|Survivor Job Database")
	static bool IsConfigured();

	/** Invalidate the cache. Call after modifying DataTable at runtime. */
	UFUNCTION(BlueprintCallable, Category="MO|Survivor Job Database")
	static void InvalidateCache();

private:
	static void EnsureCachesBuilt();
	static void BuildCaches();

	static bool bCachesDirty;
	static TMap<FName, TArray<FMOSurvivorJobDefinitionRow>> JobsByCategory;
	static TMap<EMOSurvivorJobType, FMOSurvivorJobDefinitionRow> JobsByType;

	/** Cached pointer to loaded DataTable to avoid repeated loading. */
	static TWeakObjectPtr<UDataTable> CachedDataTable;
};
