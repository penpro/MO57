/**
 * =============================================================================
 * MOMedicalSubsystem.h - Medical System DataTable Lookups
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * GameInstanceSubsystem providing DataTable lookups and calculations for the
 * medical system. Caches body part, wound, condition, and treatment definitions
 * for efficient access. Provides calculation helpers for wound parameters,
 * treatment effectiveness, and healing rates.
 *
 * KEY RESPONSIBILITIES:
 * 1. Load and cache medical DataTables (body parts, wounds, conditions, treatments)
 * 2. Provide lookup functions by enum type or ID
 * 3. Calculate wound parameters (bleed rate, infection risk, pain)
 * 4. Calculate treatment effectiveness with skill modifiers
 * 5. Calculate healing rate multipliers
 * 6. Provide display names for medical enums
 *
 * ARCHITECTURE NOTES:
 * - GameInstanceSubsystem: Survives level transitions, one per game instance
 * - Uses TSoftObjectPtr for async-friendly DataTable references
 * - Builds caches on first access (lazy initialization)
 * - Caches keyed by enum type for O(1) lookup
 *
 * CRITICAL PATTERNS:
 * 1. Cache Building:
 *    First lookup -> BuildCaches() -> Load all tables -> Populate TMaps
 *    Subsequent lookups hit cache directly
 *
 * 2. Treatment Effectiveness:
 *    Base effectiveness * skill modifier * self-treatment penalty * body part access
 *
 * 3. Healing Rate Calculation:
 *    Base rate * nutrition * infection penalty * bandage bonus * suture bonus
 *
 * MEDICAL SYSTEM CASCADE (from CLAUDE.md):
 * Wounds (bleed) -> Vitals (blood volume) -> Mental (consciousness)
 *                         |
 *                 Heart/Lung damage -> SpO2/BP -> Death timers
 *                         |
 * Metabolism (glucose) -> Vitals (blood glucose) -> Mental (confusion)
 *                         |
 * Dehydration -> Vitals (+HR, -BP, +Temp) -> Performance penalties
 *
 * KNOWN PITFALLS:
 * 1. DATATABLE PATH: TSoftObjectPtr paths must be set in project settings
 *    or via UDeveloperSettings. Missing tables cause nullptr returns.
 *
 * 2. CACHE INVALIDATION: Caches not invalidated on DataTable reimport.
 *    Restart editor or call BuildCaches() manually after changes.
 *
 * 3. ENUM SYNC: EMOBodyPartType, EMOWoundType, etc. must match DataTable
 *    row names. If enum changes, update DataTable row names.
 *
 * 4. TREATMENT STACKING: GetTreatmentsForWoundType returns multiple matches.
 *    UI should present choices; don't auto-apply all.
 *
 * RELATED FILES:
 * - MOMedicalTypes.h - Enum definitions (wound, condition, consciousness)
 * - MOBodyPartDefinitionRow.h - Body part DataTable row struct
 * - MOAnatomyComponent.h - Per-pawn body parts and wounds
 * - MOVitalsComponent.h - Heart rate, blood pressure, etc.
 * - MOMetabolismComponent.h - Nutrition affecting healing
 * - MOMentalStateComponent.h - Consciousness and shock
 *
 * DATATABLES:
 * - DT_BodyParts (~55 parts) - Body part hierarchy and properties
 * - DT_Wounds - Wound type definitions (bleed rates, infection)
 * - DT_Conditions - Condition effects and triggers
 * - DT_MedicalTreatments - Treatment requirements and effects
 *
 * TESTING CHECKLIST:
 * [ ] GetBodyPartDefinition returns valid data for all part types
 * [ ] GetWoundTypeDefinition returns valid bleed/infection params
 * [ ] GetTreatmentsForWoundType returns appropriate treatments
 * [ ] CalculateTreatmentEffectiveness scales with skill level
 * [ ] Cache survives level transitions
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MOMedicalTypes.h"
#include "MOBodyPartDefinitionRow.h"
#include "MOMedicalSubsystem.generated.h"

class UDataTable;
UCLASS()
class MOFRAMEWORKMEDICAL_API UMOMedicalSubsystem : public UGameInstanceSubsystem
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
