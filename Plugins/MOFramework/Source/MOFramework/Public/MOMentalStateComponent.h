#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOMedicalTypes.h"
#include "MOMentalStateComponent.generated.h"

class UMOVitalsComponent;
class UMOAnatomyComponent;
class UMOMetabolismComponent;

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnConsciousnessChanged, EMOConsciousnessLevel, OldLevel, EMOConsciousnessLevel, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnShockLevelChanged, float, OldShock, float, NewShock);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnLostConsciousness);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnRegainedConsciousness);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnMentalStateChanged);

// ============================================================================
// SAVE DATA
// ============================================================================

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOMentalStateSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	FMOMentalState MentalState;
};

// ============================================================================
// COMPONENT
// ============================================================================

/**
 * Component managing mental and cognitive state.
 * Handles consciousness levels, shock accumulation, and visual/motor effects.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOMentalStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOMentalStateComponent();

	// ============================================================================
	// REPLICATED STATE
	// ============================================================================

	/** Current mental state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="MO|Mental")
	FMOMentalState MentalState;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Time scale multiplier (1.0 = real time). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Config", meta=(ClampMin="0.01"))
	float TimeScaleMultiplier = 1.0f;

	/** Shock threshold for confusion (default 30). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Config")
	float ConfusionShockThreshold = 30.0f;

	/** Shock threshold for drowsiness (default 50). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Config")
	float DrowsyShockThreshold = 50.0f;

	/** Shock threshold for unconsciousness (default 70). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Config")
	float UnconsciousShockThreshold = 70.0f;

	/** Shock threshold for coma (default 90). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Config")
	float ComaShockThreshold = 90.0f;

	/** Natural shock recovery rate per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Config")
	float ShockRecoveryRate = 0.5f;

	/** Natural stress recovery rate per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Mental|Config")
	float StressRecoveryRate = 0.2f;

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Fired when consciousness level changes. */
	UPROPERTY(BlueprintAssignable, Category="MO|Mental|Events")
	FMOOnConsciousnessChanged OnConsciousnessChanged;

	/** Fired when shock level changes significantly. */
	UPROPERTY(BlueprintAssignable, Category="MO|Mental|Events")
	FMOOnShockLevelChanged OnShockLevelChanged;

	/** Fired when character loses consciousness. */
	UPROPERTY(BlueprintAssignable, Category="MO|Mental|Events")
	FMOOnLostConsciousness OnLostConsciousness;

	/** Fired when character regains consciousness. */
	UPROPERTY(BlueprintAssignable, Category="MO|Mental|Events")
	FMOOnRegainedConsciousness OnRegainedConsciousness;

	/** Fired when any mental state value changes (for UI updates). */
	UPROPERTY(BlueprintAssignable, Category="MO|Mental|Events")
	FMOOnMentalStateChanged OnMentalStateChanged;

	// ============================================================================
	// SHOCK API
	// ============================================================================

	/**
	 * Add shock from trauma.
	 * @param Amount Amount of shock to add.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Mental|Shock")
	void AddShock(float Amount);

	/**
	 * Remove shock (from treatment/recovery).
	 * @param Amount Amount of shock to remove.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Mental|Shock")
	void RemoveShock(float Amount);

	/**
	 * Add traumatic stress from witnessing events.
	 * @param Amount Amount of stress to add.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Mental|Shock")
	void AddTraumaticStress(float Amount);

	/**
	 * Add morale fatigue (long-term exhaustion).
	 * @param Amount Amount of fatigue to add.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Mental|Shock")
	void AddMoraleFatigue(float Amount);

	// ============================================================================
	// CONSCIOUSNESS API
	// ============================================================================

	/**
	 * Force a specific consciousness level (for scripted events).
	 * @param Level The consciousness level to set.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Mental|Consciousness")
	void ForceConsciousnessLevel(EMOConsciousnessLevel Level);

	/**
	 * Attempt to wake up (from unconscious).
	 * @return True if successfully woke up.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Mental|Consciousness")
	bool AttemptWakeUp();

	/**
	 * Get current consciousness level.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Mental|Consciousness")
	EMOConsciousnessLevel GetConsciousnessLevel() const;

	// ============================================================================
	// QUERY API
	// ============================================================================

	/**
	 * Get current mental state (const reference for reading).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Mental|Query")
	const FMOMentalState& GetMentalState() const { return MentalState; }

	/**
	 * Get current energy level as 0-1 (inverse of fatigue).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Mental|Query")
	float GetEnergyLevel() const;

	/**
	 * Check if character can perform any actions.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Mental|Query")
	bool CanPerformActions() const;

	/**
	 * Check if character has full mental capacity.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Mental|Query")
	bool HasFullCapacity() const;

	/**
	 * Check if character is unconscious or worse.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Mental|Query")
	bool IsUnconscious() const;

	/**
	 * Get aim penalty multiplier (1.0 = normal, higher = worse aim).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Mental|Query")
	float GetAimPenalty() const;

	/**
	 * Get movement speed penalty multiplier (1.0 = normal, lower = slower).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Mental|Query")
	float GetMovementPenalty() const;

	/**
	 * Get current tunnel vision intensity (0-1).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Mental|Query")
	float GetTunnelVisionIntensity() const;

	/**
	 * Get current vision blur intensity (0-1).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Mental|Query")
	float GetBlurredVisionIntensity() const;

	/**
	 * Get current aim shake intensity (0-1).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Mental|Query")
	float GetAimShakeIntensity() const;

	/**
	 * Get current stumble chance (0-1).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Mental|Query")
	float GetStumblingChance() const;

	/**
	 * Roll for stumble based on current state.
	 * @return True if should stumble.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Mental|Query")
	bool RollForStumble();

	// ============================================================================
	// PERSISTENCE
	// ============================================================================

	/**
	 * Build save data from current state.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Mental|Save")
	void BuildSaveData(FMOMentalStateSaveData& OutSaveData) const;

	/**
	 * Apply save data to restore state (authority only).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Mental|Save")
	bool ApplySaveDataAuthority(const FMOMentalStateSaveData& InSaveData);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// ============================================================================
	// INTERNAL STATE
	// ============================================================================

	/** Timer for periodic mental state processing. */
	FTimerHandle TickTimerHandle;

	/** Tick interval in seconds. */
	float TickInterval = 0.5f;

	/** Cached reference to vitals component. */
	UPROPERTY(Transient)
	TObjectPtr<UMOVitalsComponent> CachedVitalsComp;

	/** Cached reference to anatomy component. */
	UPROPERTY(Transient)
	TObjectPtr<UMOAnatomyComponent> CachedAnatomyComp;

	/** Cached reference to metabolism component. */
	UPROPERTY(Transient)
	TObjectPtr<UMOMetabolismComponent> CachedMetabolismComp;

	/** Previous consciousness level for change detection. */
	EMOConsciousnessLevel PreviousConsciousness = EMOConsciousnessLevel::Alert;

	/** Forced consciousness (if set, overrides calculated). */
	bool bConsciousnessForced = false;

	// ============================================================================
	// INTERNAL METHODS
	// ============================================================================

	/** Periodic tick to process mental state. */
	void TickMentalState();

	/** Calculate consciousness level based on all factors. */
	void CalculateConsciousnessLevel();

	/** Calculate visual effects based on state. */
	void CalculateVisualEffects();

	/** Calculate motor effects based on state. */
	void CalculateMotorEffects();

	/** Process shock recovery. */
	void ProcessShockRecovery(float DeltaTime);

	/** Process stress recovery. */
	void ProcessStressRecovery(float DeltaTime);

	/** Update shock from external factors. */
	void UpdateExternalShockFactors();

	/** Get total shock contribution from all sources. */
	float GetTotalShockContribution() const;
};
