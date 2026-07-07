/**
 * =============================================================================
 * MOMedicalDatabaseSettings.h - Medical System Project Settings
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Project Settings entry for configuring medical system DataTables.
 * Accessible via Project Settings -> Plugins -> MO Medical Database.
 *
 * DATATABLE REFERENCES:
 * - BodyPartDefinitionsTable: ~55 body parts with hierarchy
 * - WoundTypeDefinitionsTable: Wound types (cut, burn, fracture, etc.)
 * - ConditionDefinitionsTable: Medical conditions (infection, shock, etc.)
 * - MedicalTreatmentsTable: Treatments and required items
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] SOFT REFERENCES: All tables use TSoftObjectPtr. Call
 *   Get*Table() which loads synchronously on first access.
 *
 * [2024-02] VALIDATION: Call ValidateConfiguration() at startup to log
 *   warnings for missing/misconfigured tables.
 *
 * [2024-02] ROW STRUCTURE TAGS: RequiredAssetDataTags ensures DataTable
 *   picker only shows tables with correct row type.
 *
 * =============================================================================
 * RELATED FILES: MOMedicalSubsystem.h, MOBodyPartDefinitionRow.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MOMedicalDatabaseSettings.generated.h"

class UDataTable;

/**
 * Project Settings entry to configure medical system DataTables.
 * Accessible via Project Settings -> Plugins -> MO Medical Database.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Medical Database"))
class MOFRAMEWORKMEDICAL_API UMOMedicalDatabaseSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// UDeveloperSettings overrides — unified under "MOFramework" section in Project Settings.
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("MOFramework"); }
	virtual FName GetSectionName() const override { return TEXT("Medical Database"); }

	// ============================================================================
	// DATATABLE REFERENCES
	// ============================================================================

	/** DataTable containing FMOBodyPartDefinitionRow rows. */
	UPROPERTY(EditAnywhere, Config, Category="DataTables",
		meta=(RequiredAssetDataTags="RowStructure=/Script/MOFramework.MOBodyPartDefinitionRow"))
	TSoftObjectPtr<UDataTable> BodyPartDefinitionsTable;

	/** DataTable containing FMOWoundTypeDefinitionRow rows. */
	UPROPERTY(EditAnywhere, Config, Category="DataTables",
		meta=(RequiredAssetDataTags="RowStructure=/Script/MOFramework.MOWoundTypeDefinitionRow"))
	TSoftObjectPtr<UDataTable> WoundTypeDefinitionsTable;

	/** DataTable containing FMOConditionDefinitionRow rows. */
	UPROPERTY(EditAnywhere, Config, Category="DataTables",
		meta=(RequiredAssetDataTags="RowStructure=/Script/MOFramework.MOConditionDefinitionRow"))
	TSoftObjectPtr<UDataTable> ConditionDefinitionsTable;

	/** DataTable containing FMOMedicalTreatmentRow rows. */
	UPROPERTY(EditAnywhere, Config, Category="DataTables",
		meta=(RequiredAssetDataTags="RowStructure=/Script/MOFramework.MOMedicalTreatmentRow"))
	TSoftObjectPtr<UDataTable> MedicalTreatmentsTable;

	// ============================================================================
	// ACCESSORS
	// ============================================================================

	/** Get singleton instance. */
	static const UMOMedicalDatabaseSettings* Get();

	/** Get body part definitions DataTable. */
	UDataTable* GetBodyPartDefinitionsTable() const;

	/** Get wound type definitions DataTable. */
	UDataTable* GetWoundTypeDefinitionsTable() const;

	/** Get condition definitions DataTable. */
	UDataTable* GetConditionDefinitionsTable() const;

	/** Get medical treatments DataTable. */
	UDataTable* GetMedicalTreatmentsTable() const;

	// ============================================================================
	// VALIDATION
	// ============================================================================

	/** Check if the Medical Database is properly configured. */
	UFUNCTION(BlueprintCallable, Category="MO|Medical Database")
	static bool IsConfigured();

	/** Validate configuration and log warnings for any issues. Call at startup. */
	static void ValidateConfiguration();
};
