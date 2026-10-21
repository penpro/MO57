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
class AMOCarcassActor;
class UBehaviorTree;
class UAnimSequenceBase;
struct FMOCreatureDefinitionRow;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnCreatureDeath, AMOCreature*, Creature, AActor*, Killer);

/** The side of a creature a blow came from (AMOCreature::ClassifyHitSide): picks the hit reaction. */
UENUM()
enum class EMOCreatureHitSide : uint8
{
	Front,
	Back,
	Left,
	Right
};

/** A creature animation for every machine (AMOCreature::PlayAnimationEverywhere): the server sets it, each client plays it in OnRep_AnimCue. */
USTRUCT()
struct FMOCreatureAnimCue
{
	GENERATED_BODY()

	/** A montage, or a sequence played in the ABP's DefaultSlot; null stops the current one. */
	UPROPERTY()
	TObjectPtr<UAnimSequenceBase> Animation = nullptr;

	UPROPERTY()
	float PlayRate = 1.0f;

	/** The server's game time when it played: a client that receives the cue late (joining, or the creature coming into relevance) skips it. */
	UPROPERTY()
	float ServerTime = 0.0f;

	/** Bumped on every cue, so the same animation twice still replicates. */
	UPROPERTY()
	uint8 Serial = 0;
};

/**
 * Base class for all creatures.
 * See file header for features and pitfalls.
 */
UCLASS()
class MOFRAMEWORK_API AMOCreature : public AMOCharacter
{
	GENERATED_BODY()

public:
	AMOCreature(const FObjectInitializer& ObjectInitializer);

	/** + the behavior tree (override or the definition's), the carcass table, and the creature controller class's own. */
	virtual void GatherRuntimeAssets(TArray<FSoftObjectPath>& Out) const override;

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

	// Note: IsDead() inherited from AMOCharacter

	/** Get health percentage (0.0 to 1.0). */
	UFUNCTION(BlueprintPure, Category="MO|Creature|State")
	float GetHealthPercent() const;

	/** Get the creature's run/sprint speed (from definition). */
	UFUNCTION(BlueprintPure, Category="MO|Creature|State")
	float GetRunSpeed() const { return SprintSpeed; }

	/** Get the creature's walk speed (from definition). */
	UFUNCTION(BlueprintPure, Category="MO|Creature|State")
	float GetWalkSpeed() const { return WalkSpeed; }

	/** A body part whose destruction kills a creature outright (simpler than the human medical death rules). */
	static bool IsVitalPart(EMOBodyPartType Part);

	// ============================================================================
	// ANIMATION ON EVERY MACHINE
	// ============================================================================

	/**
	 * Play a creature animation on every machine (server only): a montage plays as itself, a sequence in the ABP's DefaultSlot (an ABP
	 * without a DefaultSlot node shows nothing: ABP_DeerDoe, N-096). The server plays it now, each client when AnimCue arrives; null stops
	 * the current one. A montage played straight on the AnimInstance shows on the server only: the bite (BTTask_CreatureAttack) and the hit
	 * reactions go through here. Returns the length played on the server (0 = nothing played).
	 */
	float PlayAnimationEverywhere(UAnimSequenceBase* Animation, float PlayRate = 1.0f);

	/** Which side of the creature a blow came from: its forward against the flat direction to the attacker (Front / Back within 45 deg). */
	static EMOCreatureHitSide ClassifyHitSide(const FVector& CreatureForward, const FVector& ToAttacker);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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

	/** Animation montage to play on death. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Creature|Animation")
	TObjectPtr<UAnimMontage> DeathMontage;

	/** A blow from Attacker: the definition's reaction for the side it came from (HitReact*, Front the fallback), on every machine. Server. */
	void PlayHitReactionFrom(const AActor* Attacker);

	/** The last cue the server played (PlayAnimationEverywhere). */
	UPROPERTY(ReplicatedUsing=OnRep_AnimCue)
	FMOCreatureAnimCue AnimCue;

	UFUNCTION()
	void OnRep_AnimCue();

	/** Plays Cue on this machine; returns its length (0 = nothing played). */
	float PlayAnimCueLocally(const FMOCreatureAnimCue& Cue);

	/** Server game time of the last hit reaction: one blow can damage several parts, and it flinches once. */
	float LastHitReactionTime = -100.0f;

	// ============================================================================
	// DEATH HANDLING
	// ============================================================================

	/** Called when the creature is killed. */
	UFUNCTION(BlueprintNativeEvent, Category="MO|Creature")
	void OnDeath(AActor* Killer);
	virtual void OnDeath_Implementation(AActor* Killer);

	/** Spawn loot items based on definition. DEPRECATED: Use carcass system. */
	UFUNCTION(BlueprintCallable, Category="MO|Creature")
	void SpawnLoot();

	/** Spawn a carcass actor at this creature's location. */
	UFUNCTION(BlueprintCallable, Category="MO|Creature")
	AMOCarcassActor* SpawnCarcass();

	/** Delay before destroying actor after death (for ragdoll/decay). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature")
	float DestroyDelayAfterDeath = 30.f;

	/** How long ragdoll plays before spawning carcass. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature")
	float RagdollToCarassDelay = 2.0f;

	/** Impulse strength applied when entering ragdoll (knockback). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Creature")
	float RagdollImpulseStrength = 5000.0f;

	/** Enable ragdoll physics on mesh. */
	UFUNCTION(BlueprintCallable, Category="MO|Creature")
	void EnableRagdoll();

	// ============================================================================
	// STATE
	// ============================================================================

	// Note: bIsDead inherited from AMOCharacter

	/** The actor that killed this creature. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Creature|State")
	TWeakObjectPtr<AActor> KillerActor;

private:
	/** Apply creature definition to components. */
	void ApplyCreatureDefinition();

	/** Handle instant death from anatomy component - overrides AMOCharacter. */
	virtual void HandleInstantDeath(EMOBodyPartType CausePart) override;

	/** Handle body part destruction - creatures die when vital parts are destroyed. */
	UFUNCTION()
	void HandleBodyPartDestroyed(EMOBodyPartType Part, bool bInstantDeath);

	/** Hurt: the controller turns to whoever did it (prey flees, a predator turns on it). Server. */
	UFUNCTION()
	void HandleBodyPartDamaged(EMOBodyPartType Part, float Damage, float NewHP);

	/** Who dealt the most recent damage (the anatomy records it on every InflictDamage): the killer, when it dies of it. */
	AActor* GetLastDamageInstigator() const;

	/** Timer handle for delayed destroy after death. */
	FTimerHandle DestroyTimerHandle;

	/** Timer handle for delayed carcass spawn. */
	FTimerHandle CarcassSpawnTimerHandle;

	/** Timer handle for freezing ragdoll (used when no carcass). */
	FTimerHandle RagdollFreezeTimerHandle;

	/** Cached movement speeds from definition. */
	float WalkSpeed = 200.0f;
	float SprintSpeed = 600.0f;

	/** Whether to spawn a carcass on death (determined by checking definition). */
	bool bSpawnCarcassOnDeath = false;

	/** Destroy the creature actor (called after death delay). */
	void PerformDestroy();

	/** Called by timer to spawn carcass after ragdoll settles. */
	void SpawnCarcassDelayed();
};
