#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MOMedicalTypes.h"
#include "MOBodyPartDefinitionRow.h"
#include "MOMedicalSubsystem.generated.h"

class UDataTable;

/**
 * GameInstance subsystem providing medical system DataTable lookups and utility functions.
 * Caches DataTable references and provides efficient lookup methods.
 */
UCLASS()
class MOFRAMEWORK_API UMOMedicalSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================================
	// SUBSYSTEM LIFECYCLE
	// ============================================================================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ============================================================================
	// DATATABLE REFERENCES
	// ============================================================================

	/** DataTable containing body part definitions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|DataTables")
	TSoftObjectPtr<UDataTable> BodyPartDefinitionsTable;

	/** DataTable containing wound type definitions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|DataTables")
	TSoftObjectPtr<UDataTable> WoundTypeDefinitionsTable;

	/** DataTable containing condition definitions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|DataTables")
	TSoftObjectPtr<UDataTable> ConditionDefinitionsTable;

	/** DataTable containing medical treatment definitions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|DataTables")
	TSoftObjectPtr<UDataTable> MedicalTreatmentsTable;

	// ============================================================================
	// BODY PART LOOKUPS
	// ============================================================================

	/**
	 * Get body part definition by type.
	 * @param PartType The body part type to look up.
	 * @param OutDefinition The output definition.
	 * @return True if found.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Medical|Lookup")
	bool GetBodyPartDefinition(EMOBodyPartType PartType, FMOBodyPartDefinitionRow& OutDefinition) const;

	/**
	 * Get all body part definitions.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Medical|Lookup")
	TArray<FMOBodyPartDefinitionRow> GetAllBodyPartDefinitions() const;

	/**
	 * Get child body parts of a parent part.
	 * @param ParentPart The parent body part.
	 * @return Array of child body part types.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Medical|Lookup")
	TArray<EMOBodyPartType> GetChildBodyParts(EMOBodyPartType ParentPart) const;

	/**
	 * Check if a body part is vital (instant death or death timer on destruction).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Medical|Lookup")
	bool IsVitalBodyPart(EMOBodyPartType PartType) const;

	// ============================================================================
	// WOUND TYPE LOOKUPS
	// ============================================================================

	/**
	 * Get wound type definition.
	 * @param WoundType The wound type to look up.
	 * @param OutDefinition The output definition.
	 * @return True if found.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Medical|Lookup")
	bool GetWoundTypeDefinition(EMOWoundType WoundType, FMOWoundTypeDefinitionRow& OutDefinition) const;

	/**
	 * Get all wound type definitions.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Medical|Lookup")
	TArray<FMOWoundTypeDefinitionRow> GetAllWoundTypeDefinitions() const;

	// ============================================================================
	// CONDITION LOOKUPS
	// ============================================================================

	/**
	 * Get condition definition.
	 * @param ConditionType The condition type to look up.
	 * @param OutDefinition The output definition.
	 * @return True if found.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Medical|Lookup")
	bool GetConditionDefinition(EMOConditionType ConditionType, FMOConditionDefinitionRow& OutDefinition) const;

	/**
	 * Get all condition definitions.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Medical|Lookup")
	TArray<FMOConditionDefinitionRow> GetAllConditionDefinitions() const;

	// ============================================================================
	// TREATMENT LOOKUPS
	// ============================================================================

	/**
	 * Get treatment definition by ID.
	 * @param TreatmentId The treatment ID to look up.
	 * @param OutDefinition The output definition.
	 * @return True if found.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Medical|Lookup")
	bool GetTreatmentDefinition(FName TreatmentId, FMOMedicalTreatmentRow& OutDefinition) const;

	/**
	 * Get all treatment definitions.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Medical|Lookup")
	TArray<FMOMedicalTreatmentRow> GetAllTreatmentDefinitions() const;

	/**
	 * Get treatments that can treat a specific wound type.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Medical|Lookup")
	TArray<FMOMedicalTreatmentRow> GetTreatmentsForWoundType(EMOWoundType WoundType) const;

	/**
	 * Get treatments that can treat a specific condition.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Medical|Lookup")
	TArray<FMOMedicalTreatmentRow> GetTreatmentsForCondition(EMOConditionType ConditionType) const;

	// ============================================================================
	// CASCADE CALCULATIONS
	// ============================================================================

	/**
	 * Calculate wound parameters from type and severity.
	 * @param WoundType Type of wound.
	 * @param Severity Severity 0-100.
	 * @param BodyPart Where the wound is located.
	 * @param OutBleedRate Output bleed rate in mL/s.
	 * @param OutInfectionRisk Output infection risk 0-1.
	 * @param OutPain Output pain level.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Medical|Calculate")
	void CalculateWoundParameters(
		EMOWoundType WoundType,
		float Severity,
		EMOBodyPartType BodyPart,
		float& OutBleedRate,
		float& OutInfectionRisk,
		float& OutPain
	) const;

	/**
	 * Calculate treatment effectiveness.
	 * @param TreatmentId Treatment being applied.
	 * @param MedicSkillLevel Skill level of the medic.
	 * @param bIsSelfTreatment True if treating self.
	 * @param BodyPart The body part being treated.
	 * @return Effectiveness multiplier (0-1+).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Medical|Calculate")
	float CalculateTreatmentEffectiveness(
		FName TreatmentId,
		int32 MedicSkillLevel,
		bool bIsSelfTreatment,
		EMOBodyPartType BodyPart
	) const;

	/**
	 * Calculate healing rate modifier based on all factors.
	 * @param NutritionMultiplier From metabolism component.
	 * @param bIsInfected Whether the wound is infected.
	 * @param bIsBandaged Whether the wound is bandaged.
	 * @param bIsSutured Whether the wound is sutured.
	 * @return Healing rate multiplier.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Medical|Calculate")
	float CalculateHealingRateMultiplier(
		float NutritionMultiplier,
		bool bIsInfected,
		bool bIsBandaged,
		bool bIsSutured
	) const;

	// ============================================================================
	// UTILITY
	// ============================================================================

	/**
	 * Get display name for a body part.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Medical|Utility")
	FText GetBodyPartDisplayName(EMOBodyPartType PartType) const;

	/**
	 * Get display name for a wound type.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Medical|Utility")
	FText GetWoundTypeDisplayName(EMOWoundType WoundType) const;

	/**
	 * Get display name for a condition type.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Medical|Utility")
	FText GetConditionDisplayName(EMOConditionType ConditionType) const;

	/**
	 * Get display name for a consciousness level.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Medical|Utility")
	FText GetConsciousnessDisplayName(EMOConsciousnessLevel Level) const;

	/**
	 * Get display name for a blood loss stage.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Medical|Utility")
	FText GetBloodLossStageDisplayName(EMOBloodLossStage Stage) const;

private:
	// ============================================================================
	// CACHED DATA
	// ============================================================================

	/** Cached body part definitions for fast lookup. */
	UPROPERTY()
	TMap<EMOBodyPartType, FMOBodyPartDefinitionRow> CachedBodyPartDefs;

	/** Cached wound type definitions. */
	UPROPERTY()
	TMap<EMOWoundType, FMOWoundTypeDefinitionRow> CachedWoundTypeDefs;

	/** Cached condition definitions. */
	UPROPERTY()
	TMap<EMOConditionType, FMOConditionDefinitionRow> CachedConditionDefs;

	/** Cached treatment definitions. */
	UPROPERTY()
	TMap<FName, FMOMedicalTreatmentRow> CachedTreatmentDefs;

	/** Whether caches have been built. */
	bool bCachesBuilt = false;

	// ============================================================================
	// INTERNAL METHODS
	// ============================================================================

	/** Build caches from DataTables. */
	void BuildCaches();

	/** Load a DataTable synchronously if needed. */
	UDataTable* LoadDataTable(TSoftObjectPtr<UDataTable>& TablePtr) const;
};
