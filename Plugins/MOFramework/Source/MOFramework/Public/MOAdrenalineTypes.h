#pragma once

#include "CoreMinimal.h"
#include "MOAdrenalineTypes.generated.h"

/**
 * =============================================================================
 * MOAdrenalineTypes.h - Adrenaline/Stress Response System Types
 * =============================================================================
 *
 * PURPOSE:
 * Models the physiological adrenaline (epinephrine) response during combat
 * and high-stress situations. Provides gameplay balance between:
 * - Allowing players to fight through injuries (pain masking, reduced bleed)
 * - Punishing recklessness (accuracy loss, tunnel vision)
 * - Rewarding skill progression (experienced fighters stay calmer)
 *
 * =============================================================================
 * DESIGN PHILOSOPHY
 * =============================================================================
 *
 * In a permadeath survival game, pure realism would make combat too punishing.
 * Adrenaline serves as a temporary buffer:
 *
 * 1. PROTECTION: During combat, adrenaline masks pain and slows bleeding
 *    through vasoconstriction, giving players time to finish the fight.
 *
 * 2. COST: High adrenaline reduces accuracy, causes tunnel vision, and
 *    drains stamina faster. You can fight, but not precisely.
 *
 * 3. CRASH: When adrenaline fades post-combat, all those masked wounds
 *    come back. The "adrenaline crash" is when carelessness kills.
 *
 * 4. SKILL GATE: Combat skill determines your "threat threshold" - the
 *    level of danger that triggers full adrenaline response. Experienced
 *    fighters stay calm against lesser threats, improving accuracy.
 *
 * =============================================================================
 * THREAT LEVEL SYSTEM
 * =============================================================================
 *
 * Threats are assessed based on:
 * - Enemy combat level vs player combat level
 * - Number of enemies
 * - Current wound severity
 * - Previous encounters (learned threats)
 *
 * ThreatLevel = (EnemyPower / PlayerPower) * EnemyCount * WoundModifier
 *
 * If ThreatLevel < ThreatThreshold (based on skill), adrenaline response
 * is dampened. This creates progression:
 *
 * - Novice (skill 0-20): Everything triggers max adrenaline
 * - Trained (skill 20-50): Small animals/weak enemies don't trigger
 * - Veteran (skill 50-80): Most single enemies don't trigger
 * - Master (skill 80-100): Only serious threats or wounds trigger
 *
 * =============================================================================
 */

// Forward declarations
class UMOSkillsComponent;
class UMOAnatomyComponent;
class UMOVitalsComponent;

/**
 * Threat level classification for adrenaline response scaling.
 */
UENUM(BlueprintType)
enum class EMOThreatLevel : uint8
{
	/** No threat detected. */
	None,

	/** Minor threat (small prey, single weak enemy). */
	Minimal,

	/** Moderate threat (equal opponent, small group). */
	Moderate,

	/** Significant threat (stronger opponent, multiple enemies). */
	High,

	/** Life-threatening (apex predator, overwhelming force, critical wounds). */
	Extreme,

	MAX UMETA(Hidden)
};

/**
 * Phase of adrenaline response cycle.
 */
UENUM(BlueprintType)
enum class EMOAdrenalinePhase : uint8
{
	/** Baseline - no active adrenaline response. */
	Baseline,

	/** Rising - adrenaline spiking in response to threat. */
	Spiking,

	/** Sustained - in active combat, adrenaline maintained. */
	Sustained,

	/** Crashing - adrenaline wearing off, masked effects returning. */
	Crashing,

	MAX UMETA(Hidden)
};

/**
 * Configuration for adrenaline response based on combat skill level.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOAdrenalineSkillModifiers
{
	GENERATED_BODY()

	/**
	 * Combat skill level this config applies to (0-100).
	 * Configs are interpolated between defined levels.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Skill")
	float SkillLevel = 0.0f;

	/**
	 * Threat threshold - threats below this don't trigger full adrenaline.
	 * 0 = everything triggers, 1 = only extreme threats trigger.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Skill", meta=(ClampMin="0", ClampMax="1"))
	float ThreatThreshold = 0.0f;

	/**
	 * Maximum adrenaline level this skill allows (0-100).
	 * Higher skill = lower max (stays calmer).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Skill")
	float MaxAdrenalineLevel = 100.0f;

	/**
	 * How fast adrenaline spikes (multiplier on base rate).
	 * Lower = slower rise = more controlled response.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Skill", meta=(ClampMin="0.1"))
	float SpikeRateMultiplier = 1.0f;

	/**
	 * How fast adrenaline decays post-combat (multiplier).
	 * Higher = faster recovery.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Skill", meta=(ClampMin="0.1"))
	float DecayRateMultiplier = 1.0f;

	/**
	 * Accuracy penalty reduction at this skill level (0-1).
	 * 0 = full penalty, 1 = no penalty even at max adrenaline.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Skill", meta=(ClampMin="0", ClampMax="1"))
	float AccuracyPenaltyReduction = 0.0f;
};

/**
 * Current adrenaline state - replicated for UI and effects.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOAdrenalineState
{
	GENERATED_BODY()

	// ============================================================================
	// CORE STATE
	// ============================================================================

	/** Current adrenaline level (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline", meta=(ClampMin="0", ClampMax="100"))
	float CurrentLevel = 0.0f;

	/** Current phase of adrenaline response. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline")
	EMOAdrenalinePhase Phase = EMOAdrenalinePhase::Baseline;

	/** Current threat level being responded to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline")
	EMOThreatLevel CurrentThreat = EMOThreatLevel::None;

	/** Time in current phase (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline")
	float TimeInPhase = 0.0f;

	/** Time since last combat event (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline")
	float TimeSinceCombat = 0.0f;

	// ============================================================================
	// WOUND MASKING
	// ============================================================================

	/**
	 * Total pain currently being masked by adrenaline.
	 * This "debt" must be paid when adrenaline crashes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Masking")
	float MaskedPainDebt = 0.0f;

	/**
	 * Bleed rate reduction currently active (mL/s reduction).
	 * Represents vasoconstriction effect.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Masking")
	float ActiveBleedReduction = 0.0f;

	// ============================================================================
	// DERIVED EFFECTS (recalculated each tick)
	// ============================================================================

	/**
	 * Current pain masking percentage (0-1).
	 * Reduces perceived pain from wounds.
	 */
	UPROPERTY(BlueprintReadOnly, Category="MO|Adrenaline|Effects")
	float PainMaskingPercent = 0.0f;

	/**
	 * Current bleed rate reduction percentage (0-1).
	 * Reduces blood loss from active wounds.
	 */
	UPROPERTY(BlueprintReadOnly, Category="MO|Adrenaline|Effects")
	float BleedReductionPercent = 0.0f;

	/**
	 * Current accuracy penalty (0-1).
	 * Applied to weapon sway/spread.
	 */
	UPROPERTY(BlueprintReadOnly, Category="MO|Adrenaline|Effects")
	float AccuracyPenalty = 0.0f;

	/**
	 * Current tunnel vision intensity (0-1).
	 * Affects FOV and peripheral awareness.
	 */
	UPROPERTY(BlueprintReadOnly, Category="MO|Adrenaline|Effects")
	float TunnelVisionIntensity = 0.0f;

	/**
	 * Current heart rate modifier (multiplier on base).
	 * Adrenaline increases heart rate.
	 */
	UPROPERTY(BlueprintReadOnly, Category="MO|Adrenaline|Effects")
	float HeartRateModifier = 1.0f;

	/**
	 * Stamina drain modifier during adrenaline (multiplier).
	 * Adrenaline initially buffs stamina, crash debuffs it.
	 */
	UPROPERTY(BlueprintReadOnly, Category="MO|Adrenaline|Effects")
	float StaminaDrainModifier = 1.0f;

	// ============================================================================
	// HELPER METHODS
	// ============================================================================

	/** Get adrenaline as percentage (0-1). */
	float GetLevelPercent() const { return CurrentLevel / 100.0f; }

	/** Check if in active adrenaline state (not baseline). */
	bool IsActive() const { return Phase != EMOAdrenalinePhase::Baseline; }

	/** Check if currently in the crash phase. */
	bool IsCrashing() const { return Phase == EMOAdrenalinePhase::Crashing; }

	/** Check if adrenaline is high enough to affect accuracy. */
	bool IsAffectingAccuracy() const { return CurrentLevel > 30.0f; }

	/** Check if adrenaline is providing significant pain masking. */
	bool IsMaskingPain() const { return PainMaskingPercent > 0.1f; }
};

/**
 * Configuration for adrenaline system behavior.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOAdrenalineConfig
{
	GENERATED_BODY()

	// ============================================================================
	// SPIKE TRIGGERS
	// ============================================================================

	/** Base adrenaline spike when entering combat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Triggers")
	float CombatEntrySpikeBase = 30.0f;

	/** Adrenaline spike per wound received (scaled by severity). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Triggers")
	float WoundSpikePerSeverity = 0.5f;

	/** Maximum spike from a single wound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Triggers")
	float MaxWoundSpike = 40.0f;

	/** Adrenaline spike when health drops below critical threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Triggers")
	float CriticalHealthSpike = 50.0f;

	// ============================================================================
	// RATE CONSTANTS
	// ============================================================================

	/** Base rate of adrenaline increase per second during spike phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Rates")
	float BaseSpikeRate = 20.0f;

	/** Rate of adrenaline decay per second when not in combat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Rates")
	float BaseDecayRate = 5.0f;

	/** Time without combat events before decay begins (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Rates")
	float CombatTimeoutBeforeDecay = 5.0f;

	/** Duration of crash phase (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Rates")
	float CrashPhaseDuration = 30.0f;

	// ============================================================================
	// EFFECT CURVES (adrenaline level -> effect intensity)
	// ============================================================================

	/**
	 * Pain masking at max adrenaline (0-1).
	 * 0.8 = 80% pain reduction at 100 adrenaline.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Effects", meta=(ClampMin="0", ClampMax="1"))
	float MaxPainMasking = 0.8f;

	/**
	 * Bleed rate reduction at max adrenaline (0-1).
	 * 0.5 = 50% reduced bleeding at 100 adrenaline.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Effects", meta=(ClampMin="0", ClampMax="1"))
	float MaxBleedReduction = 0.5f;

	/**
	 * Accuracy penalty at max adrenaline (0-1).
	 * 0.4 = 40% accuracy loss at 100 adrenaline.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Effects", meta=(ClampMin="0", ClampMax="1"))
	float MaxAccuracyPenalty = 0.4f;

	/**
	 * Tunnel vision intensity at max adrenaline (0-1).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Effects", meta=(ClampMin="0", ClampMax="1"))
	float MaxTunnelVision = 0.6f;

	/**
	 * Heart rate multiplier at max adrenaline.
	 * 2.0 = heart rate doubles at max adrenaline.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Effects", meta=(ClampMin="1.0"))
	float MaxHeartRateMultiplier = 2.0f;

	/**
	 * Adrenaline level where accuracy penalty starts (0-100).
	 * Below this, hands are steady.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Effects", meta=(ClampMin="0", ClampMax="100"))
	float AccuracyPenaltyThreshold = 30.0f;

	/**
	 * Adrenaline level where tunnel vision starts (0-100).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Effects", meta=(ClampMin="0", ClampMax="100"))
	float TunnelVisionThreshold = 50.0f;

	// ============================================================================
	// CRASH EFFECTS
	// ============================================================================

	/**
	 * Stamina drain multiplier during crash phase.
	 * 2.0 = double stamina drain during crash.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Crash", meta=(ClampMin="1.0"))
	float CrashStaminaDrainMultiplier = 2.0f;

	/**
	 * How quickly masked pain "returns" during crash (per second).
	 * 10 = 10 pain debt repaid per second.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Crash")
	float CrashPainReturnRate = 10.0f;

	/**
	 * Blur/disorientation during crash (0-1).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Crash", meta=(ClampMin="0", ClampMax="1"))
	float CrashDisorientationIntensity = 0.3f;

	// ============================================================================
	// SKILL INTEGRATION
	// ============================================================================

	/** Skill ID used for combat skill lookup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Skill")
	FName CombatSkillId = TEXT("combat");

	/** Defined skill breakpoints for adrenaline modifiers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Skill")
	TArray<FMOAdrenalineSkillModifiers> SkillModifiers;

	/** Get default configuration. */
	static FMOAdrenalineConfig GetDefault();
};

/**
 * Information about a threat for adrenaline calculation.
 * Passed when registering combat threats.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOThreatInfo
{
	GENERATED_BODY()

	/** The threatening actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Threat")
	TWeakObjectPtr<AActor> ThreatActor;

	/** Estimated combat power of the threat (0 = harmless, 100 = apex predator). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Threat")
	float ThreatPower = 50.0f;

	/** Number of this threat type (for pack/group calculations). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Threat")
	int32 ThreatCount = 1;

	/** Distance to threat (affects immediacy). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Threat")
	float Distance = 1000.0f;

	/** Whether the threat is actively attacking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Adrenaline|Threat")
	bool bIsAttacking = false;

	/** Calculate threat level enum from power/count. */
	EMOThreatLevel GetThreatLevel(float PlayerCombatPower) const
	{
		float ThreatRatio = (ThreatPower * ThreatCount) / FMath::Max(PlayerCombatPower, 1.0f);

		if (ThreatRatio < 0.25f) return EMOThreatLevel::Minimal;
		if (ThreatRatio < 0.75f) return EMOThreatLevel::Moderate;
		if (ThreatRatio < 1.5f) return EMOThreatLevel::High;
		return EMOThreatLevel::Extreme;
	}
};

// ============================================================================
// INLINE IMPLEMENTATIONS
// ============================================================================

inline FMOAdrenalineConfig FMOAdrenalineConfig::GetDefault()
{
	FMOAdrenalineConfig Config;

	// Set up skill progression breakpoints
	Config.SkillModifiers.SetNum(5);

	// Novice (0): Everything scary, max effects
	Config.SkillModifiers[0].SkillLevel = 0.0f;
	Config.SkillModifiers[0].ThreatThreshold = 0.0f;
	Config.SkillModifiers[0].MaxAdrenalineLevel = 100.0f;
	Config.SkillModifiers[0].SpikeRateMultiplier = 1.5f;
	Config.SkillModifiers[0].DecayRateMultiplier = 0.7f;
	Config.SkillModifiers[0].AccuracyPenaltyReduction = 0.0f;

	// Trained (25): Small threats don't trigger
	Config.SkillModifiers[1].SkillLevel = 25.0f;
	Config.SkillModifiers[1].ThreatThreshold = 0.2f;
	Config.SkillModifiers[1].MaxAdrenalineLevel = 90.0f;
	Config.SkillModifiers[1].SpikeRateMultiplier = 1.2f;
	Config.SkillModifiers[1].DecayRateMultiplier = 0.9f;
	Config.SkillModifiers[1].AccuracyPenaltyReduction = 0.15f;

	// Experienced (50): Most single enemies handled calmly
	Config.SkillModifiers[2].SkillLevel = 50.0f;
	Config.SkillModifiers[2].ThreatThreshold = 0.5f;
	Config.SkillModifiers[2].MaxAdrenalineLevel = 75.0f;
	Config.SkillModifiers[2].SpikeRateMultiplier = 1.0f;
	Config.SkillModifiers[2].DecayRateMultiplier = 1.2f;
	Config.SkillModifiers[2].AccuracyPenaltyReduction = 0.35f;

	// Veteran (75): Only serious threats trigger
	Config.SkillModifiers[3].SkillLevel = 75.0f;
	Config.SkillModifiers[3].ThreatThreshold = 0.75f;
	Config.SkillModifiers[3].MaxAdrenalineLevel = 60.0f;
	Config.SkillModifiers[3].SpikeRateMultiplier = 0.8f;
	Config.SkillModifiers[3].DecayRateMultiplier = 1.5f;
	Config.SkillModifiers[3].AccuracyPenaltyReduction = 0.55f;

	// Master (100): Ice cold, only extreme threats affect
	Config.SkillModifiers[4].SkillLevel = 100.0f;
	Config.SkillModifiers[4].ThreatThreshold = 0.9f;
	Config.SkillModifiers[4].MaxAdrenalineLevel = 40.0f;
	Config.SkillModifiers[4].SpikeRateMultiplier = 0.6f;
	Config.SkillModifiers[4].DecayRateMultiplier = 2.0f;
	Config.SkillModifiers[4].AccuracyPenaltyReduction = 0.75f;

	return Config;
}
