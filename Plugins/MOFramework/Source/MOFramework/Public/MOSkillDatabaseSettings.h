/**
 * =============================================================================
 * MOSkillDatabaseSettings.h - Skill Database Project Settings
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Project Settings for the skill database system. Points to the central
 * skill definitions DataTable and provides cached lookups by category.
 * Accessible via Project Settings > Plugins > MO Skill Database.
 *
 * CACHING:
 * - SkillsByCategory: Map of EMOSkillCategory -> array of skill IDs
 * - CachedDataTable: Weak pointer to loaded DataTable
 * - Lazy initialization via EnsureCachesBuilt()
 *
 * FALLBACK PATH:
 * FallbackSkillsDataTablePath is used if soft reference fails in packaged
 * builds. Format: /Game/Path/To/DT_Skills.DT_Skills
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] CACHE INVALIDATION: Call InvalidateCache() after runtime
 *   DataTable modifications.
 *
 * [2024-02] STATIC CACHES: Cache data is static. Safe for read, not for
 *   concurrent writes.
 *
 * [2024-02] SYNC LOADING: GetSkillIcon() loads textures synchronously.
 *   Avoid in tight loops.
 *
 * =============================================================================
 * RELATED FILES: MOSkillDefinitionRow.h, MOSkillsComponent.h, DT_Skills
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MOSkillDefinitionRow.h"

#include "MOSkillDatabaseSettings.generated.h"

class UDataTable;

/**
 * Project Settings entry to point the plugin at a skill definition DataTable.
 *
 * CACHING ARCHITECTURE:
 * Maintains cached index of skills by category for O(1) filtering.
 * Cache is lazily initialized on first access.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="MO Skill Database"))
class MOFRAMEWORK_API UMOSkillDatabaseSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// UDeveloperSettings overrides
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("MO Skill Database"); }

	/** The central DataTable containing FMOSkillDefinitionRow rows. */
	UPROPERTY(EditAnywhere, Config, Category="Database")
	TSoftObjectPtr<UDataTable> SkillDefinitionsDataTable;

	/**
	 * Fallback path for Skills DataTable if soft reference fails in packaged builds.
	 * Format: /Game/Path/To/DT_Skills.DT_Skills
	 */
	UPROPERTY(EditAnywhere, Config, Category="Database")
	FString FallbackSkillsDataTablePath;

	UDataTable* GetSkillDefinitionsDataTable() const;

	/** Look up a skill definition by ID. Returns pointer or nullptr if not found. */
	static const FMOSkillDefinitionRow* GetSkillDefinition(FName SkillId);

	/** Look up a skill definition by ID. Returns true if found. */
	UFUNCTION(BlueprintCallable, Category="MO|Skill Database", meta=(DisplayName="Get Skill Definition"))
	static bool GetSkillDefinitionBP(FName SkillId, FMOSkillDefinitionRow& OutDefinition);

	/** Get the icon for a skill (loads synchronously). Returns nullptr if not found. */
	UFUNCTION(BlueprintCallable, Category="MO|Skill Database")
	static UTexture2D* GetSkillIcon(FName SkillId);

	/** Get display name for a skill. Returns empty text if not found. */
	UFUNCTION(BlueprintCallable, Category="MO|Skill Database")
	static FText GetSkillDisplayName(FName SkillId);

	/** Get all skill IDs from the database. */
	UFUNCTION(BlueprintCallable, Category="MO|Skill Database")
	static void GetAllSkillIds(TArray<FName>& OutSkillIds);

	/** Get skills by category. O(1) cached lookup. */
	UFUNCTION(BlueprintCallable, Category="MO|Skill Database")
	static void GetSkillsByCategory(EMOSkillCategory Category, TArray<FName>& OutSkillIds);

	/** Check if the Skill Database is properly configured. */
	UFUNCTION(BlueprintCallable, Category="MO|Skill Database")
	static bool IsConfigured();

	/** Invalidate the cache. Call after modifying DataTable at runtime. */
	UFUNCTION(BlueprintCallable, Category="MO|Skill Database")
	static void InvalidateCache();

private:
	static void EnsureCachesBuilt();
	static void BuildCaches();

	static bool bCachesDirty;
	static TMap<EMOSkillCategory, TArray<FName>> SkillsByCategory;

	/** Cached pointer to loaded DataTable to avoid repeated loading. */
	static TWeakObjectPtr<UDataTable> CachedDataTable;
};
