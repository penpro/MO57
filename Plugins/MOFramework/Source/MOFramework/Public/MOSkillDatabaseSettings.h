#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MOSkillDefinitionRow.h"

#include "MOSkillDatabaseSettings.generated.h"

class UDataTable;

/**
 * Project Settings entry to point the plugin at a skill definition DataTable.
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

	/** Check if the Skill Database is properly configured. */
	UFUNCTION(BlueprintCallable, Category="MO|Skill Database")
	static bool IsConfigured();
};
