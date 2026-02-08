#pragma once

#include "CoreMinimal.h"
#include "MOBodyPartTypes.h"  // For EMOConsciousnessLevel
#include "MOMentalTypes.generated.h"

/**
 * Mental and cognitive state.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOMentalState
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
