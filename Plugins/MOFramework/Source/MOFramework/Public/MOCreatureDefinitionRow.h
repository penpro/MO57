#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MOCreatureDefinitionRow.generated.h"

class UBehaviorTree;

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

	// ============================================================================
	// AI
	// ============================================================================

	/** Behavior tree asset for this creature type. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
	TSoftObjectPtr<UBehaviorTree> BehaviorTree;

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
	// LOOT
	// ============================================================================

	/** Items dropped when creature dies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot")
	TArray<FMOLootEntry> LootEntries;
};
