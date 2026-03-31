/**
 * =============================================================================
 * MOPersonalityComponent.h - Character Personality Traits
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Manages personality traits for a character. Personality affects task
 * performance, mood responses, and social interactions. Uses a three-axis
 * model with values ranging from -1.0 to 1.0.
 *
 * PERSONALITY AXES:
 * - Conscientiousness: Diligent (-1) to Adaptable (+1)
 * - Sociability: Reserved (-1) to Social (+1)
 * - Stability: Volatile (-1) to Stable (+1)
 *
 * EFFECTS:
 * - Diligent characters take longer but produce higher quality
 * - Social characters perform better when others are nearby
 * - Stable characters have less mood variance
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-03] INITIALIZATION: Traits default to 0 (neutral). Call RandomizeTraits()
 *   at spawn or set explicitly via SetTraits(). AI pawns should randomize.
 *
 * [2026-03] PERFORMANCE MODIFIERS: GetTaskSpeedModifier() and GetTaskQualityModifier()
 *   return multipliers based on conscientiousness. These are suggestions -
 *   task systems decide whether to apply them.
 *
 * =============================================================================
 * RELATED FILES: MOColonyTypes.h, MOCharacterHistoryComponent.h, MOSurvivorController.h
 * LAST UPDATED: 2026-03-28
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOColonyTypes.h"
#include "MOPersonalityComponent.generated.h"

/**
 * Delegate fired when personality traits change.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOPersonalityChangedDelegate, const FMOPersonalityTraits&, NewTraits);

/**
 * Component managing character personality traits.
 * See file header for personality model and effects.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOPersonalityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOPersonalityComponent();

	// ============================================================================
	// TRAIT ACCESS
	// ============================================================================

	/** Get all personality traits. */
	UFUNCTION(BlueprintPure, Category = "MO|Personality")
	const FMOPersonalityTraits& GetTraits() const { return Traits; }

	/** Set all personality traits. */
	UFUNCTION(BlueprintCallable, Category = "MO|Personality")
	void SetTraits(const FMOPersonalityTraits& NewTraits);

	/** Get a specific trait value by axis. */
	UFUNCTION(BlueprintPure, Category = "MO|Personality")
	float GetTraitValue(EPersonalityAxis Axis) const { return Traits.GetTraitValue(Axis); }

	/** Set a specific trait value by axis. */
	UFUNCTION(BlueprintCallable, Category = "MO|Personality")
	void SetTraitValue(EPersonalityAxis Axis, float Value);

	/** Randomize all personality traits. */
	UFUNCTION(BlueprintCallable, Category = "MO|Personality")
	void RandomizeTraits();

	// ============================================================================
	// PERSONALITY EFFECTS
	// ============================================================================

	/**
	 * Get task speed modifier based on conscientiousness.
	 * Diligent (-1): 0.8x speed, Adaptable (+1): 1.2x speed
	 * @return Speed multiplier (0.8 to 1.2)
	 */
	UFUNCTION(BlueprintPure, Category = "MO|Personality")
	float GetTaskSpeedModifier() const;

	/**
	 * Get task quality modifier based on conscientiousness.
	 * Diligent (-1): 1.2x quality, Adaptable (+1): 0.8x quality
	 * @return Quality multiplier (0.8 to 1.2)
	 */
	UFUNCTION(BlueprintPure, Category = "MO|Personality")
	float GetTaskQualityModifier() const;

	/**
	 * Get social presence bonus/penalty based on sociability and nearby characters.
	 * @param NearbyCharacterCount Number of colony members within social range
	 * @return Performance modifier (-0.15 to +0.15)
	 */
	UFUNCTION(BlueprintPure, Category = "MO|Personality")
	float GetSocialPresenceModifier(int32 NearbyCharacterCount) const;

	/**
	 * Get mood variance based on stability.
	 * Volatile (-1): High variance, Stable (+1): Low variance
	 * @return Mood variance multiplier (0.5 to 1.5)
	 */
	UFUNCTION(BlueprintPure, Category = "MO|Personality")
	float GetMoodVarianceModifier() const;

	// ============================================================================
	// DESCRIPTORS
	// ============================================================================

	/**
	 * Get a human-readable description of a trait axis value.
	 * e.g., "Very Diligent", "Somewhat Social", "Neutral"
	 */
	UFUNCTION(BlueprintPure, Category = "MO|Personality")
	FText GetTraitDescription(EPersonalityAxis Axis) const;

	/**
	 * Get a short personality summary.
	 * e.g., "Diligent, Social, Stable"
	 */
	UFUNCTION(BlueprintPure, Category = "MO|Personality")
	FText GetPersonalitySummary() const;

	// ============================================================================
	// EVENTS
	// ============================================================================

	/** Fired when traits change. */
	UPROPERTY(BlueprintAssignable, Category = "MO|Personality|Events")
	FMOPersonalityChangedDelegate OnPersonalityChanged;

	// ============================================================================
	// SAVE/LOAD
	// ============================================================================

	/** Build save data for persistence. */
	UFUNCTION(BlueprintCallable, Category = "MO|Personality|Save")
	void BuildSaveData(FMOPersonalityTraits& OutSaveData) const;

	/** Apply save data (authority only). */
	UFUNCTION(BlueprintCallable, Category = "MO|Personality|Save")
	bool ApplySaveDataAuthority(const FMOPersonalityTraits& InSaveData);

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_Traits();

	/** Personality traits (replicated). */
	UPROPERTY(ReplicatedUsing = OnRep_Traits)
	FMOPersonalityTraits Traits;
};
