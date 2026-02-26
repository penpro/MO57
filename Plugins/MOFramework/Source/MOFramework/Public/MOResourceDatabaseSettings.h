/**
 * =============================================================================
 * MOResourceDatabaseSettings.h - Resource Node Database Project Settings
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Project Settings for the resource node database system. Points to the central
 * resource DataTable (DT_Resources) containing FMOResourceNodeDefinitionRow entries.
 * Accessible via Project Settings > Plugins > MO Resource Database.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-02] FIRST USE: Configure the DataTable path in Project Settings before
 *   using any resource harvesting systems.
 *
 * =============================================================================
 * RELATED FILES: MOResourceNodeDefinitionRow.h, MOHarvestSubsystem.h, DT_Resources
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MOResourceNodeDefinitionRow.h"

#include "MOResourceDatabaseSettings.generated.h"

class UDataTable;

/**
 * Project Settings entry to point the plugin at a resource definition DataTable.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="MO Resource Database"))
class MOFRAMEWORK_API UMOResourceDatabaseSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// UDeveloperSettings overrides
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("MO Resource Database"); }

	/** The central DataTable containing FMOResourceNodeDefinitionRow rows. */
	UPROPERTY(EditAnywhere, Config, Category="Database")
	TSoftObjectPtr<UDataTable> ResourceDefinitionsDataTable;

	/** Get the loaded resource definitions DataTable. */
	UDataTable* GetResourceDefinitionsDataTable() const;

	/** Look up a resource definition by row name. Returns pointer or nullptr if not found. */
	static const FMOResourceNodeDefinitionRow* GetResourceDefinition(FName ResourceNodeId);

	/** Check if the Resource Database is properly configured. */
	UFUNCTION(BlueprintCallable, Category="MO|Resource Database")
	static bool IsConfigured();
};
