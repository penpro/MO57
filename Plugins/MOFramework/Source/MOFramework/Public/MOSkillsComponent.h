#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOSkillDefinitionRow.h"

#include "MOSkillsComponent.generated.h"

/**
 * Tracks progress for a single skill.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOSkillProgress
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Skill")
	FName SkillId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Skill")
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Skill")
	float CurrentXP = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="MO|Skill")
	float XPToNextLevel = 100.0f;

	float GetLevelProgress() const
	{
		return XPToNextLevel > 0.0f ? CurrentXP / XPToNextLevel : 0.0f;
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOOnSkillLevelUp, FName, SkillId, int32, OldLevel, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOOnExperienceGained, FName, SkillId, float, XPGained, float, TotalXP);

/**
 * Component that manages skill levels and experience.
 * Integrates with skill definition DataTable for XP curves.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOSkillsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOSkillsComponent();

	/** Array of skill progress data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="MO|Skills")
	TArray<FMOSkillProgress> Skills;

	// Delegates
	UPROPERTY(BlueprintAssignable, Category="MO|Skills|Events")
	FMOOnSkillLevelUp OnSkillLevelUp;

	UPROPERTY(BlueprintAssignable, Category="MO|Skills|Events")
	FMOOnExperienceGained OnExperienceGained;

	/**
	 * Add experience to a skill. Will automatically level up if threshold is reached.
	 * @param SkillId The skill to add XP to
	 * @param XPAmount Amount of XP to add
	 * @return True if XP was successfully added
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Skills")
	bool AddExperience(FName SkillId, float XPAmount);

	/**
	 * Get the current level of a skill.
	 * @param SkillId The skill to query
	 * @return Current level (1 if skill not found or not started)
	 */
	UFUNCTION(BlueprintPure, Category="MO|Skills")
	int32 GetSkillLevel(FName SkillId) const;

	/**
	 * Get the full progress data for a skill.
	 * @param SkillId The skill to query
	 * @param OutProgress The progress data (valid only if return is true)
	 * @return True if skill was found
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Skills")
	bool GetSkillProgress(FName SkillId, FMOSkillProgress& OutProgress) const;

	/**
	 * Check if the player has at least the specified skill level.
	 * @param SkillId The skill to check
	 * @param RequiredLevel The minimum level required
	 * @return True if skill level >= RequiredLevel
	 */
	UFUNCTION(BlueprintPure, Category="MO|Skills")
	bool HasSkillLevel(FName SkillId, int32 RequiredLevel) const;

	/**
	 * Get all skills the player has started (has any progress in).
	 * @param OutSkillIds Array to fill with skill IDs
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Skills")
	void GetAllSkillIds(TArray<FName>& OutSkillIds) const;

	/**
	 * Initialize a skill at level 1 if not already present.
	 * @param SkillId The skill to initialize
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Skills")
	void InitializeSkill(FName SkillId);

	/**
	 * Set a skill to a specific level (for debug/admin purposes).
	 * @param SkillId The skill to modify
	 * @param Level The level to set
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Skills")
	void SetSkillLevel(FName SkillId, int32 Level);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/**
	 * Find skill progress by ID. Returns nullptr if not found.
	 */
	FMOSkillProgress* FindSkillProgress(FName SkillId);
	const FMOSkillProgress* FindSkillProgress(FName SkillId) const;

	/**
	 * Calculate XP required for a specific level using the skill's XP curve.
	 * Formula: BaseXP * (Level ^ Exponent)
	 */
	float CalculateXPForLevel(const FMOSkillDefinitionRow* SkillDef, int32 Level) const;

	/**
	 * Get the skill definition from the database.
	 * Returns nullptr if not found.
	 */
	const FMOSkillDefinitionRow* GetSkillDefinition(FName SkillId) const;

	/**
	 * Process level ups after XP is added.
	 */
	void ProcessLevelUps(FMOSkillProgress& Progress, const FMOSkillDefinitionRow* SkillDef);
};
