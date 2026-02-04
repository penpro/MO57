#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOAdrenalineTypes.h"
#include "MOAdrenalineComponent.generated.h"

class UMOVitalsComponent;
class UMOAnatomyComponent;
class UMOMentalStateComponent;
class UMOSkillsComponent;

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnAdrenalineChanged, float, OldLevel, float, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnAdrenalinePhaseChanged, EMOAdrenalinePhase, OldPhase, EMOAdrenalinePhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnThreatDetected, AActor*, ThreatActor, EMOThreatLevel, ThreatLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnAdrenalineCrashStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnAdrenalineCrashEnded);

// ============================================================================
// SAVE DATA
// ============================================================================

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOAdrenalineSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	FMOAdrenalineState State;
};

// ============================================================================
// COMPONENT
// ============================================================================

/**
 * Component managing the adrenaline/stress response system.
 *
 * Integrates with:
 * - UMOVitalsComponent: Heart rate modulation, stamina effects
 * - UMOAnatomyComponent: Wound detection for pain masking, bleed reduction
 * - UMOMentalStateComponent: Shock and trauma interaction
 * - UMOSkillsComponent: Combat skill affects adrenaline response
 *
 * Key Gameplay Features:
 * - Pain Masking: High adrenaline reduces perceived pain from wounds
 * - Bleed Reduction: Vasoconstriction slows blood loss during combat
 * - Accuracy Penalty: Shaky hands at high adrenaline reduces weapon accuracy
 * - Skill Progression: Experienced fighters have dampened adrenaline response
 * - Crash Phase: When adrenaline fades, masked effects come back
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOAdrenalineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOAdrenalineComponent();

	// ============================================================================
	// REPLICATED STATE
	// ============================================================================

	/** Current adrenaline state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="MO|Adrenaline")
	FMOAdrenalineState State;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Adrenaline system configuration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Config")
	FMOAdrenalineConfig Config;

	/** Time scale multiplier (1.0 = real time). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Config")
	float TimeScaleMultiplier = 1.0f;

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Fired when adrenaline level changes significantly. */
	UPROPERTY(BlueprintAssignable, Category="MO|Adrenaline|Events")
	FMOOnAdrenalineChanged OnAdrenalineChanged;

	/** Fired when adrenaline phase transitions. */
	UPROPERTY(BlueprintAssignable, Category="MO|Adrenaline|Events")
	FMOOnAdrenalinePhaseChanged OnPhaseChanged;

	/** Fired when a new threat is detected. */
	UPROPERTY(BlueprintAssignable, Category="MO|Adrenaline|Events")
	FMOOnThreatDetected OnThreatDetected;

	/** Fired when crash phase begins. */
	UPROPERTY(BlueprintAssignable, Category="MO|Adrenaline|Events")
	FMOOnAdrenalineCrashStarted OnCrashStarted;

	/** Fired when crash phase ends (back to baseline). */
	UPROPERTY(BlueprintAssignable, Category="MO|Adrenaline|Events")
	FMOOnAdrenalineCrashEnded OnCrashEnded;

	// ============================================================================
	// COMBAT API - Call these from your combat system
	// ============================================================================

	/**
	 * Register entering combat with a threat.
	 * Triggers adrenaline spike based on threat assessment.
	 *
	 * @param ThreatInfo Information about the threat being engaged.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Adrenaline|Combat")
	void EnterCombat(const FMOThreatInfo& ThreatInfo);

	/**
	 * Register exiting combat.
	 * Begins adrenaline decay after timeout.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Adrenaline|Combat")
	void ExitCombat();

	/**
	 * Register receiving damage/wound.
	 * Spikes adrenaline and begins pain masking.
	 *
	 * @param WoundSeverity Severity of the wound (0-100).
	 * @param PainAmount Pain value being masked.
	 * @param BleedRate Bleed rate being reduced.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Adrenaline|Combat")
	void OnWoundReceived(float WoundSeverity, float PainAmount, float BleedRate);

	/**
	 * Register a new threat in awareness.
	 * Used for threat assessment without full combat entry.
	 *
	 * @param ThreatInfo Information about the new threat.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Adrenaline|Combat")
	void RegisterThreat(const FMOThreatInfo& ThreatInfo);

	/**
	 * Refresh combat timer (extends sustained phase).
	 * Call when combat actions occur to prevent decay.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Adrenaline|Combat")
	void RefreshCombatTimer();

	// ============================================================================
	// QUERY API
	// ============================================================================

	/** Get current adrenaline level (0-100). */
	UFUNCTION(BlueprintPure, Category="MO|Adrenaline|Query")
	float GetAdrenalineLevel() const { return State.CurrentLevel; }

	/** Get adrenaline as percentage (0-1). */
	UFUNCTION(BlueprintPure, Category="MO|Adrenaline|Query")
	float GetAdrenalinePercent() const { return State.GetLevelPercent(); }

	/** Get current adrenaline phase. */
	UFUNCTION(BlueprintPure, Category="MO|Adrenaline|Query")
	EMOAdrenalinePhase GetPhase() const { return State.Phase; }

	/** Check if adrenaline is active (not baseline). */
	UFUNCTION(BlueprintPure, Category="MO|Adrenaline|Query")
	bool IsAdrenalineActive() const { return State.IsActive(); }

	/** Check if in crash phase. */
	UFUNCTION(BlueprintPure, Category="MO|Adrenaline|Query")
	bool IsCrashing() const { return State.IsCrashing(); }

	// ============================================================================
	// EFFECT QUERIES - Use these for gameplay systems
	// ============================================================================

	/**
	 * Get current pain masking percentage.
	 * Apply this to reduce effective pain from wounds.
	 * Returns 0-1 where 1 = full masking.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Adrenaline|Effects")
	float GetPainMaskingPercent() const { return State.PainMaskingPercent; }

	/**
	 * Get current bleed reduction percentage.
	 * Apply this to reduce blood loss rate.
	 * Returns 0-1 where 1 = full reduction.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Adrenaline|Effects")
	float GetBleedReductionPercent() const { return State.BleedReductionPercent; }

	/**
	 * Get current accuracy penalty.
	 * Apply this to weapon spread/sway.
	 * Returns 0-1 where 1 = maximum penalty.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Adrenaline|Effects")
	float GetAccuracyPenalty() const { return State.AccuracyPenalty; }

	/**
	 * Get current tunnel vision intensity.
	 * Apply this to FOV/vignette effects.
	 * Returns 0-1.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Adrenaline|Effects")
	float GetTunnelVisionIntensity() const { return State.TunnelVisionIntensity; }

	/**
	 * Get current heart rate modifier.
	 * Apply this to vitals component heart rate.
	 * Returns multiplier (1.0 = normal, 2.0 = doubled).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Adrenaline|Effects")
	float GetHeartRateModifier() const { return State.HeartRateModifier; }

	/**
	 * Get current stamina drain modifier.
	 * Returns multiplier (1.0 = normal).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Adrenaline|Effects")
	float GetStaminaDrainModifier() const { return State.StaminaDrainModifier; }

	/**
	 * Calculate effective accuracy given base accuracy.
	 * Convenience function that applies adrenaline penalty.
	 *
	 * @param BaseAccuracy Base accuracy value (0-1).
	 * @return Effective accuracy after adrenaline penalty.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Adrenaline|Effects")
	float CalculateEffectiveAccuracy(float BaseAccuracy) const;

	/**
	 * Calculate effective bleed rate given base rate.
	 * Applies adrenaline vasoconstriction reduction.
	 *
	 * @param BaseBleedRate Base bleed rate in mL/s.
	 * @return Effective bleed rate after reduction.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Adrenaline|Effects")
	float CalculateEffectiveBleedRate(float BaseBleedRate) const;

	// ============================================================================
	// SKILL INTEGRATION
	// ============================================================================

	/**
	 * Get current combat skill level (cached from skills component).
	 * Returns 0 if no skills component found.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Adrenaline|Skill")
	float GetCombatSkillLevel() const;

	/**
	 * Get the effective threat threshold based on current skill.
	 * Threats below this threshold have dampened adrenaline response.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Adrenaline|Skill")
	float GetThreatThreshold() const;

	/**
	 * Assess threat level relative to player capability.
	 *
	 * @param ThreatInfo The threat to assess.
	 * @return Threat level classification.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Adrenaline|Skill")
	EMOThreatLevel AssessThreatLevel(const FMOThreatInfo& ThreatInfo) const;

	// ============================================================================
	// PERSISTENCE
	// ============================================================================

	/** Build save data from current state. */
	UFUNCTION(BlueprintCallable, Category="MO|Adrenaline|Save")
	void BuildSaveData(FMOAdrenalineSaveData& OutSaveData) const;

	/** Apply save data to restore state. */
	UFUNCTION(BlueprintCallable, Category="MO|Adrenaline|Save")
	bool ApplySaveData(const FMOAdrenalineSaveData& InSaveData);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// ============================================================================
	// INTERNAL STATE
	// ============================================================================

	/** Timer for periodic updates. */
	FTimerHandle TickTimerHandle;

	/** Tick interval in seconds. */
	float TickInterval = 0.1f;

	/** Whether currently in combat. */
	bool bInCombat = false;

	/** Active threats being tracked. */
	TArray<FMOThreatInfo> ActiveThreats;

	/** Cached combat skill level. */
	float CachedCombatSkill = 0.0f;

	/** Time until cached skill refresh. */
	float SkillCacheTimer = 0.0f;

	// ============================================================================
	// CACHED COMPONENT REFERENCES
	// ============================================================================

	UPROPERTY(Transient)
	TObjectPtr<UMOVitalsComponent> CachedVitalsComp;

	UPROPERTY(Transient)
	TObjectPtr<UMOAnatomyComponent> CachedAnatomyComp;

	UPROPERTY(Transient)
	TObjectPtr<UMOMentalStateComponent> CachedMentalComp;

	UPROPERTY(Transient)
	TObjectPtr<UMOSkillsComponent> CachedSkillsComp;

	// ============================================================================
	// INTERNAL METHODS
	// ============================================================================

	/** Periodic update tick. */
	void TickAdrenaline();

	/** Update adrenaline level based on current phase. */
	void UpdateAdrenalineLevel(float DeltaTime);

	/** Recalculate all derived effects from current level. */
	void RecalculateEffects();

	/** Transition to a new phase. */
	void TransitionToPhase(EMOAdrenalinePhase NewPhase);

	/** Process spiking phase. */
	void ProcessSpikingPhase(float DeltaTime);

	/** Process sustained phase. */
	void ProcessSustainedPhase(float DeltaTime);

	/** Process crashing phase. */
	void ProcessCrashingPhase(float DeltaTime);

	/** Get interpolated skill modifiers for current skill level. */
	FMOAdrenalineSkillModifiers GetInterpolatedSkillModifiers() const;

	/** Apply heart rate modifier to vitals. */
	void ApplyVitalsModifiers();

	/** Refresh cached component references. */
	void RefreshCachedComponents();

	/** Refresh cached combat skill. */
	void RefreshCachedSkill();

	/** Calculate spike amount for entering combat. */
	float CalculateCombatEntrySpike(const FMOThreatInfo& ThreatInfo) const;

	/** Calculate spike amount for receiving wound. */
	float CalculateWoundSpike(float Severity) const;

	/** Broadcast change events if significant. */
	void CheckAndBroadcastChanges(float OldLevel, EMOAdrenalinePhase OldPhase);
};
