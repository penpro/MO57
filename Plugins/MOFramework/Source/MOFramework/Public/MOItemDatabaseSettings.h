#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

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
	/** The central DataTable containing FMOItemDefinitionRow rows. */
	UPROPERTY(EditAnywhere, Config, Category="Database")
	TSoftObjectPtr<UDataTable> ItemDefinitionsDataTable;

	UDataTable* GetItemDefinitionsDataTable() const;
};
