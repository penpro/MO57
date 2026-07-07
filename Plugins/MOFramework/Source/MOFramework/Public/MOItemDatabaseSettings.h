#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MOItemDefinitionRow.h"

#include "MOItemDatabaseSettings.generated.h"

class UDataTable;

/**
 * Project Settings entry to point the plugin at an item definition DataTable.
 * This avoids re-wiring references across multiple blueprints.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Item Database"))
class MOFRAMEWORK_API UMOItemDatabaseSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// UDeveloperSettings overrides — unified under "MOFramework" section in Project Settings.
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("MOFramework"); }
	virtual FName GetSectionName() const override { return TEXT("Item Database"); }

	/** The central DataTable containing FMOItemDefinitionRow rows. */
	UPROPERTY(EditAnywhere, Config, Category="Database")
	TSoftObjectPtr<UDataTable> ItemDefinitionsDataTable;

	/**
	 * Fallback path for Items DataTable if soft reference fails in packaged builds.
	 * Format: /Game/Path/To/DT_Items.DT_Items
	 */
	UPROPERTY(EditAnywhere, Config, Category="Database")
	FString FallbackItemsDataTablePath;

	UDataTable* GetItemDefinitionsDataTable() const;

	/** Look up an item definition by ID. Returns true if found. */
	UFUNCTION(BlueprintCallable, Category="MO|Item Database", meta=(DisplayName="Get Item Definition"))
	static bool GetItemDefinition(FName ItemDefinitionId, FMOItemDefinitionRow& OutDefinition);

	/** Forage window check (V2.4): true when the item can be gathered in the
	 *  given season (0=Spring..3=Winter). Unknown items are year-round. */
	UFUNCTION(BlueprintPure, Category="MO|Items")
	static bool IsItemInForageSeason(FName ItemDefinitionId, int32 SeasonIndex);

	/** Get just the icon for an item (loads synchronously). Returns nullptr if not found. */
	UFUNCTION(BlueprintCallable, Category="MO|Item Database")
	static UTexture2D* GetItemIconSmall(FName ItemDefinitionId);

	/** Get the large icon for an item (loads synchronously). Returns nullptr if not found. */
	UFUNCTION(BlueprintCallable, Category="MO|Item Database")
	static UTexture2D* GetItemIconLarge(FName ItemDefinitionId);

	/** Get display name for an item. Returns empty text if not found. */
	UFUNCTION(BlueprintCallable, Category="MO|Item Database")
	static FText GetItemDisplayName(FName ItemDefinitionId);

	/** Check if the Item Database is properly configured. Logs a warning if not. */
	UFUNCTION(BlueprintCallable, Category="MO|Item Database")
	static bool IsConfigured();

	/** Validate configuration and log warnings for any issues. Call at startup. */
	static void ValidateConfiguration();

	/**
	 * Invalidate the item definition cache.
	 * Call this if the DataTable is modified at runtime (rare).
	 *
	 * Does NOT clear mod registrations — those are re-merged on the next
	 * cache build. Use ClearModItems() to drop those.
	 *
	 * BlueprintCallable for parity with UMORecipeDatabaseSettings::InvalidateCache
	 * so editor-python tooling (ue.py refresh-data) can refresh after MCP edits.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Item Database")
	static void InvalidateCache();

	// =========================================================================
	// MOD OVERLAY API (used by MO.Mod.* console commands + future loader UI)
	// =========================================================================
	//
	// Mod rows live in a separate static map and get merged on top of the
	// base cache. If a mod ItemId matches a base row, the mod overrides it.
	// Mod registrations survive InvalidateCache — they only go away via
	// ClearModItems() or process restart.
	//
	// Modders typically wouldn't call these C++ APIs directly; the console
	// commands MO.Mod.LoadItems / MO.Mod.ClearMods provide the user-facing path.

	/**
	 * Register or replace a single item by ID. Modder/cheat path — survives
	 * InvalidateCache. If the cache is already built, the row is also
	 * inserted into the live cache so subsequent lookups see it immediately.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Item Database|Mod")
	static void RegisterModItem(FName ItemId, const FMOItemDefinitionRow& Row);

	/**
	 * Iterate every row of SourceTable (must contain FMOItemDefinitionRow
	 * rows) and register each as a mod item. Returns count merged. Safe to
	 * call on a partially-broken table — skips null rows and logs them.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Item Database|Mod")
	static int32 MergeModItemTable(UDataTable* SourceTable);

	/** Drop all mod registrations and invalidate the cache so base reloads cleanly. */
	UFUNCTION(BlueprintCallable, Category="MO|Item Database|Mod")
	static void ClearModItems();

	/** Number of currently registered mod items (useful for MO.Mod.Status). */
	UFUNCTION(BlueprintCallable, Category="MO|Item Database|Mod")
	static int32 GetModItemCount();

private:
	/** Cached item definitions for fast lookup. */
	static TMap<FName, FMOItemDefinitionRow> CachedItemDefinitions;

	/**
	 * Mod overlay — rows registered via RegisterModItem / MergeModItemTable.
	 * Merged on top of CachedItemDefinitions in BuildCacheIfNeeded so mod
	 * rows survive cache rebuilds. Mods win on ID collision.
	 */
	static TMap<FName, FMOItemDefinitionRow> ModItemDefinitions;

	/** Whether the cache has been built. */
	static bool bCacheBuilt;

	/** Cached DataTable pointer to avoid repeated loading. */
	static TWeakObjectPtr<UDataTable> CachedDataTable;

	/** Whether we've logged the DataTable load (to avoid log spam). */
	static bool bDataTableLoggedOnce;

	/** Build the cache from the DataTable. */
	static void BuildCacheIfNeeded();
};
