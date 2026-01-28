#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"

#include "MOItemDefinitionRow.generated.h"

/**
 * High-level classification for items.
 * Keep this broad and stable. More granular categorization should use Tags.
 */
UENUM(BlueprintType)
enum class EMOItemType : uint8
{
	None UMETA(DisplayName="None"),
	Consumable UMETA(DisplayName="Consumable"),
	Material UMETA(DisplayName="Material"),
	Tool UMETA(DisplayName="Tool"),
	Weapon UMETA(DisplayName="Weapon"),
	Ammo UMETA(DisplayName="Ammo"),
	Armor UMETA(DisplayName="Armor"),
	KeyItem UMETA(DisplayName="Key Item"),
	Quest UMETA(DisplayName="Quest"),
	Currency UMETA(DisplayName="Currency"),
	Misc UMETA(DisplayName="Misc"),
};

UENUM(BlueprintType)
enum class EMOItemRarity : uint8
{
	Common UMETA(DisplayName="Common"),
	Uncommon UMETA(DisplayName="Uncommon"),
	Rare UMETA(DisplayName="Rare"),
	Epic UMETA(DisplayName="Epic"),
	Legendary UMETA(DisplayName="Legendary"),
};

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOItemUIVisual
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|UI")
	TSoftObjectPtr<UTexture2D> IconSmall;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|UI")
	TSoftObjectPtr<UTexture2D> IconLarge;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|UI")
	FLinearColor Tint = FLinearColor::White;
};

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOItemWorldVisual
{
	GENERATED_BODY()

	/** Actor class to spawn when dropping this item into the world.
	 *  Should be AMOWorldItem or a subclass. If not set, uses default AMOWorldItem. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|World")
	TSoftClassPtr<AActor> WorldActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|World")
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|World")
	TSoftObjectPtr<UMaterialInterface> MaterialOverride;

	/** Relative transform applied to the WorldItem's mesh component. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|World")
	FTransform RelativeTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|World")
	bool bSimulatePhysics = false;
};

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOItemScalarProperty
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Props")
	FName Key = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Props")
	float Value = 0.f;
};

/**
 * Nutrition data for consumable items.
 * Models realistic macronutrients, vitamins, and minerals.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOItemNutrition
{
	GENERATED_BODY()

	// Macronutrients
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Nutrition")
	float Calories = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Nutrition")
	float WaterContent = 0.0f;  // ml

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Nutrition")
	float Protein = 0.0f;  // grams

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Nutrition")
	float Carbohydrates = 0.0f;  // grams

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Nutrition")
	float Fat = 0.0f;  // grams

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Nutrition")
	float Fiber = 0.0f;  // grams

	// Vitamins (percentage of daily value, 0-100+)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Nutrition|Vitamins")
	float VitaminA = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Nutrition|Vitamins")
	float VitaminB = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Nutrition|Vitamins")
	float VitaminC = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Nutrition|Vitamins")
	float VitaminD = 0.0f;

	// Minerals (percentage of daily value, 0-100+)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Nutrition|Minerals")
	float Iron = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Nutrition|Minerals")
	float Calcium = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Nutrition|Minerals")
	float Potassium = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Nutrition|Minerals")
	float Sodium = 0.0f;
};

/**
 * Inspection data - defines what skills gain XP and what knowledge can be learned
 * when a player inspects this item.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOItemInspection
{
	GENERATED_BODY()

	/** Skills that gain XP when this item is inspected. Map of SkillId -> XP amount. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Inspection")
	TMap<FName, float> SkillExperienceGrants;

	/** Knowledge IDs that can be learned from inspecting this item. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Inspection")
	TArray<FName> KnowledgeIds;

	/** Minimum skill level required to learn specific knowledge from inspection.
	 *  Map of KnowledgeId -> required skill level. Knowledge not in this map has no requirement. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Inspection")
	TMap<FName, int32> KnowledgeSkillRequirements;
};

/**
 * DataTable row that defines an item.
 * The row name is the canonical ItemDefinitionId (example: "apple01").
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOItemDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Optional sanity field, row name is the real ID. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Core")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Core")
	EMOItemType ItemType = EMOItemType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Core")
	EMOItemRarity Rarity = EMOItemRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Core")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Core", meta=(MultiLine=true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Core")
	FText ShortDescription;

	/** Free-form tags (example: Food, Fruit, Healing, Quest). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Core")
	TArray<FName> Tags;

	/** If stackable, this is the cap per slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Stacking", meta=(ClampMin="1"))
	int32 MaxStackSize = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Economy", meta=(ClampMin="0.0"))
	float Weight = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Economy", meta=(ClampMin="0.0"))
	float BaseValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Flags")
	bool bConsumable = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Flags")
	bool bEquippable = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Flags")
	bool bQuestItem = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Flags")
	bool bCanDrop = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Flags")
	bool bCanTrade = true;

	/** Flexible numeric payload for future systems (Damage, Healing, Warmth, etc). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Props")
	TArray<FMOItemScalarProperty> ScalarProperties;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|UI")
	FMOItemUIVisual UI;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|World")
	FMOItemWorldVisual WorldVisual;

	/** Nutrition data for consumable items. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Nutrition")
	FMOItemNutrition Nutrition;

	/** Inspection and knowledge data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Inspection")
	FMOItemInspection Inspection;
};
