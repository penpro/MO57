#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"

#include "MORecipeDefinitionRow.generated.h"

class AMOBuildableActor;

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

// ============================================================================
// BUILDING PLACEMENT & CONSTRUCTION TYPES
// ============================================================================

/**
 * Placement preferences for a building type.
 * Controls what surfaces the building can be placed on and rotation constraints.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOBuildingPlacementData
{
	GENERATED_BODY()

	// --- Surface Preferences ---

	/** If true, this building prefers placement on ground (upward-facing surfaces). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Placement")
	bool bPrefersGroundPlacement = true;

	/** If true, this building prefers placement on walls (vertical surfaces). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Placement")
	bool bPrefersWallPlacement = false;

	/** If true, this building prefers placement on ceilings (downward-facing surfaces). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Placement")
	bool bPrefersCeilingPlacement = false;

	// --- Rotation Constraints ---

	/** If true, player can rotate around Z axis (Q/E keys). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Rotation")
	bool bAllowZRotation = true;

	/** If true, player can rotate around X axis (pitch, W/S keys). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Rotation")
	bool bAllowXRotation = false;

	/** If true, player can rotate around Y axis (roll, A/D keys). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Rotation")
	bool bAllowYRotation = false;

	/** Degrees per rotation key press. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Rotation", meta=(ClampMin="1.0", ClampMax="90.0"))
	float RotationIncrement = 15.0f;

	// --- Collision ---

	/** If true, placement is only valid when ghost doesn't overlap other actors. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Collision")
	bool bRequiresNoCollision = true;

	// --- Visual ---

	/** Static mesh to display as the ghost preview. If not set, uses the buildable actor's mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Visual")
	TSoftObjectPtr<UStaticMesh> PreviewMesh;

	/** Scale of the preview mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Visual")
	FVector PreviewScale = FVector(1.0f, 1.0f, 1.0f);

	// --- Actor ---

	/** Actor class to spawn when this building is placed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Actor")
	TSubclassOf<AMOBuildableActor> BuildableActorClass;
};

/**
 * A single build part - either an item to consume or an action to perform.
 * Used for weighted time distribution during construction.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOBuildPart
{
	GENERATED_BODY()

	/**
	 * Item definition ID to consume. If set, this part consumes items.
	 * If NAME_None, this is an action-based part (use ActionId).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Part")
	FName ItemDefinitionId = NAME_None;

	/**
	 * Action ID for non-item parts (e.g., "Dig", "Hammer", "Weave").
	 * Used when ItemDefinitionId is NAME_None.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Part")
	FName ActionId = NAME_None;

	/** How many items to consume OR how many action repetitions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Part", meta=(ClampMin="1"))
	int32 Quantity = 1;

	/**
	 * Weight for time distribution. Higher weight = more time spent on this part.
	 * Total build time is distributed proportionally by weight.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Part", meta=(ClampMin="1"))
	int32 Weight = 1;

	/** Returns true if this is an item-based part. */
	bool IsItemPart() const { return !ItemDefinitionId.IsNone(); }

	/** Returns true if this is an action-based part. */
	bool IsActionPart() const { return ItemDefinitionId.IsNone() && !ActionId.IsNone(); }
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

	// ---- Tool Requirement Flags ----

	/**
	 * If true, this tool is absolutely required - recipe cannot be done without it.
	 * Example: Skinning REQUIRES a knife, no exceptions.
	 * If false, tool is optional but provides benefits (faster, better quality, etc.).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Requirement")
	bool bIsRequired = true;

	/**
	 * When tool is NOT required (bIsRequired=false) but is missing, multiply craft time by this value.
	 * Example: Digging with hands (no shovel) takes 5x longer, so set to 5.0.
	 * Only applies when bIsRequired is false. Set to 1.0 for no penalty.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Requirement",
		meta=(ClampMin="1.0", EditCondition="!bIsRequired", EditConditionHides))
	float MissingToolTimeMultiplier = 1.0f;

	/**
	 * When tool is NOT required but is missing, multiply output quality by this value.
	 * Example: Crafting without proper tools yields lower quality results.
	 * Only applies when bIsRequired is false. Set to 1.0 for no penalty.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Requirement",
		meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="!bIsRequired", EditConditionHides))
	float MissingToolQualityMultiplier = 1.0f;
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

	/** Knowledge ID that, when reaching DiscoveryKnowledgeLevel, unlocks this recipe. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Discovery")
	FName DiscoveryKnowledgeId = NAME_None;

	/** Knowledge level required to unlock this recipe (0 = unlocks when knowledge first learned, 1+ = specific level). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Discovery", meta=(ClampMin="0"))
	int32 DiscoveryKnowledgeLevel = 1;

	/** Skill level that auto-unlocks this recipe (0 = doesn't auto-unlock). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Discovery", meta=(ClampMin="0"))
	int32 DiscoverySkillLevel = 0;

	// ============================================================================
	// BUILDING SYSTEM (only used when bIsBuilding is true)
	// ============================================================================

	/** If true, this recipe is for a placeable building rather than a crafted item. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Building")
	bool bIsBuilding = false;

	/** Placement preferences for this building (surfaces, rotation, collision). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Building", meta=(EditCondition="bIsBuilding", EditConditionHides))
	FMOBuildingPlacementData PlacementData;

	/**
	 * Weighted build parts for construction.
	 * Each part is an item or action that contributes to the build.
	 * Time is distributed proportionally by weight.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Building", meta=(EditCondition="bIsBuilding", EditConditionHides))
	TArray<FMOBuildPart> BuildParts;

	/** Total time in seconds to complete construction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Building", meta=(EditCondition="bIsBuilding", EditConditionHides, ClampMin="0.0"))
	float TotalBuildTime = 60.0f;

	/** Range in Unreal Units for gathering materials during construction (~150 UU = 5 feet). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Building", meta=(EditCondition="bIsBuilding", EditConditionHides, ClampMin="0.0"))
	float BuildRange = 150.0f;

	// --- Building-Specific Properties ---

	/** For crafting station buildings: the station type this building provides. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Building|Station", meta=(EditCondition="bIsBuilding", EditConditionHides))
	EMOCraftingStation ProvidedStationType = EMOCraftingStation::None;

	/** For container buildings: number of inventory slots. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Building|Container", meta=(EditCondition="bIsBuilding", EditConditionHides, ClampMin="0"))
	int32 ContainerSlotCount = 0;

	/** For crafting stations: whether the station requires fuel to operate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Building|Station", meta=(EditCondition="bIsBuilding", EditConditionHides))
	bool bRequiresFuel = false;

	/** For crafting stations: max fuel capacity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Building|Station", meta=(EditCondition="bIsBuilding && bRequiresFuel", EditConditionHides, ClampMin="0.0"))
	float MaxFuel = 100.0f;

	/** For crafting stations: fuel consumption rate per second when active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Building|Station", meta=(EditCondition="bIsBuilding && bRequiresFuel", EditConditionHides, ClampMin="0.0"))
	float FuelConsumptionRate = 1.0f;

	/** For crafting stations: item IDs accepted as fuel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Recipe|Building|Station", meta=(EditCondition="bIsBuilding && bRequiresFuel", EditConditionHides))
	TArray<FName> AcceptedFuelItems;
};
