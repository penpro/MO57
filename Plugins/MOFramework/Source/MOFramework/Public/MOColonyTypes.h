/**
 * =============================================================================
 * MOColonyTypes.h - Colony Management Shared Types
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Shared enums and structs for the colony management system. Defines
 * alert states, personality traits, relationships, and history entries.
 * Created early in Stage 1 to avoid circular dependencies.
 *
 * SYSTEMS USING THESE TYPES:
 * - UMOColonyManagerSubsystem - Alert management
 * - UMOPersonalityComponent - Personality traits
 * - UMOCharacterHistoryComponent - History and relationships
 * - UMOColonyPortrait - Portrait widget (uses EAlertState)
 * - UMOColonyBarWidget - Colony bar (uses EAlertState)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-03] UHT REQUIREMENT: This file has USTRUCT/UENUM so UHT processes it.
 *   All types are properly exposed to Blueprint.
 *
 * =============================================================================
 * RELATED FILES: MOColonyManagerSubsystem.h, MOPersonalityComponent.h,
 *                MOCharacterHistoryComponent.h, MOColonyPortrait.h
 * LAST UPDATED: 2026-03-28
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOColonyTypes.generated.h"

// =============================================================================
// ALERT SYSTEM
// =============================================================================

/**
 * Alert state for colony characters.
 * Used by colony portrait and colony bar to show character status.
 */
UENUM(BlueprintType)
enum class EAlertState : uint8
{
	/** No alert - character is fine */
	None UMETA(DisplayName = "None"),

	/** Tier 3 - Notable event, colony log only */
	Notable UMETA(DisplayName = "Notable"),

	/** Tier 2 - Urgent, persistent orange indicator */
	Urgent UMETA(DisplayName = "Urgent"),

	/** Tier 1 - Critical, pulsing red, cannot dismiss */
	Critical UMETA(DisplayName = "Critical")
};

/**
 * Alert entry in the colony manager queue.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOColonyAlert
{
	GENERATED_BODY()

	/** Unique ID for this alert. Generated per-instance via FGuid::NewGuid()
	 *  in the constructor, so it is intentionally non-deterministic — exempt
	 *  it from UE's member-initialization determinism test. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alert", Meta = (IgnoreForMemberInitializationTest))
	FGuid AlertId;

	/** Character this alert is for */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alert")
	FGuid CharacterGuid;

	/** Alert severity tier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alert")
	EAlertState AlertState = EAlertState::None;

	/** Short alert title */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alert")
	FText Title;

	/** Longer alert description */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alert")
	FText Description;

	/** When the alert was created */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alert")
	FDateTime Timestamp;

	/** Whether this alert has been acknowledged by the player */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alert")
	bool bAcknowledged = false;

	FMOColonyAlert()
	{
		AlertId = FGuid::NewGuid();
		Timestamp = FDateTime::Now();
	}
};

// =============================================================================
// PERSONALITY SYSTEM
// =============================================================================

/**
 * Personality axis identifiers.
 * Each axis is a spectrum from -1.0 to 1.0.
 */
UENUM(BlueprintType)
enum class EPersonalityAxis : uint8
{
	/** Diligent (-1) to Adaptable (+1) - affects task performance style */
	Conscientiousness UMETA(DisplayName = "Conscientiousness"),

	/** Reserved (-1) to Social (+1) - affects performance near others */
	Sociability UMETA(DisplayName = "Sociability"),

	/** Volatile (-1) to Stable (+1) - affects mood swing intensity */
	Stability UMETA(DisplayName = "Stability")
};

/**
 * Personality trait values for a character.
 * Each value ranges from -1.0 to 1.0.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOPersonalityTraits
{
	GENERATED_BODY()

	/**
	 * Conscientiousness: Diligent (-1) to Adaptable (+1)
	 * - Diligent: Methodical, slower, higher quality output
	 * - Adaptable: Quick, takes shortcuts, variable quality
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float Conscientiousness = 0.0f;

	/**
	 * Sociability: Reserved (-1) to Social (+1)
	 * - Reserved: Performs better alone, dislikes crowds
	 * - Social: Performs better near others, seeks company
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float Sociability = 0.0f;

	/**
	 * Stability: Volatile (-1) to Stable (+1)
	 * - Volatile: Strong mood swings, high ceiling and floor
	 * - Stable: Consistent mood, moderate performance
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float Stability = 0.0f;

	/** Get trait value by axis enum */
	float GetTraitValue(EPersonalityAxis Axis) const
	{
		switch (Axis)
		{
		case EPersonalityAxis::Conscientiousness: return Conscientiousness;
		case EPersonalityAxis::Sociability: return Sociability;
		case EPersonalityAxis::Stability: return Stability;
		default: return 0.0f;
		}
	}

	/** Set trait value by axis enum */
	void SetTraitValue(EPersonalityAxis Axis, float Value)
	{
		Value = FMath::Clamp(Value, -1.0f, 1.0f);
		switch (Axis)
		{
		case EPersonalityAxis::Conscientiousness: Conscientiousness = Value; break;
		case EPersonalityAxis::Sociability: Sociability = Value; break;
		case EPersonalityAxis::Stability: Stability = Value; break;
		}
	}

	/** Generate random personality traits */
	static FMOPersonalityTraits Random()
	{
		FMOPersonalityTraits Traits;
		Traits.Conscientiousness = FMath::FRandRange(-1.0f, 1.0f);
		Traits.Sociability = FMath::FRandRange(-1.0f, 1.0f);
		Traits.Stability = FMath::FRandRange(-1.0f, 1.0f);
		return Traits;
	}
};

// =============================================================================
// RELATIONSHIP SYSTEM
// =============================================================================

/**
 * Types of relationships between characters.
 */
UENUM(BlueprintType)
enum class ERelationshipType : uint8
{
	/** No special relationship */
	None UMETA(DisplayName = "None"),

	/** Mentor teaching a student */
	Mentor UMETA(DisplayName = "Mentor"),

	/** Student learning from a mentor */
	Student UMETA(DisplayName = "Student"),

	/** Close friends */
	Friend UMETA(DisplayName = "Friend"),

	/** Competitive rivals */
	Rival UMETA(DisplayName = "Rival"),

	/** Romantic relationship */
	Romantic UMETA(DisplayName = "Romantic"),

	/** Active hostility */
	Enemy UMETA(DisplayName = "Enemy"),

	// --- Family (V2.3 data model; sim arrives V2.5) ---
	Spouse UMETA(DisplayName = "Spouse"),
	Parent UMETA(DisplayName = "Parent"),
	Child UMETA(DisplayName = "Child")
};

/**
 * Relationship between two characters.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOCharacterRelationship
{
	GENERATED_BODY()

	/** The other character's GUID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relationship")
	FGuid OtherCharacterGuid;

	/** Type of relationship */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relationship")
	ERelationshipType RelationshipType = ERelationshipType::None;

	/** Relationship strength (-1.0 hostile to 1.0 close) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relationship", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float Strength = 0.0f;

	/** Accumulated interaction time (seconds) - builds relationships */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relationship")
	float ProximityTime = 0.0f;

	/** When the relationship was established */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relationship")
	FDateTime EstablishedDate;

	FMOCharacterRelationship()
	{
		EstablishedDate = FDateTime::Now();
	}
};

// =============================================================================
// HISTORY SYSTEM
// =============================================================================

/**
 * Types of history entries for character event log.
 */
UENUM(BlueprintType)
enum class EHistoryEntryType : uint8
{
	/** Task completed */
	TaskComplete UMETA(DisplayName = "Task Complete"),

	/** Task failed */
	TaskFailed UMETA(DisplayName = "Task Failed"),

	/** Skill increased */
	SkillGain UMETA(DisplayName = "Skill Gain"),

	/** Learned new knowledge */
	KnowledgeGain UMETA(DisplayName = "Knowledge Gain"),

	/** Took damage */
	Injured UMETA(DisplayName = "Injured"),

	/** Health restored */
	Healed UMETA(DisplayName = "Healed"),

	/** Relationship changed */
	RelationshipChange UMETA(DisplayName = "Relationship Change"),

	/** Mood changed significantly */
	MoodChange UMETA(DisplayName = "Mood Change"),

	/** Combat event */
	Combat UMETA(DisplayName = "Combat"),

	/** Generic activity */
	Activity UMETA(DisplayName = "Activity")
};

/**
 * Entry in a character's history log.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOCharacterHistoryEntry
{
	GENERATED_BODY()

	/** When this event occurred */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "History")
	FDateTime Timestamp;

	/** Type of event */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "History")
	EHistoryEntryType EntryType = EHistoryEntryType::Activity;

	/** Short description of the event */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "History")
	FText Description;

	/** Optional related character GUID (for relationship events) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "History")
	FGuid RelatedCharacterGuid;

	/** Optional related item/skill/recipe ID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "History")
	FName RelatedId = NAME_None;

	/** Optional numeric value (damage amount, skill level, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "History")
	float NumericValue = 0.0f;

	FMOCharacterHistoryEntry()
	{
		Timestamp = FDateTime::Now();
	}
};

// =============================================================================
// ACTIVITY STATE
// =============================================================================

/**
 * Current activity state for a colony character.
 * Used for status display in colony bar and overview.
 */
UENUM(BlueprintType)
enum class EColonyActivityState : uint8
{
	/** Idle, no task assigned */
	Idle UMETA(DisplayName = "Idle"),

	/** Working on assigned task */
	Working UMETA(DisplayName = "Working"),

	/** Resting */
	Resting UMETA(DisplayName = "Resting"),

	/** Sleeping */
	Sleeping UMETA(DisplayName = "Sleeping"),

	/** In combat */
	Fighting UMETA(DisplayName = "Fighting"),

	/** Fleeing from danger */
	Fleeing UMETA(DisplayName = "Fleeing"),

	/** Currently possessed by player */
	Possessed UMETA(DisplayName = "Possessed"),

	/** Incapacitated */
	Incapacitated UMETA(DisplayName = "Incapacitated"),

	/** Dead */
	Dead UMETA(DisplayName = "Dead")
};

// =============================================================================
// V1 SETTLEMENT LOOP TYPES (colony manager + history save payloads)
// =============================================================================

/** Save payload for a character's event log (oldest first). */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOCharacterHistorySaveData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMOCharacterHistoryEntry> Entries;

	/** Relationship graph (V2.3) — includes family (Spouse/Parent/Child). */
	UPROPERTY()
	TArray<FMOCharacterRelationship> Relationships;

	UPROPERTY()
	bool bHasValidData = false;
};

/** One settlement (V1: a single settlement per world). */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOSettlementRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settlement", meta=(IgnoreForMemberInitializationTest))
	FGuid SettlementId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settlement")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settlement")
	FVector Center = FVector::ZeroVector;

	/** Communal reach: storages/houses inside this radius belong to the settlement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settlement", meta=(ClampMin="1000"))
	float Radius = 20000.0f;

	bool IsValid() const { return SettlementId.IsValid(); }
};

/** Inputs to the mood function — all read from the REAL simulations. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOVillagerMoodInputs
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mood") bool bStarving = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mood") bool bDehydrated = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mood") float Wetness = 0.0f;          // 0..1
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mood") float Shock = 0.0f;            // 0..100
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mood") float TraumaticStress = 0.0f;  // 0..100
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mood") float MoraleFatigue = 0.0f;    // 0..100
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mood") bool bHasHome = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mood") float UnhousedHours = 0.0f;    // consecutive
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mood") float MoodVarianceModifier = 1.0f; // personality Stability (0.5 stable .. 1.5 volatile)
};

/** A standing order (V2.1): keep TargetCount of an item in communal storage
 *  by crafting RecipeId. The Medieval-Dynasty-style scale abstraction — the
 *  PLAYER sets intent, villagers do the real labor through the V0 job flow. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOColonyQuota
{
	GENERATED_BODY()

	/** Item to keep stocked (measured across settlement communal storage). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quota")
	FName OutputItemId;

	/** Recipe villagers run to produce it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quota")
	FName RecipeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quota", meta=(ClampMin="0"))
	int32 TargetCount = 0;

	/** Higher fills first when idle hands are scarce. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quota")
	int32 Priority = 0;
};

/** Save payload for the whole settlement layer. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOColonySaveData
{
	GENERATED_BODY()

	UPROPERTY() FMOSettlementRecord Settlement;
	UPROPERTY() TMap<FGuid, FGuid> Residency;            // pawn -> house building
	UPROPERTY() TMap<FGuid, float> VillagerMood;         // pawn -> 0..1
	UPROPERTY() TMap<FGuid, float> VillagerUnhousedHours;
	UPROPERTY() TMap<FGuid, FMOCharacterHistorySaveData> VillagerHistory;
	UPROPERTY() TArray<FMOColonyQuota> Quotas;
	UPROPERTY() bool bHasValidData = false;
};
