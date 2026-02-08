#pragma once

#include "CoreMinimal.h"
#include "MOActivityTypes.generated.h"

/**
 * Activity level for physical exertion tracking.
 * Used to drive calorie expenditure, stamina drain, and fatigue accumulation.
 *
 * =============================================================================
 * INTEGRATION GUIDE
 * =============================================================================
 *
 * The activity system connects movement, crafting, building, and combat to the
 * medical/metabolism systems. External systems call SetActivityLevel() to report
 * what the player is doing, and the vitals component handles the downstream effects.
 *
 * INTEGRATION POINTS:
 *
 * 1. MOVEMENT SYSTEM (Required Integration)
 *    - In your Character or CharacterMovementComponent:
 *    - Call VitalsComp->SetActivityLevel() when movement mode changes
 *    - Example integration in Character::Tick():
 *      if (VitalsComp)
 *      {
 *          float Speed = GetVelocity().Size();
 *          if (Speed > SprintThreshold)
 *              VitalsComp->SetActivityLevel(EMOActivityLevel::Sprinting);
 *          else if (Speed > JogThreshold)
 *              VitalsComp->SetActivityLevel(EMOActivityLevel::Jogging);
 *          else if (Speed > WalkThreshold)
 *              VitalsComp->SetActivityLevel(EMOActivityLevel::Walking);
 *          else
 *              VitalsComp->SetActivityLevel(EMOActivityLevel::Idle);
 *      }
 *
 * 2. CRAFTING SYSTEM (Optional Integration)
 *    - When player is at crafting station, set appropriate work level
 *    - Map recipe ActionId to activity level:
 *      - "Weave", "Carve" -> LightWork
 *      - "Hammer", "Saw", "Cook" -> MediumWork
 *      - "Forge", "Smelt" -> HeavyWork
 *    - When leaving station or craft completes, restore previous activity
 *
 * 3. BUILDING SYSTEM (Optional Integration)
 *    - When player is actively building (interacting with ghost):
 *      - Map build action to activity: "Dig" -> HeavyWork, "Assemble" -> MediumWork
 *    - When stepping away or build completes, restore Idle
 *
 * 4. COMBAT SYSTEM (Required Integration)
 *    - On entering combat: VitalsComp->SetActivityLevel(EMOActivityLevel::Combat)
 *    - On exiting combat: Restore to movement-based activity
 *    - Combat activity has high stamina drain for balance
 *
 * 5. UI INTEGRATION
 *    - Bind to OnStaminaChanged for stamina bar updates
 *    - Bind to OnStaminaDepleted to show exhaustion indicator
 *    - Use GetStaminaPercent() for bar fill amount
 *    - Use CanStartActivity() to gray out sprint button when stamina low
 *
 * STAMINA FLOW:
 *    Walking/Idle -> Stamina recovers
 *    Jogging -> Moderate drain
 *    Sprinting/Combat -> Fast drain
 *    Depleted -> Forced downgrade to lower activity
 *
 * CALORIE FLOW:
 *    Activity -> Calorie burn (via MOMetabolismComponent)
 *    Calorie burn -> Uses glycogen/fat stores
 *    Low stores -> Reduced stamina recovery
 *
 * =============================================================================
 */
UENUM(BlueprintType)
enum class EMOActivityLevel : uint8
{
	/** Sleeping, lying down. Minimal energy expenditure. */
	Resting,

	/** Standing still or sitting. BMR only. */
	Idle,

	/** Walking at comfortable pace. ~3-4 km/h. */
	Walking,

	/** Jogging at moderate pace. ~8-10 km/h. */
	Jogging,

	/** Full sprint. ~15-20+ km/h. High stamina drain. */
	Sprinting,

	/** Light crafting: weaving, carving, simple assembly. */
	LightWork,

	/** Medium crafting: hammering, sawing, cooking. */
	MediumWork,

	/** Heavy crafting: forging, chopping, mining. */
	HeavyWork,

	/** Active combat: fighting, dodging. Maximum exertion. */
	Combat,

	MAX UMETA(Hidden)
};

/**
 * Configuration for a specific activity level.
 * Defines energy costs, stamina drain rates, and other activity-specific parameters.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOActivityConfig
{
	GENERATED_BODY()

	/** Multiplier applied to BMR for calorie expenditure (1.0 = BMR only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity")
	float CalorieMultiplier = 1.0f;

	/** Exertion level this activity generates (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity", meta=(ClampMin="0", ClampMax="100"))
	float ExertionLevel = 0.0f;

	/** Stamina drain per second (0-100 scale). 0 = no drain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity", meta=(ClampMin="0"))
	float StaminaDrainPerSecond = 0.0f;

	/** Fatigue accumulation per hour of sustained activity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity", meta=(ClampMin="0"))
	float FatiguePerHour = 0.0f;

	/** Whether this activity counts as cardio training. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity")
	bool bIsCardioTraining = false;

	/** Whether this activity counts as strength training. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity")
	bool bIsStrengthTraining = false;

	/** Training intensity if this is a training activity (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity", meta=(ClampMin="0", ClampMax="1"))
	float TrainingIntensity = 0.0f;

	/** Whether stamina is required to maintain this activity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity")
	bool bRequiresStamina = false;

	/** Minimum stamina required to start this activity (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity", meta=(ClampMin="0", ClampMax="1"))
	float MinimumStaminaToStart = 0.0f;

	/** Get default config for an activity level. */
	static FMOActivityConfig GetDefaultConfig(EMOActivityLevel Level);
};

/**
 * Current activity state tracking.
 * Replicated for UI and network sync.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOActivityState
{
	GENERATED_BODY()

	/** Current activity level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity")
	EMOActivityLevel CurrentActivity = EMOActivityLevel::Idle;

	/** Time spent in current activity (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity")
	float TimeInCurrentActivity = 0.0f;

	/** Total time active today (seconds, excludes Resting/Idle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity")
	float TotalActiveTimeToday = 0.0f;

	/** Current stamina (0-100). Drains with intense activity, recovers at rest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity", meta=(ClampMin="0", ClampMax="100"))
	float CurrentStamina = 100.0f;

	/** Maximum stamina capacity (affected by fitness). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Activity")
	float MaxStamina = 100.0f;

	/** Get stamina as percentage (0-1). */
	float GetStaminaPercent() const { return MaxStamina > 0.0f ? CurrentStamina / MaxStamina : 0.0f; }

	/** Check if stamina is depleted. */
	bool IsStaminaDepleted() const { return CurrentStamina <= 1.0f; }

	/** Check if in an active state (not resting or idle). */
	bool IsActive() const { return CurrentActivity > EMOActivityLevel::Idle; }
};
