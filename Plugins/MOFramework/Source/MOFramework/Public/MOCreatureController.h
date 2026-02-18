#pragma once

#include "CoreMinimal.h"
#include "MOAIController.h"
#include "MOCreatureTypes.h"
#include "Perception/AIPerceptionTypes.h"
#include "MOCreatureController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
struct FMOCreatureDefinitionRow;

/**
 * AI Controller for creatures (prey and predators).
 * Extends MOAIController with perception capabilities and creature-specific helpers.
 */
UCLASS()
class MOFRAMEWORK_API AMOCreatureController : public AMOAIController
{
	GENERATED_BODY()

public:
	AMOCreatureController();

	// ============================================================================
	// QUERY METHODS (for BT nodes)
	// ============================================================================

	/** Get the current threat target from perception. */
	UFUNCTION(BlueprintPure, Category="MO|Creature|AI")
	AActor* GetCurrentThreat() const;

	/** Get the last known location of the current threat. */
	UFUNCTION(BlueprintPure, Category="MO|Creature|AI")
	FVector GetLastKnownThreatLocation() const;

	/** Get health percentage of the controlled creature. */
	UFUNCTION(BlueprintPure, Category="MO|Creature|AI")
	float GetHealthPercent() const;

	/** Check if the creature should flee based on health threshold. */
	UFUNCTION(BlueprintPure, Category="MO|Creature|AI")
	bool ShouldFlee() const;

	/** Check if currently in combat (has valid threat). */
	UFUNCTION(BlueprintPure, Category="MO|Creature|AI")
	bool IsInCombat() const;

	/** Get distance to the current threat. */
	UFUNCTION(BlueprintPure, Category="MO|Creature|AI")
	float GetDistanceToThreat() const;

	/** Check if the threat is within attack range. */
	UFUNCTION(BlueprintPure, Category="MO|Creature|AI")
	bool IsInAttackRange() const;

	// ============================================================================
	// ACTIVITY STATE
	// ============================================================================

	/** Get current activity state. */
	UFUNCTION(BlueprintPure, Category="MO|Creature|AI")
	EMOCreatureActivityState GetActivityState() const { return CurrentActivityState; }

	/** Set activity state (updates blackboard). */
	UFUNCTION(BlueprintCallable, Category="MO|Creature|AI")
	void SetActivityState(EMOCreatureActivityState NewState);

	/** Check if creature is dead. */
	UFUNCTION(BlueprintPure, Category="MO|Creature|AI")
	bool IsDead() const { return CurrentActivityState == EMOCreatureActivityState::Dead; }

	/** Mark creature as dead. */
	UFUNCTION(BlueprintCallable, Category="MO|Creature|AI")
	void SetDead();

	// ============================================================================
	// PERCEPTION SETUP
	// ============================================================================

	/** Apply perception settings from creature definition. */
	UFUNCTION(BlueprintCallable, Category="MO|Creature|AI")
	void ApplyPerceptionSettings(const FMOCreatureDefinitionRow& Definition);

	// ============================================================================
	// COMBAT SETTINGS
	// ============================================================================

	/** Set the flee health threshold. */
	UFUNCTION(BlueprintCallable, Category="MO|Creature|AI")
	void SetFleeHealthThreshold(float Threshold) { FleeHealthThreshold = FMath::Clamp(Threshold, 0.f, 1.f); }

	/** Set the attack range. */
	UFUNCTION(BlueprintCallable, Category="MO|Creature|AI")
	void SetAttackRange(float Range) { AttackRange = FMath::Max(0.f, Range); }

	/** Get the attack range. */
	UFUNCTION(BlueprintPure, Category="MO|Creature|AI")
	float GetAttackRange() const { return AttackRange; }

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	// ============================================================================
	// PERCEPTION
	// ============================================================================

	// Note: PerceptionComponent is inherited from AAIController via SetPerceptionComponent()

	/** Sight sense configuration. */
	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	/** Hearing sense configuration. */
	UPROPERTY()
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	// ============================================================================
	// PERCEPTION DEFAULTS
	// ============================================================================

	/** Default sight radius (cm). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Perception")
	float DefaultSightRadius = 1500.f;

	/** Default radius at which sight is lost. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Perception")
	float DefaultLoseSightRadius = 2000.f;

	/** Default peripheral vision half-angle (degrees). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Perception")
	float DefaultPeripheralVisionAngle = 60.f;

	/** Default hearing range (cm). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Perception")
	float DefaultHearingRange = 2000.f;

	/** Maximum age for stimuli before being forgotten (seconds). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Perception")
	float MaxStimulusAge = 10.f;

	/** How long to remember a threat after losing sight (seconds). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Perception")
	float ThreatMemoryDuration = 5.f;

	// ============================================================================
	// COMBAT SETTINGS
	// ============================================================================

	/** Health percentage at which creature flees. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Combat")
	float FleeHealthThreshold = 0.3f;

	/** Attack range (cm). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Combat")
	float AttackRange = 150.f;

	// ============================================================================
	// ACTIVITY SETTINGS
	// ============================================================================

	/** Chance per minute to start resting during the day (0-1). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Activity", meta=(ClampMin="0", ClampMax="1"))
	float DaytimeRestChancePerMinute = 0.1f;

	/** How long to rest before returning to active (seconds). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Activity")
	float RestDurationMin = 10.f;

	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Activity")
	float RestDurationMax = 30.f;

	/** Whether this creature sleeps at night. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Activity")
	bool bSleepsAtNight = true;

	// ============================================================================
	// STATE
	// ============================================================================

	/** Current activity state. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Creature|State")
	EMOCreatureActivityState CurrentActivityState = EMOCreatureActivityState::Active;

	/** Current threat actor (from perception). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Creature|State")
	TWeakObjectPtr<AActor> CurrentThreatActor;

	/** Last known location of threat. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Creature|State")
	FVector LastKnownThreatLocation;

	/** Spawn/home location for returning. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Creature|State")
	FVector HomeLocation;

	/** Timer handle for threat memory expiration. */
	FTimerHandle ThreatMemoryTimerHandle;

	/** Actor we're remembering even after losing sight. */
	UPROPERTY()
	TWeakObjectPtr<AActor> RememberedThreatActor;

	/** Pending perception settings to apply after SetupPerception. */
	float PendingSightRadius = -1.f;
	float PendingLoseSightRadius = -1.f;
	float PendingPeripheralVisionAngle = -1.f;
	float PendingHearingRange = -1.f;

private:
	/** Setup perception component and sense configs (defers ConfigureSense to next tick). */
	void SetupPerception();

	/** Called next tick after SetupPerception to finalize when listener ID is valid. */
	void FinalizePerceptionSetup();

	/** Apply any pending perception settings that were requested before perception was ready. */
	void ApplyPendingPerceptionSettings();

	/** Called when perception detects or loses an actor. */
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	/** Evaluate and update threat based on perceived actors. */
	void UpdateThreatAssessment();

	/** Update blackboard with current threat info. */
	void UpdateBlackboardThreatInfo();
};
