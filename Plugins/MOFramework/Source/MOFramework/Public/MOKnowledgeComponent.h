/**
 * =============================================================================
 * MOKnowledgeComponent.h - Item Inspection & Knowledge Discovery System
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Pawn component tracking item knowledge learned through inspection.
 * Implements skill-gated knowledge discovery with diminishing returns.
 * Items grant XP to skills and unlock knowledge entries based on inspection.
 *
 * KEY RESPONSIBILITIES:
 * 1. Track inspection progress per ItemDefinitionId
 * 2. Award skill XP and knowledge from item inspection
 * 3. Implement diminishing returns based on skill level
 * 4. Provide knowledge queries for recipe/action gating
 * 5. Support direct knowledge grants (quests, tutorials)
 *
 * ARCHITECTURE NOTES:
 * - ItemKnowledge array stores per-item inspection progress
 * - AllLearnedKnowledge is flat array for fast HasKnowledge() lookups
 * - Requires UMOSkillsComponent for XP grants and level checks
 * - Item inspection data comes from FMOItemDefinitionRow.Inspection field
 *
 * INSPECTION FLOW:
 * Player inspects item -> InspectItem(ItemDefId, SkillsComp)
 * -> Find/create FMOItemKnowledgeProgress -> Check skill level gates
 * -> Calculate XP based on learning potential -> Award XP to skills
 * -> Unlock knowledge if skill requirements met -> Return FMOInspectionResult
 *
 * LEARNING POTENTIAL (diminishing returns):
 * - Full: First inspection or skill level well below cap
 * - Partial: Approaching skill cap, reduced XP per inspection
 * - None: Skill cap reached or all knowledge unlocked
 *
 * CRITICAL PATTERNS:
 * 1. Inspection:
 *    InspectItem() -> Increment InspectionCount -> Check skill gates
 *    -> Award XP to SkillsComponent -> Unlock eligible knowledge
 *    -> Broadcast OnItemInspected with result
 *
 * 2. Knowledge Check:
 *    HasKnowledge(KnowledgeId) uses AllLearnedKnowledge for O(n) lookup
 *    HasAllKnowledge() for recipe requirements
 *
 * KNOWN PITFALLS:
 * 1. SKILL COMPONENT REQUIRED: InspectItem() requires SkillsComponent.
 *    Pass nullptr only for knowledge-only items (no XP grants).
 *
 * 2. KNOWLEDGE ID FORMAT: Knowledge IDs are FNames from DataTable.
 *    Case-sensitive. Typos create orphan knowledge entries.
 *
 * 3. DUAL STORAGE: Knowledge stored both in ItemKnowledge[].UnlockedKnowledge
 *    AND AllLearnedKnowledge. Keep in sync - GrantKnowledge() handles this.
 *
 * 4. INSPECTION COUNT: InspectionCount tracks raw inspections, not
 *    successful learning. Use for analytics, not progression.
 *
 * RELATED FILES:
 * - MOItemDefinitionRow.h - Item's Inspection field defines XP grants
 * - MOSkillsComponent.h - Receives XP from inspection
 * - MOCharacterUIController.h - Shows inspection UI
 * - MOInventoryComponent.h - Source of items to inspect
 *
 * TESTING CHECKLIST:
 * [ ] InspectItem grants correct XP to skills
 * [ ] Knowledge unlocks when skill requirements met
 * [ ] HasKnowledge returns true after learning
 * [ ] Diminishing returns reduce XP appropriately
 * [ ] GrantKnowledge works for direct grants
 * [ ] OnItemInspected fires with correct result
 * [ ] Save/load preserves knowledge state
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

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
 * Enum describing the learning potential from an item.
 */
UENUM(BlueprintType)
enum class EMOLearningPotential : uint8
{
	/** Full learning potential - first inspection or low skill level. */
	Full,
	/** Partial learning potential - approaching skill cap, diminishing returns. */
	Partial,
	/** No learning potential - skill cap reached or all knowledge learned. */
	None
};

/**
 * Represents XP granted to a single skill or knowledge entry during inspection.
 * Used to cycle through showing each affected entry in notifications.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOInspectionXPGrant
{
	GENERATED_BODY()

	/** The skill or knowledge ID that received XP. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Knowledge")
	FName Id = NAME_None;

	/** If true, this is a knowledge entry. If false, it's a skill. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Knowledge")
	bool bIsKnowledge = false;

	/** Amount of XP granted. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Knowledge")
	float XPAmount = 0.0f;

	/** The level before XP was granted. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Knowledge")
	int32 LevelBefore = 0;

	/** The level after XP was granted. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Knowledge")
	int32 LevelAfter = 0;

	/** True if this grant caused a level up. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Knowledge")
	bool bLeveledUp = false;
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

	/** All XP grants from this inspection (both skills and knowledge).
	 *  Use this to cycle through notification displays. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Knowledge")
	TArray<FMOInspectionXPGrant> XPGrants;

	/** Whether this was the first time inspecting this item. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Knowledge")
	bool bFirstInspection = false;

	/** Learning potential remaining for this item. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Knowledge")
	EMOLearningPotential LearningPotential = EMOLearningPotential::Full;

	/** Feedback message to display to the player. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Knowledge")
	FText FeedbackMessage;

	/** True if there's nothing more to learn from this item. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Knowledge")
	bool bNothingMoreToLearn = false;
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

	/**
	 * Check if there is anything more to learn from inspecting an item.
	 * Returns false if skill cap is reached and all knowledge is learned.
	 * @param ItemDefinitionId The item to check
	 * @param SkillsComponent The player's skills (for level checks)
	 * @return True if inspection would provide some benefit
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Knowledge")
	bool CanLearnMoreFromItem(FName ItemDefinitionId, UMOSkillsComponent* SkillsComponent) const;

	/**
	 * Get the learning potential for an item (Full, Partial, or None).
	 * @param ItemDefinitionId The item to check
	 * @param SkillsComponent The player's skills (for level checks)
	 * @return Learning potential enum
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Knowledge")
	EMOLearningPotential GetLearningPotential(FName ItemDefinitionId, UMOSkillsComponent* SkillsComponent) const;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/**
	 * Find item knowledge progress by ID. Returns nullptr if not found.
	 */
	FMOItemKnowledgeProgress* FindItemKnowledge(FName ItemDefinitionId);
	const FMOItemKnowledgeProgress* FindItemKnowledge(FName ItemDefinitionId) const;
};
