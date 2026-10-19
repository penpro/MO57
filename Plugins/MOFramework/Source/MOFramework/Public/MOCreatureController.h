/**
 * =============================================================================
 * MOCreatureController.h - Wildlife AI Controller with Perception
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * AI Controller for wildlife creatures (prey and predators). Extends MOAIController
 * with AI perception for sight/hearing, threat assessment, combat decisions, and
 * activity states (Active, Resting, Sleeping, Dead).
 *
 * KEY RESPONSIBILITIES:
 * 1. Configure AI perception (sight, hearing) from creature definition
 * 2. Detect and track threats (players, predators)
 * 3. Decide flee vs fight based on health threshold
 * 4. Manage activity states and blackboard sync
 * 5. Support rest/sleep behaviors based on time of day
 *
 * OWNERSHIP:
 * - Owner: Possesses creature pawns (deer, wolves, etc.)
 * - Lifespan: Exists while creature is alive
 *
 * PERCEPTION SYSTEM:
 * - SightConfig: Configurable radius, lose sight radius, peripheral angle
 * - HearingConfig: Range-based hearing for player sounds
 * - OnTargetPerceptionUpdated(): Callback when actors detected/lost
 * - UpdateThreatAssessment(): Evaluates threat priority
 *
 * THREAT ASSESSMENT:
 * OnTargetPerceptionUpdated()
 * -> UpdateThreatAssessment()
 * -> Set CurrentThreatActor if hostile detected
 * -> Start ThreatMemoryTimerHandle for memory
 * -> UpdateBlackboardThreatInfo()
 *
 * ACTIVITY STATES:
 * EMOCreatureActivityState:
 * - Active: Normal wandering/foraging
 * - Resting: Brief rest (daytime chance)
 * - Sleeping: Night sleep (if bSleepsAtNight)
 * - Fleeing: Running from threat
 * - Fighting: Engaged in combat
 * - Dead: Creature died
 *
 * COMBAT DECISION FLOW:
 * ShouldFlee() - Check if health below FleeHealthThreshold
 * IsInCombat() - Check if CurrentThreatActor valid
 * IsInAttackRange() - Check if threat within AttackRange
 *
 * CRITICAL PATTERNS:
 * 1. PERCEPTION SETUP: Call SetupPerception() in BeginPlay
 * 2. LISTENER REGISTRATION: FinalizePerceptionSetup registers the listener itself (RequestStimuliListenerUpdate) before ConfigureSense: a component with no
 *    senses at OnRegister is never registered by the engine, so waiting for an id never ends
 * 3. THREAT MEMORY: ThreatMemoryDuration keeps threat after losing sight
 * 4. BLACKBOARD SYNC: UpdateBlackboardThreatInfo() after threat changes
 *
 * KNOWN PITFALLS:
 * 1. PERCEPTION TIMING: ready at the end of OnPossess (settings applied earlier are kept as Pending* and applied then)
 * 2. PENDING SETTINGS: ApplyPerceptionSettings before setup stores to Pending*
 * 3. NULL VITALS: GetHealthPercent returns 1.0 if no vitals component
 *
 * RELATED FILES:
 * - MOAIController.h - Base class
 * - MOCreatureTypes.h - EMOCreatureActivityState
 * - MOCreatureDefinitionRow.h - Definition with perception settings
 * - BTService_CreatureActivity.h - Updates activity state in BT
 *
 * TESTING CHECKLIST:
 * [ ] Creature detects player via sight
 * [ ] Creature reacts to hearing stimuli
 * [ ] Threat memory expires after duration
 * [ ] Flee behavior triggers at health threshold
 * [ ] Rest/sleep states work with time of day
 * [ ] Death state stops all AI behavior
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

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

	// ============================================================================
	// DAMAGE REACTION / PURSUIT
	// ============================================================================

	/**
	 * Server: the creature was hurt (AMOCreature, from its anatomy). Whoever did it becomes the threat for WoundedPanicSeconds whether
	 * or not it was ever seen or heard -- prey runs from it leg after leg, a predator turns on it. Null (the environment) changes nothing.
	 */
	void NotifyDamaged(AActor* DamageInstigator);

	/** True while a wound's flight lasts (NotifyDamaged): the threat is held regardless of perception. */
	UFUNCTION(BlueprintPure, Category="MO|Creature|AI")
	bool IsPanicked() const;

	/** True for a creature that chases its threat (definition bIsPredator): its pursuit is leashed to home and given up when not gaining. */
	bool PursuesThreats() const { return bPursuesThreats; }

	/** The affiliations every creature sense detects: all of them (MO57 has no teams; OnTargetPerceptionUpdated decides what is a threat). */
	static FAISenseAffiliationFilter SenseAllAffiliations();

	/**
	 * One look at a chase (UpdatePursuit, 5 Hz): progress is being within reach (AttackReach) or closer than the last mark by a step;
	 * InOutMarkDistance / InOutLastGainTime carry the mark between looks. Returns why to give it up (past the leash from home, or no
	 * progress for NoGainSeconds), or nullptr to keep chasing. A zero leash or NoGainSeconds disables that test.
	 */
	static const TCHAR* JudgeChase(float DistanceToQuarry, float DistanceFromHome, double Now, float AttackReach, float LeashRadius,
		float NoGainSeconds, float& InOutMarkDistance, double& InOutLastGainTime);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
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

	/** From the definition (ApplyBehaviorSettings): see FMOCreatureDefinitionRow. */
	bool bPursuesThreats = false;
	float WoundedPanicSeconds = 45.f;
	float ChaseLeashRadius = 20000.f;
	float ChaseNoGainSeconds = 20.f;
	float ChaseGiveUpCooldownSeconds = 120.f;

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

	/** True once FinalizePerceptionSetup has registered the perception listener and configured its senses (ConfigureSense is allowed from then on). */
	bool bPerceptionFinalized = false;

	/** True once the initial blackboard values were pushed. The blackboard asset is bound AFTER OnPossess (the behavior tree starts later), so this is done on the first tick it exists. */
	bool bBlackboardPrimed = false;

	/** Pending perception settings to apply after SetupPerception. */
	float PendingSightRadius = -1.f;
	float PendingLoseSightRadius = -1.f;
	float PendingPeripheralVisionAngle = -1.f;
	float PendingHearingRange = -1.f;

	/** Time accumulator for throttled blackboard updates. */
	float ThreatUpdateAccumulator = 0.0f;

	/** World time a wound's flight ends (NotifyDamaged); before it the threat is held whatever perception says. */
	double PanicUntilTime = -1.0;

	/** The quarry the current chase measures, the closest it got, and when it last closed in (UpdatePursuit). */
	TWeakObjectPtr<AActor> ChaseQuarry;
	float ChaseBestDistance = MAX_FLT;
	double ChaseLastGainTime = 0.0;

	/** Quarries given up on, until when (world time): perception ignores them till then. */
	TMap<TWeakObjectPtr<AActor>, double> GivenUpUntil;

	/** Interval between blackboard threat updates (seconds). */
	static constexpr float ThreatUpdateInterval = 0.2f;

private:
	/** Creates the sense configs and binds the perception delegate, then finalizes (OnPossess). */
	void SetupPerception();

	/** Registers the perception listener (it has no id otherwise), configures the senses, applies any pending settings. */
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

	/** The definition's behavior numbers (flee threshold, attack range, panic, pursuit), applied at once whether perception is ready or not. */
	void ApplyBehaviorSettings(const FMOCreatureDefinitionRow& Definition);

	/** A pursuer's chase (5 Hz): gives it up past ChaseLeashRadius from home or after ChaseNoGainSeconds without closing in. */
	void UpdatePursuit();

	/** Drop the current threat and ignore it for ChaseGiveUpCooldownSeconds. */
	void GiveUpChase(const TCHAR* Reason);

	/** Forget the threat after Seconds unless something renews it (a sighting, a sound, a wound). */
	void StartThreatForgetTimer(AActor* Threat, float Seconds);
};
