#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MORecipeDefinitionRow.h"

#include "MOCraftingSubsystem.generated.h"

class UMOKnowledgeComponent;
class UMOSkillsComponent;
class UMOInventoryComponent;

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

	/** XP granted to skills (SkillId -> XP amount). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	TMap<FName, float> XPGranted;
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
	 * Get the crafting time for a recipe.
	 * @param RecipeId The recipe to query
	 * @return Craft time in seconds (0 if not found)
	 */
	UFUNCTION(BlueprintPure, Category="MO|Crafting")
	float GetRecipeCraftTime(FName RecipeId) const;

private:
	/**
	 * Check if player has required knowledge for a recipe.
	 */
	bool HasRequiredKnowledge(
		const FMORecipeDefinitionRow* Recipe,
		UMOKnowledgeComponent* KnowledgeComponent,
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
