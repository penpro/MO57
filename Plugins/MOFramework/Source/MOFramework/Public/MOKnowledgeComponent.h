#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "MOKnowledgeComponent.generated.h"

class UMOSkillsComponent;

/**
 * Tracks inspection progress for a single item definition.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOItemKnowledgeProgress
{
	GENERATED_BODY()

	/** The item definition this progress is for. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Knowledge")
	FName ItemDefinitionId = NAME_None;

	/** Number of times this item has been inspected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Knowledge")
	int32 InspectionCount = 0;

	/** Knowledge IDs that have been unlocked from inspecting this item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Knowledge")
	TArray<FName> UnlockedKnowledge;

	/** Skill level when this item was last inspected (for gating checks). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Knowledge")
	float LastInspectionSkillLevel = 0.0f;
};

/**
 * Result of an inspection action.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOInspectionResult
{
	GENERATED_BODY()

	/** Whether the inspection was successful. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Knowledge")
	bool bSuccess = false;

	/** New knowledge IDs learned from this inspection. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Knowledge")
	TArray<FName> NewKnowledge;

	/** XP granted to each skill from this inspection. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Knowledge")
	TMap<FName, float> XPGranted;

	/** Whether this was the first time inspecting this item. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Knowledge")
	bool bFirstInspection = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnKnowledgeLearned, FName, KnowledgeId, FName, FromItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnItemInspected, FName, ItemDefinitionId, const FMOInspectionResult&, Result);

/**
 * Component that tracks item knowledge learned through inspection.
 * Implements skill-gated knowledge discovery with diminishing returns.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOKnowledgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOKnowledgeComponent();

	/** Array of item inspection progress. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="MO|Knowledge")
	TArray<FMOItemKnowledgeProgress> ItemKnowledge;

	/** Flat array of all learned knowledge IDs for quick lookups. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="MO|Knowledge")
	TArray<FName> AllLearnedKnowledge;

	// Delegates
	UPROPERTY(BlueprintAssignable, Category="MO|Knowledge|Events")
	FMOOnKnowledgeLearned OnKnowledgeLearned;

	UPROPERTY(BlueprintAssignable, Category="MO|Knowledge|Events")
	FMOOnItemInspected OnItemInspected;

	// Configuration
	/** Multiplier applied to XP rewards on subsequent inspections (diminishing returns). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Knowledge|Config", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DiminishingReturnsFactor = 0.5f;

	/** After this many inspections, no more XP is granted for an item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Knowledge|Config", meta=(ClampMin="1"))
	int32 MaxInspectionsForXP = 5;

	/**
	 * Inspect an item, potentially learning knowledge and gaining skill XP.
	 * @param ItemDefinitionId The item to inspect
	 * @param SkillsComponent The player's skills (for level requirements and XP grants)
	 * @return Result of the inspection
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Knowledge")
	FMOInspectionResult InspectItem(FName ItemDefinitionId, UMOSkillsComponent* SkillsComponent);

	/**
	 * Check if a specific knowledge ID has been learned.
	 * @param KnowledgeId The knowledge to check
	 * @return True if learned
	 */
	UFUNCTION(BlueprintPure, Category="MO|Knowledge")
	bool HasKnowledge(FName KnowledgeId) const;

	/**
	 * Check if all knowledge IDs in the array have been learned.
	 * @param KnowledgeIds Array of knowledge to check
	 * @return True if all are learned
	 */
	UFUNCTION(BlueprintPure, Category="MO|Knowledge")
	bool HasAllKnowledge(const TArray<FName>& KnowledgeIds) const;

	/**
	 * Check if any knowledge ID in the array has been learned.
	 * @param KnowledgeIds Array of knowledge to check
	 * @return True if at least one is learned
	 */
	UFUNCTION(BlueprintPure, Category="MO|Knowledge")
	bool HasAnyKnowledge(const TArray<FName>& KnowledgeIds) const;

	/**
	 * Get the inspection progress for a specific item.
	 * @param ItemDefinitionId The item to query
	 * @param OutProgress The progress data (valid only if return is true)
	 * @return True if item has been inspected at least once
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Knowledge")
	bool GetInspectionProgress(FName ItemDefinitionId, FMOItemKnowledgeProgress& OutProgress) const;

	/**
	 * Get all learned knowledge IDs.
	 * @param OutKnowledgeIds Array to fill with knowledge IDs
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Knowledge")
	void GetAllLearnedKnowledge(TArray<FName>& OutKnowledgeIds) const;

	/**
	 * Get all items that have been inspected.
	 * @param OutItemIds Array to fill with item definition IDs
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Knowledge")
	void GetAllInspectedItems(TArray<FName>& OutItemIds) const;

	/**
	 * Directly grant a knowledge ID (for quest rewards, etc).
	 * @param KnowledgeId The knowledge to grant
	 * @return True if newly learned, false if already known
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Knowledge")
	bool GrantKnowledge(FName KnowledgeId);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/**
	 * Find item knowledge progress by ID. Returns nullptr if not found.
	 */
	FMOItemKnowledgeProgress* FindItemKnowledge(FName ItemDefinitionId);
	const FMOItemKnowledgeProgress* FindItemKnowledge(FName ItemDefinitionId) const;

	/**
	 * Calculate XP multiplier based on inspection count (diminishing returns).
	 */
	float GetXPMultiplier(int32 InspectionCount) const;
};
