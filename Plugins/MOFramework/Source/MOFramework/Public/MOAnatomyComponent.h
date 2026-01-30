#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOMedicalTypes.h"
#include "MOAnatomyComponent.generated.h"

class UMOVitalsComponent;
class UMOMentalStateComponent;
struct FMOBodyPartDefinitionRow;
struct FMOWoundTypeDefinitionRow;

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOOnBodyPartDamaged, EMOBodyPartType, Part, float, Damage, float, NewHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnBodyPartDestroyed, EMOBodyPartType, Part, bool, bInstantDeath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnWoundInflicted, const FGuid&, WoundId, EMOWoundType, WoundType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnWoundHealed, const FGuid&, WoundId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnConditionAdded, const FGuid&, ConditionId, EMOConditionType, ConditionType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnConditionRemoved, const FGuid&, ConditionId, EMOConditionType, ConditionType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnInstantDeath, EMOBodyPartType, CausePart);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnDeathTimer, EMOBodyPartType, Part, float, RemainingSeconds);

// ============================================================================
// SAVE DATA
// ============================================================================

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOBodyPartSaveEntry
{
	GENERATED_BODY()

	UPROPERTY()
	EMOBodyPartType PartType = EMOBodyPartType::None;

	UPROPERTY()
	EMOBodyPartStatus Status = EMOBodyPartStatus::Healthy;

	UPROPERTY()
	float CurrentHP = 100.0f;

	UPROPERTY()
	float MaxHP = 100.0f;

	UPROPERTY()
	float BoneDensity = 1.0f;
};

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWoundSaveEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid WoundId;

	UPROPERTY()
	EMOBodyPartType BodyPart = EMOBodyPartType::None;

	UPROPERTY()
	EMOWoundType WoundType = EMOWoundType::None;

	UPROPERTY()
	float Severity = 0.0f;

	UPROPERTY()
	float BleedRate = 0.0f;

	UPROPERTY()
	float InfectionRisk = 0.0f;

	UPROPERTY()
	float HealingProgress = 0.0f;

	UPROPERTY()
	bool bIsBandaged = false;

	UPROPERTY()
	bool bIsSutured = false;

	UPROPERTY()
	bool bIsInfected = false;

	UPROPERTY()
	float InfectionSeverity = 0.0f;

	UPROPERTY()
	float TimeSinceInflicted = 0.0f;
};

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOConditionSaveEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid ConditionId;

	UPROPERTY()
	EMOConditionType ConditionType = EMOConditionType::None;

	UPROPERTY()
	EMOBodyPartType AffectedPart = EMOBodyPartType::None;

	UPROPERTY()
	float Severity = 0.0f;

	UPROPERTY()
	float Duration = 0.0f;

	UPROPERTY()
	bool bIsTreated = false;
};

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOAnatomySaveData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMOBodyPartSaveEntry> BodyParts;

	UPROPERTY()
	TArray<FMOWoundSaveEntry> Wounds;

	UPROPERTY()
	TArray<FMOConditionSaveEntry> Conditions;
};

// ============================================================================
// COMPONENT
// ============================================================================

/**
 * Component managing body parts, wounds, and medical conditions.
 * Tracks damage to individual body parts with finger/toe-level granularity.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOAnatomyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOAnatomyComponent();

	// ============================================================================
	// REPLICATED STATE
	// ============================================================================

	/** State of all body parts. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="MO|Anatomy")
	TArray<FMOBodyPartState> BodyParts;

	/** Active wounds. Uses FastArraySerializer for efficient replication. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="MO|Anatomy")
	FMOWoundList Wounds;

	/** Active conditions. Uses FastArraySerializer for efficient replication. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="MO|Anatomy")
	FMOConditionList Conditions;

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Fired when a body part takes damage. */
	UPROPERTY(BlueprintAssignable, Category="MO|Anatomy|Events")
	FMOOnBodyPartDamaged OnBodyPartDamaged;

	/** Fired when a body part is destroyed. */
	UPROPERTY(BlueprintAssignable, Category="MO|Anatomy|Events")
	FMOOnBodyPartDestroyed OnBodyPartDestroyed;

	/** Fired when a wound is inflicted. */
	UPROPERTY(BlueprintAssignable, Category="MO|Anatomy|Events")
	FMOOnWoundInflicted OnWoundInflicted;

	/** Fired when a wound fully heals. */
	UPROPERTY(BlueprintAssignable, Category="MO|Anatomy|Events")
	FMOOnWoundHealed OnWoundHealed;

	/** Fired when a condition is added. */
	UPROPERTY(BlueprintAssignable, Category="MO|Anatomy|Events")
	FMOOnConditionAdded OnConditionAdded;

	/** Fired when a condition is removed. */
	UPROPERTY(BlueprintAssignable, Category="MO|Anatomy|Events")
	FMOOnConditionRemoved OnConditionRemoved;

	/** Fired on instant death (brain/heart destroyed). */
	UPROPERTY(BlueprintAssignable, Category="MO|Anatomy|Events")
	FMOOnInstantDeath OnInstantDeath;

	/** Fired when a death timer is ticking (lungs destroyed, etc). */
	UPROPERTY(BlueprintAssignable, Category="MO|Anatomy|Events")
	FMOOnDeathTimer OnDeathTimer;

	// ============================================================================
	// DAMAGE API
	// ============================================================================

	/**
	 * Inflict damage to a body part, potentially creating a wound.
	 * @param Part The body part to damage.
	 * @param Damage Amount of damage.
	 * @param WoundType Type of wound to create.
	 * @return True if damage was applied.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Anatomy|Damage")
	bool InflictDamage(EMOBodyPartType Part, float Damage, EMOWoundType WoundType);

	/**
	 * Inflict damage with a custom wound.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Anatomy|Damage")
	bool InflictWound(const FMOWound& Wound);

	/**
	 * Apply healing to a wound.
	 * @param WoundId The wound to heal.
	 * @param HealAmount Amount of healing progress to add (0-100).
	 * @return True if healing was applied.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Anatomy|Damage")
	bool HealWound(const FGuid& WoundId, float HealAmount);

	// ============================================================================
	// TREATMENT API
	// ============================================================================

	/**
	 * Apply medical treatment to a wound.
	 * @param WoundId The wound to treat.
	 * @param TreatmentId ID of the treatment from DataTable.
	 * @param MedicSkillLevel Skill level of the person applying treatment.
	 * @param bIsSelfTreatment True if treating oneself.
	 * @return True if treatment was successfully applied.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Anatomy|Treatment")
	bool ApplyTreatment(const FGuid& WoundId, FName TreatmentId, int32 MedicSkillLevel, bool bIsSelfTreatment = false);

	/**
	 * Apply bandage to a wound.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Anatomy|Treatment")
	bool ApplyBandage(const FGuid& WoundId, float Quality = 1.0f);

	/**
	 * Apply sutures to a wound.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Anatomy|Treatment")
	bool ApplySutures(const FGuid& WoundId, float Quality = 1.0f);

	// ============================================================================
	// CONDITION API
	// ============================================================================

	/**
	 * Add a medical condition.
	 * @param ConditionType Type of condition.
	 * @param Part Affected body part (None = systemic).
	 * @param InitialSeverity Starting severity (0-100).
	 * @return True if condition was added.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Anatomy|Conditions")
	bool AddCondition(EMOConditionType ConditionType, EMOBodyPartType Part = EMOBodyPartType::None, float InitialSeverity = 10.0f);

	/**
	 * Remove a condition by ID.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Anatomy|Conditions")
	bool RemoveCondition(const FGuid& ConditionId);

	/**
	 * Check if a condition type is active.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Anatomy|Conditions")
	bool HasCondition(EMOConditionType ConditionType) const;

	/**
	 * Get a condition by type.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Anatomy|Conditions")
	bool GetConditionByType(EMOConditionType ConditionType, FMOCondition& OutCondition) const;

	// ============================================================================
	// QUERY API
	// ============================================================================

	/**
	 * Get total blood loss rate from all wounds (mL/second).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Anatomy|Query")
	float GetTotalBleedRate() const;

	/**
	 * Get total pain level from all wounds and conditions (0-100).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Anatomy|Query")
	float GetTotalPainLevel() const;

	/**
	 * Check if a body part is functional.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Anatomy|Query")
	bool IsBodyPartFunctional(EMOBodyPartType Part) const;

	/**
	 * Get a body part's state.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Anatomy|Query")
	bool GetBodyPartState(EMOBodyPartType Part, FMOBodyPartState& OutState) const;

	/**
	 * Get all wounds on a specific body part.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Anatomy|Query")
	TArray<FMOWound> GetWoundsOnPart(EMOBodyPartType Part) const;

	/**
	 * Get all active wounds.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Anatomy|Query")
	TArray<FMOWound> GetAllWounds() const;

	/**
	 * Get all active conditions.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Anatomy|Query")
	TArray<FMOCondition> GetAllConditions() const;

	/**
	 * Check if character can move (legs functional).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Anatomy|Query")
	bool CanMove() const;

	/**
	 * Check if character can grip (hands functional).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Anatomy|Query")
	bool CanGrip() const;

	/**
	 * Check if character can see (at least one eye functional).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Anatomy|Query")
	bool CanSee() const;

	/**
	 * Check if character can hear (at least one ear functional).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Anatomy|Query")
	bool CanHear() const;

	// ============================================================================
	// PERSISTENCE
	// ============================================================================

	/**
	 * Build save data from current state.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Anatomy|Save")
	void BuildSaveData(FMOAnatomySaveData& OutSaveData) const;

	/**
	 * Apply save data to restore state (authority only).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Anatomy|Save")
	bool ApplySaveDataAuthority(const FMOAnatomySaveData& InSaveData);

	// ============================================================================
	// REPLICATION CALLBACKS (called by FastArray)
	// ============================================================================

	void OnWoundReplicatedAdd(const FMOWound& Wound);
	void OnWoundReplicatedChange(const FMOWound& Wound);
	void OnWoundReplicatedRemove(const FMOWound& Wound);

	void OnConditionReplicatedAdd(const FMOCondition& Condition);
	void OnConditionReplicatedChange(const FMOCondition& Condition);
	void OnConditionReplicatedRemove(const FMOCondition& Condition);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Tick interval for wound/condition processing. */
	UPROPERTY(EditAnywhere, Category="MO|Anatomy|Config", meta=(ClampMin="0.1"))
	float TickInterval = 1.0f;

	/** Time scale multiplier (1.0 = real time). */
	UPROPERTY(EditAnywhere, Category="MO|Anatomy|Config", meta=(ClampMin="0.01"))
	float TimeScaleMultiplier = 1.0f;

	// ============================================================================
	// INTERNAL STATE
	// ============================================================================

	/** Timer for periodic processing. */
	FTimerHandle TickTimerHandle;

	/** Death timer for non-instant death body parts (lungs, etc). */
	FTimerHandle DeathTimerHandle;

	/** Which vital body part triggered the death timer. */
	EMOBodyPartType DeathTimerPart = EMOBodyPartType::None;

	/** Remaining time on death timer. */
	float DeathTimerRemaining = 0.0f;

	/** Cached reference to vitals component. */
	UPROPERTY(Transient)
	TObjectPtr<UMOVitalsComponent> CachedVitalsComp;

	/** Cached reference to mental state component. */
	UPROPERTY(Transient)
	TObjectPtr<UMOMentalStateComponent> CachedMentalComp;

	// ============================================================================
	// INTERNAL METHODS
	// ============================================================================

	/** Initialize all body parts from DataTable. */
	void InitializeBodyParts();

	/** Get body part state pointer for modification. */
	FMOBodyPartState* GetBodyPartStateMutable(EMOBodyPartType Part);

	/** Periodic tick for wound healing, infection, etc. */
	void TickAnatomy();

	/** Process wound healing and infection. */
	void ProcessWound(FMOWound& Wound, float DeltaTime);

	/** Process condition progression. */
	void ProcessCondition(FMOCondition& Condition, float DeltaTime);

	/** Check for instant death conditions. */
	void CheckDeathConditions(EMOBodyPartType DestroyedPart);

	/** Start death timer for vital organ. */
	void StartDeathTimer(EMOBodyPartType Part, float Seconds);

	/** Tick the death timer. */
	void TickDeathTimer();

	/** Apply blood loss to vitals component. */
	void ApplyBloodLoss(float AmountML);

	/** Apply shock to mental state component. */
	void ApplyShock(float Amount);

	/** Get body part definition from DataTable. */
	bool GetBodyPartDefinition(EMOBodyPartType Part, FMOBodyPartDefinitionRow& OutDef) const;

	/** Get wound type definition from DataTable. */
	bool GetWoundTypeDefinition(EMOWoundType Type, FMOWoundTypeDefinitionRow& OutDef) const;
};
