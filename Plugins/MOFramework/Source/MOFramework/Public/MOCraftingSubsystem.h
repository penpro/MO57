#pragma once

/**
 * =============================================================================
 * MOCraftingSubsystem.h
 * =============================================================================
 *
 * PURPOSE:
 *   World subsystem handling all crafting validation and execution.
 *   Single source of truth for "can this recipe be crafted" checks.
 *
 * RESPONSIBILITIES:
 *   - Recipe availability checks (knowledge, skills, discovery)
 *   - Crafting validation (ingredients, tools, station)
 *   - Craft execution (consume inputs, produce outputs, grant XP)
 *   - Tool effectiveness and degradation
 *
 * =============================================================================
 * BEST PRACTICES
 * =============================================================================
 *
 * 1. ALWAYS use IsRecipeAvailable() as the single source of truth for
 *    whether a player can see/access a recipe. Don't duplicate this logic.
 *
 * 2. Tool system uses TArray<FMOToolCapability> - items can have multiple
 *    tool types with different effectiveness ratings. Use FindBestTool()
 *    to search for matching tools, not direct ToolType comparisons.
 *
 * 3. Recipe DataTable uses EMOToolType enum (NOT FName strings).
 *    - Hatchet was merged into Axe (no separate Hatchet type)
 *    - Use StaticEnum<EMOToolType>() for enum-to-string conversion
 *
 * 4. Two-phase crafting: CanCraftRecipe() validates, ExecuteCraft() executes.
 *    Never skip validation before execution.
 *
 * 5. For queued crafting, use ProduceOutputsOnly() when ingredients were
 *    already consumed at queue time.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] TOOL TYPE ARRAYS: Changed from TArray<FName> to TArray<EMOToolType>.
 *   WHY: Type safety - FName allowed typos that compiled but failed silently.
 *   SYMPTOM: Compile error "cannot convert FName to EMOToolType".
 *   FIX: Replace FName("Hammer") with EMOToolType::Hammer.
 *   SEARCH: "MissingTools" in code for all affected callsites.
 *
 * [2024-02] MULTI-TOOL ITEMS: Items have ToolCapabilities array, not single ToolType.
 *   WHY: Realism - a hammerstone is both Hammer(0.8) and Chisel(0.5).
 *   SYMPTOM if using direct comparison: Multi-tool items not found, recipes
 *   requiring hammer fail even when hammerstone is in inventory.
 *   FIX: Use GetToolEffectiveness(Item, ToolType) instead of Item.ToolType==X.
 *   Also use FindBestTool() to search inventory for matching tools.
 *
 * [2024-02] HATCHET REMOVED: Merged into Axe enum value.
 *   WHY: Hatchet and Axe had identical functionality, redundant enum.
 *   SYMPTOM: Compile error "EMOToolType::Hatchet not found".
 *   FIX: Replace EMOToolType::Hatchet with EMOToolType::Axe in code and DataTables.
 *   SEARCH: Grep for "Hatchet" in .cpp, .h, and CSV/JSON recipe files.
 *
 * [2024-02] DEPRECATED MISSINGTOOLS FIELD: FMOCraftingValidation.MissingTools is
 *   deprecated. Use MissingRequiredTools + MissingOptionalTools instead.
 *   SYMPTOM: Warning in IDE, field may be removed in future.
 *   FIX: Access MissingRequiredTools for blocking tools, MissingOptionalTools
 *   for penalty-only tools. Both are TArray<EMOToolType>.
 *
 * [2024-02] VALIDATION BEFORE EXECUTION: Never call ExecuteCraft() without
 *   calling CanCraftRecipe() first.
 *   SYMPTOM if skipped: Ingredients consumed even when tools/station missing,
 *   player loses resources with no output.
 *   FIX: Always if (CanCraftRecipe(...)) { ExecuteCraft(...); }
 *
 * [SEVERITY GUIDE]
 *   TOOL TYPE ARRAYS: Compile-time error (cannot ship)
 *   MULTI-TOOL ITEMS: Logic bug (recipes fail unexpectedly)
 *   HATCHET REMOVED: Compile-time error (cannot ship)
 *   VALIDATION SKIP: Critical runtime bug (resource loss)
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 *
 * - MORecipeDefinitionRow.h    : Recipe DataTable row structure
 * - MOItemDefinitionRow.h      : Item definitions with ToolCapabilities
 * - MOCraftingUIController.h   : UI controller for crafting menu
 * - MOCraftingQueueComponent.h : Per-pawn crafting queue
 * - DT_Recipes                 : Recipe DataTable asset
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
 * When compile errors occur related to this file:
 * - Check if enum values changed (EMOToolType, EMOCraftingStation)
 * - Check if component interfaces changed
 * - Verify DataTable row structure matches expectations
 *
 * =============================================================================
 */

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MORecipeDefinitionRow.h"

#include "MOCraftingSubsystem.generated.h"

class UMOKnowledgeComponent;
class UMOSkillsComponent;
class UMOInventoryComponent;

/**
 * Result of checking recipe availability (knowledge/skill/discovery requirements).
 * This is the "can player ever do this" check, separate from "can they do it right now".
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMORecipeAvailability
{
	GENERATED_BODY()

	/** Whether the recipe is available to the player (meets all unlock requirements). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	bool bIsAvailable = false;

	/** Whether the recipe has been discovered (if bRequiresDiscovery is true). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	bool bIsDiscovered = true;

	/** Knowledge IDs that are missing. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	TArray<FName> MissingKnowledge;

	/** Required skill level (0 if met or no requirement). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	int32 RequiredSkillLevel = 0;

	/** Current skill level (only set if requirement not met). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	int32 CurrentSkillLevel = 0;

	/** Discovery knowledge ID (for UI to show what unlocks this). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	FName DiscoveryKnowledgeId = NAME_None;

	/** Discovery knowledge level required (for UI). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	int32 DiscoveryKnowledgeLevel = 0;

	/** Current discovery knowledge level (for UI progress). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	int32 CurrentDiscoveryKnowledgeLevel = 0;

	/** Human-readable reason if not available. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	FText UnavailableReason;
};

/**
 * Result of checking if a recipe can be crafted.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOCraftingValidation
{
	GENERATED_BODY()

	/** Whether the recipe can be crafted. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	bool bCanCraft = false;

	/** Human-readable reason if cannot craft. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	FText FailureReason;

	/** Missing ingredients (ItemDefId -> quantity needed). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	TMap<FName, int32> MissingIngredients;

	/** Missing knowledge IDs. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	TArray<FName> MissingKnowledge;

	/** Required skill level if not met (0 if met). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	int32 RequiredSkillLevel = 0;

	/** Current skill level if requirement not met (0 if met). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	int32 CurrentSkillLevel = 0;

	/** Whether the correct station is being used. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	bool bCorrectStation = true;

	/** Tool types that are missing (deprecated - use MissingRequiredTools). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	TArray<EMOToolType> MissingTools;

	/** Tool types that are absolutely required and missing - blocks crafting. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	TArray<EMOToolType> MissingRequiredTools;

	/** Tool types that are optional but missing - applies time/quality penalty. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	TArray<EMOToolType> MissingOptionalTools;

	/**
	 * Craft time multiplier from missing optional tools.
	 * Multiply base craft time by this value. 1.0 = no penalty.
	 */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	float MissingToolTimeMultiplier = 1.0f;

	/**
	 * Quality multiplier from missing optional tools.
	 * Multiply output quality by this value. 1.0 = no penalty.
	 */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	float MissingToolQualityMultiplier = 1.0f;

	/** Whether the recipe has been discovered (only relevant if recipe requires discovery). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	bool bRecipeDiscovered = true;

	/** Whether the station has fuel (only relevant for fuel-required stations). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	bool bHasFuel = true;
};

/**
 * Result of executing a craft.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOCraftResult
{
	GENERATED_BODY()

	/** Whether the craft succeeded. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	bool bSuccess = false;

	/** Items produced (ItemDefId -> quantity). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	TMap<FName, int32> ProducedItems;

	/** Items that couldn't be added due to inventory full (ItemDefId -> quantity). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	TMap<FName, int32> FailedItems;

	/** XP granted to skills (SkillId -> XP amount). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	TMap<FName, float> XPGranted;

	/** Returns true if any items couldn't be added to inventory. */
	bool HasFailedItems() const { return FailedItems.Num() > 0; }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnCraftCompleted, FName, RecipeId, const FMOCraftResult&, Result);

/**
 * World subsystem that handles crafting logic and validation.
 */
UCLASS()
class MOFRAMEWORK_API UMOCraftingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Delegates
	UPROPERTY(BlueprintAssignable, Category="MO|Crafting|Events")
	FMOOnCraftCompleted OnCraftCompleted;

	// =========================================================================
	// CENTRALIZED AVAILABILITY CHECK
	// =========================================================================

	/**
	 * Check if a recipe is available to the player (meets unlock requirements).
	 * This checks discovery, knowledge, and skill requirements.
	 * Does NOT check ingredients, tools, or station - use CanCraftRecipe for that.
	 *
	 * Use this as the single source of truth for "can player see/use this recipe".
	 *
	 * @param RecipeId The recipe to check
	 * @param KnowledgeComponent Player's knowledge
	 * @param SkillsComponent Player's skills
	 * @return Availability result with details
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting")
	FMORecipeAvailability IsRecipeAvailable(
		FName RecipeId,
		UMOKnowledgeComponent* KnowledgeComponent,
		UMOSkillsComponent* SkillsComponent
	) const;

	/**
	 * Overload that takes a recipe pointer directly (for internal use).
	 */
	FMORecipeAvailability IsRecipeAvailable(
		const FMORecipeDefinitionRow* Recipe,
		UMOKnowledgeComponent* KnowledgeComponent,
		UMOSkillsComponent* SkillsComponent
	) const;

	// =========================================================================
	// RECIPE QUERIES
	// =========================================================================

	/**
	 * Get all recipes that the player can potentially craft (meets knowledge/skill requirements).
	 * Does NOT check for ingredient availability.
	 * @param KnowledgeComponent Player's knowledge (for recipe visibility)
	 * @param SkillsComponent Player's skills (for level requirements)
	 * @param Station The crafting station being used (None = hand crafting)
	 * @param OutRecipeIds Array to fill with available recipe IDs
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting")
	void GetAvailableRecipes(
		UMOKnowledgeComponent* KnowledgeComponent,
		UMOSkillsComponent* SkillsComponent,
		EMOCraftingStation Station,
		TArray<FName>& OutRecipeIds
	) const;

	/**
	 * Get all recipes the player can craft RIGHT NOW (has ingredients, meets all requirements).
	 * @param KnowledgeComponent Player's knowledge
	 * @param SkillsComponent Player's skills
	 * @param InventoryComponent Player's inventory (for ingredient check)
	 * @param Station The crafting station being used
	 * @param OutRecipeIds Array to fill with craftable recipe IDs
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting")
	void GetCraftableRecipes(
		UMOKnowledgeComponent* KnowledgeComponent,
		UMOSkillsComponent* SkillsComponent,
		UMOInventoryComponent* InventoryComponent,
		EMOCraftingStation Station,
		TArray<FName>& OutRecipeIds
	) const;

	/**
	 * Check if a specific recipe can be crafted, with detailed failure info.
	 * @param RecipeId The recipe to check
	 * @param KnowledgeComponent Player's knowledge
	 * @param SkillsComponent Player's skills
	 * @param InventoryComponent Player's inventory
	 * @param Station The crafting station being used
	 * @return Validation result with details
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting")
	FMOCraftingValidation CanCraftRecipe(
		FName RecipeId,
		UMOKnowledgeComponent* KnowledgeComponent,
		UMOSkillsComponent* SkillsComponent,
		UMOInventoryComponent* InventoryComponent,
		EMOCraftingStation Station
	) const;

	/**
	 * Execute a craft, consuming ingredients and producing outputs.
	 * @param RecipeId The recipe to craft
	 * @param InventoryComponent Inventory to consume from and add to
	 * @param SkillsComponent Skills to grant XP to
	 * @return Result of the craft
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting")
	FMOCraftResult ExecuteCraft(
		FName RecipeId,
		UMOInventoryComponent* InventoryComponent,
		UMOSkillsComponent* SkillsComponent
	);

	/**
	 * Produce outputs for a recipe without consuming ingredients.
	 * Use this when ingredients have already been consumed (e.g., from crafting queue).
	 * @param RecipeId The recipe to produce outputs for
	 * @param InventoryComponent Inventory to add outputs to
	 * @param SkillsComponent Skills to grant XP to
	 * @return Result of the production
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting")
	FMOCraftResult ProduceOutputsOnly(
		FName RecipeId,
		UMOInventoryComponent* InventoryComponent,
		UMOSkillsComponent* SkillsComponent
	);

	/**
	 * Get the crafting time for a recipe.
	 * @param RecipeId The recipe to query
	 * @return Craft time in seconds (0 if not found)
	 */
	UFUNCTION(BlueprintPure, Category="MO|Crafting")
	float GetRecipeCraftTime(FName RecipeId) const;

	// --- Tool System ---

	/**
	 * Check if the inventory has all required tools for a recipe.
	 * @param Recipe The recipe to check
	 * @param Inventory The inventory to search
	 * @param OutMissingTools Optional array to fill with missing tool types
	 * @return True if all tools are present with sufficient quality
	 */
	bool HasRequiredTools(
		const FMORecipeDefinitionRow* Recipe,
		UMOInventoryComponent* Inventory,
		TArray<EMOToolType>* OutMissingTools = nullptr
	) const;

	/**
	 * Degrade tools in the inventory based on recipe requirements.
	 * Call this after a successful craft to reduce tool durability.
	 * @param RecipeId The recipe that was crafted
	 * @param Inventory The inventory containing the tools
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting")
	void DegradeToolsForRecipe(FName RecipeId, UMOInventoryComponent* Inventory);

	/**
	 * Get the adjusted craft time for a recipe based on tool quality.
	 * Higher quality tools reduce craft time.
	 * @param RecipeId The recipe to query
	 * @param Inventory The inventory to check for tools
	 * @return Adjusted craft time in seconds
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting")
	float GetAdjustedCraftTime(FName RecipeId, UMOInventoryComponent* Inventory) const;

	/**
	 * Find the best tool of a given type in the inventory.
	 * Searches through items with matching tool capabilities.
	 * @param Inventory The inventory to search
	 * @param ToolType The tool type to find
	 * @param MinEffectiveness Minimum effectiveness required (0.0 - 1.0)
	 * @param OutItemGuid The GUID of the best matching tool
	 * @param OutEffectiveness The effectiveness of the found tool for this type
	 * @return True if a matching tool was found
	 */
	bool FindBestTool(
		UMOInventoryComponent* Inventory,
		EMOToolType ToolType,
		float MinEffectiveness,
		FGuid& OutItemGuid,
		float& OutEffectiveness
	) const;

private:
	/**
	 * Check if player has required knowledge for a recipe.
	 * Checks KnowledgeComponent first, then falls back to SkillsComponent
	 * (skill level >= 1) for each knowledge entry.
	 */
	bool HasRequiredKnowledge(
		const FMORecipeDefinitionRow* Recipe,
		UMOKnowledgeComponent* KnowledgeComponent,
		UMOSkillsComponent* SkillsComponent,
		TArray<FName>* OutMissingKnowledge = nullptr
	) const;

	/**
	 * Check if player meets skill requirements for a recipe.
	 */
	bool MeetsSkillRequirements(
		const FMORecipeDefinitionRow* Recipe,
		UMOSkillsComponent* SkillsComponent,
		int32* OutRequiredLevel = nullptr,
		int32* OutCurrentLevel = nullptr
	) const;

	/**
	 * Check if player has all ingredients for a recipe.
	 */
	bool HasIngredients(
		const FMORecipeDefinitionRow* Recipe,
		UMOInventoryComponent* InventoryComponent,
		UMOKnowledgeComponent* KnowledgeComponent,
		TMap<FName, int32>* OutMissingIngredients = nullptr
	) const;
};
