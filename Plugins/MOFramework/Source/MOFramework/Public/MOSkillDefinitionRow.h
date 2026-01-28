#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"

#include "MOSkillDefinitionRow.generated.h"

/**
 * Broad skill categories for organization and filtering.
 */
UENUM(BlueprintType)
enum class EMOSkillCategory : uint8
{
	None UMETA(DisplayName="None"),
	Survival UMETA(DisplayName="Survival"),      // Cooking, Farming, Foraging
	Crafting UMETA(DisplayName="Crafting"),      // Smithing, Carpentry, Tailoring
	Combat UMETA(DisplayName="Combat"),          // Melee, Ranged, Defense
	Knowledge UMETA(DisplayName="Knowledge"),    // Nutrition, Herbalism, Medicine
	Social UMETA(DisplayName="Social"),          // Trading, Leadership
};

/**
 * DataTable row that defines a skill.
 * The row name is the canonical SkillId (example: "cooking", "herbalism").
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOSkillDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Optional sanity field, row name is the real ID. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Skill|Core")
	FName SkillId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Skill|Core")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Skill|Core", meta=(MultiLine=true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Skill|Core")
	EMOSkillCategory Category = EMOSkillCategory::None;

	/** Maximum achievable level for this skill. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Skill|Progression", meta=(ClampMin="1"))
	int32 MaxLevel = 100;

	/** Base XP required to level up at level 1.
	 *  Formula: XP needed for level N = BaseXPPerLevel * (N ^ XPExponent) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Skill|Progression", meta=(ClampMin="1.0"))
	float BaseXPPerLevel = 100.0f;

	/** Exponent for XP curve scaling. Higher values = steeper curve at higher levels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Skill|Progression", meta=(ClampMin="1.0"))
	float XPExponent = 1.5f;

	/** Icon displayed in skill UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Skill|UI")
	TSoftObjectPtr<UTexture2D> Icon;
};
