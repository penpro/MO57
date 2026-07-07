#pragma once

/**
 * =============================================================================
 * MOItemDefinitionRow.h
 * =============================================================================
 *
 * PURPOSE:
 *   DataTable row structure for item definitions (DT_Items).
 *   Central definition for all item properties, visuals, and behaviors.
 *
 * RESPONSIBILITIES:
 *   - Item identification (ItemId, DisplayName, Description)
 *   - Item classification (EMOItemType, Tags, Rarity)
 *   - Physical properties (Weight, MaxStackSize, BaseValue)
 *   - Tool capabilities (ToolCapabilities array with effectiveness)
 *   - Weapon properties (WeaponProfileId linking to DT_Weapons)
 *   - Visual data (UI icons, world mesh, held transform)
 *   - Nutrition data for consumables
 *   - Inspection/knowledge grants
 *
 * =============================================================================
 * BEST PRACTICES
 * =============================================================================
 *
 * 1. TOOL CAPABILITIES (Multi-Tool System):
 *    - Items can have multiple tool types via TArray<FMOToolCapability>
 *    - Each capability has ToolType + Effectiveness (0.0 - 1.0)
 *    - Use HasToolCapability() and GetToolEffectiveness() helpers
 *    - Example: Claw hammer = [{Hammer, 1.0}, {Pickaxe, 0.6}]
 *
 * 2. ENUM USAGE:
 *    - Use EMOToolType enum, NOT FName strings for tool types
 *    - Use EMOItemType for broad categorization, Tags for specifics
 *    - StaticEnum<EMOToolType>() for enum-to-string conversion
 *
 * 3. WEAPON INTEGRATION:
 *    - Set bIsWeapon = true for combat items
 *    - Set WeaponProfileId to reference row in DT_Weapons
 *    - Weapon damage profiles are in FMOWeaponDamageProfileRow
 *
 * 4. JSON IMPORT/EXPORT:
 *    - UE exports JSON as UTF-16 encoding
 *    - ToolCapabilities format: [{"ToolType":"Hammer","Effectiveness":0.8}]
 *    - Use Tools/update_tool_capabilities.py for batch updates
 *
 * 5. VISUAL DATA:
 *    - UI.IconSmall/IconLarge: Soft references (TSoftObjectPtr)
 *    - WorldVisual: Mesh for dropped items
 *    - HeldVisual: Mesh and transform for held items
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] Hatchet Removed: EMOToolType::Hatchet was merged into Axe.
 *           All items/recipes using Hatchet must use Axe instead.
 *
 * [2024-02] ToolType vs ToolCapabilities: OLD format used single ToolType
 *           and ToolQuality fields. NEW format uses ToolCapabilities array.
 *           Use the Python migration script for conversion.
 *
 * [2024-02] Effectiveness Values: 1.0 = purpose-built tool, 0.5 = adequate
 *           substitute, 0.3 = barely usable. Don't exceed 1.0.
 *
 * [2024-01] NSLOCTEXT in JSON: Display names use NSLOCTEXT format. Extract
 *           readable text via regex: NSLOCTEXT\([^,]+,\s*[^,]+,\s*"([^"]+)"
 *
 * =============================================================================
 * DATA TABLE NOTES
 * =============================================================================
 *
 * DT_Items DataTable:
 * - Row name = canonical ItemDefinitionId (e.g., "Hammerstone01")
 * - ItemId field is optional sanity check (row name is authoritative)
 * - Import from JSON recommended over CSV for complex structs
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 *
 * - MOInventoryComponent.h      : Inventory storage using these definitions
 * - MOCraftingSubsystem.h       : Tool requirement checking
 * - MOWeaponTypes.h             : Weapon damage profile definitions
 * - MORecipeDefinitionRow.h     : Recipe ingredient references
 * - Tools/update_tool_capabilities.py : JSON migration script
 *
 * =============================================================================
 * CLAUDE: UPDATE THIS HEADER
 * =============================================================================
 *
 * When you encounter issues with this file:
 * 1. Add a dated entry to KNOWN PITFALLS section
 * 2. Include the symptom and solution
 * 3. Update BEST PRACTICES if a new pattern emerges
 *
 * When adding new item properties:
 * - Add UPROPERTY with appropriate EditCondition
 * - Update JSON export/import scripts if needed
 * - Add helper methods for complex lookups
 *
 * =============================================================================
 */

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

/**
 * Tool types for crafting and harvesting.
 * Use this enum instead of FName strings to avoid typos and enable dropdown selection.
 * Items can have multiple tool capabilities with different effectiveness ratings.
 */
UENUM(BlueprintType)
enum class EMOToolType : uint8
{
	None UMETA(DisplayName="None (No Tool)"),

	// Cutting/Chopping
	Axe UMETA(DisplayName="Axe"),           // Includes hatchets (small axes)
	Saw UMETA(DisplayName="Saw"),

	// Mining/Breaking
	Pickaxe UMETA(DisplayName="Pickaxe"),
	Hammer UMETA(DisplayName="Hammer"),
	Chisel UMETA(DisplayName="Chisel"),

	// Cutting/Slicing
	Knife UMETA(DisplayName="Knife"),
	Cleaver UMETA(DisplayName="Cleaver"),
	Scissors UMETA(DisplayName="Scissors"),

	// Digging/Farming
	Shovel UMETA(DisplayName="Shovel"),
	Hoe UMETA(DisplayName="Hoe"),
	Sickle UMETA(DisplayName="Sickle"),

	// Crafting
	Needle UMETA(DisplayName="Needle"),
	Awl UMETA(DisplayName="Awl"),
	Tongs UMETA(DisplayName="Tongs"),
	Mallet UMETA(DisplayName="Mallet"),

	// Containers (for crafting that needs liquid/heat containment)
	Pot UMETA(DisplayName="Pot/Cauldron"),
	Crucible UMETA(DisplayName="Crucible"),
	Mortar UMETA(DisplayName="Mortar & Pestle"),

	// Misc
	Rope UMETA(DisplayName="Rope"),
	Firestarter UMETA(DisplayName="Firestarter"),
};

/**
 * Represents an item's capability to function as a specific tool type.
 * An item can have multiple tool capabilities (e.g., a claw hammer can be Hammer + Pickaxe).
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOToolCapability
{
	GENERATED_BODY()

	/** The tool type this item can function as. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Tool")
	EMOToolType ToolType = EMOToolType::None;

	/**
	 * How effective this item is as this tool type (0.0 - 1.0).
	 * 1.0 = purpose-built tool (steel pickaxe as Pickaxe)
	 * 0.7 = good alternative (claw hammer as Pickaxe)
	 * 0.5 = makeshift (rock as Hammer)
	 * 0.3 = barely usable (stick as Hammer)
	 * Affects craft time and output quality.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Tool", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Effectiveness = 1.0f;

	FMOToolCapability() = default;
	FMOToolCapability(EMOToolType InType, float InEffectiveness = 1.0f)
		: ToolType(InType), Effectiveness(InEffectiveness) {}
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

/**
 * Equipment slot type for items. Determines which equipment slot(s) the item can be equipped to.
 */
UENUM(BlueprintType)
enum class EMOEquipmentSlotType : uint8
{
	None UMETA(DisplayName="None (Not Equippable)"),
	Head UMETA(DisplayName="Head"),
	Chest UMETA(DisplayName="Chest"),
	Hands UMETA(DisplayName="Hands (Gloves)"),
	Legs UMETA(DisplayName="Legs"),
	Feet UMETA(DisplayName="Feet"),
	Back UMETA(DisplayName="Back (Backpack)"),
	Held UMETA(DisplayName="Held (Left/Right Hand)"),
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

/**
 * Visual data for how an item appears when held in hands.
 * Separate from WorldVisual since held items need different positioning.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOItemHeldVisual
{
	GENERATED_BODY()

	/** Static mesh to display when held. If null, uses WorldVisual.StaticMesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Held")
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	/** Material override for held mesh. If null, uses mesh default or WorldVisual.MaterialOverride. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Held")
	TSoftObjectPtr<UMaterialInterface> MaterialOverride;

	/**
	 * If true, left hand uses a mirrored version of RightHandTransform (flips Y scale).
	 * If false, left hand uses LeftHandTransform for independent positioning.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Held")
	bool bMirrorForLeftHand = true;

	/** Transform offset for right hand. Position/rotation relative to hand socket. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Held")
	FTransform RightHandTransform = FTransform::Identity;

	/**
	 * Transform offset for left hand (only used when bMirrorForLeftHand is false).
	 * TIP: Set up RightHandTransform first, then copy those values here and adjust.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Held")
	FTransform LeftHandTransform = FTransform::Identity;

	/** Get the appropriate transform for a given hand. */
	FTransform GetTransformForHand(bool bIsLeftHand) const
	{
		if (!bIsLeftHand)
		{
			return RightHandTransform;
		}

		if (bMirrorForLeftHand)
		{
			// Mirror the right hand transform
			FTransform Mirrored = RightHandTransform;
			FVector Scale = Mirrored.GetScale3D();
			Scale.Y *= -1.0f; // Mirror on Y axis
			Mirrored.SetScale3D(Scale);
			return Mirrored;
		}

		return LeftHandTransform;
	}
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
 * Defines XP granted to a skill or knowledge when inspecting an item.
 * Both skills and knowledge use the same leveling system.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOInspectionGrant
{
	GENERATED_BODY()

	/** The skill or knowledge ID to grant XP to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Inspection")
	FName Id = NAME_None;

	/** If true, this is a knowledge entry (shows in knowledge panel). If false, it's a skill. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Inspection")
	bool bIsKnowledge = false;

	/** Amount of XP to grant per inspection. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Inspection")
	float XPAmount = 100.0f;

	/** Maximum level this item can contribute XP towards for this entry.
	 *  Once the level is at or above this, no more XP is granted from this item.
	 *  0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Inspection", meta=(ClampMin="0"))
	int32 MaxLevel = 0;
};

/**
 * Inspection data - defines what skills and knowledge gain XP when inspecting this item.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOItemInspection
{
	GENERATED_BODY()

	/** Skills and knowledge that gain XP when this item is inspected.
	 *  Each entry can be either a skill or knowledge with its own XP amount and max level cap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Inspection")
	TArray<FMOInspectionGrant> Grants;
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

	/**
	 * Seasonal forage window (V2.4). Empty = available year-round. Otherwise
	 * a comma list of season names ("Summer,Autumn") — harvesting this item
	 * from a resource node outside its window yields nothing (berries do not
	 * exist in winter). Season indices: 0=Spring 1=Summer 2=Autumn 3=Winter.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Item|Core")
	FString ForageSeasons;

	/** True when this item can be foraged in the given season (see ForageSeasons). */
	bool IsInForageSeason(int32 SeasonIndex) const
	{
		if (ForageSeasons.IsEmpty())
		{
			return true;
		}
		static const TCHAR* Names[] = { TEXT("Spring"), TEXT("Summer"), TEXT("Autumn"), TEXT("Winter") };
		if (SeasonIndex < 0 || SeasonIndex > 3)
		{
			return true;
		}
		return ForageSeasons.Contains(Names[SeasonIndex]);
	}

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

	/** Which equipment slot type this item can be equipped to. Only used if bEquippable is true. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Flags", meta=(EditCondition="bEquippable"))
	EMOEquipmentSlotType EquipmentSlotType = EMOEquipmentSlotType::None;

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

	/** Visual data for when item is held in hands. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Held")
	FMOItemHeldVisual HeldVisual;

	/** Nutrition data for consumable items. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Nutrition")
	FMOItemNutrition Nutrition;

	/** Inspection and knowledge data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Inspection")
	FMOItemInspection Inspection;

	// --- Tool Properties ---

	/** If true, this item can function as a crafting tool. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Tool")
	bool bIsTool = false;

	/**
	 * Tool capabilities this item has. An item can function as multiple tool types.
	 * Example: A claw hammer has [{Hammer, 1.0}, {Pickaxe, 0.6}]
	 * Example: A sharp rock has [{Knife, 0.4}, {Hammer, 0.3}]
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Tool", meta=(EditCondition="bIsTool"))
	TArray<FMOToolCapability> ToolCapabilities;

	/** Maximum durability (0 = indestructible). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Tool", meta=(EditCondition="bIsTool", ClampMin="0"))
	int32 MaxDurability = 0;

	// --- Weapon Properties ---

	/** If true, this item can be used as a weapon in combat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Weapon")
	bool bIsWeapon = false;

	/**
	 * Row name in DT_Weapons referencing this weapon's damage profile.
	 * Links to FMOWeaponDamageProfileRow for attack stats, grip type, etc.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Item|Weapon", meta=(EditCondition="bIsWeapon"))
	FName WeaponProfileId = NAME_None;

	// --- Helper Functions ---

	/** Check if this item can function as the specified tool type. */
	bool HasToolCapability(EMOToolType InToolType) const
	{
		for (const FMOToolCapability& Cap : ToolCapabilities)
		{
			if (Cap.ToolType == InToolType)
			{
				return true;
			}
		}
		return false;
	}

	/** Get the effectiveness of this item as the specified tool type. Returns 0 if not capable. */
	float GetToolEffectiveness(EMOToolType InToolType) const
	{
		for (const FMOToolCapability& Cap : ToolCapabilities)
		{
			if (Cap.ToolType == InToolType)
			{
				return Cap.Effectiveness;
			}
		}
		return 0.0f;
	}

	/** Get the primary (most effective) tool type. Returns None if not a tool. */
	EMOToolType GetPrimaryToolType() const
	{
		EMOToolType Best = EMOToolType::None;
		float BestEffectiveness = 0.0f;
		for (const FMOToolCapability& Cap : ToolCapabilities)
		{
			if (Cap.Effectiveness > BestEffectiveness)
			{
				Best = Cap.ToolType;
				BestEffectiveness = Cap.Effectiveness;
			}
		}
		return Best;
	}

	/** Check if this item has a valid weapon profile for combat. */
	bool HasWeaponProfile() const
	{
		return bIsWeapon && !WeaponProfileId.IsNone();
	}
};
