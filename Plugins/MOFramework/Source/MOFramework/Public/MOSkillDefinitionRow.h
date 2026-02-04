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

	/** Describes how to gain XP for this skill (displayed in UI). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Skill|Core", meta=(MultiLine=true))
	FText HowToIncrease;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Skill|Core")
	EMOSkillCategory Category = EMOSkillCategory::None;

	/** Maximum achievable level for this skill. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Skill|Progression", meta=(ClampMin="1"))
	int32 MaxLevel = 100;

	/** Base XP required to go from level 0 to level 1.
	 *  Formula: XP for level N->N+1 = BaseXPPerLevel * XPScaleFactor^N */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Skill|Progression", meta=(ClampMin="1.0"))
	float BaseXPPerLevel = 100.0f;

	/** Scale factor applied per level. Each level requires ScaleFactor times more XP than the previous.
	 *  Example with 1.25: Level 0->1 = 100 XP, Level 1->2 = 125 XP, Level 2->3 = 156 XP, etc. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Skill|Progression", meta=(ClampMin="1.0", DisplayName="XP Scale Factor"))
	float XPExponent = 1.25f;

	/** Icon displayed in skill UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Skill|UI")
	TSoftObjectPtr<UTexture2D> Icon;
};
