#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "MOBodyPartTypes.h"
#include "MOWoundTypes.generated.h"

// Forward declarations
class UMOAnatomyComponent;

/**
 * Represents an active wound on a body part.
 * Uses FFastArraySerializerItem for efficient replication.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWound : public FFastArraySerializerItem
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
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWoundList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMOWound> Wounds;

	UPROPERTY(NotReplicated, Transient)
	TObjectPtr<UMOAnatomyComponent> OwnerComponent;

	void SetOwner(UMOAnatomyComponent* InOwner) { OwnerComponent = InOwner; }

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
struct MOFRAMEWORK_API FMOCondition : public FFastArraySerializerItem
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
struct MOFRAMEWORK_API FMOConditionList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMOCondition> Conditions;

	UPROPERTY(NotReplicated, Transient)
	TObjectPtr<UMOAnatomyComponent> OwnerComponent;

	void SetOwner(UMOAnatomyComponent* InOwner) { OwnerComponent = InOwner; }

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
