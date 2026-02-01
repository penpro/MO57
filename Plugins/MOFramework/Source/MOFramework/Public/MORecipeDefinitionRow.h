#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"

#include "MORecipeDefinitionRow.generated.h"

/**
 * Types of crafting stations where recipes can be performed.
 */
UENUM(BlueprintType)
enum class EMOCraftingStation : uint8
{
	None UMETA(DisplayName="None"),           // Can craft anywhere (hand crafting)
	Campfire UMETA(DisplayName="Campfire"),
	Workbench UMETA(DisplayName="Workbench"),
	Forge UMETA(DisplayName="Forge"),
	Alchemy UMETA(DisplayName="Alchemy"),
	Kitchen UMETA(DisplayName="Kitchen"),
	Loom UMETA(DisplayName="Loom"),
};

/**
 * A tool required for a recipe (not consumed, just needs to be in inventory).
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOToolRequirement
{
	GENERATED_BODY()

	/** Tool type required (matches FMOItemDefinitionRow::ToolType). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe")
	FName ToolType = NAME_None;

	/** Minimum tool quality required (0 = any quality). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe", meta=(ClampMin="0.0"))
	float MinQuality = 0.0f;

	/** Durability consumed per craft (0 = no consumption). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe", meta=(ClampMin="0"))
	int32 DurabilityConsumed = 1;
};

/**
 * A single ingredient required for a recipe.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMORecipeIngredient
{
	GENERATED_BODY()

	/** Item definition ID required. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe")
	FName ItemDefinitionId = NAME_None;

	/** Quantity required. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe", meta=(ClampMin="1"))
	int32 Quantity = 1;

	/** If true, item must be known (inspected) to use in this recipe. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe")
	bool bRequiresKnowledge = false;
};

/**
 * A single output produced by a recipe.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMORecipeOutput
{
	GENERATED_BODY()

	/** Item definition ID produced. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe")
	FName ItemDefinitionId = NAME_None;

	/** Quantity produced. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe", meta=(ClampMin="1"))
	int32 Quantity = 1;

	/** Chance to produce this output (1.0 = 100%). Useful for byproducts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Chance = 1.0f;
};

/**
 * DataTable row that defines a crafting recipe.
 * The row name is the canonical RecipeId (example: "recipe_apple_pie").
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMORecipeDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Optional sanity field, row name is the real ID. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Core")
	FName RecipeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Core")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Core", meta=(MultiLine=true))
	FText Description;

	/** Ingredients consumed when crafting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Crafting")
	TArray<FMORecipeIngredient> Ingredients;

	/** Outputs produced when crafting completes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Crafting")
	TArray<FMORecipeOutput> Outputs;

	/** Time in seconds to complete the craft. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Crafting", meta=(ClampMin="0.0"))
	float CraftTime = 1.0f;

	/** Crafting station required. None = can craft anywhere. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Requirements")
	EMOCraftingStation RequiredStation = EMOCraftingStation::None;

	/** Skill ID that governs this recipe (for level requirements and XP). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Requirements")
	FName RequiredSkillId = NAME_None;

	/** Minimum skill level required to craft this recipe. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Requirements", meta=(ClampMin="0"))
	int32 RequiredSkillLevel = 0;

	/** Knowledge IDs that must be learned before this recipe becomes available. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Requirements")
	TArray<FName> RequiredKnowledge;

	/** XP granted to RequiredSkillId upon successful craft. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Progression", meta=(ClampMin="0.0"))
	float SkillXPReward = 10.0f;

	/** Icon displayed in crafting UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|UI")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Category for UI organization (e.g., "Food", "Tools", "Weapons"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|UI")
	FName Category = NAME_None;

	// --- Tool Requirements ---

	/** Tools required but not consumed (must be in inventory). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Requirements")
	TArray<FMOToolRequirement> RequiredTools;

	// --- Discovery ---

	/** If true, recipe starts hidden until discovered. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Discovery")
	bool bRequiresDiscovery = false;

	/** Knowledge ID that, when learned, unlocks this recipe. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Discovery")
	FName DiscoveryKnowledgeId = NAME_None;

	/** Skill level that auto-unlocks this recipe (0 = doesn't auto-unlock). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Discovery", meta=(ClampMin="0"))
	int32 DiscoverySkillLevel = 0;
};
