/**
 * =============================================================================
 * MOMentalTypes.h - Consciousness & Mental State Types
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Defines types for the mental state simulation: consciousness levels, shock,
 * trauma, and cognitive effects. Drives visual impairment, motor control loss,
 * and unconsciousness states.
 *
 * CONSCIOUSNESS CASCADE:
 * Trauma → Shock → Consciousness drop → Motor control loss → Death
 *
 * CONSCIOUSNESS LEVELS (from MOBodyPartTypes.h):
 * - Alert: Normal function
 * - Drowsy: Mild impairment
 * - Confused: Moderate impairment, stumbling
 * - Stuporous: Severe impairment, requires assistance
 * - Unconscious: No motor control, vulnerable
 * - Coma: Requires medical intervention
 * - Dead: Terminal state
 *
 * VISUAL/MOTOR EFFECTS:
 * - AimShakeIntensity: Weapon sway (affects accuracy)
 * - TunnelVisionIntensity: Peripheral vision loss (affects awareness)
 * - BlurredVisionIntensity: Central vision blur (affects identification)
 * - StumblingChance: Random stumble events (affects movement)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] INTENSITY RANGE: All effect intensities are 0-1, NOT 0-100.
 *   1.0 = maximum effect (complete tunnel vision, constant stumbling).
 *
 * [2024-02] SHOCK VS TRAUMA: ShockAccumulation is physical shock (blood loss,
 *   pain). TraumaticStress is psychological (witnessing death). Both affect
 *   consciousness but recover differently.
 *
 * [2024-02] MOTOR CONTROL: HasMotorControl() returns false at Unconscious or
 *   worse. Use this to block player input, not just movement speed penalties.
 *
 * [2024-02] MORALE FATIGUE: Long-term stat that accumulates over days. Doesn't
 *   reset with sleep. Requires rest, good food, positive events to recover.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOMentalStateComponent.h - Uses these types for simulation
 * - MOVitalsComponent.h - Blood loss contributes to shock
 * - MOAnatomyComponent.h - Wounds contribute to shock/trauma
 * - MOAdrenalineComponent.h - Combat stress affects mental state
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOBodyPartTypes.h"  // For EMOConsciousnessLevel
#include "MOMentalTypes.generated.h"

/**
 * Mental and cognitive state.
 * See file header for consciousness cascade and effect details.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORKCORE_API FMOMentalState
{
	GENERATED_BODY()

	/** Current consciousness level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Consciousness")
	EMOConsciousnessLevel Consciousness = EMOConsciousnessLevel::Alert;

	/** Shock accumulation from trauma (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Shock", meta=(ClampMin="0", ClampMax="100"))
	float ShockAccumulation = 0.0f;

	/** Traumatic stress from witnessing events (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Stress", meta=(ClampMin="0", ClampMax="100"))
	float TraumaticStress = 0.0f;

	/** Long-term morale fatigue (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Stress", meta=(ClampMin="0", ClampMax="100"))
	float MoraleFatigue = 0.0f;

	// ---- Visual/Motor Effects (0-1 intensity) ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Effects", meta=(ClampMin="0", ClampMax="1"))
	float AimShakeIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Effects", meta=(ClampMin="0", ClampMax="1"))
	float TunnelVisionIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Effects", meta=(ClampMin="0", ClampMax="1"))
	float BlurredVisionIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Effects", meta=(ClampMin="0", ClampMax="1"))
	float StumblingChance = 0.0f;

	// ---- Control Checks ----

	/** Check if character has motor control. */
	bool HasMotorControl() const
	{
		return Consciousness < EMOConsciousnessLevel::Unconscious;
	}

	/** Check if character can make decisions. */
	bool CanMakeDecisions() const
	{
		return Consciousness <= EMOConsciousnessLevel::Confused;
	}

	/** Check if character can perform complex actions. */
	bool CanPerformComplexActions() const
	{
		return Consciousness == EMOConsciousnessLevel::Alert;
	}
};
