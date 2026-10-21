/**
 * =============================================================================
 * MOCreatureDefinitionRow.h - Creature DataTable Definition
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * DataTable row structure for creature definitions. Defines stats, behavior,
 * perception, and loot for all creature types.
 *
 * SECTIONS:
 * - Identity: DisplayName, Description
 * - Stats: HealthModifier, WalkSpeed, RunSpeed
 * - Combat: BaseDamage, AttackRange, AttackCooldown
 * - Behavior: AggroRange, FleeThreshold, bIsPredator
 * - AI: BehaviorTree asset reference
 * - Perception: SightRadius, HearingRange, etc.
 * - Loot: TArray<FMOLootEntry> for death drops
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] HEALTH MODIFIER: Multiplied with anatomy component base health,
 *   not absolute health value. 1.0 = normal, 2.0 = double health.
 *
 * [2024-02] PREDATOR FLAG: bIsPredator determines base AI class behavior.
 *   True = AMOPredatorCreature behaviors, False = AMOPreyCreature behaviors.
 *
 * [2024-02] LOOT CHANCE: FMOLootEntry.DropChance is 0.0-1.0 probability,
 *   not percentage. 1.0 = guaranteed drop.
 *
 * =============================================================================
 * RELATED FILES: MOCreature.h, MOPreyCreature.h, MOPredatorCreature.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MOCreatureDefinitionRow.generated.h"

class UBehaviorTree;
class UAnimSequenceBase;

/**
 * Loot entry for creature drops.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOLootEntry
{
	GENERATED_BODY()

	/** Item definition ID to drop. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot")
	FName ItemDefinitionId;

	/** Minimum quantity to drop. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot", meta=(ClampMin="1"))
	int32 MinQuantity = 1;

	/** Maximum quantity to drop. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot", meta=(ClampMin="1"))
	int32 MaxQuantity = 1;

	/** Chance to drop (0.0 to 1.0). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DropChance = 1.0f;
};

/**
 * DataTable row for creature definitions.
 * Defines stats, behavior, and loot for creatures.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOCreatureDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	// ============================================================================
	// IDENTITY
	// ============================================================================

	/** Display name shown in UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	FText DisplayName;

	/** Description of the creature. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	FText Description;

	// ============================================================================
	// STATS
	// ============================================================================

	/** Base health modifier (multiplied with anatomy component). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	float HealthModifier = 1.0f;

	/** Walking movement speed (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats", meta=(ClampMin="0"))
	float WalkSpeed = 200.f;

	/** Running movement speed (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats", meta=(ClampMin="0"))
	float RunSpeed = 600.f;

	// ============================================================================
	// COMBAT
	// ============================================================================

	/** Base damage per attack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0"))
	float BaseDamage = 10.f;

	/** Attack range (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0"))
	float AttackRange = 150.f;

	/** Cooldown between attacks (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0"))
	float AttackCooldown = 2.0f;

	// ============================================================================
	// BEHAVIOR
	// ============================================================================

	/** Range at which creature detects threats and becomes aggressive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behavior", meta=(ClampMin="0"))
	float AggroRange = 1000.f;

	/** Health percentage at which creature starts fleeing (0.0 to 1.0). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behavior", meta=(ClampMin="0.0", ClampMax="1.0"))
	float FleeThreshold = 0.3f;

	/** If true, creature will attack first. If false, flees by default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behavior")
	bool bIsPredator = false;

	/**
	 * A wounded animal's flight (s): whatever hurt it stays its threat this long, seen or heard or not, so it runs leg after leg
	 * (the behavior tree's flee legs chain while it has a threat) -- a deer at 7.5 m/s for 45 s covers ~340 m. 0 = no flight from damage.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behavior", meta=(ClampMin="0"))
	float WoundedPanicSeconds = 45.f;

	/** A pursuer (bIsPredator) gives up its chase this far (cm) from home (its den: where it spawned). 0 = no leash. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behavior", meta=(ClampMin="0"))
	float ChaseLeashRadius = 20000.f;

	/** A pursuer gives up when it has not closed on its quarry for this long (s): a chase it is not winning. 0 = never. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behavior", meta=(ClampMin="0"))
	float ChaseNoGainSeconds = 20.f;

	/** After giving up, a pursuer ignores that quarry for this long (s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behavior", meta=(ClampMin="0"))
	float ChaseGiveUpCooldownSeconds = 120.f;

	// ============================================================================
	// AI
	// ============================================================================

	/** Behavior tree asset for this creature type. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
	TSoftObjectPtr<UBehaviorTree> BehaviorTree;

	// ============================================================================
	// ANIMATION
	// ============================================================================

	/** The flinch when a blow lands from the front (a sequence or a montage for this species' skeleton; it plays in the ABP's DefaultSlot on
	 *  every machine: AMOCreature::PlayAnimationEverywhere). Left / Right / Back fall back to it; none = no reaction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TSoftObjectPtr<UAnimSequenceBase> HitReactFront;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TSoftObjectPtr<UAnimSequenceBase> HitReactLeft;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TSoftObjectPtr<UAnimSequenceBase> HitReactRight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
	TSoftObjectPtr<UAnimSequenceBase> HitReactBack;

	// ============================================================================
	// PERCEPTION
	// ============================================================================

	/** Sight perception radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Perception", meta=(ClampMin="0"))
	float SightRadius = 1500.f;

	/** Radius at which sight is lost. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Perception", meta=(ClampMin="0"))
	float LoseSightRadius = 2000.f;

	/** Peripheral vision half-angle (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Perception", meta=(ClampMin="0", ClampMax="180"))
	float PeripheralVisionAngle = 60.f;

	/** Hearing perception range. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Perception", meta=(ClampMin="0"))
	float HearingRange = 2000.f;

	// ============================================================================
	// CARCASS / LOOT
	// ============================================================================

	/** Carcass definition row name (in DT_CarcassDefinitions). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Carcass")
	FName CarcassDefinitionId;

	/** Carcass definitions DataTable reference. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Carcass")
	TSoftObjectPtr<UDataTable> CarcassDataTable;

	/** DEPRECATED: Use carcass system instead. Items dropped when creature dies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot")
	TArray<FMOLootEntry> LootEntries;
};
