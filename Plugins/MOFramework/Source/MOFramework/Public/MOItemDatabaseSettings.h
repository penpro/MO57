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
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="MO Item Database"))
class MOFRAMEWORK_API UMOItemDatabaseSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// UDeveloperSettings overrides
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("MO Item Database"); }

	/** The central DataTable containing FMOItemDefinitionRow rows. */
	UPROPERTY(EditAnywhere, Config, Category="Database")
	TSoftObjectPtr<UDataTable> ItemDefinitionsDataTable;

	UDataTable* GetItemDefinitionsDataTable() const;

	/** Look up an item definition by ID. Returns true if found. */
	UFUNCTION(BlueprintCallable, Category="MO|Item Database", meta=(DisplayName="Get Item Definition"))
	static bool GetItemDefinition(FName ItemDefinitionId, FMOItemDefinitionRow& OutDefinition);

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
};
