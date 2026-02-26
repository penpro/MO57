/**
 * =============================================================================
 * MOCreature.h - Base Wildlife/Monster Class
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Base class for all AI-controlled wildlife (prey, predators). Inherits from
 * AMOCharacter for full medical/combat integration. DataTable-driven for
 * easy balancing and configuration.
 *
 * CREATURE TYPES:
 * - Prey (AMOPreyCreature): Flees from threats, can be hunted
 * - Predator (AMOPredatorCreature): Attacks players/prey
 *
 * FEATURES:
 * - DataTable-driven stats (DT_CreatureDefinitions)
 * - Behavior tree AI (flee, attack, rest, wander)
 * - Perception-based threat detection
 * - Loot drops on death (configurable in DataTable)
 * - Activity state machine (Active, Resting, Sleeping, Fleeing, Fighting)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] SUBSET COMPONENTS: Creatures have Vitals and Anatomy but NOT full
 *   mental/metabolism components. Check null before accessing.
 *
 * [2024-02] AI CONTROLLER: Creatures use AMOCreatureController. Set in
 *   defaults or DataTable. Wrong controller = no AI behavior.
 *
 * [2024-02] BEHAVIOR TREE: BehaviorTree reference must be set. Check
 *   CreatureDefinition.BehaviorTree in DataTable row.
 *
 * [2024-02] DEATH HANDLING: OnCreatureDeath fires BEFORE actor is destroyed.
 *   Use this for loot spawning and death effects.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOPreyCreature.h / MOPredatorCreature.h - Subclasses
 * - MOCreatureController.h - AI controller
 * - MOCreatureDefinitionRow.h - DataTable row
 * - MOCreatureTypes.h - Activity state enum
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOCharacter.h"
#include "MOBodyPartTypes.h"
#include "MOCreature.generated.h"

class AMOCreatureController;
class UBehaviorTree;
struct FMOCreatureDefinitionRow;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnCreatureDeath, AMOCreature*, Creature, AActor*, Killer);

/**
 * Base class for all creatures.
 * See file header for features and pitfalls.
 */
UCLASS()
class MOFRAMEWORK_API AMOCreature : public AMOCharacter
{
	GENERATED_BODY()

public:
	AMOCreature();

	// ============================================================================
	// DEFINITION
	// ============================================================================

	/** Creature definition from DataTable (stats, behavior, loot). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Creature")
	FDataTableRowHandle CreatureDefinition;

	/** Get the creature definition row. Returns nullptr if not found. */
	const FMOCreatureDefinitionRow* GetCreatureDefinitionRow() const;

	// ============================================================================
	// AI CONTROLLER
	// ============================================================================

	/** Controller class to spawn for this creature. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature|AI")
	TSubclassOf<AMOCreatureController> CreatureControllerClass;

	/** Behavior tree override (if not using definition's tree). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Creature|AI")
	TSoftObjectPtr<UBehaviorTree> BehaviorTreeOverride;

	/** Get the creature's AI controller (casted). */
	UFUNCTION(BlueprintPure, Category="MO|Creature|AI")
	AMOCreatureController* GetCreatureController() const;

	// ============================================================================
	// STATE QUERIES
	// ============================================================================

	/** Check if the creature is dead. */
	UFUNCTION(BlueprintPure, Category="MO|Creature|State")
	bool IsDead() const { return bIsDead; }

	/** Get health percentage (0.0 to 1.0). */
	UFUNCTION(BlueprintPure, Category="MO|Creature|State")
	float GetHealthPercent() const;

	// ============================================================================
	// EVENTS
	// ============================================================================

	/** Fired when this creature dies. */
	UPROPERTY(BlueprintAssignable, Category="MO|Creature|Events")
	FMOOnCreatureDeath OnCreatureDeath;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ============================================================================
	// HIT REACTIONS
	// ============================================================================

	/** Animation montage to play when hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Creature|Animation")
	TObjectPtr<UAnimMontage> HitReactionMontage;

	/** Animation montage to play on death. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Creature|Animation")
	TObjectPtr<UAnimMontage> DeathMontage;

	/** Play hit reaction animation. Called when creature takes damage. */
	UFUNCTION(BlueprintCallable, Category="MO|Creature")
	void PlayHitReaction();

	// ============================================================================
	// DEATH HANDLING
	// ============================================================================

	/** Called when the creature is killed. */
	UFUNCTION(BlueprintNativeEvent, Category="MO|Creature")
	void OnDeath(AActor* Killer);
	virtual void OnDeath_Implementation(AActor* Killer);

	/** Spawn loot items based on definition. */
	UFUNCTION(BlueprintCallable, Category="MO|Creature")
	void SpawnLoot();

	/** Delay before destroying actor after death (for ragdoll/decay). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature")
	float DestroyDelayAfterDeath = 30.f;

	// ============================================================================
	// STATE
	// ============================================================================

	/** Whether this creature is dead. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Creature|State")
	bool bIsDead = false;

	/** The actor that killed this creature. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Creature|State")
	TWeakObjectPtr<AActor> KillerActor;

private:
	/** Apply creature definition to components. */
	void ApplyCreatureDefinition();

	/** Handle instant death from anatomy component. */
	UFUNCTION()
	void HandleInstantDeath(EMOBodyPartType CausePart);

	/** Timer handle for delayed destroy after death. */
	FTimerHandle DestroyTimerHandle;

	/** Destroy the creature actor (called after death delay). */
	void PerformDestroy();
};
