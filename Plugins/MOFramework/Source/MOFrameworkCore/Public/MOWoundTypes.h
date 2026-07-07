/**
 * =============================================================================
 * MOWoundTypes.h - Medical Wound System Types
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Defines wound data structures for the medical system. Wounds are tracked
 * per body part, have severity, bleeding rate, infection risk, and treatment
 * state. Uses FastArraySerializer for efficient network replication.
 *
 * WOUND LIFECYCLE:
 * 1. Wound inflicted → FMOWound created with WoundId
 * 2. Bleeding → BleedRate drains blood volume via MOVitalsComponent
 * 3. Treatment → Bandage stops bleeding, suture enables healing
 * 4. Healing → HealingProgress increases over time
 * 5. Complete → Wound removed when HealingProgress >= 100
 *
 * INFECTION SYSTEM:
 * - InfectionRisk rolls each tick for untreated wounds
 * - bIsInfected triggers systemic effects (fever, etc.)
 * - InfectionSeverity increases without antibiotics
 * - Severe infection → sepsis → death
 *
 * TREATMENT FLAGS:
 * - bIsBandaged: Reduces BleedRate by ~90%
 * - bIsSutured: Required for deep wounds to heal properly
 * - bIsInfected: Requires antibiotics to clear
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-01] GUID UNIQUENESS: WoundId must be unique per pawn. Generated in
 *   ApplyWound(). Don't manually assign unless loading from save.
 *
 * [2024-02] BLEED RATE UNITS: BleedRate is mL/second, not mL/tick. Multiply
 *   by DeltaTime in tick updates.
 *
 * [2024-02] SEVERITY VS HEALING: Severity is initial wound severity (affects
 *   healing time). HealingProgress is 0-100% completion. Don't confuse them.
 *
 * [2024-02] FASTARRAY PATTERN: FMOWoundList uses FastArraySerializer.
 *   Call MarkItemDirty() after modifying a wound. See MOInventoryComponent
 *   for detailed FastArray documentation.
 *
 * [2024-02] WOUND TYPE EFFECTS: Different EMOWoundType have different base
 *   BleedRate and InfectionRisk. Laceration bleeds more than contusion.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOAnatomyComponent.h - Owns and manages wounds
 * - MOVitalsComponent.h - Blood volume affected by bleed
 * - MOBodyPartTypes.h - EMOBodyPartType enum
 * - MOMedicalSubsystem.h - Treatment definitions
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "MOBodyPartTypes.h"
#include "MOWoundTypes.generated.h"

// Forward declarations

/**
 * Represents an active wound on a body part.
 * See file header for wound lifecycle and treatment details.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORKCORE_API FMOWound : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** Unique identifier for this wound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound", meta=(IgnoreForMemberInitializationTest))
	FGuid WoundId;

	/** Which body part this wound is on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound")
	EMOBodyPartType BodyPart = EMOBodyPartType::None;

	/** Type of wound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound")
	EMOWoundType WoundType = EMOWoundType::None;

	/** Severity of the wound (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound", meta=(ClampMin="0", ClampMax="100"))
	float Severity = 0.0f;

	/** Blood loss rate in mL/second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound", meta=(ClampMin="0"))
	float BleedRate = 0.0f;

	/** Probability of infection per tick (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound", meta=(ClampMin="0", ClampMax="1"))
	float InfectionRisk = 0.0f;

	/** Healing progress (0-100%). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound", meta=(ClampMin="0", ClampMax="100"))
	float HealingProgress = 0.0f;

	/** Whether the wound has been bandaged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound")
	bool bIsBandaged = false;

	/** Whether the wound has been sutured (for deep wounds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound")
	bool bIsSutured = false;

	/** Whether the wound is infected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound")
	bool bIsInfected = false;

	/** Severity of the infection if infected (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound", meta=(ClampMin="0", ClampMax="100"))
	float InfectionSeverity = 0.0f;

	/** Time since the wound was inflicted (game seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Wound")
	float TimeSinceInflicted = 0.0f;

	FMOWound()
	{
		WoundId = FGuid::NewGuid();
	}
};

/**
 * FastArray container for wounds.
 */

/**
 * Replication-callback contract for whoever owns the wound/condition lists.
 * Plain C++ interface (deliberately NOT a UInterface): it must stay invisible
 * to UHT so the types layer carries no component dependency — the C1 carve
 * rule. The owning component implements this and outlives the lists it owns,
 * so the raw back-pointer cannot dangle.
 */
class IMOWoundListOwner
{
public:
	virtual ~IMOWoundListOwner() = default;
	virtual void OnWoundReplicatedAdd(const FMOWound& Wound) = 0;
	virtual void OnWoundReplicatedChange(const FMOWound& Wound) = 0;
	virtual void OnWoundReplicatedRemove(const FMOWound& Wound) = 0;
	virtual void OnConditionReplicatedAdd(const FMOCondition& Condition) = 0;
	virtual void OnConditionReplicatedChange(const FMOCondition& Condition) = 0;
	virtual void OnConditionReplicatedRemove(const FMOCondition& Condition) = 0;
};

USTRUCT(BlueprintType)
struct MOFRAMEWORKCORE_API FMOWoundList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMOWound> Wounds;

	/** Non-UPROPERTY on purpose: plain interface pointer keeps this header
	 *  free of component types (C1). Owner outlives the list. */
	IMOWoundListOwner* OwnerComponent = nullptr;

	void SetOwner(IMOWoundListOwner* InOwner) { OwnerComponent = InOwner; }

	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FMOWound, FMOWoundList>(Wounds, DeltaParams, *this);
	}

	// Helper methods
	FMOWound* FindWoundById(const FGuid& WoundId);
	const FMOWound* FindWoundById(const FGuid& WoundId) const;
	void AddWound(const FMOWound& NewWound);
	bool RemoveWound(const FGuid& WoundId);
};

template<>
struct TStructOpsTypeTraits<FMOWoundList> : public TStructOpsTypeTraitsBase2<FMOWoundList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

/**
 * Represents an active medical condition or disease.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORKCORE_API FMOCondition : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** Unique identifier for this condition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Condition", meta=(IgnoreForMemberInitializationTest))
	FGuid ConditionId;

	/** Type of condition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Condition")
	EMOConditionType ConditionType = EMOConditionType::None;

	/** Affected body part (None = systemic). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Condition")
	EMOBodyPartType AffectedPart = EMOBodyPartType::None;

	/** Severity of the condition (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Condition", meta=(ClampMin="0", ClampMax="100"))
	float Severity = 0.0f;

	/** Time this condition has been active (game seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Condition")
	float Duration = 0.0f;

	/** Whether treatment has been applied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Medical|Condition")
	bool bIsTreated = false;

	FMOCondition()
	{
		ConditionId = FGuid::NewGuid();
	}
};

/**
 * FastArray container for conditions.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORKCORE_API FMOConditionList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMOCondition> Conditions;

	/** Non-UPROPERTY on purpose: plain interface pointer keeps this header
	 *  free of component types (C1). Owner outlives the list. */
	IMOWoundListOwner* OwnerComponent = nullptr;

	void SetOwner(IMOWoundListOwner* InOwner) { OwnerComponent = InOwner; }

	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FMOCondition, FMOConditionList>(Conditions, DeltaParams, *this);
	}

	// Helper methods
	FMOCondition* FindConditionById(const FGuid& ConditionId);
	const FMOCondition* FindConditionById(const FGuid& ConditionId) const;
	FMOCondition* FindConditionByType(EMOConditionType Type);
	void AddCondition(const FMOCondition& NewCondition);
	bool RemoveCondition(const FGuid& ConditionId);
};

template<>
struct TStructOpsTypeTraits<FMOConditionList> : public TStructOpsTypeTraitsBase2<FMOConditionList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};
