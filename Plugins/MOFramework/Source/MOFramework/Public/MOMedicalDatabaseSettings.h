#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MOMedicalDatabaseSettings.generated.h"

class UDataTable;

/**
 * Project Settings entry to configure medical system DataTables.
 * Accessible via Project Settings -> Plugins -> MO Medical Database.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="MO Medical Database"))
class MOFRAMEWORK_API UMOMedicalDatabaseSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// UDeveloperSettings overrides
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("MO Medical Database"); }

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
